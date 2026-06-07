#include "classifier/RegexClassifier.h"

namespace Mc {

RegexClassifier::RegexClassifier() = default;

ClassificationResult RegexClassifier::classify(const QString& title,
												const QString& /*language*/,
												const QString& /*codec*/) const
{
	const QString t = title.toLower();

	if (t.contains("comment") || t.contains("komment") || t.contains("director"))
		return { TrackType::Commentary, 0.9 };
	if (t.contains("sdh") || t.contains("hearing") || t.contains("cc"))
		return { TrackType::Sdh, 0.9 };
	if (t.contains("forced"))
		return { TrackType::Forced, 0.95 };
	if (t.contains("sign") || t.contains("song"))
		return { TrackType::Signs, 0.85 };
	if (t.isEmpty())
		return { TrackType::Main, 0.6 };

	return { TrackType::Main, 0.7 };
}

} // namespace Mc
