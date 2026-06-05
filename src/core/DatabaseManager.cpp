#include "core/DatabaseManager.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QDir>
#include <QDateTime>
#include <QDebug>
#include <QVariant>

namespace Mc {

// ── Singleton ─────────────────────────────────────────────────────────────────

DatabaseManager& DatabaseManager::instance()
{
    static DatabaseManager s_instance;
    return s_instance;
}

DatabaseManager::DatabaseManager(QObject* parent)
    : QObject(parent)
{}

DatabaseManager::~DatabaseManager()
{
    close();
}

// ── Open / Close ──────────────────────────────────────────────────────────────

bool DatabaseManager::open(const QString& dbPath)
{
    if (m_db.isOpen())
        return true;

    if (dbPath.isEmpty()) {
        const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir().mkpath(dataDir);
        m_dbPath = dataDir + "/mediacurator.db";
    } else {
        m_dbPath = dbPath;
    }

    m_db = QSqlDatabase::addDatabase("QSQLITE", "mc_main");
    m_db.setDatabaseName(m_dbPath);

    if (!m_db.open()) {
        qCritical() << "DatabaseManager: failed to open database:" << m_db.lastError().text();
        return false;
    }

    // Enable WAL mode and foreign keys
    QSqlQuery pragma(m_db);
    pragma.exec("PRAGMA journal_mode=WAL");
    pragma.exec("PRAGMA foreign_keys=ON");
    pragma.exec("PRAGMA synchronous=NORMAL");

    if (!initSchema()) {
        qCritical() << "DatabaseManager: schema init failed";
        return false;
    }

    emit databaseOpened();
    return true;
}

void DatabaseManager::close()
{
    if (m_db.isOpen())
        m_db.close();
}

bool DatabaseManager::isOpen() const
{
    return m_db.isOpen();
}

// ── Schema ────────────────────────────────────────────────────────────────────

bool DatabaseManager::initSchema()
{
    static const char* schema = R"(
        CREATE TABLE IF NOT EXISTS schema_version (
            version INTEGER NOT NULL
        );

        CREATE TABLE IF NOT EXISTS scan_runs (
            id             INTEGER PRIMARY KEY AUTOINCREMENT,
            start_time     INTEGER NOT NULL,
            end_time       INTEGER,
            root_path      TEXT NOT NULL,
            files_scanned  INTEGER DEFAULT 0,
            files_added    INTEGER DEFAULT 0,
            files_updated  INTEGER DEFAULT 0,
            files_removed  INTEGER DEFAULT 0
        );

        CREATE TABLE IF NOT EXISTS files (
            id                INTEGER PRIMARY KEY AUTOINCREMENT,
            path              TEXT UNIQUE NOT NULL,
            filename          TEXT NOT NULL,
            size_bytes        INTEGER NOT NULL DEFAULT 0,
            container         TEXT,
            duration_s        REAL DEFAULT 0,
            overall_bitrate   INTEGER DEFAULT 0,
            original_language TEXT,
            scan_time         INTEGER NOT NULL DEFAULT 0,
            scan_run_id       INTEGER REFERENCES scan_runs(id),
            needs_rescan      INTEGER DEFAULT 0
        );

        CREATE INDEX IF NOT EXISTS idx_files_path ON files(path);

        CREATE TABLE IF NOT EXISTS streams (
            id                  INTEGER PRIMARY KEY AUTOINCREMENT,
            file_id             INTEGER NOT NULL REFERENCES files(id) ON DELETE CASCADE,
            stream_index        INTEGER NOT NULL,
            codec_type          TEXT NOT NULL,
            codec_name          TEXT,
            language            TEXT,
            title               TEXT,
            track_type          TEXT DEFAULT 'main',
            type_confidence     REAL DEFAULT 0,
            channels            INTEGER DEFAULT 0,
            sample_rate         INTEGER DEFAULT 0,
            bit_rate            INTEGER DEFAULT 0,
            width               INTEGER DEFAULT 0,
            height              INTEGER DEFAULT 0,
            hdr_format          TEXT,
            is_default          INTEGER DEFAULT 0,
            is_forced           INTEGER DEFAULT 0,
            is_hearing_impaired INTEGER DEFAULT 0,
            is_visual_impaired  INTEGER DEFAULT 0,
            pixel_format        TEXT,
            frame_rate          TEXT,
            codec_level         TEXT,
            codec_profile       TEXT,
            extra_json          TEXT
        );

        CREATE INDEX IF NOT EXISTS idx_streams_file_id ON streams(file_id);

        CREATE TABLE IF NOT EXISTS jobs (
            id                      INTEGER PRIMARY KEY AUTOINCREMENT,
            file_id                 INTEGER REFERENCES files(id),
            status                  TEXT DEFAULT 'pending',
            job_type                TEXT NOT NULL,
            command_args_json       TEXT NOT NULL DEFAULT '[]',
            dry_run                 INTEGER DEFAULT 1,
            created_at              INTEGER NOT NULL DEFAULT 0,
            started_at              INTEGER DEFAULT 0,
            finished_at             INTEGER DEFAULT 0,
            result_code             INTEGER DEFAULT -1,
            output_log              TEXT,
            estimated_saving_bytes  INTEGER DEFAULT 0
        );

        CREATE TABLE IF NOT EXISTS preferences (
            key    TEXT PRIMARY KEY,
            value  TEXT
        );

        CREATE TABLE IF NOT EXISTS codec_hierarchy (
            id             INTEGER PRIMARY KEY AUTOINCREMENT,
            lossless_codec TEXT NOT NULL,
            lossy_codec    TEXT NOT NULL,
            notes          TEXT,
            UNIQUE(lossless_codec, lossy_codec)
        );
    )";

