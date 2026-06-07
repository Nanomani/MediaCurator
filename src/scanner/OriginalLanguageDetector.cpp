#include "scanner/OriginalLanguageDetector.h"

namespace Mc {

QString OriginalLanguageDetector::detect(const FileRecord& /*file*/, const QList<StreamRecord>& streams)
{
	// Heuristic: use the language of the first non-undetermined, non-empty audio track.
	// This works for the vast majority of single-language source files.
	for (const StreamRecord& s : streams) {
		if (s.codecType == "audio" && !s.language.isEmpty() && s.language != "und")
			return s.language;
	}
	return {};
}

} // namespace Mc
