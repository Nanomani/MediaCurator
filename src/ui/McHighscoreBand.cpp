#include "ui/McHighscoreBand.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPalette>

namespace Mc {

McHighscoreBand::McHighscoreBand(QWidget* parent)
	: QWidget(parent)
{
	setCursor(Qt::PointingHandCursor);
	setAutoFillBackground(true);

	const QColor h = palette().color(QPalette::Highlight);
	QPalette pal = palette();
	pal.setColor(QPalette::Window, QColor(h.red(), h.green(), h.blue(), 40));
	setPalette(pal);

	auto* layout = new QHBoxLayout(this);
	layout->setContentsMargins(10, 4, 10, 4);

	m_label = new QLabel(this);
	m_label->setTextFormat(Qt::RichText);
	layout->addWidget(m_label, 1);
}

void McHighscoreBand::setEntries(const QList<HighscoreEntry>& topEntries)
{
	QStringList parts;
	int rank = 1;
	for (const HighscoreEntry& e : topEntries) {
		parts << QStringLiteral("%1. %2 — %3 MB").arg(rank++).arg(e.name.toHtmlEscaped()).arg(e.score);
	}
	m_label->setText(QStringLiteral("\U0001F3C6 %1&nbsp;&nbsp;&nbsp;› %2")
	                      .arg(parts.join(QStringLiteral("&nbsp;&nbsp;&nbsp;")), tr("View leaderboard")));
}

void McHighscoreBand::mousePressEvent(QMouseEvent* e)
{
	if (e->button() == Qt::LeftButton)
		emit clicked();
	QWidget::mousePressEvent(e);
}

} // namespace Mc