    // Execute each statement separated by semicolons
    const QStringList statements = QString::fromUtf8(schema).split(';', Qt::SkipEmptyParts);
    for (const QString& stmt : statements) {
        const QString trimmed = stmt.trimmed();
        if (trimmed.isEmpty()) continue;
        QSqlQuery q(m_db);
        if (!q.exec(trimmed)) {
            qWarning() << "Schema statement failed:" << q.lastError().text() << "\n" << trimmed;
            return false;
        }
    }

    // Seed codec hierarchy if empty
    QSqlQuery check(m_db);
    check.exec("SELECT COUNT(*) FROM codec_hierarchy");
    if (check.next() && check.value(0).toInt() == 0) {
        const QList<QPair<QString,QString>> pairs = {
            {"truehd",  "eac3"},
            {"truehd",  "ac3"},
            {"dtshd_ma","dts"},
            {"flac",    "aac"},
            {"flac",    "mp3"},
            {"pcm_s16le","aac"},
            {"pcm_s24le","aac"},
            {"pcm_s32le","aac"},
        };
        QSqlQuery ins(m_db);
        ins.prepare("INSERT OR IGNORE INTO codec_hierarchy(lossless_codec,lossy_codec) VALUES(?,?)");
        for (const auto& [lossless, lossy] : pairs) {
            ins.addBindValue(lossless);
            ins.addBindValue(lossy);
            ins.exec();
        }
    }

    return true;
}

// ── Scan runs ─────────────────────────────────────────────────────────────────

qint64 DatabaseManager::beginScanRun(const QString& rootPath)
{
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO scan_runs(start_time, root_path) VALUES(?, ?)");
    q.addBindValue(QDateTime::currentSecsSinceEpoch());
    q.addBindValue(rootPath);
    if (!q.exec()) {
        qWarning() << "beginScanRun failed:" << q.lastError().text();
        return -1;
    }
    return q.lastInsertId().toLongLong();
}

void DatabaseManager::endScanRun(qint64 scanRunId, int added, int updated, int removed)
{
    QSqlQuery q(m_db);
    q.prepare("UPDATE scan_runs SET end_time=?, files_added=?, files_updated=?, files_removed=? WHERE id=?");
    q.addBindValue(QDateTime::currentSecsSinceEpoch());
    q.addBindValue(added);
    q.addBindValue(updated);
    q.addBindValue(removed);
    q.addBindValue(scanRunId);
    q.exec();
}

// ── Files ─────────────────────────────────────────────────────────────────────

std::optional<qint64> DatabaseManager::upsertFile(const FileRecord& rec)
{
    QSqlQuery q(m_db);
    q.prepare(R"(
        INSERT INTO files(path, filename, size_bytes, container, duration_s,
                          overall_bitrate, original_language, scan_time, scan_run_id, needs_rescan)
        VALUES(?,?,?,?,?,?,?,?,?,?)
        ON CONFLICT(path) DO UPDATE SET
            filename=excluded.filename,
            size_bytes=excluded.size_bytes,
            container=excluded.container,
            duration_s=excluded.duration_s,
            overall_bitrate=excluded.overall_bitrate,
            original_language=excluded.original_language,
            scan_time=excluded.scan_time,
            scan_run_id=excluded.scan_run_id,
            needs_rescan=excluded.needs_rescan
    )");
    q.addBindValue(rec.path);
    q.addBindValue(rec.filename);
    q.addBindValue(rec.sizeBytes);
    q.addBindValue(rec.container);
    q.addBindValue(rec.durationSec);
    q.addBindValue(rec.overallBitrate);
    q.addBindValue(rec.originalLanguage);
    q.addBindValue(rec.scanTime);
    q.addBindValue(rec.scanRunId > 0 ? QVariant(rec.scanRunId) : QVariant());
    q.addBindValue(rec.needsRescan ? 1 : 0);

    if (!q.exec()) {
        qWarning() << "upsertFile failed:" << q.lastError().text();
        return std::nullopt;
    }

    // Return the rowid — for upsert, lastInsertId returns the row id
    const qint64 insertedId = q.lastInsertId().toLongLong();
    if (insertedId > 0)
        return insertedId;

    // Was an update — fetch the existing id
    QSqlQuery sel(m_db);
    sel.prepare("SELECT id FROM files WHERE path=?");
    sel.addBindValue(rec.path);
    if (sel.exec() && sel.next())
        return sel.value(0).toLongLong();

    return std::nullopt;
}

