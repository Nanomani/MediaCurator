#pragma once

#include "core/DatabaseManager.h"
#include <QString>
#include <QList>

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

	/** Rough estimated size saving in bytes (lossy: based on bitrate × duration) */
	qint64 estimatedSavingBytes() const {
		qint64 saving = 0;
		for (const auto& t : tracks) {
			if (t.decision == Decision::Remove && t.stream.bitRate > 0)
				saving += static_cast<qint64>(t.stream.bitRate / 8.0 * file.durationSec);
		}
		return saving;
	}
};

} // namespace Mc
