#pragma once
#include "core/DatabaseManager.h"
#include <QColor>
#include <QPersistentModelIndex>
#include <QRect>
#include <QStyledItemDelegate>

class QPainter;
class QAbstractItemView;

namespace Mc {

/**
 * McJobCardDelegate — paints one job as a card in the job panel list view.
 *
 * Header row: filename (bold) | size | status pill | ▶
 * Track rows: kept tracks normal, removed tracks dimmed + red strike.
 * For Proposed jobs, audio/subtitle badges are clickable — toggles track inclusion.
 */
class McJobCardDelegate : public QStyledItemDelegate
{
	Q_OBJECT
public:
	explicit McJobCardDelegate(QObject* parent = nullptr);

	void  paint(QPainter* painter, const QStyleOptionViewItem& option,
	            const QModelIndex& index) const override;
	QSize sizeHint(const QStyleOptionViewItem& option,
	               const QModelIndex& index) const override;
	bool  helpEvent(QHelpEvent* event, QAbstractItemView* view,
	                const QStyleOptionViewItem& option,
	                const QModelIndex& index) override;
	bool  editorEvent(QEvent* event, QAbstractItemModel* model,
	                  const QStyleOptionViewItem& option,
	                  const QModelIndex& index) override;
	bool  eventFilter(QObject* obj, QEvent* event) override;

	bool  handlePress(const QPoint& viewportPos, const QRect& itemRect,
	                  const QFont& viewFont, const QModelIndex& index);

	// Shared helpers also used by McFileCardDelegate — kept static/public
	static QString codecLabel(const StreamRecord& s);
	static QString channelStr(int channels);
	static QString buildBadgeText(const StreamRecord& s);
	static QColor  badgeColor(const QString& codecType);
	static QString formatSize(qint64 bytes);
	static QString formatSaved(qint64 bytes);

	static constexpr int kPosterW = 80;  // exposed so callers can compute poster-column hit rects

signals:
	void playRequested(const QModelIndex& index);
	void streamToggleRequested(const QModelIndex& index, int streamIndex);
	void imdbPageRequested(const QModelIndex& index);

private:
	static QRect playButtonRect(const QRect& contentRect);
	static QRect imdbButtonRect(const QRect& contentRect);

	// Returns streamIndex of the hovered audio/subtitle badge, or -1.
	// hasImdbBtn: pass true when the IMDb button column is reserved (narrows badge area).
	static int hitTestBadgeStream(const QPoint& pos, const QRect& itemRect,
	                               const QList<StreamRecord>& all,
	                               const QList<StreamRecord>& kept,
	                               const QFont& baseFont,
	                               bool hasImdbBtn = false);

	static int  drawBadge(QPainter* p, int x, int y, int h,
	                      const QString& text, const QColor& bg, const QFont& font,
	                      bool removed = false,
	                      bool hasTip = false, const QColor& cardBg = {},
	                      bool hovered = false);
	int  drawBadgeRow(QPainter* p, QRect rowRect,
	                  const QList<StreamRecord>& present,
	                  const QList<StreamRecord>& removed,
	                  const QFont& badgeFont,
	                  const QColor& cardBg,
	                  int hoveredStreamIndex = -1) const;
	static QColor  statusColor(const QString& status);
	static QString statusLabel(const QString& status);

	QAbstractItemView*    m_view             = nullptr;
	QPersistentModelIndex m_lastHoveredIndex;
	mutable QPoint        m_lastMousePos     {-1, -1}; // viewport coords; -1,-1 = outside

	static constexpr int kPadH        = 10;
	static constexpr int kPadV        = 6;
	static constexpr int kFolderH     = 13;
	static constexpr int kFolderGap   = 1;
	static constexpr int kHeaderH     = 24;
	static constexpr int kSepGap      = 4;
	static constexpr int kBadgeH    = 17;
	static constexpr int kRowGap    = 4;
	static constexpr int kBadgeGap  = 4;
	static constexpr int kBadgePad  = 6;
	static constexpr int kPlayBtnW  = 24;
	static constexpr int kPosterGap = 8;
};

} // namespace Mc