std::optional<FileRecord> DatabaseManager::fileByPath(const QString& path) const
{
    QSqlQuery q(m_db);
    q.prepare("SELECT * FROM files WHERE path=?");
    q.addBindValue(path);
    if (!q.exec() || !q.next())
        return std::nullopt;

    FileRecord r;
    r.id               = q.value("id").toLongLong();
    r.path             = q.value("path").toString();
    r.filename         = q.value("filename").toString();
    r.sizeBytes        = q.value("size_bytes").toLongLong();
    r.container        = q.value("container").toString();
    r.durationSec      = q.value("duration_s").toDouble();
    r.overallBitrate   = q.value("overall_bitrate").toLongLong();
    r.originalLanguage = q.value("original_language").toString();
    r.scanTime         = q.value("scan_time").toLongLong();
    r.needsRescan      = q.value("needs_rescan").toInt() != 0;
    return r;
}

QList<FileRecord> DatabaseManager::allFiles() const
{
    QList<FileRecord> result;
    QSqlQuery q("SELECT * FROM files ORDER BY filename", m_db);
    while (q.next()) {
        FileRecord r;
        r.id               = q.value("id").toLongLong();
        r.path             = q.value("path").toString();
        r.filename         = q.value("filename").toString();
        r.sizeBytes        = q.value("size_bytes").toLongLong();
        r.container        = q.value("container").toString();
        r.durationSec      = q.value("duration_s").toDouble();
        r.overallBitrate   = q.value("overall_bitrate").toLongLong();
        r.originalLanguage = q.value("original_language").toString();
        r.scanTime         = q.value("scan_time").toLongLong();
        r.needsRescan      = q.value("needs_rescan").toInt() != 0;
        result.append(r);
    }
    return result;
}

bool DatabaseManager::deleteFile(qint64 fileId)
{
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM files WHERE id=?");
    q.addBindValue(fileId);
    const bool ok = q.exec();
    if (ok)
        emit fileDeleted(fileId);
    return ok;
}

// ── Streams ───────────────────────────────────────────────────────────────────

bool DatabaseManager::insertStreams(qint64 fileId, const QList<StreamRecord>& streams)
{
    if (!deleteStreamsForFile(fileId))
        return false;

    QSqlQuery q(m_db);
    q.prepare(R"(
        INSERT INTO streams(file_id, stream_index, codec_type, codec_name, language, title,
            track_type, type_confidence, channels, sample_rate, bit_rate, width, height,
            hdr_format, is_default, is_forced, is_hearing_impaired, is_visual_impaired,
            pixel_format, frame_rate, codec_level, codec_profile, extra_json)
        VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)
    )");

    m_db.transaction();
    for (const auto& s : streams) {
        q.addBindValue(fileId);
        q.addBindValue(s.streamIndex);
        q.addBindValue(s.codecType);
        q.addBindValue(s.codecName);
        q.addBindValue(s.language);
        q.addBindValue(s.title);
        q.addBindValue(s.trackType);
        q.addBindValue(s.typeConfidence);
        q.addBindValue(s.channels);
        q.addBindValue(s.sampleRate);
        q.addBindValue(s.bitRate);
        q.addBindValue(s.width);
        q.addBindValue(s.height);
        q.addBindValue(s.hdrFormat);
        q.addBindValue(s.isDefault ? 1 : 0);
        q.addBindValue(s.isForced ? 1 : 0);
        q.addBindValue(s.isHearingImpaired ? 1 : 0);
        q.addBindValue(s.isVisualImpaired ? 1 : 0);
        q.addBindValue(s.pixelFormat);
        q.addBindValue(s.frameRate);
        q.addBindValue(s.codecLevel);
        q.addBindValue(s.codecProfile);
        q.addBindValue(s.extraJson);
        if (!q.exec()) {
            qWarning() << "insertStreams row failed:" << q.lastError().text();
            m_db.rollback();
            return false;
        }
    }
    m_db.commit();
    return true;
}

