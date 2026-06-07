#pragma once
#include <QElapsedTimer>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListView;
class QPushButton;
class QTimer;

namespace Mc {

class JobQueue;
class McJobListModel;

class McJobPanel : public QWidget
{
	Q_OBJECT
public:
	explicit McJobPanel(QWidget* parent = nullptr);

	void setJobQueue(JobQueue* queue);
	void refresh();

public slots:
	void onJobStatusChanged(qint64 jobId, const QString& status);
	void repaintCards();

signals:
	void previewRequested(qint64 fileId);
	void playRequested(const QString& filePath);
	void editImdbLinkRequested(qint64 fileId);
	void refreshPosterRequested(qint64 fileId);

private slots:
	void onQueueSelected();
	void onQueueAll();
	void onUnqueueSelected();
	void onRemoveSelected();
	void onPreviewCommand();
	void onShowSummary();
	void onStart();
	void onPause();
	void onCancel();

private:
	void setupUi();
	void updateFooter();
	void updateStatusCombo();
	bool eventFilter(QObject* obj, QEvent* event) override;
	static QString formatSaved(qint64 bytes);

	McJobListModel* m_model               = nullptr;
	QListView*      m_listView            = nullptr;
	QLabel*         m_footerLabel         = nullptr;
	QPushButton*    m_btnQueueSelected    = nullptr;
	QPushButton*    m_btnUnqueue          = nullptr;
	QPushButton*    m_btnQueueAll         = nullptr;
	QPushButton*    m_btnStart            = nullptr;
	QPushButton*    m_btnPause            = nullptr;
	QPushButton*    m_btnCancel           = nullptr;
	QPushButton*    m_btnRemove           = nullptr;
	QPushButton*    m_btnPreviewCmd       = nullptr;
	QPushButton*    m_btnSummary          = nullptr;
	QLineEdit*      m_filterEdit          = nullptr;
	QComboBox*      m_statusFilter        = nullptr;
	QCheckBox*      m_chkAutoTrack        = nullptr;
	JobQueue*       m_queue               = nullptr;
	QElapsedTimer   m_jobTimer;
	QTimer*         m_etaTimer            = nullptr;
};

} // namespace Mc
