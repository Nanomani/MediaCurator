#pragma once
#include <QList>
#include <QWidget>

#include "engine/HighscoreClient.h"

class QLabel;

namespace Mc {

// McHighscoreBand — collapsible strip (like a notification banner) showing
// the top entries of the dreamlo leaderboard. Purely a display widget —
// McMainWindow owns all data flow (fetch, submit, visibility). Clicking
// anywhere on the band opens the full leaderboard dialog.
class McHighscoreBand : public QWidget
{
	Q_OBJECT
public:
	explicit McHighscoreBand(QWidget* parent = nullptr);

	void setEntries(const QList<HighscoreEntry>& topEntries);

signals:
	void clicked();

protected:
	void mousePressEvent(QMouseEvent* e) override;

private:
	QLabel* m_label = nullptr;
};

} // namespace Mc