bool DatabaseManager::deleteStreamsForFile(qint64 fileId)
{
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM streams WHERE file_id=?");
    q.addBindValue(fileId);
    return q.exec();
}

QList<StreamRecord> DatabaseManager::streamsForFile(qint64 fileId) const
{
    QList<StreamRecord> result;
    QSqlQuery q(m_db);
    q.prepare("SELECT * FROM streams WHERE file_id=? ORDER BY stream_index");
    q.addBindValue(fileId);
    if (!q.exec()) return result;

    while (q.next()) {
        StreamRecord s;
        s.id                = q.value("id").toLongLong();
        s.fileId            = q.value("file_id").toLongLong();
        s.streamIndex       = q.value("stream_index").toInt();
        s.codecType         = q.value("codec_type").toString();
        s.codecName         = q.value("codec_name").toString();
        s.language          = q.value("language").toString();
        s.title             = q.value("title").toString();
        s.trackType         = q.value("track_type").toString();
        s.typeConfidence    = q.value("type_confidence").toDouble();
        s.channels          = q.value("channels").toInt();
        s.sampleRate        = q.value("sample_rate").toInt();
        s.bitRate           = q.value("bit_rate").toLongLong();
        s.width             = q.value("width").toInt();
        s.height            = q.value("height").toInt();
        s.hdrFormat         = q.value("hdr_format").toString();
        s.isDefault         = q.value("is_default").toInt() != 0;
        s.isForced          = q.value("is_forced").toInt() != 0;
        s.isHearingImpaired = q.value("is_hearing_impaired").toInt() != 0;
        s.isVisualImpaired  = q.value("is_visual_impaired").toInt() != 0;
        s.pixelFormat       = q.value("pixel_format").toString();
        s.frameRate         = q.value("frame_rate").toString();
        s.codecLevel        = q.value("codec_level").toString();
        s.codecProfile      = q.value("codec_profile").toString();
        s.extraJson         = q.value("extra_json").toString();
        result.append(s);
    }
    return result;
}

// ── Jobs ──────────────────────────────────────────────────────────────────────

qint64 DatabaseManager::insertJob(const JobRecord& job)
{
    QSqlQuery q(m_db);
    q.prepare(R"(
        INSERT INTO jobs(file_id, status, job_type, command_args_json,
                         dry_run, created_at, estimated_saving_bytes)
        VALUES(?,?,?,?,?,?,?)
    )");
    q.addBindValue(job.fileId > 0 ? QVariant(job.fileId) : QVariant());
    q.addBindValue(job.status.isEmpty() ? "pending" : job.status);
    q.addBindValue(job.jobType);
    q.addBindValue(job.commandArgsJson);
    q.addBindValue(job.dryRun ? 1 : 0);
    q.addBindValue(QDateTime::currentSecsSinceEpoch());
    q.addBindValue(job.estimatedSavingBytes);
    if (!q.exec()) {
        qWarning() << "insertJob failed:" << q.lastError().text();
        return -1;
    }
    return q.lastInsertId().toLongLong();
}

bool DatabaseManager::updateJobStatus(qint64 jobId, const QString& status, int resultCode, const QString& log)
{
    QSqlQuery q(m_db);
    if (status == "running") {
        q.prepare("UPDATE jobs SET status=?, started_at=? WHERE id=?");
        q.addBindValue(status);
        q.addBindValue(QDateTime::currentSecsSinceEpoch());
        q.addBindValue(jobId);
    } else {
        q.prepare("UPDATE jobs SET status=?, finished_at=?, result_code=?, output_log=? WHERE id=?");
        q.addBindValue(status);
        q.addBindValue(QDateTime::currentSecsSinceEpoch());
        q.addBindValue(resultCode);
        q.addBindValue(log);
        q.addBindValue(jobId);
    }
    const bool ok = q.exec();
    if (ok)
        emit jobStatusChanged(jobId, status);
    return ok;
}

// ── Preferences ───────────────────────────────────────────────────────────────

bool DatabaseManager::setPref(const QString& key, const QString& value)
{
    QSqlQuery q(m_db);
    q.prepare("INSERT OR REPLACE INTO preferences(key,value) VALUES(?,?)");
    q.addBindValue(key);
    q.addBindValue(value);
    return q.exec();
}

QString DatabaseManager::getPref(const QString& key, const QString& defaultValue) const
{
    QSqlQuery q(m_db);
    q.prepare("SELECT value FROM preferences WHERE key=?");
    q.addBindValue(key);
    if (q.exec() && q.next())
        return q.value(0).toString();
    return defaultValue;
}

} // namespace Mc
