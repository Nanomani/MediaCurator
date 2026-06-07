#include "ui/McFileListModel.h"

namespace Mc {

McFileListModel::McFileListModel(QObject* parent)
	: QAbstractListModel(parent)
{}

// ── Filter helpers ────────────────────────────────────────────────────────────

bool McFileListModel::entryPassesFilter(const FileEntry& e) const
{
	if (!m_filterText.isEmpty() &&
	    !e.file.filename.contains(m_filterText, Qt::CaseInsensitive))
		return false;
	if (m_filterHasRemovals && !m_filesWithJobs.contains(e.file.id))
		return false;
	if (m_filterMissingImdb && m_posterPaths.contains(e.file.id))
		return false;
	return true;
}

void McFileListModel::applyFilter()
{
	beginResetModel();
	m_entries.clear();
	for (const auto& e : m_allEntries)
		if (entryPassesFilter(e)) m_entries.append(e);
	endResetModel();
}

// ── Public API ────────────────────────────────────────────────────────────────

void McFileListModel::reload()
{
	// Refresh proposed-job file IDs for the "with removals" filter
	m_filesWithJobs.clear();
	for (const auto& j : DatabaseManager::instance().allJobsForPanel())
		if (j.status == "proposed") m_filesWithJobs.insert(j.fileId);

	auto& db = DatabaseManager::instance();
	m_allEntries.clear();
	const QList<FileRecord>                    files   = db.allFiles();
	const QHash<qint64, QList<StreamRecord>>   streams = db.allStreamsGrouped();
	m_allEntries.reserve(files.size());
	for (const FileRecord& f : files)
		m_allEntries.append({ f, streams.value(f.id) });

	// Pre-load poster paths already stored in the DB so they show immediately on startup.
	// New posters arriving via PosterManager::posterReady are merged in via onPosterReady().
	m_posterPaths = db.allDonePosterPaths();

	applyFilter();
}

void McFileListModel::refreshJobFilter()
{
	m_filesWithJobs.clear();
	for (const auto& j : DatabaseManager::instance().allJobsForPanel())
		if (j.status == "proposed") m_filesWithJobs.insert(j.fileId);
	applyFilter();
}

void McFileListModel::applyEntry(const FileEntry& entry)
{
	const qint64 fileId = entry.file.id;

	bool foundInAll = false;
	for (int i = 0; i < m_allEntries.size(); ++i) {
		if (m_allEntries[i].file.id == fileId) {
			m_allEntries[i] = entry;
			foundInAll = true;
			break;
		}
	}
	if (!foundInAll) m_allEntries.append(entry);

	const bool passes = entryPassesFilter(entry);
	for (int i = 0; i < m_entries.size(); ++i) {
		if (m_entries[i].file.id == fileId) {
			if (passes) {
				m_entries[i] = entry;
				const QModelIndex idx = index(i);
				emit dataChanged(idx, idx);
			} else {
				beginRemoveRows({}, i, i);
				m_entries.removeAt(i);
				endRemoveRows();
			}
			return;
		}
	}
	if (passes) {
		beginInsertRows({}, m_entries.size(), m_entries.size());
		m_entries.append(entry);
		endInsertRows();
	}
}

void McFileListModel::reloadFile(qint64 fileId)
{
	auto& db = DatabaseManager::instance();
	const auto fileOpt = db.fileById(fileId);
	if (!fileOpt) return;
	applyEntry({ *fileOpt, db.streamsForFile(fileId) });
}

void McFileListModel::applyFileUpdate(const FileRecord& file, const QList<StreamRecord>& streams)
{
	applyEntry({ file, streams });
}

void McFileListModel::removeEntry(qint64 fileId)
{
	for (int i = 0; i < m_allEntries.size(); ++i) {
		if (m_allEntries[i].file.id == fileId) {
			m_allEntries.removeAt(i);
			break;
		}
	}
	for (int i = 0; i < m_entries.size(); ++i) {
		if (m_entries[i].file.id == fileId) {
			beginRemoveRows({}, i, i);
			m_entries.removeAt(i);
			endRemoveRows();
			return;
		}
	}
}

void McFileListModel::setFilterText(const QString& text)
{
	if (m_filterText == text) return;
	m_filterText = text;
	applyFilter();
}

void McFileListModel::setFilterHasRemovals(bool on)
{
	if (m_filterHasRemovals == on) return;
	m_filterHasRemovals = on;
	applyFilter();
}

void McFileListModel::onPosterReady(qint64 fileId, const QString& imagePath)
{
	m_posterPaths[fileId] = imagePath;
	m_posterVersions[fileId]++;
	if (m_filterMissingImdb) {
		applyFilter();
		return;
	}
	for (int row = 0; row < m_entries.size(); ++row) {
		if (m_entries.at(row).file.id == fileId) {
			const QModelIndex idx = index(row);
			// Specify roles so Qt issues a targeted viewport update rather than a
			// full doItemsLayout() (which empty roles triggers via SizeHintRole).
			emit dataChanged(idx, idx, {PosterRole, PosterVersionRole});
			break;
		}
	}
}

void McFileListModel::setFilterMissingImdb(bool on)
{
	if (m_filterMissingImdb == on) return;
	m_filterMissingImdb = on;
	applyFilter();
}

// ── QAbstractListModel ────────────────────────────────────────────────────────

int McFileListModel::rowCount(const QModelIndex& parent) const
{
	if (parent.isValid()) return 0;
	return m_entries.size();
}

QVariant McFileListModel::data(const QModelIndex& index, int role) const
{
	if (!index.isValid() || index.row() >= m_entries.size())
		return {};

	const FileEntry& e = m_entries.at(index.row());

	switch (role) {
	case Qt::DisplayRole: return e.file.filename;
	case FileRole:        return QVariant::fromValue(e.file);
	case StreamsRole:     return QVariant::fromValue(e.streams);
	case PosterRole:        return m_posterPaths.value(e.file.id);
	case PosterVersionRole: return m_posterVersions.value(e.file.id, 0);
	case OverridesRole:   return QVariant::fromValue(m_forcedRemovals.value(e.file.id));
	default:              return {};
	}
}

void McFileListModel::toggleForcedRemoval(qint64 fileId, int streamIndex)
{
	auto& set = m_forcedRemovals[fileId];
	if (set.contains(streamIndex))
		set.remove(streamIndex);
	else
		set.insert(streamIndex);

	for (int row = 0; row < m_entries.size(); ++row) {
		if (m_entries.at(row).file.id == fileId) {
			const QModelIndex idx = index(row);
			emit dataChanged(idx, idx, { OverridesRole });
			break;
		}
	}
}

} // namespace Mc
