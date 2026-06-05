#include "ui/McMainWindow.h"
#include "ui/McTrackTableModel.h"
#include "scanner/ScanWorker.h"
#include "core/DatabaseManager.h"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QFileDialog>
#include <QHeaderView>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QProgressBar>
#include <QSettings>
#include <QSortFilterProxyModel>
#include <QStatusBar>
#include <QTableView>
#include <QToolBar>
#include <QVBoxLayout>

namespace Mc {

McMainWindow::McMainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("MediaCurator");
    setMinimumSize(900, 600);

    // Restore geometry
    QSettings s;
    restoreGeometry(s.value("mainWindow/geometry").toByteArray());
    restoreState(s.value("mainWindow/state").toByteArray());

    setupUi();
    setupMenuBar();
    setupStatusBar();

    // Load existing data from DB
    onRefreshView();
}

McMainWindow::~McMainWindow() = default;

void McMainWindow::setupUi()
{
    // Central widget — just the table for Phase 1
    auto* central = new QWidget(this);
    auto* layout  = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);

    m_tableModel = new McTrackTableModel(this);

    auto* proxyModel = new QSortFilterProxyModel(this);
    proxyModel->setSourceModel(m_tableModel);
    proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);

    auto* tableView = new QTableView(this);
    tableView->setModel(proxyModel);
    tableView->setSortingEnabled(true);
    tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableView->setAlternatingRowColors(true);
    tableView->setShowGrid(false);
    tableView->horizontalHeader()->setStretchLastSection(true);
    tableView->horizontalHeader()->setSectionsMovable(true);
    tableView->verticalHeader()->setVisible(false);
    tableView->verticalHeader()->setDefaultSectionSize(22);
    tableView->sortByColumn(0, Qt::AscendingOrder);

    layout->addWidget(tableView);
    setCentralWidget(central);
}

void McMainWindow::setupMenuBar()
{
    // File menu
    QMenu* fileMenu = menuBar()->addMenu(tr("&File"));

    auto* scanAction = new QAction(tr("&Scan Folder…"), this);
    scanAction->setShortcut(QKeySequence("Ctrl+O"));
    connect(scanAction, &QAction::triggered, this, &McMainWindow::onScanFolder);
    fileMenu->addAction(scanAction);

    fileMenu->addSeparator();

    auto* quitAction = new QAction(tr("&Quit"), this);
    quitAction->setShortcut(QKeySequence::Quit);
    connect(quitAction, &QAction::triggered, qApp, &QApplication::quit);
    fileMenu->addAction(quitAction);

    // View menu
    QMenu* viewMenu = menuBar()->addMenu(tr("&View"));

    auto* refreshAction = new QAction(tr("&Refresh"), this);
    refreshAction->setShortcut(QKeySequence::Refresh);
    connect(refreshAction, &QAction::triggered, this, &McMainWindow::onRefreshView);
    viewMenu->addAction(refreshAction);

    // Tools menu
    QMenu* toolsMenu = menuBar()->addMenu(tr("&Tools"));
    auto* settingsAction = new QAction(tr("&Settings…"), this);
    connect(settingsAction, &QAction::triggered, this, &McMainWindow::onSettings);
    toolsMenu->addAction(settingsAction);

    // Help menu
    QMenu* helpMenu = menuBar()->addMenu(tr("&Help"));
    auto* aboutAction = new QAction(tr("&About MediaCurator…"), this);
    connect(aboutAction, &QAction::triggered, this, &McMainWindow::onAbout);
    helpMenu->addAction(aboutAction);
}

void McMainWindow::setupStatusBar()
{
    m_statusLabel = new QLabel(tr("Ready"), this);
    statusBar()->addWidget(m_statusLabel, 1);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setMaximumWidth(200);
    m_progressBar->setVisible(false);
    statusBar()->addPermanentWidget(m_progressBar);
}

