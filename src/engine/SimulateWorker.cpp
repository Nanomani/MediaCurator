#include "engine/SimulateWorker.h"
#include "engine/RuleEngine.h"
#include "scanner/OriginalLanguageDetector.h"
#include "core/DatabaseManager.h"
#include "core/UserProfile.h"

namespace Mc {

SimulateWorker::SimulateWorker(QList<FileRecord> files,
                               const UserProfile* profile,
                               QObject* parent)
	: QObject(parent)
	, m_files(std::move(files))
	, m_profile(profile)
{}

void SimulateWorker::run()
{
	const int total = m_files.size();
	m_decisions.reserve(total);
	auto& db = DatabaseManager::instance();
	int filesAffected = 0;

	for (int i = 0; i < total; ++i) {
		if (m_cancelled.loadRelaxed()) break;

		FileRecord file = m_files.at(i);
		emit progress(i + 1, total, file.filename);

		const auto streams = db.streamsForFile(file.id);

		if (file.originalLanguage.isEmpty()) {
			const QString lang = OriginalLanguageDetector::detect(file, streams);
			if (!lang.isEmpty()) {
				db.updateFileOriginalLanguage(file.id, lang);
				file.originalLanguage = lang;
			}
		}

		RuleEngine engine(m_profile);
		FileDecision decision = engine.evaluateFile(file, streams);
		if (decision.removalCount() > 0) {
			int audioRemoved = 0;
			for (const auto& td : decision.tracks)
				if (td.decision == Decision::Remove && td.stream.codecType == QLatin1String("audio"))
					++audioRemoved;
			if (!m_profile->skipSubtitleOnlyJobs() || audioRemoved > 0) {
				++filesAffected;
				m_decisions.append(std::move(decision));
			}
		}
	}

	emit finished(total, filesAffected);
}

} // namespace Mc
