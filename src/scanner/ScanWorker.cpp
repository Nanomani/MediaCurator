#include "scanner/ScanWorker.h"
#include "core/DatabaseManager.h"

#include <QDirIterator>
#include <QFileInfo>
#include <QDebug>

namespace Mc {

const QStringList& ScanWorker::videoExtensions()
{
    static const QStringList exts = {
        "mkv", "mka", "mks",       // Matroska
        "mp4", "m4v", "m4a",       // MP4
        "avi",                      // AVI
        "wmv", "wma", "asf",       // Windows Media
        "m2ts", "ts", "m2t",       // MPEG transport stream
        "vob",                      // DVD
        "ogm", "ogg", "ogv",       // Ogg
        "flv",                      // Flash
        "mov", "qt",               // QuickTime
        "webm",                     // WebM
        "divx",                     // DivX
    };
    return exts;
}

ScanWorker::ScanWorker(const QString& ffprobePath, QObject* parent)
    : QObject(parent)
    , m_ffprobePath(ffprobePath)
{}

void ScanWorker::run()
{
    if (m_rootPath.isEmpty()) {
        emit error("No scan path set");
        emit finished(0, 0, 0, 0);
        return;
    }

    // Collect all video files first so we can report progress
    const QStringList files = collectVideoFiles(m_rootPath);
    if (files.isEmpty()) {
        emit finished(0, 0, 0, 0);
        return;
    }

    auto& db = DatabaseManager::instance();
    const qint64 scanRunId = db.beginScanRun(m_rootPath);

    FfprobeScanner scanner(m_ffprobePath);
    int added = 0, updated = 0, failed = 0;
    int index = 0;

    for (const QString& filePath : files) {
        if (m_cancelled.loadRelaxed()) {
            qDebug() << "ScanWorker: cancelled at" << index << "/" << files.size();
            break;
        }

        emit progress(++index, files.size(), QFileInfo(filePath).fileName());

        // Check if we already have this file (for add vs update counting)
        const bool existed = db.fileByPath(filePath).has_value();

        const auto result = scanner.scanFile(filePath, scanRunId);
        if (!result.success) {
            qWarning() << "ScanWorker: scan failed for" << filePath << "—" << result.errorMessage;
            ++failed;
            emit fileScanned(filePath, false);
            continue;
        }

        const auto fileId = db.upsertFile(result.file);
        if (!fileId) {
            qWarning() << "ScanWorker: DB upsert failed for" << filePath;
            ++failed;
            emit fileScanned(filePath, false);
            continue;
        }

        db.insertStreams(*fileId, result.streams);

        if (existed) ++updated;
        else         ++added;

        emit fileScanned(filePath, true);
    }

    db.endScanRun(scanRunId, added, updated, 0 /* removals handled separately */);
    emit finished(index, added, updated, failed);
}

QStringList ScanWorker::collectVideoFiles(const QString& rootPath) const
{
    QStringList result;
    const QStringList& exts = videoExtensions();

    QDirIterator it(rootPath, QDirIterator::Subdirectories | QDirIterator::FollowSymlinks);
    while (it.hasNext()) {
        const QString path = it.next();
        if (m_cancelled.loadRelaxed())
            break;

        const QFileInfo fi(path);
        if (!fi.isFile()) continue;

        const QString ext = fi.suffix().toLower();
        if (exts.contains(ext))
            result.append(path);
    }
    return result;
}

} // namespace Mc
