#pragma once

#include "engine/TrackDecision.h"
#include "core/DatabaseManager.h"
#include <QAtomicInt>
#include <QObject>

namespace Mc {

class UserProfile;

class SimulateWorker : public QObject
{
	Q_OBJECT
public:
	explicit SimulateWorker(QList<FileRecord> files,
	                        const UserProfile* profile,
	                        QObject* parent = nullptr);

	void cancel() { m_cancelled.storeRelaxed(1); }
	const QList<FileDecision>& decisions() const { return m_decisions; }

public slots:
	void run();

signals:
	void progress(int current, int total, const QString& filename);
	void finished(int analyzed, int filesAffected);

private:
	QList<FileRecord>   m_files;
	const UserProfile*  m_profile;
	QList<FileDecision> m_decisions;
	QAtomicInt          m_cancelled{0};
};

} // namespace Mc
