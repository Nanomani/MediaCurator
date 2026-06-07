#pragma once
#include "engine/TrackDecision.h"
#include "core/UserProfile.h"
#include <QObject>

namespace Mc {
/**
 * RuleEngine — evaluates user policy rules against a file's tracks
 * and returns per-track keep/remove decisions.
 *
 * Rules are applied in tiers:
 *   1. Codec hierarchy (a track is redundant when a higher-quality sibling exists)
 *   2. Language policy (understood languages + always-keep-original)
 *   3. Track type (commentary, SDH, forced)
 */
class RuleEngine : public QObject {
	Q_OBJECT
public:
	explicit RuleEngine(const UserProfile* profile, QObject* parent = nullptr);
	FileDecision evaluateFile(const FileRecord& file, const QList<StreamRecord>& streams) const;

private:
	static QString audioFormatId(const StreamRecord& s);
	int  audioQualityTier(const StreamRecord& s) const;
	bool isAudioFormatDisabled(const StreamRecord& s) const;
	bool isRedundantAudio(const StreamRecord& s, const QList<StreamRecord>& siblings) const;

	static QString subtitleFormatId(const StreamRecord& s);
	int  subtitleQualityTier(const StreamRecord& s) const;
	bool isSubtitleFormatDisabled(const StreamRecord& s) const;
	bool isRedundantSubtitle(const StreamRecord& s, const QList<StreamRecord>& siblings) const;

	bool isInUnderstoodLanguage(const QString& lang) const;

	const UserProfile* m_profile;
};
} // namespace Mc
