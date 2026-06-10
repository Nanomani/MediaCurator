#pragma once

#include "core/DatabaseManager.h"
#include <QString>
#include <QList>
#include <QSet>

namespace Mc {

enum class Decision {
	Keep,       // Track should be kept
	Remove,     // Track should be removed
	Unsure,     // Needs user review
};

/** A rule engine decision for a single stream track */
struct TrackDecision {
	StreamRecord    stream;
	Decision        decision = Decision::Keep;
	QString         reason;             // Human-readable explanation
	bool            userOverride = false; // User manually changed this decision
};

// Proportional savings estimate: removed tracks' share of total declared bitrate,
// applied to the known file size. The denominator is floored at the actual container
// bitrate (fileSize * 8 / duration) so video tracks that declare no bitrate don't
// collapse the denominator to just the audio tracks.
inline qint64 estimateSavingBytes(const QList<StreamRecord>& allStreams,
                                   const QSet<int>& removedStreamIndices,
                                   qint64 fileSizeBytes,
                                   double durationSec)
{
	if (fileSizeBytes <= 0) return 0;
	double totalBr = 0.0, removedBr = 0.0;
	for (const StreamRecord& s : allStreams) {
		const double br = s.bitRate > 0 ? static_cast<double>(s.bitRate)
		                : s.codecType == QLatin1String("subtitle") ? 50'000.0 : 0.0;
		totalBr += br;
		if (removedStreamIndices.contains(s.streamIndex))
			removedBr += br;
	}
	if (durationSec > 0)
		totalBr = qMax(totalBr, static_cast<double>(fileSizeBytes) * 8.0 / durationSec);
	if (totalBr <= 0.0) return 0;
	return static_cast<qint64>(removedBr / totalBr * static_cast<double>(fileSizeBytes));
}

/** All decisions for a single file */
struct FileDecision {
	FileRecord              file;
	QList<TrackDecision>    tracks;

	/** How many tracks are marked for removal */
	int removalCount() const {
		int n = 0;
		for (const auto& t : tracks)
			if (t.decision == Decision::Remove) ++n;
		return n;
	}

	qint64 estimatedSavingBytes() const {
		QSet<int> removed;
		QList<StreamRecord> all;
		for (const auto& t : tracks) {
			all << t.stream;
			if (t.decision == Decision::Remove)
				removed.insert(t.stream.streamIndex);
		}
		return Mc::estimateSavingBytes(all, removed, file.sizeBytes, file.durationSec);
	}
};

} // namespace Mc
