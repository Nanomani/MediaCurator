#pragma once

#include "core/DatabaseManager.h"
#include <QAtomicInt>
#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>

namespace Mc {

class UserProfile;

class AnalyzeWorker : public QObject
{
	Q_OBJECT
public:
	explicit AnalyzeWorker(QList<FileRecord> files,
	                       const UserProfile* profile,
	                       const QString& mkvmergePath,
	                       QHash<qint64, QSet<int>> forcedRemovals,
	                       QObject* parent = nullptr);

	void cancel() { m_cancelled.storeRelaxed(1); }

public slots:
	void run();

signals:
	void progress(int current, int total, const QString& filename);
	void jobProposed(qint64 fileId);
	void finished(int analyzed, int created);

private:
	bool analyzeFile(const FileRecord& file);

	QList<FileRecord>        m_files;
	const UserProfile*       m_profile;
	QString                  m_mkvmergePath;
	QHash<qint64, QSet<int>> m_forcedRemovals;
	QAtomicInt               m_cancelled{0};
};

} // namespace Mc
