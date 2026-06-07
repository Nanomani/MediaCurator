#include "ui/McMainWindow.h"
#include "ui/ImdbSearchDialog.h"
#include "ui/McManageFoldersDialog.h"
#include "ui/SvgIcon.h"
#include "ui/McFileCardDelegate.h"
#include "ui/McFileListModel.h"
#include "ui/McJobPanel.h"
#include "ui/McPreviewDialog.h"
#include "ui/McSettingsDialog.h"
#include "engine/ActionEngine.h"
#include "engine/AnalyzeWorker.h"
#include "engine/JobQueue.h"
#include "engine/PosterManager.h"
#include "engine/RuleEngine.h"
#include "engine/TrackDecision.h"
#include "scanner/NfoParser.h"
#include "scanner/OriginalLanguageDetector.h"
#include "scanner/ScanWorker.h"
#include "core/DatabaseManager.h"
#include "core/ExternalTools.h"
#include "core/UserProfile.h"

#ifdef Q_OS_WIN
#include <shobjidl.h>
#endif

#include <QAbstractItemView>
#include <QTimer>
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QLineEdit>
#include <QCursor>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QPixmap>
#include <QListView>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QProcess>
#include <QProgressBar>
#include <QGuiApplication>
#include <QScreen>
#include <QSettings>
#include <QSplitter>
#include <QDialog>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QStatusBar>
#include <QToolBar>
#include <QUrl>
#include <QVBoxLayout>

