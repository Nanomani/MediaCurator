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
 *   1. Codec hierarchy (lossless supersedes lossy sibling)
 *   2. Language policy (understood languages + always-keep-original)
 *   3. Track type (commentary, SDH, forced)
 */
class RuleEngine : public QObject {
    Q_OBJECT
public:
    explicit RuleEngine(const UserProfile* profile, QObject* parent = nullptr);
    FileDecision evaluateFile(const FileRecord& file, const QList<StreamRecord>& streams) const;

private:
    bool isRedundantLossy(const StreamRecord& s, const QList<StreamRecord>& siblings) const;
    bool isInUnderstoodLanguage(const QString& lang) const;

    const UserProfile* m_profile;
};
} // namespace Mc
