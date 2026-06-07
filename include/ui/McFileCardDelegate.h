#pragma once
#include "core/DatabaseManager.h"
#include <QColor>
#include <QFont>
#include <QFontMetrics>
#include <QHash>
#include <QPersistentModelIndex>
#include <QRect>
#include <QSet>
#include <QSize>
#include <QStyledItemDelegate>

class QPainter;
class QAbstractItemView;

namespace Mc {

class McFileCardDelegate : public QStyledItemDelegate
{
	Q_OBJECT
public:
	explicit McFileCardDelegate(QObject* parent = nullptr);

	void  paint(QPainter* painter, const QStyleOptionViewItem& option,
	            const QModelIndex& index) const override;
	QSize sizeHint(const QStyleOptionViewItem& option,
	               const QModelIndex& index) const override;
	bool  editorEvent(QEvent* event, QAbstractItemModel* model,
	                  const QStyleOptionViewItem& option,
	                  const QModelIndex& index) override;
	bool  helpEvent(QHelpEvent* event, QAbstractItemView* view,
	                const QStyleOptionViewItem& option,
	                const QModelIndex& index) override;
	bool  eventFilter(QObject* obj, QEvent* event) override;

	bool  handlePress(const QPoint& viewportPos, const QRect& itemRect,
	                  const QFont& viewFont, const QModelIndex& index);

	static constexpr int kPosterW = 80;  // exposed so callers can compute poster-column hit rects

public slots:
	void invalidateSizeCacheFor(qint64 fileId);
	void clearSizeCache();

signals:
	void playRequested(const QModelIndex& index);
	void trackToggled(qint64 fileId, int streamIndex);

private:
	static QRect   playButtonRect(const QRect& contentRect);
	static QString formatDuration(double sec);
	static QString formatSize(qint64 bytes);
	static QString codecLabel(const StreamRecord& s);
	static QString channelStr(int channels);
	static QString buildBadgeText(const StreamRecord& s);
	static QColor  badgeColor(const QString& codecType);
	static int     drawBadge(QPainter* p, int x, int y, int h,
	                         const QString& text, const QColor& bg, const QFont& font,
	                         bool removed = false,
	                         bool hasTip = false, const QColor& cardBg = {});

	int  drawBadgeRow(QPainter* p, QRect rowRect,
	                  const QList<StreamRecord>& tracks,
	                  const QFont& badgeFont,
	                  const QColor& cardBg,
	                  const QSet<int>& forcedRemovals = {}) const;

	static bool hitTestInteractive(const QPoint& pos, const QRect& itemRect);

	QAbstractItemView*    m_view             = nullptr;
	QPersistentModelIndex m_lastHoveredIndex;
	mutable QPoint        m_lastMousePos     {-1, -1}; // viewport coords; -1,-1 = outside

	mutable QHash<int, QSize> m_sizeCache;
	mutable int               m_cacheWidth   = 0;
	mutable QFont             m_badgeFont;
	mutable QFontMetrics      m_badgeFm      { QFont{} };

	static constexpr int kPadH        = 10;
	static constexpr int kPadV        = 6;
	static constexpr int kPadBottom   = 9;
	static constexpr int kFolderH     = 13;  // folder label row height
	static constexpr int kFolderGap   = 1;   // gap between folder row and filename header
	static constexpr int kHeaderH     = 20;
	static constexpr int kSepGap      = 4;
	static constexpr int kBadgeH    = 18;
	static constexpr int kRowGap    = 4;
	static constexpr int kBadgeGap  = 4;
	static constexpr int kBadgePad  = 6;
	static constexpr int kPlayBtnW  = 28;
	static constexpr int kPosterGap = 8;   // gap between poster column and content (≈ kPadV)
};

} // namespace Mc
