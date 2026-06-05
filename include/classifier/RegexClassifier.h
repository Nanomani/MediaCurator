#pragma once
#include "classifier/TrackClassifier.h"

namespace Mc {
/**
 * RegexClassifier — fast keyword/regex-based track type classifier.
 * Ships with the application, no internet or model download required.
 * Rules loaded from data/track_classifier.json at startup.
 */
class RegexClassifier : public TrackClassifier {
public:
    RegexClassifier();
    ClassificationResult classify(const QString& title,
                                   const QString& language,
                                   const QString& codec) const override;
private:
    // Loaded from data/track_classifier.json
    // See src/classifier/RegexClassifier.cpp for rule format
};
} // namespace Mc
