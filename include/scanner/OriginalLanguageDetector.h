#pragma once
#include "core/DatabaseManager.h"
#include <QString>
#include <QList>

namespace Mc {
/** Heuristics to detect the original language of a media file from its tracks. */
class OriginalLanguageDetector {
public:
    // Returns ISO 639-2 code of the most likely original language, or empty string
    static QString detect(const FileRecord& file, const QList<StreamRecord>& streams);
};
} // namespace Mc
