#pragma once
#include <QDialog>
#include <QList>
#include <QString>

#include "engine/HighscoreClient.h"

class QTableWidget;

namespace Mc {

// McHighscoreDialog — full leaderboard view (top ~50 entries), opened by
// clicking the McHighscoreBand. Highlights the row matching the local
// player's stored name.
class McHighscoreDialog : public QDialog
{
	Q_OBJECT
public:
	explicit McHighscoreDialog(const QList<HighscoreEntry>& entries,
	                            const QString& localPlayerName,
	                            QWidget* parent = nullptr);

	void setEntries(const QList<HighscoreEntry>& entries);   // live-refresh while open

signals:
	void refreshRequested();

private:
	void rebuildTable(const QList<HighscoreEntry>& entries);

	QTableWidget* m_table = nullptr;
	QString       m_localPlayerName;
};

} // namespace Mc
