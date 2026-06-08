#pragma once

#include "engine/TrackDecision.h"
#include <QDialog>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>

namespace Mc {

class McWhatIfDialog : public QDialog
{
	Q_OBJECT
public:
	explicit McWhatIfDialog(int totalFiles, QWidget* parent = nullptr);

	void onProgress(int current, int total, const QString& filename);
	void onFinished(const QList<FileDecision>& decisions);

signals:
	void analyzeRequested();

private:
	void buildResults(const QList<FileDecision>& decisions);
	void centerOnParent();

	int           m_totalFiles    = 0;
	QWidget*      m_progressPage  = nullptr;
	QProgressBar* m_progressBar   = nullptr;
	QLabel*       m_fileLabel     = nullptr;
	QWidget*      m_resultsPage   = nullptr;
	QPushButton*  m_cancelBtn     = nullptr;
	QPushButton*  m_closeBtn      = nullptr;
	QPushButton*  m_analyzeBtn    = nullptr;
};

} // namespace Mc
