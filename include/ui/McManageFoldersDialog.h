#pragma once

#include <QDialog>

class QAction;
class QPoint;
class QTableWidget;

namespace Mc {

class McManageFoldersDialog : public QDialog
{
	Q_OBJECT
public:
	explicit McManageFoldersDialog(QWidget* parent = nullptr);

	bool anyRemoved() const { return m_anyRemoved; }
	bool anyAdded()   const { return m_anyAdded; }

signals:
	// Emitted as soon as the user confirms adding a folder (before exec() returns).
	// Connect this to McMainWindow::createScanWorker so the existing scan path
	// handles it — no duplicate worker inside the dialog.
	void folderAdded(const QString& path);

private slots:
	void onAddFolder();
	void onRemoveSelected();
	void onSelectionChanged();
	void showContextMenu(const QPoint& pos);

private:
	void loadFolders();

	QTableWidget* m_table      = nullptr;
	QAction*      m_actAdd     = nullptr;
	QAction*      m_actRemove  = nullptr;
	bool          m_anyRemoved = false;
	bool          m_anyAdded   = false;
};

} // namespace Mc
