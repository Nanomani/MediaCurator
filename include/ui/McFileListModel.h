#pragma once
#include "core/DatabaseManager.h"
#include <QAbstractListModel>
#include <QHash>
#include <QList>
#include <QSet>
#include <QString>

namespace Mc {

struct FileEntry {
	FileRecord          file;
	QList<StreamRecord> streams;
};

class McFileListModel : public QAbstractListModel
{
	Q_OBJECT
public:
	enum Roles {
		FileRole          = Qt::UserRole + 1,
		StreamsRole       = Qt::UserRole + 2,
		PosterRole        = Qt::UserRole + 3,   // QString — absolute path to cached poster image
		OverridesRole     = Qt::UserRole + 4,   // QSet<int> — stream indices forced to Remove by user
		PosterVersionRole = Qt::UserRole + 5,   // int — increments each time a poster changes
	};

	explicit McFileListModel(QObject* parent = nullptr);

	int      rowCount(const QModelIndex& parent = {}) const override;
	QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

	void reload();
	void reloadFile(qint64 fileId);
	void applyFileUpdate(const Mc::FileRecord& file, const QList<Mc::StreamRecord>& streams);
	void removeEntry(qint64 fileId);
	void refreshJobFilter();        // re-query proposed jobs and reapply filter
	int  fileCount() const { return m_entries.size(); }
	int  totalCount() const { return m_allEntries.size(); }

	/** Returns the set of stream indices the user has force-marked for removal. */
	QSet<int> forcedRemovalsFor(qint64 fileId) const { return m_forcedRemovals.value(fileId); }

public slots:
	void setFilterText(const QString& text);
	void setFilterHasRemovals(bool on);
	void setFilterMissingImdb(bool on);
	void onPosterReady(qint64 fileId, const QString& imagePath);
	void toggleForcedRemoval(qint64 fileId, int streamIndex);

private:
	bool entryPassesFilter(const FileEntry& e) const;
	void applyFilter();
	void applyEntry(const FileEntry& entry);

	QList<FileEntry>          m_allEntries;      // full unfiltered set
	QList<FileEntry>          m_entries;         // visible (filtered) set
	QSet<qint64>              m_filesWithJobs;
	QHash<qint64, QString>    m_posterPaths;     // fileId → cached image path
	QHash<qint64, int>        m_posterVersions;  // fileId → version counter (increments on update)
	QHash<qint64, QSet<int>>  m_forcedRemovals;  // fileId → stream indices user wants removed
	QString                   m_filterText;
	bool                      m_filterHasRemovals  = false;
	bool                      m_filterMissingImdb  = false;
};

} // namespace Mc
