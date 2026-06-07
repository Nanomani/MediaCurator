#pragma once

#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMainWindow>
#include <QProgressBar>
#include <QPushButton>
#include <QSplitter>
#include <QStringList>
#include <QThread>
#include <QTimer>

#ifdef Q_OS_WIN
struct ITaskbarList3;
#endif

namespace Mc {

class AnalyzeWorker;
class JobQueue;
class McFileListModel;
class McJobPanel;
class ScanWorker;
class UserProfile;

class McMainWindow : public QMainWindow
{
	Q_OBJECT
public:
	explicit McMainWindow(QWidget* parent = nullptr);
	~McMainWindow() override;

protected:
	void closeEvent(QCloseEvent* event) override;

private slots:
	void onScanFolder();
	void onScanLibrary();
	void onRemoveFolder();
	void onScanProgress(int current, int total, const QString& currentFile);
	void onScanFinished(int scanned, int added, int updated, int failed, int skipped, int removed);
	void onRefreshView();
	void onAnalyzeLibrary();
	void onAnalyzeProgress(int current, int total, const QString& filename);
	void onAnalyzeJobProposed(qint64 fileId);
	void onAnalyzeFinished(int analyzed, int created);
	void onAbout();
	void onDonate();
	void onSettings();
	void onShowPreview(qint64 fileId);

private:
	void setupUi();
	void setupActions();
	void setupToolBar();
	void setupMenuBar();
	void setupStatusBar();
	void createScanWorker(const QString& folderPath);
	void setScanningState(bool scanning);
	void updateSavedLabel();
	void launchInVlc(const QString& rawPath);
	bool analyzeSingleFile(qint64 fileId);
#ifdef Q_OS_WIN
	void setTaskbarProgress(int value, int total = 100);
	void clearTaskbarProgress();
#endif

	// Actions (created once, shared between toolbar and menu)
	QAction* m_actScanFolder    = nullptr;
	QAction* m_actScanLibrary   = nullptr;
	QAction* m_actRemoveFolder  = nullptr;
	QAction* m_actAnalyze       = nullptr;
	QAction* m_actSettings      = nullptr;
	QAction* m_actRefresh       = nullptr;

	UserProfile*     m_profile     = nullptr;
	McFileListModel* m_listModel   = nullptr;
	QListView*       m_listView    = nullptr;
	QLineEdit*       m_filterEdit          = nullptr;
	QPushButton*     m_btnFilterRemovals   = nullptr;
	QPushButton*     m_btnFilterMissingImdb = nullptr;
	McJobPanel*      m_jobPanel    = nullptr;
	JobQueue*        m_jobQueue    = nullptr;
	QLabel*          m_statusLabel  = nullptr;
	QLabel*          m_savedLabel   = nullptr;
	QProgressBar*    m_progressBar  = nullptr;
	QPushButton*     m_btnCancelScan     = nullptr;
	QPushButton*     m_btnCancelAnalyze  = nullptr;
	QSplitter*       m_splitter          = nullptr;
	QStringList      m_pendingRoots;
	QTimer*          m_analyzeRefreshTimer = nullptr;
	QThread*         m_scanThread      = nullptr;
	ScanWorker*      m_scanWorker      = nullptr;
	QThread*         m_analyzeThread   = nullptr;
	AnalyzeWorker*   m_analyzeWorker   = nullptr;
	int              m_analyzeJobCount = 0;
	QString          m_currentJobFilename;
#ifdef Q_OS_WIN
	ITaskbarList3*   m_taskbar = nullptr;
#endif
};

} // namespace Mc