namespace Mc {

McMainWindow::McMainWindow(QWidget* parent)
	: QMainWindow(parent)
{
	setWindowTitle("MediaCurator");
	setMinimumSize(900, 600);

	QSettings s;
	const bool firstLaunch = !s.contains("mainWindow/geometry");
	if (!firstLaunch) {
		restoreGeometry(s.value("mainWindow/geometry").toByteArray());
		restoreState(s.value("mainWindow/state").toByteArray());
	}
	// Splitter state is restored after setupUi() creates m_splitter

	m_profile  = new UserProfile(this);
	m_profile->load();

	m_jobQueue = new JobQueue(this);
	m_jobQueue->setWriteJobLog(m_profile->writeJobLog());

	connect(m_profile, &UserProfile::profileChanged, this, [this]() {
		m_jobQueue->setWriteJobLog(m_profile->writeJobLog());
		PosterManager::instance().setTmdbApiKey(m_profile->tmdbApiKey());
	});

	setupActions();
	setupUi();
	if (const QByteArray sp = s.value("mainWindow/splitter").toByteArray(); !sp.isEmpty())
		m_splitter->restoreState(sp);
	else
		m_splitter->setSizes({ 600, 600 });

	if (firstLaunch) {
		if (QScreen* screen = QGuiApplication::primaryScreen()) {
			const QRect avail = screen->availableGeometry();
			setGeometry(avail.adjusted(50, 50, -50, -50));
		}
	}
	setupToolBar();
	setupMenuBar();
	setupStatusBar();

	connect(m_jobQueue, &JobQueue::fileRescanned,
	        m_listModel, &McFileListModel::reloadFile);
	connect(m_jobQueue, &JobQueue::fileRescanned,
	        this, [this](qint64 fid) {
		if (auto* d = qobject_cast<McFileCardDelegate*>(m_listView->itemDelegate()))
			d->invalidateSizeCacheFor(fid);
	});

	m_analyzeRefreshTimer = new QTimer(this);
	m_analyzeRefreshTimer->setSingleShot(true);
	m_analyzeRefreshTimer->setInterval(300);
	connect(m_analyzeRefreshTimer, &QTimer::timeout, this, [this] {
		m_jobPanel->refresh();
		m_listModel->refreshJobFilter();
	});

	connect(m_jobQueue, &JobQueue::jobStarted, this, [this](qint64 jobId) {
		for (const auto& j : DatabaseManager::instance().allJobsForPanel()) {
			if (j.jobId == jobId) { m_currentJobFilename = j.filename; break; }
		}
		m_progressBar->setRange(0, 100);
		m_progressBar->setValue(0);
		m_progressBar->setVisible(true);
		m_statusLabel->setText(tr("Processing '%1'…").arg(m_currentJobFilename));
	});

	connect(m_jobQueue, &JobQueue::progressChanged, this, [this](qint64, int pct) {
		if (pct >= 100) {
			// mkvmerge has finished encoding but is still flushing/closing the
			// output file before the process exits. Switch to an indeterminate
			// animation so the UI doesn't look frozen at 100%.
			m_progressBar->setRange(0, 0);
			m_statusLabel->setText(tr("Finishing '%1'…").arg(m_currentJobFilename));
#ifdef Q_OS_WIN
			if (m_taskbar)
				m_taskbar->SetProgressState(reinterpret_cast<HWND>(winId()), TBPF_INDETERMINATE);
#endif
		} else {
			m_progressBar->setRange(0, 100);
			m_progressBar->setValue(pct);
#ifdef Q_OS_WIN
			setTaskbarProgress(pct);
#endif
		}
	});

	connect(m_jobQueue, &JobQueue::jobFinished, this, [this](qint64, bool success, qint64) {
		updateSavedLabel();
		m_progressBar->setRange(0, 100);
		m_progressBar->setVisible(false);
		m_progressBar->setValue(0);
		if (!success) {
			m_statusLabel->setText(tr("Job failed for '%1'").arg(m_currentJobFilename));
#ifdef Q_OS_WIN
			if (m_taskbar) {
				m_taskbar->SetProgressState(reinterpret_cast<HWND>(winId()), TBPF_ERROR);
			}
#endif
		} else if (m_jobQueue->isPaused()) {
			// More jobs are waiting but the queue is paused; allFinished won't
			// fire until the user resumes and the last job completes.
			m_statusLabel->setText(tr("Paused — click Start to resume"));
#ifdef Q_OS_WIN
			clearTaskbarProgress();
#endif
		}
		// Non-paused success: runNext() fires synchronously from onJobFinished,
		// so either jobStarted (next job) or allFinished (queue empty) will
		// update the status immediately after this handler returns.
	});

	connect(m_jobQueue, &JobQueue::allFinished, this, [this] {
		m_progressBar->setVisible(false);
		m_progressBar->setValue(0);
		m_currentJobFilename.clear();
		updateSavedLabel();
		const int total = m_listModel->totalCount();
		m_statusLabel->setText(tr("All jobs finished — %1 files in library").arg(total));
#ifdef Q_OS_WIN
		clearTaskbarProgress();
#endif
	});

	connect(m_jobQueue, &JobQueue::warning,
	        this, [this](const QString& msg) { m_statusLabel->setText(msg); });

	// ── Poster manager ────────────────────────────────────────────────────────
	auto& pm = PosterManager::instance();
	pm.start(m_profile->tmdbApiKey());
	connect(&pm, &PosterManager::posterReady,
	        m_listModel, &McFileListModel::onPosterReady);

	onRefreshView();
}

McMainWindow::~McMainWindow()
{
#ifdef Q_OS_WIN
	if (m_taskbar) { m_taskbar->Release(); m_taskbar = nullptr; }
#endif
}

void McMainWindow::setupUi()
{
	m_splitter = new QSplitter(Qt::Vertical, this);
	auto* splitter = m_splitter;
	splitter->setChildrenCollapsible(false);

	// ── File list (top) ───────────────────────────────────────────────────────
	m_listModel = new McFileListModel(this);

	m_listView = new QListView(this);
	m_listView->setModel(m_listModel);
	auto* fileDelegate = new McFileCardDelegate(m_listView);
	connect(fileDelegate, &McFileCardDelegate::playRequested,
	        this, [this](const QModelIndex& idx) {
		launchInVlc(idx.data(McFileListModel::FileRole).value<FileRecord>().path);
	});

	// Library is a static/informational view — badge toggle is handled in the job queue.
	// pressed still fires so handlePress can route play-button clicks.
	connect(m_listView, &QAbstractItemView::pressed,
	        this, [this, fileDelegate](const QModelIndex& idx) {
		if (!idx.isValid()) return;
		const QPoint pos = m_listView->viewport()->mapFromGlobal(QCursor::pos());
		fileDelegate->handlePress(pos, m_listView->visualRect(idx), m_listView->font(), idx);
	});
	m_listView->setItemDelegate(fileDelegate);

	// Double-click on the poster column → open IMDb search dialog
	connect(m_listView, &QAbstractItemView::doubleClicked, this,
	        [this](const QModelIndex& idx) {
		if (!idx.isValid()) return;
		const QPoint pos    = m_listView->viewport()->mapFromGlobal(QCursor::pos());
		const QRect  iRect  = m_listView->visualRect(idx);
		if (pos.x() > iRect.left() + McFileCardDelegate::kPosterW) return;
		const FileRecord file     = idx.data(McFileListModel::FileRole).value<FileRecord>();
		const QString    suggested = NfoParser::titleFromFilename(file.filename);
		const QString    existing  = NfoParser::readImdbId(file.path);
		ImdbSearchDialog dlg(file.path, suggested, existing, m_profile->tmdbApiKey(), this);
		if (dlg.exec() == QDialog::Accepted) {
			const QString id = dlg.selectedImdbId();
			if (!id.isEmpty()) {
				NfoParser::writeMovieNfo(file.path, id, dlg.selectedTitle(), dlg.selectedYear());
				PosterManager::instance().refresh(file.id, dlg.selectedPosterPath(), dlg.selectedImageData(), id);
				m_listView->viewport()->repaint();
			}
		}
	});

	m_listView->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_listView->setMouseTracking(true);
	m_listView->viewport()->setMouseTracking(true);
	m_listView->setUniformItemSizes(false);
	m_listView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	m_listView->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
	m_listView->setAlternatingRowColors(true);
	m_listView->setSpacing(0);
	m_listView->setContextMenuPolicy(Qt::CustomContextMenu);

	connect(m_listView, &QListView::customContextMenuRequested,
	        this, [this](const QPoint& pos) {
		const QModelIndex idx = m_listView->indexAt(pos);
		if (!idx.isValid()) return;
		const FileRecord file = idx.data(McFileListModel::FileRole).value<FileRecord>();

		QMenu menu(this);

		auto* previewAction = menu.addAction(svgIcon(":/icons/visibility.svg"),
		                                      tr("&Preview Tracks…"));
		connect(previewAction, &QAction::triggered,
		        this, [this, file] { onShowPreview(file.id); });

		auto* analyzeAction = menu.addAction(svgIcon(":/icons/manage_search.svg"),
		                                      tr("&Analyze File"));
		connect(analyzeAction, &QAction::triggered, this, [this, file] {
			const bool created = analyzeSingleFile(file.id);
			m_jobPanel->refresh();
			m_listModel->refreshJobFilter();
			m_statusLabel->setText(created
			    ? tr("Proposed job for %1").arg(file.filename)
			    : tr("No removals found for %1").arg(file.filename));
		});

		menu.addSeparator();

		auto* refreshPosterAction = menu.addAction(svgIcon(":/icons/refresh.svg"),
		                                            tr("Refresh &Poster"));
		connect(refreshPosterAction, &QAction::triggered, this, [file] {
			PosterManager::instance().refresh(file.id);
		});

		auto* imdbAction = menu.addAction(svgIcon(":/icons/link.svg"),
		                                   tr("Edit &IMDb Link…"));
		connect(imdbAction, &QAction::triggered, this, [this, file] {
			const QString suggested = NfoParser::titleFromFilename(file.filename);
			const QString existing  = NfoParser::readImdbId(file.path);
			ImdbSearchDialog dlg(file.path, suggested, existing,
			                      m_profile->tmdbApiKey(), this);
			if (dlg.exec() != QDialog::Accepted) return;
			const QString imdbId = dlg.selectedImdbId();
			if (imdbId.isEmpty()) return;
			NfoParser::writeMovieNfo(file.path, imdbId,
			                          dlg.selectedTitle(), dlg.selectedYear());
			PosterManager::instance().refresh(file.id, dlg.selectedPosterPath(), dlg.selectedImageData(), imdbId);
			m_listView->viewport()->repaint();
		});

		menu.addSeparator();

		auto* openFolderAction = menu.addAction(svgIcon(":/icons/folder_open.svg"),
		                                         tr("Open &Containing Folder"));
		connect(openFolderAction, &QAction::triggered, this, [file] {
			QDesktopServices::openUrl(
				QUrl::fromLocalFile(QFileInfo(file.path).absolutePath()));
		});

		auto* playAction = menu.addAction(svgIcon(":/icons/play_arrow.svg"),
		                                   tr("Play in &VLC"));
		connect(playAction, &QAction::triggered,
		        this, [this, file] { launchInVlc(file.path); });

		menu.exec(m_listView->viewport()->mapToGlobal(pos));
	});

	// ── Filter bar ────────────────────────────────────────────────────────────
	m_filterEdit = new QLineEdit(this);
	m_filterEdit->setPlaceholderText(tr("Filter by filename…"));
	m_filterEdit->setClearButtonEnabled(true);

	m_btnFilterRemovals = new QPushButton(tr("With removals"), this);
	m_btnFilterRemovals->setCheckable(true);
	m_btnFilterRemovals->setToolTip(tr("Show only files with proposed track removals"));

	m_btnFilterMissingImdb = new QPushButton(tr("Missing poster"), this);
	m_btnFilterMissingImdb->setCheckable(true);
	m_btnFilterMissingImdb->setToolTip(tr("Show only files without a poster (no IMDb link set)"));

	auto* filterBar    = new QWidget(this);
	auto* filterLayout = new QHBoxLayout(filterBar);
	filterLayout->setContentsMargins(4, 4, 4, 2);
	filterLayout->setSpacing(6);
	filterLayout->addWidget(m_filterEdit, 1);
	filterLayout->addWidget(m_btnFilterRemovals);
	filterLayout->addWidget(m_btnFilterMissingImdb);

	connect(m_filterEdit, &QLineEdit::textChanged, this, [this](const QString& text) {
		qint64 anchorId = -1;
		if (text.isEmpty()) {
			const auto sel = m_listView->selectionModel()->selectedIndexes();
			if (!sel.isEmpty())
				anchorId = sel.first().data(McFileListModel::FileRole).value<FileRecord>().id;
		}
		m_listModel->setFilterText(text);
		if (text.isEmpty() && anchorId >= 0) {
			for (int row = 0; row < m_listModel->rowCount(); ++row) {
				const QModelIndex idx = m_listModel->index(row);
				if (idx.data(McFileListModel::FileRole).value<FileRecord>().id == anchorId) {
					m_listView->setCurrentIndex(idx);
					m_listView->scrollTo(idx, QAbstractItemView::PositionAtCenter);
					break;
				}
			}
		}
	});
	connect(m_btnFilterRemovals, &QPushButton::toggled,
	        m_listModel,         &McFileListModel::setFilterHasRemovals);
	connect(m_btnFilterMissingImdb, &QPushButton::toggled,
	        m_listModel,            &McFileListModel::setFilterMissingImdb);

	auto* topWidget = new QWidget(this);
	auto* topLayout = new QVBoxLayout(topWidget);
	topLayout->setContentsMargins(0, 0, 0, 0);
	topLayout->setSpacing(0);
	topLayout->addWidget(filterBar);
	topLayout->addWidget(m_listView, 1);

	splitter->addWidget(topWidget);

	// ── Job panel (bottom) ────────────────────────────────────────────────────
	m_jobPanel = new McJobPanel(this);
	m_jobPanel->setJobQueue(m_jobQueue);
	m_jobPanel->setMinimumHeight(120);
	splitter->addWidget(m_jobPanel);

	connect(m_jobPanel, &McJobPanel::previewRequested,
	        this, &McMainWindow::onShowPreview);

	connect(m_jobPanel, &McJobPanel::playRequested,
	        this, &McMainWindow::launchInVlc);

	connect(m_jobPanel, &McJobPanel::editImdbLinkRequested,
	        this, [this](qint64 fileId) {
		const auto fileOpt = DatabaseManager::instance().fileById(fileId);
		if (!fileOpt) return;
		// Prefer DB-saved id so the dialog shows the correct existing link even
		// when no .nfo file is present on disk.
		QString existingId;
		if (const auto pr = DatabaseManager::instance().posterForFile(fileId))
			existingId = pr->imdbId;
		if (existingId.isEmpty())
			existingId = NfoParser::readImdbId(fileOpt->path);
		ImdbSearchDialog dlg(fileOpt->path,
		                     NfoParser::titleFromFilename(fileOpt->filename),
		                     existingId,
		                     m_profile->tmdbApiKey(), this);
		if (dlg.exec() == QDialog::Accepted) {
			const QString id = dlg.selectedImdbId();
			if (!id.isEmpty()) {
				NfoParser::writeMovieNfo(fileOpt->path, id,
				                        dlg.selectedTitle(), dlg.selectedYear());
				PosterManager::instance().refresh(fileId, dlg.selectedPosterPath(), dlg.selectedImageData(), id);
				m_listView->viewport()->repaint();
			}
		}
	});

	connect(m_jobPanel, &McJobPanel::refreshPosterRequested,
	        this, [](qint64 fileId) {
		PosterManager::instance().refresh(fileId);
	});

	splitter->setStretchFactor(0, 3);
	splitter->setStretchFactor(1, 1);
	splitter->setSizes({ 450, 160 });

	setCentralWidget(splitter);
}

void McMainWindow::setupActions()
{
	m_actScanFolder = new QAction(tr("Add Folder…"), this);
	m_actScanFolder->setShortcut(QKeySequence("Ctrl+O"));
	m_actScanFolder->setToolTip(tr("Add a folder to the library and scan it (Ctrl+O)"));
	m_actScanFolder->setIcon(svgIcon(":/icons/folder_open.svg"));
	connect(m_actScanFolder, &QAction::triggered, this, &McMainWindow::onScanFolder);

	m_actScanLibrary = new QAction(tr("Scan Library"), this);
	m_actScanLibrary->setShortcut(QKeySequence("Ctrl+Shift+O"));
	m_actScanLibrary->setToolTip(tr("Rescan all library folders (Ctrl+Shift+O)"));
	m_actScanLibrary->setIcon(svgIcon(":/icons/refresh.svg"));
	connect(m_actScanLibrary, &QAction::triggered, this, &McMainWindow::onScanLibrary);

	m_actRemoveFolder = new QAction(tr("Manage Library Folders…"), this);
	m_actRemoveFolder->setToolTip(tr("View and remove folders from the library database"));
	m_actRemoveFolder->setIcon(svgIcon(":/icons/delete.svg"));
	connect(m_actRemoveFolder, &QAction::triggered, this, &McMainWindow::onRemoveFolder);

	m_actAnalyze = new QAction(tr("Analyze Library"), this);
	m_actAnalyze->setShortcut(QKeySequence("Ctrl+Shift+A"));
	m_actAnalyze->setToolTip(tr("Run rule engine across all files and propose jobs (Ctrl+Shift+A)"));
	m_actAnalyze->setIcon(svgIcon(":/icons/manage_search.svg"));
	connect(m_actAnalyze, &QAction::triggered, this, &McMainWindow::onAnalyzeLibrary);

	m_actSettings = new QAction(tr("Settings…"), this);
	m_actSettings->setToolTip(tr("Edit language preferences and external tool paths"));
	m_actSettings->setIcon(svgIcon(":/icons/settings.svg"));
	connect(m_actSettings, &QAction::triggered, this, &McMainWindow::onSettings);

	m_actRefresh = new QAction(tr("Refresh"), this);
	m_actRefresh->setShortcut(QKeySequence::Refresh);
	m_actRefresh->setToolTip(tr("Reload library from database (F5)"));
	m_actRefresh->setIcon(svgIcon(":/icons/refresh.svg"));
	connect(m_actRefresh, &QAction::triggered, this, &McMainWindow::onRefreshView);
}

void McMainWindow::setupToolBar()
{
	auto* tb = addToolBar(tr("Main"));
	tb->setObjectName("mainToolBar");   // needed for saveState/restoreState
	tb->setMovable(false);
	tb->setIconSize({ 24, 24 });
	tb->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

	tb->setStyleSheet(
	    "QToolButton {"
	    "  padding: 4px 8px;"
	    "  border-radius: 4px;"
	    "  border: none;"
	    "}"
	    "QToolButton:hover {"
	    "  background: rgba(128,128,128,50);"
	    "}"
	    "QToolButton:pressed {"
	    "  background: rgba(128,128,128,90);"
	    "}");

	tb->addAction(m_actScanFolder);
	tb->addAction(m_actScanLibrary);
	tb->addAction(m_actAnalyze);
	tb->addSeparator();
	tb->addAction(m_actRefresh);

	// Settings pushed to the far right via a spacer widget
	auto* spacer = new QWidget(this);
	spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	tb->addWidget(spacer);
	tb->addAction(m_actSettings);
}

void McMainWindow::setupMenuBar()
{
	// File menu
	QMenu* fileMenu = menuBar()->addMenu(tr("&File"));
	fileMenu->addAction(m_actScanFolder);
	fileMenu->addAction(m_actScanLibrary);
	fileMenu->addAction(m_actRemoveFolder);
	fileMenu->addSeparator();
	auto* quitAction = new QAction(svgIcon(":/icons/logout.svg"), tr("&Quit"), this);
	quitAction->setShortcut(QKeySequence::Quit);
	connect(quitAction, &QAction::triggered, qApp, &QApplication::quit);
	fileMenu->addAction(quitAction);

	// View menu
	QMenu* viewMenu = menuBar()->addMenu(tr("&View"));
	viewMenu->addAction(m_actRefresh);

	// Tools menu
	QMenu* toolsMenu = menuBar()->addMenu(tr("&Tools"));
	toolsMenu->addAction(m_actAnalyze);
	toolsMenu->addSeparator();
	toolsMenu->addAction(m_actSettings);

	// Help menu
	QMenu* helpMenu = menuBar()->addMenu(tr("&Help"));
	auto* donateMenuAction = new QAction(svgIcon(":/icons/dollar.svg"), tr("&Donate…"), this);
	connect(donateMenuAction, &QAction::triggered, this, &McMainWindow::onDonate);
	helpMenu->addAction(donateMenuAction);
	helpMenu->addSeparator();
	auto* aboutAction = new QAction(tr("&About MediaCurator…"), this);
	aboutAction->setIcon(QApplication::windowIcon());
	connect(aboutAction, &QAction::triggered, this, &McMainWindow::onAbout);
	helpMenu->addAction(aboutAction);

	// Donate button — far right corner of the menu bar
	auto* donateBtn = new QPushButton(svgIcon(":/icons/dollar.svg"), tr("Donate"), this);
	donateBtn->setFlat(true);
	donateBtn->setToolTip(tr("Support MediaCurator"));
	connect(donateBtn, &QPushButton::clicked, this, &McMainWindow::onDonate);
	menuBar()->setCornerWidget(donateBtn, Qt::TopRightCorner);
}

void McMainWindow::setupStatusBar()
{
	m_statusLabel = new QLabel(tr("Ready"), this);
	statusBar()->addWidget(m_statusLabel, 1);

	m_progressBar = new QProgressBar(this);
	m_progressBar->setMaximumWidth(200);
	m_progressBar->setVisible(false);
	statusBar()->addPermanentWidget(m_progressBar);

	m_btnCancelScan = new QPushButton(tr("Cancel Scan"), this);
	m_btnCancelScan->setVisible(false);
	connect(m_btnCancelScan, &QPushButton::clicked, this, [this] {
		if (m_scanWorker) m_scanWorker->cancel();
		m_btnCancelScan->setEnabled(false);
		m_btnCancelScan->setText(tr("Cancelling…"));
	});
	statusBar()->addPermanentWidget(m_btnCancelScan);

	m_btnCancelAnalyze = new QPushButton(tr("Cancel Analyze"), this);
	m_btnCancelAnalyze->setVisible(false);
	connect(m_btnCancelAnalyze, &QPushButton::clicked, this, [this] {
		if (m_analyzeWorker) m_analyzeWorker->cancel();
		m_btnCancelAnalyze->setEnabled(false);
		m_btnCancelAnalyze->setText(tr("Cancelling…"));
	});
	statusBar()->addPermanentWidget(m_btnCancelAnalyze);

	m_savedLabel = new QLabel(this);
	m_savedLabel->setVisible(false);
	statusBar()->addPermanentWidget(m_savedLabel);
}

void McMainWindow::closeEvent(QCloseEvent* event)
{
	if (m_jobQueue->hasActiveJob()) {
		const auto answer = QMessageBox::question(
		    this, tr("Jobs Running"),
		    tr("A job is currently running. Closing now will interrupt it and may leave a temporary file on disk.\n\nClose anyway?"),
		    QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
		if (answer != QMessageBox::Yes) {
			event->ignore();
			return;
		}
		m_jobQueue->cancel();
	}

	// Stop the scan worker before destroying the window.
	// cancel() sets an atomic flag; the worker checks it between files.
	// We wait up to 8 s for the current FFprobe call to finish naturally;
	// only then terminate to avoid leaking a child process.
	if (m_scanThread && m_scanThread->isRunning()) {
		if (m_scanWorker) m_scanWorker->cancel();
		if (!m_scanThread->wait(8000)) {
			m_scanThread->terminate();
			m_scanThread->wait(1000);
		}
	}

	PosterManager::instance().stop();

	QSettings s;
	s.setValue("mainWindow/geometry",     saveGeometry());
	s.setValue("mainWindow/state",        saveState());
	s.setValue("mainWindow/splitter",     m_splitter->saveState());
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

	QSettings s;
	QStringList roots = s.value("scan/roots").toStringList();
	// Migrate from legacy single-folder key
	if (roots.isEmpty()) {
		const QString legacy = s.value("scan/lastFolder").toString();
		if (!legacy.isEmpty()) roots << legacy;
	}
	const QString hint = roots.isEmpty() ? QString() : roots.last();

	const QString folder = QFileDialog::getExistingDirectory(
		this, tr("Add Media Folder"), hint,
		QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
	);
	if (folder.isEmpty()) return;

	if (!roots.contains(folder))
		roots << folder;
	s.setValue("scan/roots", roots);
	s.remove("scan/lastFolder");

	createScanWorker(folder);
}

void McMainWindow::onScanLibrary()
{
	if (m_scanThread && m_scanThread->isRunning()) {
		QMessageBox::information(this, tr("Scan in progress"),
			tr("A scan is already running. Please wait for it to finish."));
		return;
	}

	QSettings s;
	QStringList roots = s.value("scan/roots").toStringList();
	// Migrate from legacy single-folder key
	if (roots.isEmpty()) {
		const QString legacy = s.value("scan/lastFolder").toString();
		if (!legacy.isEmpty()) {
			roots << legacy;
			s.setValue("scan/roots", roots);
			s.remove("scan/lastFolder");
		}
	}

	if (roots.isEmpty()) {
		// No folders configured yet — redirect to Add Folder
		onScanFolder();
		return;
	}

	m_pendingRoots = roots;
	createScanWorker(m_pendingRoots.takeFirst());
}

void McMainWindow::onRemoveFolder()
{
	McManageFoldersDialog dlg(this);

	// Route Add Folder through the existing scan infrastructure — the dialog
	// is just a view and emits a signal rather than owning its own worker.
	connect(&dlg, &McManageFoldersDialog::folderAdded, this, [this](const QString& path) {
		if (m_scanThread && m_scanThread->isRunning())
			m_pendingRoots << path;
		else
			createScanWorker(path);
	});

	dlg.exec();

	if (dlg.anyRemoved() || dlg.anyAdded())
		onRefreshView();
}

void McMainWindow::createScanWorker(const QString& folderPath)
{
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

	connect(m_scanThread, &QThread::started,   m_scanWorker, &ScanWorker::run);
	connect(m_scanWorker, &ScanWorker::finished, m_scanThread, &QThread::quit);
	connect(m_scanWorker, &ScanWorker::finished, m_scanWorker, &QObject::deleteLater);
	connect(m_scanThread, &QThread::finished,  m_scanThread,  &QObject::deleteLater);

	connect(m_scanWorker, &ScanWorker::progress,       this,        &McMainWindow::onScanProgress);
	connect(m_scanWorker, &ScanWorker::finished,       this,        &McMainWindow::onScanFinished);
	connect(m_scanWorker, &ScanWorker::fileProcessed,  m_listModel, &McFileListModel::applyFileUpdate);
	connect(m_scanWorker, &ScanWorker::fileRemoved,    m_listModel, &McFileListModel::removeEntry);

	setScanningState(true);
	m_scanThread->start();
}

void McMainWindow::onScanProgress(int current, int total, const QString& currentFile)
{
	m_progressBar->setMaximum(total);   // 0 → indeterminate animated bar
	m_progressBar->setValue(current);
	if (total > 0)
		m_statusLabel->setText(tr("Scanning %1/%2: %3").arg(current).arg(total).arg(currentFile));
	else
		m_statusLabel->setText(tr("Scanning (%1 found): %2").arg(current).arg(currentFile));
}

void McMainWindow::onScanFinished(int scanned, int added, int updated, int failed, int skipped, int removed)
{
	m_scanThread = nullptr;
	m_scanWorker = nullptr;

	if (!m_pendingRoots.isEmpty()) {
		m_statusLabel->setText(tr("Folder scan done — %1 more to go…").arg(m_pendingRoots.size()));
		createScanWorker(m_pendingRoots.takeFirst());
		return;
	}

	setScanningState(false);

	QStringList parts;
	if (added   > 0) parts << tr("%1 added").arg(added);
	if (updated > 0) parts << tr("%1 updated").arg(updated);
	if (skipped > 0) parts << tr("%1 unchanged").arg(skipped);
	if (removed > 0) parts << tr("%1 removed").arg(removed);
	if (failed  > 0) parts << tr("%1 failed").arg(failed);

	const QString summary = parts.isEmpty() ? tr("nothing to do") : parts.join(", ");
	m_statusLabel->setText(
		tr("Scan complete — %1 of %2 files: %3")
		.arg(scanned - skipped).arg(scanned).arg(summary)
	);

	// Model was updated incrementally via fileProcessed/fileRemoved signals — no full reload needed.
	m_jobPanel->refresh();
	updateSavedLabel();
	const int shown = m_listModel->fileCount();
	const int total = m_listModel->totalCount();
	if (shown < total)
		m_statusLabel->setText(tr("Scan complete (%1 of %2 shown): %3").arg(shown).arg(total).arg(summary));
}

void McMainWindow::onRefreshView()
{
	m_listModel->reload();
	m_jobPanel->refresh();
	updateSavedLabel();
	if (m_progressBar->isVisible()) return;
	const int shown = m_listModel->fileCount();
	const int total = m_listModel->totalCount();
	if (shown < total)
		m_statusLabel->setText(tr("%1 of %2 files").arg(shown).arg(total));
	else
		m_statusLabel->setText(tr("%1 files in library").arg(total));
}

bool McMainWindow::analyzeSingleFile(qint64 fileId)
{
	auto& db = DatabaseManager::instance();

	const auto fileOpt = db.fileById(fileId);
	if (!fileOpt) return false;

	FileRecord f = *fileOpt;
	const auto streams = db.streamsForFile(f.id);

	if (f.originalLanguage.isEmpty()) {
		const QString lang = OriginalLanguageDetector::detect(f, streams);
		if (!lang.isEmpty()) {
			db.updateFileOriginalLanguage(f.id, lang);
			f.originalLanguage = lang;
		}
	}

	RuleEngine   engine(m_profile);
	ActionEngine actions(ExternalTools::instance().mkvmergePath());

	FileDecision decision = engine.evaluateFile(f, streams);

	// Apply any manual badge overrides the user toggled in the file card.
	const QSet<int> forcedRemovals = m_listModel->forcedRemovalsFor(f.id);
	for (auto& td : decision.tracks) {
		if (forcedRemovals.contains(td.stream.streamIndex)
		        && td.decision == Decision::Keep) {
			td.decision      = Decision::Remove;
			td.reason        = tr("Manually marked for removal");
			td.userOverride  = true;
		}
	}

	if (decision.removalCount() == 0) return false;

	int audioRemoved = 0, subRemoved = 0;
	for (const auto& td : decision.tracks) {
		if (td.decision != Decision::Remove) continue;
		if (td.stream.codecType == "audio")    ++audioRemoved;
		if (td.stream.codecType == "subtitle") ++subRemoved;
	}

	if (m_profile->skipSubtitleOnlyJobs() && audioRemoved == 0) return false;

	QStringList parts;
	if (audioRemoved > 0) parts << tr("%1 audio").arg(audioRemoved);
	if (subRemoved   > 0) parts << tr("%1 subtitle").arg(subRemoved);
	const QString summary = tr("Remove ") + parts.join(", ");

	// Build human-readable description of removed tracks
	QStringList descLines;
	for (const auto& td : decision.tracks) {
		if (td.decision != Decision::Remove) continue;
		const StreamRecord& s = td.stream;
		QString line = QStringLiteral("  [%1] %2").arg(s.codecType.toUpper(), s.codecName);
		if (!s.language.isEmpty() && s.language != "und")
			line += QStringLiteral(" (%1)").arg(s.language);
		if (!s.title.isEmpty())
			line += QStringLiteral(" \"%1\"").arg(s.title);
		if (!td.reason.isEmpty())
			line += QStringLiteral(" — %1").arg(td.reason);
		descLines << line;
	}
	const QString descriptionText = descLines.join('\n');

	const QString     outputPath      = f.path + ".tmp";
	const QStringList args            = actions.buildCommand(decision, outputPath);
	QJsonArray        arr;
	for (const QString& a : args) arr.append(a);

	JobRecord job;
	job.fileId          = f.id;
	job.status          = "proposed";
	job.jobType         = "remux";
	job.commandArgsJson = QJsonDocument(arr).toJson(QJsonDocument::Compact);
	job.summary         = summary;
	job.descriptionText = descriptionText;
	(void)db.insertJob(job);
	return true;
}

void McMainWindow::onAnalyzeLibrary()
{
	if (m_analyzeThread) return;   // already running

	auto& db = DatabaseManager::instance();
	db.clearJobsByStatus("proposed");

	const auto files = db.allFiles();
	if (files.isEmpty()) {
		m_statusLabel->setText(tr("No files in library to analyze."));
		return;
	}

	// Pre-fetch forced removals from the UI model — must happen on the main thread
	QHash<qint64, QSet<int>> forcedRemovals;
	for (const auto& file : files) {
		const auto fr = m_listModel->forcedRemovalsFor(file.id);
		if (!fr.isEmpty())
			forcedRemovals.insert(file.id, fr);
	}

	m_analyzeJobCount = 0;
	m_progressBar->setMaximum(files.size());
	m_progressBar->setValue(0);
	m_progressBar->setVisible(true);
	m_actScanFolder->setEnabled(false);
	m_actAnalyze->setEnabled(false);
	m_btnCancelAnalyze->setEnabled(true);
	m_btnCancelAnalyze->setText(tr("Cancel Analyze"));
	m_btnCancelAnalyze->setVisible(true);

	m_analyzeThread = new QThread(this);
	m_analyzeWorker = new AnalyzeWorker(files, m_profile,
	                                    ExternalTools::instance().mkvmergePath(),
	                                    std::move(forcedRemovals));
	m_analyzeWorker->moveToThread(m_analyzeThread);

	connect(m_analyzeThread, &QThread::started,                m_analyzeWorker, &AnalyzeWorker::run);
	connect(m_analyzeWorker, &AnalyzeWorker::finished, m_analyzeThread,         &QThread::quit);
	connect(m_analyzeWorker, &AnalyzeWorker::finished, m_analyzeWorker,         &QObject::deleteLater);
	connect(m_analyzeThread, &QThread::finished, this, [this] {
		m_analyzeThread->deleteLater();
		m_analyzeThread = nullptr;
	});

	connect(m_analyzeWorker, &AnalyzeWorker::progress,   this, &McMainWindow::onAnalyzeProgress);
	connect(m_analyzeWorker, &AnalyzeWorker::jobProposed, this, &McMainWindow::onAnalyzeJobProposed);
	connect(m_analyzeWorker, &AnalyzeWorker::finished,    this, &McMainWindow::onAnalyzeFinished);

	m_analyzeThread->start();
}

void McMainWindow::onAnalyzeProgress(int current, int total, const QString& filename)
{
	m_progressBar->setValue(current);
	m_statusLabel->setText(tr("Analyzing %1/%2: %3").arg(current).arg(total).arg(filename));
}

void McMainWindow::onAnalyzeJobProposed(qint64 /*fileId*/)
{
	++m_analyzeJobCount;
	// Debounce: collapse bursts of proposed-job signals into one refresh
	// fired 300 ms after the last one. onAnalyzeFinished does a final refresh.
	m_analyzeRefreshTimer->start();
}

void McMainWindow::onAnalyzeFinished(int /*analyzed*/, int created)
{
	m_analyzeWorker = nullptr;
	m_progressBar->setVisible(false);
	m_btnCancelAnalyze->setVisible(false);
	m_actScanFolder->setEnabled(true);
	m_actAnalyze->setEnabled(true);

	m_jobPanel->refresh();
	m_listModel->refreshJobFilter();
	updateSavedLabel();
	m_statusLabel->setText(tr("Analyze complete — %1 job(s) proposed").arg(created));
}

void McMainWindow::setScanningState(bool scanning)
{
	m_progressBar->setVisible(scanning);
	m_btnCancelScan->setVisible(scanning);
	if (scanning) {
		m_btnCancelScan->setEnabled(true);
		m_btnCancelScan->setText(tr("Cancel Scan"));
	} else {
		m_progressBar->setValue(0);
	}
	// Disable scan/analyze actions while scanning
	m_actScanFolder->setEnabled(!scanning);
	m_actAnalyze->setEnabled(!scanning);
}

void McMainWindow::updateSavedLabel()
{
	qint64 total = 0;
	const auto jobs = DatabaseManager::instance().allJobsForPanel();
	for (const auto& r : jobs)
		if (r.status == "done") total += r.savedBytes;

	if (total > 0) {
		const double gb = total / 1073741824.0;
		const QString text = gb >= 1.0
		    ? tr("Saved: %1 GB").arg(gb, 0, 'f', 2)
		    : tr("Saved: %1 MB").arg(total / 1048576.0, 0, 'f', 1);
		m_savedLabel->setText(text);
		m_savedLabel->setVisible(true);
	} else {
		m_savedLabel->setVisible(false);
	}
}

void McMainWindow::launchInVlc(const QString& rawPath)
{
	const QUrl url = QUrl::fromLocalFile(rawPath);
	const QString vlc = ExternalTools::instance().vlcPath();
	if (!vlc.isEmpty()) {
		QProcess::startDetached(vlc, { url.toString() });
		return;
	}
	QDesktopServices::openUrl(url);
}

void McMainWindow::onSettings()
{
	McSettingsDialog dlg(m_profile, this);
	dlg.exec();
}

void McMainWindow::onAbout()
{
	QMessageBox::about(this, tr("About MediaCurator"),
		tr("<h3>MediaCurator %1</h3>"
		   "<p>Scan your video library with ffprobe, apply smart policy rules "
		   "to identify redundant audio and subtitle tracks, then use MKVToolNix "
		   "to losslessly strip them &mdash; no re-encoding, no quality loss.</p>"
		   "<p><b>Author:</b> Jacob Pedersen &mdash; Bleze Software<br>"
		   "<b>License:</b> Apache 2.0 &mdash; open source, free to use and modify.</p>"
		   "<p><small>"
		   "<b>Built with:</b> Qt %2 &middot; SQLite &middot; nlohmann/json<br>"
		   "<b>Bundled tools:</b> ffprobe (LGPL) &middot; MKVToolNix (GPL)"
		   "</small></p>")
		.arg(QCoreApplication::applicationVersion(), QString(qVersion()))
	);
}

void McMainWindow::onDonate()
{
	auto* dlg = new QDialog(this);
	dlg->setWindowTitle(tr("Support MediaCurator"));
	dlg->setAttribute(Qt::WA_DeleteOnClose);
	dlg->setFixedWidth(420);

	auto* layout = new QVBoxLayout(dlg);
	layout->setSpacing(10);
	layout->setContentsMargins(16, 16, 16, 16);

	auto* intro = new QLabel(
		tr("If MediaCurator saves you money on storage or just makes managing "
		   "your media library less painful, consider saying thanks or supporting "
		   "continued development."), dlg);
	intro->setWordWrap(true);
	layout->addWidget(intro);

	layout->addSpacing(6);

	auto* ghBtn = new QPushButton(svgIcon(":/icons/link.svg"), tr("GitHub Sponsors"), dlg);
	connect(ghBtn, &QPushButton::clicked, []() {
		QDesktopServices::openUrl(QUrl("https://github.com/sponsors/your-username"));
	});
	layout->addWidget(ghBtn);

	auto* bmcBtn = new QPushButton(svgIcon(":/icons/link.svg"), tr("Buy Me a Coffee"), dlg);
	connect(bmcBtn, &QPushButton::clicked, []() {
		QDesktopServices::openUrl(QUrl("https://buymeacoffee.com/your-username"));
	});
	layout->addWidget(bmcBtn);

	layout->addSpacing(6);

	auto* lightningLabel = new QLabel(tr("⚡ Lightning"), dlg);
	lightningLabel->setStyleSheet("font-weight: bold;");
	layout->addWidget(lightningLabel);

	// QR code — centered, scaled to 180 px
	auto* qrLabel = new QLabel(dlg);
	{
		QPixmap qr(":/icons/lightning_qr.png");
		qrLabel->setPixmap(qr.scaled(180, 180, Qt::KeepAspectRatio, Qt::SmoothTransformation));
	}
	qrLabel->setAlignment(Qt::AlignHCenter);
	layout->addWidget(qrLabel);

	auto* lightningRow = new QHBoxLayout;
	auto* lightningEdit = new QLineEdit(QStringLiteral("bleze@cake.cash"), dlg);
	lightningEdit->setReadOnly(true);
	lightningEdit->setFrame(false);
	lightningEdit->setStyleSheet("background: transparent;");
	lightningRow->addWidget(lightningEdit, 1);
	auto* copyBtn = new QPushButton(tr("Copy"), dlg);
	copyBtn->setFixedWidth(64);
	connect(copyBtn, &QPushButton::clicked, [lightningEdit, copyBtn]() {
		QApplication::clipboard()->setText(lightningEdit->text());
		copyBtn->setText(tr("Copied!"));
		QTimer::singleShot(1500, copyBtn, [copyBtn]() { copyBtn->setText(tr("Copy")); });
	});
	lightningRow->addWidget(copyBtn);
	layout->addLayout(lightningRow);

	layout->addStretch();

	auto* btnBox = new QDialogButtonBox(QDialogButtonBox::Close, dlg);
	connect(btnBox, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
	layout->addWidget(btnBox);

	btnBox->button(QDialogButtonBox::Close)->setFocus();
	dlg->exec();
}

void McMainWindow::onShowPreview(qint64 fileId)
{
	auto& db = DatabaseManager::instance();

	const auto fileOpt = db.fileById(fileId);
	if (!fileOpt) return;

	const auto streams = db.streamsForFile(fileId);

	FileRecord f = *fileOpt;
	if (f.originalLanguage.isEmpty()) {
		const QString lang = OriginalLanguageDetector::detect(f, streams);
		if (!lang.isEmpty()) {
			db.updateFileOriginalLanguage(f.id, lang);
			f.originalLanguage = lang;
		}
	}

	RuleEngine engine(m_profile);
	const FileDecision decision = engine.evaluateFile(f, streams);

	McPreviewDialog dlg(decision, this);
	dlg.exec();
}

// ── Windows taskbar progress ──────────────────────────────────────────────────

#ifdef Q_OS_WIN
void McMainWindow::setTaskbarProgress(int value, int total)
{
	if (!m_taskbar) {
		if (FAILED(CoCreateInstance(CLSID_TaskbarList, nullptr, CLSCTX_INPROC_SERVER,
		                             IID_ITaskbarList3,
		                             reinterpret_cast<void**>(&m_taskbar)))) {
			m_taskbar = nullptr;
			return;
		}
		m_taskbar->HrInit();
	}
	const HWND hwnd = reinterpret_cast<HWND>(winId());
	m_taskbar->SetProgressState(hwnd, TBPF_NORMAL);
	m_taskbar->SetProgressValue(hwnd, static_cast<ULONGLONG>(value),
	                                   static_cast<ULONGLONG>(total));
}

void McMainWindow::clearTaskbarProgress()
{
	if (!m_taskbar) return;
	m_taskbar->SetProgressState(reinterpret_cast<HWND>(winId()), TBPF_NOPROGRESS);
}
#endif

} // namespace Mc
