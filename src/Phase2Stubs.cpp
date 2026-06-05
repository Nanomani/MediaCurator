// Phase 2 stubs — full implementation in later phases
// These exist so the Phase 1 build compiles cleanly.

#include "core/AppSettings.h"
#include "core/ExternalTools.h"
#include "scanner/OriginalLanguageDetector.h"
#include "engine/RuleEngine.h"
#include "engine/ActionEngine.h"
#include "engine/JobQueue.h"
#include "engine/RemuxJob.h"
#include "classifier/TrackClassifier.h"
#include "classifier/RegexClassifier.h"
#include "ui/McLibraryPanel.h"
#include "ui/McFilterPanel.h"
#include "ui/McJobPanel.h"
#include "ui/McPreviewDialog.h"
#include "ui/McSettingsDialog.h"

#include <QSettings>
#include <QCoreApplication>
#include <QDebug>

namespace Mc {

// ── AppSettings ────────────────────────────────────────────────────────────────

AppSettings& AppSettings::instance()
{
    static AppSettings s;
    return s;
}

QVariant AppSettings::value(const QString& key, const QVariant& defaultValue) const
{
    QSettings s;
    return s.value(key, defaultValue);
}

void AppSettings::setValue(const QString& key, const QVariant& value)
{
    QSettings s;
    s.setValue(key, value);
}

// ── ExternalTools ─────────────────────────────────────────────────────────────

ExternalTools& ExternalTools::instance()
{
    static ExternalTools s;
    return s;
}

QString ExternalTools::findTool(const QString& name) const
{
    const QString exeDir = QCoreApplication::applicationDirPath();
#ifdef Q_OS_WIN
    const QString platformDir = "windows";
    const QString ext = ".exe";
#elif defined(Q_OS_MACOS)
    const QString platformDir = "macos";
    const QString ext;
#else
    const QString platformDir = "linux";
    const QString ext;
#endif
    const QString candidate = exeDir + "/tools/" + platformDir + "/" + name + ext;
    return QFile::exists(candidate) ? candidate : name + ext; // fallback: hope it's in PATH
}

QString ExternalTools::ffprobePath()     const { if (m_ffprobePath.isEmpty()) m_ffprobePath = findTool("ffprobe"); return m_ffprobePath; }
QString ExternalTools::mkvmergePath()    const { if (m_mkvmergePath.isEmpty()) m_mkvmergePath = findTool("mkvmerge"); return m_mkvmergePath; }
QString ExternalTools::mkvpropeditPath() const { if (m_mkvpropeditPath.isEmpty()) m_mkvpropeditPath = findTool("mkvpropedit"); return m_mkvpropeditPath; }

bool    ExternalTools::validateAll()       { return true; } // TODO Phase 3
QString ExternalTools::ffprobeVersion()   const { return {}; }
QString ExternalTools::mkvmergeVersion()  const { return {}; }

// ── OriginalLanguageDetector ──────────────────────────────────────────────────

QString OriginalLanguageDetector::detect(const FileRecord& /*file*/, const QList<StreamRecord>& streams)
{
    // Simple heuristic: first non-undetermined audio track's language
    for (const auto& s : streams) {
        if (s.codecType == "audio" && !s.language.isEmpty() && s.language != "und")
            return s.language;
    }
    return {};
}

// ── RuleEngine ────────────────────────────────────────────────────────────────

RuleEngine::RuleEngine(const UserProfile* profile, QObject* parent)
    : QObject(parent), m_profile(profile) {}

bool RuleEngine::isInUnderstoodLanguage(const QString& lang) const
{
    if (!m_profile) return false;
    return m_profile->understoodLanguages().contains(lang, Qt::CaseInsensitive);
}

bool RuleEngine::isRedundantLossy(const StreamRecord& s, const QList<StreamRecord>& siblings) const
{
    // Codec hierarchy: if a lossless sibling exists for the same language, this lossy is redundant
    static const QHash<QString, QString> losslessOf = {
        {"ac3",  "truehd"},
        {"eac3", "truehd"},
        {"dts",  "dtshd_ma"},
        {"aac",  "flac"},
        {"mp3",  "flac"},
    };
    const auto it = losslessOf.find(s.codecName);
    if (it == losslessOf.end()) return false;

    const QString& lossless = it.value();
    for (const auto& sibling : siblings) {
        if (&sibling == &s) continue;
        if (sibling.codecType == "audio" &&
            sibling.language == s.language &&
            sibling.codecName == lossless)
            return true;
    }
    return false;
}

FileDecision RuleEngine::evaluateFile(const FileRecord& file, const QList<StreamRecord>& streams) const
{
    FileDecision fd;
    fd.file = file;

    const QList<StreamRecord> audioTracks = [&]{
        QList<StreamRecord> r;
        for (const auto& s : streams) if (s.codecType == "audio") r << s;
        return r;
    }();

    for (const StreamRecord& s : streams) {
        TrackDecision td;
        td.stream = s;
        td.decision = Decision::Keep;

        if (s.codecType == "audio") {
            // Tier 1: codec hierarchy (redundant lossy)
            if (isRedundantLossy(s, audioTracks)) {
                td.decision = Decision::Remove;
                td.reason = QStringLiteral("Redundant lossy copy — lossless sibling exists for language '%1'").arg(s.language);
            }
            // Tier 2: language policy
            else if (m_profile) {
                const bool isOriginal = !file.originalLanguage.isEmpty() &&
                                        s.language == file.originalLanguage &&
                                        m_profile->alwaysKeepOriginalAudio();
                const bool isUnderstood = isInUnderstoodLanguage(s.language);
                if (!isOriginal && !isUnderstood && !s.language.isEmpty()) {
                    td.decision = Decision::Remove;
                    td.reason = QStringLiteral("Language '%1' not in understood languages and not original").arg(s.language);
                }
            }
        }
        else if (s.codecType == "subtitle") {
            if (m_profile) {
                const bool isUnderstood = isInUnderstoodLanguage(s.language);
                const bool isForced = s.isForced && m_profile->keepForcedSubtitlesAlways();
                if (!isUnderstood && !isForced) {
                    td.decision = Decision::Remove;
                    td.reason = QStringLiteral("Subtitle language '%1' not in understood languages").arg(s.language);
                }
            }
        }

        fd.tracks.append(td);
    }
    return fd;
}

// ── ActionEngine ──────────────────────────────────────────────────────────────

ActionEngine::ActionEngine(const QString& mkvmergePath, QObject* parent)
    : QObject(parent), m_mkvmergePath(mkvmergePath) {}

QStringList ActionEngine::buildCommand(const FileDecision& decision, const QString& outputPath) const
{
    QStringList keepAudio, keepSub;
    for (const auto& td : decision.tracks) {
        if (td.decision == Decision::Remove) continue;
        if (td.stream.codecType == "audio")    keepAudio << QString::number(td.stream.streamIndex);
        if (td.stream.codecType == "subtitle") keepSub   << QString::number(td.stream.streamIndex);
    }

    QStringList args = {"-o", outputPath};
    if (!keepAudio.isEmpty()) args << "--audio-tracks"    << keepAudio.join(',');
    if (!keepSub.isEmpty())   args << "--subtitle-tracks" << keepSub.join(',');
    args << decision.file.path;
    return args;
}

// ── JobQueue ──────────────────────────────────────────────────────────────────

JobQueue::JobQueue(QObject* parent) : QObject(parent) {}
void JobQueue::enqueue(qint64 jobId) { m_queue.append(jobId); }
void JobQueue::start()  { m_running = true; }
void JobQueue::pause()  { m_running = false; }
void JobQueue::cancel() { m_running = false; m_queue.clear(); }

// ── RemuxJob ──────────────────────────────────────────────────────────────────

RemuxJob::RemuxJob(qint64 jobId, const QString& mkvmergePath, const QStringList& args, QObject* parent)
    : QObject(parent), m_jobId(jobId), m_mkvmergePath(mkvmergePath), m_args(args) {}

void RemuxJob::run()
{
    // TODO Phase 3: full implementation
    emit finished(-1, "Not yet implemented");
}

// ── RegexClassifier ───────────────────────────────────────────────────────────

RegexClassifier::RegexClassifier() = default;

ClassificationResult RegexClassifier::classify(const QString& title,
                                                 const QString& /*language*/,
                                                 const QString& /*codec*/) const
{
    const QString t = title.toLower();

    if (t.contains("comment") || t.contains("komment") || t.contains("director"))
        return {TrackType::Commentary, 0.9};
    if (t.contains("sdh") || t.contains("hearing") || t.contains("hörgeschädigte") || t.contains("cc"))
        return {TrackType::Sdh, 0.9};
    if (t.contains("forced") || t.contains("forced subtitle"))
        return {TrackType::Forced, 0.95};
    if (t.contains("sign") || t.contains("song"))
        return {TrackType::Signs, 0.85};
    if (t.isEmpty())
        return {TrackType::Main, 0.6};

    return {TrackType::Main, 0.7};
}

// ── UI stubs ──────────────────────────────────────────────────────────────────

McLibraryPanel::McLibraryPanel(QWidget* parent) : QWidget(parent) {}
McFilterPanel::McFilterPanel(QWidget* parent)   : QWidget(parent) {}
McJobPanel::McJobPanel(QWidget* parent)         : QWidget(parent) {}
McPreviewDialog::McPreviewDialog(QWidget* parent) : QDialog(parent) {}
McSettingsDialog::McSettingsDialog(QWidget* parent) : QDialog(parent) {}

} // namespace Mc
