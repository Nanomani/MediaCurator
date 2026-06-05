#pragma once
#include <QString>

namespace Mc {

enum class TrackType { Main, Commentary, Sdh, Forced, Signs, HearingImpaired, Unknown };

struct ClassificationResult {
    TrackType type       = TrackType::Unknown;
    double    confidence = 0.0;
};

/** Abstract interface for track type classifiers. */
class TrackClassifier {
public:
    virtual ~TrackClassifier() = default;
    virtual ClassificationResult classify(const QString& title,
                                           const QString& language,
                                           const QString& codec) const = 0;
};

} // namespace Mc
