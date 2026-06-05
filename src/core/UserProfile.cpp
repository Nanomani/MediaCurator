#include "core/UserProfile.h"
#include "core/DatabaseManager.h"

#include <QJsonDocument>
#include <QJsonArray>

namespace Mc {

static const char* PREF_KEY = "user_profile_json";

UserProfile::UserProfile(QObject* parent)
    : QObject(parent)
{}

void UserProfile::setUnderstoodLanguages(const QStringList& langs)
{
    if (m_understoodLanguages != langs) {
        m_understoodLanguages = langs;
        emit profileChanged();
    }
}

void UserProfile::setAlwaysKeepOriginalAudio(bool v)
{
    if (m_alwaysKeepOriginalAudio != v) {
        m_alwaysKeepOriginalAudio = v;
        emit profileChanged();
    }
}

void UserProfile::setKeepCommentaryIfUnderstood(bool v)
{
    if (m_keepCommentaryIfUnderstood != v) {
        m_keepCommentaryIfUnderstood = v;
        emit profileChanged();
    }
}

void UserProfile::setKeepForcedSubtitlesAlways(bool v)
{
    if (m_keepForcedSubtitlesAlways != v) {
        m_keepForcedSubtitlesAlways = v;
        emit profileChanged();
    }
}

void UserProfile::setKeepSdhSubtitles(bool v)
{
    if (m_keepSdhSubtitles != v) {
        m_keepSdhSubtitles = v;
        emit profileChanged();
    }
}

void UserProfile::setKeepOriginalLanguageSubtitle(bool v)
{
    if (m_keepOriginalLanguageSub != v) {
        m_keepOriginalLanguageSub = v;
        emit profileChanged();
    }
}

void UserProfile::setPreferredAudioCodecOrder(const QStringList& order)
{
    if (m_preferredAudioCodecOrder != order) {
        m_preferredAudioCodecOrder = order;
        emit profileChanged();
    }
}

// ── Serialization ─────────────────────────────────────────────────────────────

QJsonObject UserProfile::toJson() const
{
    QJsonObject o;
    o["understood_languages"]           = QJsonArray::fromStringList(m_understoodLanguages);
    o["always_keep_original_audio"]     = m_alwaysKeepOriginalAudio;
    o["keep_commentary_if_understood"]  = m_keepCommentaryIfUnderstood;
    o["keep_forced_subtitles_always"]   = m_keepForcedSubtitlesAlways;
    o["keep_sdh_subtitles"]             = m_keepSdhSubtitles;
    o["keep_original_language_subtitle"]= m_keepOriginalLanguageSub;
    o["preferred_audio_codec_order"]    = QJsonArray::fromStringList(m_preferredAudioCodecOrder);
    return o;
}

bool UserProfile::fromJson(const QJsonObject& json)
{
    if (json.isEmpty()) return false;

    if (json.contains("understood_languages")) {
        QStringList langs;
        for (const auto& v : json["understood_languages"].toArray())
            langs << v.toString();
        m_understoodLanguages = langs;
    }
    m_alwaysKeepOriginalAudio    = json["always_keep_original_audio"].toBool(true);
    m_keepCommentaryIfUnderstood = json["keep_commentary_if_understood"].toBool(true);
    m_keepForcedSubtitlesAlways  = json["keep_forced_subtitles_always"].toBool(true);
    m_keepSdhSubtitles           = json["keep_sdh_subtitles"].toBool(true);
    m_keepOriginalLanguageSub    = json["keep_original_language_subtitle"].toBool(false);

    if (json.contains("preferred_audio_codec_order")) {
        QStringList order;
        for (const auto& v : json["preferred_audio_codec_order"].toArray())
            order << v.toString();
        if (!order.isEmpty())
            m_preferredAudioCodecOrder = order;
    }
    return true;
}

void UserProfile::save()
{
    const QJsonDocument doc(toJson());
    DatabaseManager::instance().setPref(PREF_KEY, QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
}

bool UserProfile::load()
{
    const QString stored = DatabaseManager::instance().getPref(PREF_KEY);
    if (stored.isEmpty()) return false;

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(stored.toUtf8(), &err);
    if (doc.isNull()) return false;

    return fromJson(doc.object());
}

} // namespace Mc