void McMainWindow::closeEvent(QCloseEvent* event)
{
    QSettings s;
    s.setValue("mainWindow/geometry", saveGeometry());
    s.setValue("mainWindow/state", saveState());
    event->accept();
}

// ── Slots ─────────────────────────────────────────────────────────────────────

void McMainWindow::onScanFolder()
{
    if (m_scanThread && m_scanThread->isRunning()) {
        QMessageBox::information(this, tr("Scan in progress"),
            tr("A scan is already running. Please wait for it to finish."));
        return;
    }

    const QString folder = QFileDialog::getExistingDirectory(
        this, tr("Select Media Folder"), {},
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
    );
    if (folder.isEmpty()) return;

    createScanWorker(folder);
}

void McMainWindow::createScanWorker(const QString& folderPath)
{
    // Locate ffprobe relative to the executable
    const QString exeDir = QCoreApplication::applicationDirPath();
#ifdef Q_OS_WIN
    const QString ffprobePath = exeDir + "/tools/windows/ffprobe.exe";
#elif defined(Q_OS_MACOS)
    const QString ffprobePath = exeDir + "/tools/macos/ffprobe";
#else
    const QString ffprobePath = exeDir + "/tools/linux/ffprobe";
#endif

    m_scanThread = new QThread(this);
    m_scanWorker = new ScanWorker(ffprobePath);
    m_scanWorker->setRootPath(folderPath);
    m_scanWorker->moveToThread(m_scanThread);

    connect(m_scanThread, &QThread::started,  m_scanWorker, &ScanWorker::run);
    connect(m_scanWorker, &ScanWorker::finished, m_scanThread, &QThread::quit);
    connect(m_scanWorker, &ScanWorker::finished, m_scanWorker, &QObject::deleteLater);
    connect(m_scanThread, &QThread::finished, m_scanThread,  &QObject::deleteLater);

    connect(m_scanWorker, &ScanWorker::progress, this, &McMainWindow::onScanProgress);
    connect(m_scanWorker, &ScanWorker::finished, this, &McMainWindow::onScanFinished);

    setScanningState(true);
    m_scanThread->start();
}

void McMainWindow::onScanProgress(int current, int total, const QString& currentFile)
{
    m_progressBar->setMaximum(total);
    m_progressBar->setValue(current);
    m_statusLabel->setText(tr("Scanning %1/%2: %3").arg(current).arg(total).arg(currentFile));
}

void McMainWindow::onScanFinished(int scanned, int added, int updated, int failed)
{
    setScanningState(false);
    m_statusLabel->setText(
        tr("Scan complete — %1 files scanned, %2 added, %3 updated, %4 failed")
        .arg(scanned).arg(added).arg(updated).arg(failed)
    );
    m_scanThread = nullptr;
    m_scanWorker = nullptr;
    onRefreshView();
}

void McMainWindow::onRefreshView()
{
    m_tableModel->reload();
    const int count = m_tableModel->rowCount();
    if (m_progressBar->isVisible()) return; // Don't overwrite scan message
    m_statusLabel->setText(tr("%1 streams in library").arg(count));
}

void McMainWindow::setScanningState(bool scanning)
{
    m_progressBar->setVisible(scanning);
    if (!scanning) m_progressBar->setValue(0);
}

void McMainWindow::onSettings()
{
    QMessageBox::information(this, tr("Settings"),
        tr("Settings dialog coming in Phase 2."));
}

void McMainWindow::onAbout()
{
    QMessageBox::about(this, tr("About MediaCurator"),
        tr("<h3>MediaCurator</h3>"
           "<p>Version 0.1.0 — Phase 1 (Scanner)</p>"
           "<p>Smart media library curation tool.<br>"
           "Scan, filter, and clean your video collection using "
           "ffprobe and MKVToolNix.</p>"
           "<p>License: Apache 2.0<br>"
           "Donations welcome — see project page.</p>")
    );
}

} // namespace Mc
