#pragma once
#include "engine/TrackDecision.h"
#include <QObject>
#include <QStringList>

namespace Mc {

/** Builds mkvmerge command-line arguments from a FileDecision. */
class ActionEngine : public QObject {
	Q_OBJECT
public:
	explicit ActionEngine(const QString& mkvmergePath, QObject* parent = nullptr);
	QStringList buildCommand(const FileDecision& decision, const QString& outputPath) const;
private:
	QString m_mkvmergePath;
};

} // namespace Mc
