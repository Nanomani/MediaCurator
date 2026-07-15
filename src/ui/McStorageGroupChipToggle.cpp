#include "ui/McStorageGroupChipToggle.h"
#include "ui/McCardDelegate.h"

#include <QMouseEvent>
#include <QPainter>

namespace Mc {

McStorageGroupChipToggle::McStorageGroupChipToggle(int group, QWidget* parent)
    : QWidget(parent), m_group(group)
{
	setMouseTracking(true);
	setCursor(Qt::PointingHandCursor);
	setToolTip(tr("Show/hide storage group %1").arg(group));
}

void McStorageGroupChipToggle::setChecked(bool on)
{
	if (m_checked == on) return;
	m_checked = on;
	update();
}

void McStorageGroupChipToggle::paintEvent(QPaintEvent*)
{
	QPainter p(this);
	const qreal dpr = devicePixelRatioF();
	const double opacity = m_checked ? 1.0 : (m_hovered ? 0.55 : 0.25);
	McCardDelegate::drawGroupChip(&p, 0, (height() - kChipH) / 2, kChipH, m_group, font(), dpr, opacity);
}

void McStorageGroupChipToggle::mousePressEvent(QMouseEvent* event)
{
	if (event->button() != Qt::LeftButton) return;
	m_checked = !m_checked;
	update();
	emit toggled(m_checked);
}

void McStorageGroupChipToggle::enterEvent(QEnterEvent*) { m_hovered = true; update(); }
void McStorageGroupChipToggle::leaveEvent(QEvent*)      { m_hovered = false; update(); }

QSize McStorageGroupChipToggle::sizeHint() const
{
	return { McCardDelegate::groupChipWidth(m_group, font()), kChipH };
}

} // namespace Mc
