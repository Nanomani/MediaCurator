#pragma once

#include <QObject>
#include <QStringList>
#include <QJsonObject>

namespace Mc {

/**
 * UserProfile — the user's curation policy.
 * Serialized as JSON and stored in the preferences table.
 */
class UserProfile : public QObject
{
    Q_OBJECT
public:
    explicit UserProfile(QObject* parent = nullptr);

    // Languages the user understands (ISO 639-2, e.g. "dan", "eng")
    QStringList understoodLanguages() const { return m_understoodLanguages; }
    void setUnderstoodLanguages(const QStringList& langs);

    // Always keep audio in the file's detected original language,
    // even if it's not in understoodLanguages. Prevents dubbing removal.
    bool alwaysKeepOriginalAudio() const { return m_alwaysKeepOriginalAudio; }
    void setAlwaysKeepOriginalAudio(bool v);

    // Keep commentary tracks if they are in an understood language
    bool keepCommentaryIfUnderstood() const { return m_keepCommentaryIfUnderstood; }
    void setKeepCommentaryIfUnderstood(bool v);

    // Keep forced subtitle tracks regardless of language
    bool keepForcedSubtitlesAlways() const { return m_keepForcedSubtitlesAlways; }
    void setKeepForcedSubtitlesAlways(bool v);

    // Keep SDH/HI subtitle tracks (in understood languages)
    bool keepSdhSubtitles() const { return m_keepSdhSubtitles; }
    void setKeepSdhSubtitles(bool v);

    // Keep subtitles for original language even if not understood
    bool keepOriginalLanguageSubtitle() const { return m_keepOriginalLanguageSub; }
    void setKeepOriginalLanguageSubtitle(bool v);

    // Preferred codec priority order (best first)
    QStringList preferredAudioCodecOrder() const { return m_preferredAudioCodecOrder; }
    void setPreferredAudioCodecOrder(const QStringList& order);

    // Serialization
    [[nodiscard]] QJsonObject toJson() const;
    bool fromJson(const QJsonObject& json);

    void save();    // persist to DatabaseManager::preferences
    bool load();    // load from DatabaseManager::preferences

signals:
    void profileChanged();

private:
    QStringList m_understoodLanguages     = {"eng"};
    bool        m_alwaysKeepOriginalAudio = true;
    bool        m_keepCommentaryIfUnderstood = true;
    bool        m_keepForcedSubtitlesAlways  = true;
    bool        m_keepSdhSubtitles           = true;
    bool        m_keepOriginalLanguageSub    = false;
    QStringList m_preferredAudioCodecOrder  = {
        "truehd", "dtshd_ma", "flac", "pcm_s24le", "pcm_s16le",
        "eac3", "dts", "ac3", "aac", "mp3"
    };
};

} // namespace Mc
