#pragma once

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QString>
#include <QList>
#include <optional>

namespace Mc {

// Mirrors the 'files' table row
struct FileRecord {
    qint64      id = -1;
    QString     path;
    QString     filename;
    qint64      sizeBytes = 0;
    QString     container;
    double      durationSec = 0.0;
    qint64      overallBitrate = 0;
    QString     originalLanguage;
    qint64      scanTime = 0;
    qint64      scanRunId = -1;
    bool        needsRescan = false;
};

// Mirrors the 'streams' table row
struct StreamRecord {
    qint64      id = -1;
    qint64      fileId = -1;
    int         streamIndex = 0;
    QString     codecType;      // video|audio|subtitle|data
    QString     codecName;
    QString     language;       // ISO 639-2
    QString     title;
    QString     trackType;      // classified: main|commentary|sdh|forced|signs|...
    double      typeConfidence = 0.0;
    int         channels = 0;
    int         sampleRate = 0;
    qint64      bitRate = 0;
    int         width = 0;
    int         height = 0;
    QString     hdrFormat;
    bool        isDefault = false;
    bool        isForced = false;
    bool        isHearingImpaired = false;
    bool        isVisualImpaired = false;
    QString     pixelFormat;
    QString     frameRate;
    QString     codecLevel;
    QString     codecProfile;
    QString     extraJson;
};

// Mirrors the 'jobs' table row
struct JobRecord {
    qint64      id = -1;
    qint64      fileId = -1;
    QString     status;         // pending|running|done|failed|cancelled
    QString     jobType;        // remux|tag_edit
    QString     commandArgsJson;
    bool        dryRun = true;
    qint64      createdAt = 0;
    qint64      startedAt = 0;
    qint64      finishedAt = 0;
    int         resultCode = -1;
    QString     outputLog;
    qint64      estimatedSavingBytes = 0;
};

/**
 * DatabaseManager — singleton access to the SQLite database.
 *
 * All SQL lives here. No other class should issue raw SQL queries.
 * The database file is stored in QStandardPaths::AppDataLocation.
 */
class DatabaseManager : public QObject
{
    Q_OBJECT
public:
    static DatabaseManager& instance();

    // Opens (or creates) the database. Call once at startup.
    bool open(const QString& dbPath = {});
    void close();
    bool isOpen() const;

    QString databasePath() const { return m_dbPath; }

    // ── Scan runs ────────────────────────────────────────────────────────────
    [[nodiscard]] qint64 beginScanRun(const QString& rootPath);
    void endScanRun(qint64 scanRunId, int added, int updated, int removed);

    // ── Files ────────────────────────────────────────────────────────────────
    [[nodiscard]] std::optional<qint64> upsertFile(const FileRecord& rec);
    [[nodiscard]] std::optional<FileRecord> fileById(qint64 id) const;
    [[nodiscard]] std::optional<FileRecord> fileByPath(const QString& path) const;
    QList<FileRecord> allFiles() const;
    bool deleteFile(qint64 fileId);

    // ── Streams ──────────────────────────────────────────────────────────────
    bool insertStreams(qint64 fileId, const QList<StreamRecord>& streams);
    bool deleteStreamsForFile(qint64 fileId);
    QList<StreamRecord> streamsForFile(qint64 fileId) const;
    QList<StreamRecord> allStreams() const;

    // ── Jobs ─────────────────────────────────────────────────────────────────
    [[nodiscard]] qint64 insertJob(const JobRecord& job);
    bool updateJobStatus(qint64 jobId, const QString& status, int resultCode = -1, const QString& log = {});
    QList<JobRecord> pendingJobs() const;
    QList<JobRecord> allJobs() const;

    // ── Preferences ──────────────────────────────────────────────────────────
    bool setPref(const QString& key, const QString& value);
    QString getPref(const QString& key, const QString& defaultValue = {}) const;

signals:
    void databaseOpened();
    void fileAdded(qint64 fileId);
    void fileUpdated(qint64 fileId);
    void fileDeleted(qint64 fileId);
    void jobStatusChanged(qint64 jobId, const QString& status);

private:
    explicit DatabaseManager(QObject* parent = nullptr);
    ~DatabaseManager() override;
    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;

    bool initSchema();
    bool runMigrations();
    [[nodiscard]] QSqlQuery prepare(const QString& sql) const;

    QSqlDatabase m_db;
    QString      m_dbPath;
};

} // namespace Mc
