#include "ui/McFileCardDelegate.h"
#include "ui/McFileListModel.h"
#include "classifier/RegexClassifier.h"
#include "core/ExternalTools.h"

#include <QAbstractItemView>
#include <QDir>
#include <QFileInfo>
#include <QFontMetrics>
#include <QHelpEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmapCache>
#include <QStyle>
#include <QStyleOption>
#include <QStyleOptionViewItem>
#include <QToolTip>

namespace Mc {

McFileCardDelegate::McFileCardDelegate(QObject* parent)
	: QStyledItemDelegate(parent)
{
	if (auto* view = qobject_cast<QAbstractItemView*>(parent)) {
		m_view = view;
		view->viewport()->installEventFilter(this);
	}
}

// ── Static helpers ─────────────────────────────────────────────────────────────

QString McFileCardDelegate::formatDuration(double sec)
{
	if (sec <= 0) return {};
	const int t = static_cast<int>(sec);
	const int h = t / 3600, m = (t % 3600) / 60, s = t % 60;
	return h > 0
		? QStringLiteral("%1:%2:%3").arg(h).arg(m, 2, 10, QChar('0')).arg(s, 2, 10, QChar('0'))
		: QStringLiteral("%1:%2").arg(m).arg(s, 2, 10, QChar('0'));
}

QString McFileCardDelegate::formatSize(qint64 bytes)
{
	const double gb = bytes / 1073741824.0;
	if (gb >= 1.0) return QStringLiteral("%1 GB").arg(gb, 0, 'f', 2);
	return QStringLiteral("%1 MB").arg(bytes / 1048576.0, 0, 'f', 1);
}

QString McFileCardDelegate::codecLabel(const StreamRecord& s)
{
	const QString n = s.codecName.toLower();
	if (n == "h264")                    return "H.264";
	if (n == "hevc")                    return "H.265";
	if (n == "av1")                     return "AV1";
	if (n == "vp9")                     return "VP9";
	if (n == "vp8")                     return "VP8";
	if (n == "mpeg4")                   return "MPEG-4";
	if (n == "mpeg2video")              return "MPEG-2";
	if (n == "dts") {
		const QString p = s.codecProfile.toLower();
		const QString t = s.title.toLower();
		if (p.contains("dts:x") || p.contains(":x") || t.contains("dts:x") || t.contains("dts-x"))
			return "DTS:X";
		if (p.contains("ma") || t.contains("master audio") || t.contains("dts-hd ma"))
			return "DTS-HD MA";
		if (p.contains("hra") || t.contains("hra"))
			return "DTS-HD HRA";
		return "DTS";
	}
	if (n == "truehd") {
		const QString p = s.codecProfile.toLower();
		const QString t = s.title.toLower();
		return (p.contains("atmos") || t.contains("atmos")) ? "Atmos" : "TrueHD";
	}
	if (n == "eac3")                    return "DD+";
	if (n == "ac3")                     return "DD";
	if (n == "aac")                     return "AAC";
	if (n == "mp3")                     return "MP3";
	if (n == "flac")                    return "FLAC";
	if (n == "opus")                    return "Opus";
	if (n == "vorbis")                  return "Vorbis";
	if (n.startsWith("pcm_"))           return "PCM";
	if (n == "subrip" || n == "srt")    return "SRT";
	if (n == "ass" || n == "ssa")       return "ASS";
	if (n == "hdmv_pgs_subtitle")       return "PGS";
	if (n == "dvd_subtitle")            return "VobSub";
	if (n == "webvtt")                  return "VTT";
	return s.codecName.toUpper();
}

QString McFileCardDelegate::channelStr(int ch)
{
	switch (ch) {
	case 1: return "1.0";
	case 2: return "2.0";
	case 6: return "5.1";
	case 7: return "6.1";
	case 8: return "7.1";
	default: return ch > 0 ? QString::number(ch) + "ch" : QString();
	}
}

QString McFileCardDelegate::buildBadgeText(const StreamRecord& s)
{
	QString t = codecLabel(s);
	if (s.codecType == "video") {
		if (s.width > 0)
			t += QStringLiteral("  %1×%2").arg(s.width).arg(s.height);
		if (!s.hdrFormat.isEmpty())
			t += "  " + s.hdrFormat;
	} else if (s.codecType == "audio") {
		const QString ch = channelStr(s.channels);
		if (!ch.isEmpty()) t += "  " + ch;
		if (!s.language.isEmpty() && s.language != "und")
			t += "  " + s.language.toUpper();
		if (s.isDefault) t += "  \xE2\x98\x85"; // ★ default track
	} else if (s.codecType == "subtitle") {
		if (!s.language.isEmpty() && s.language != "und")
			t += "  " + s.language.toUpper();
		if (s.isForced) t += "  \xE2\x97\x8F"; // ● forced marker
		if (s.isDefault) t += "  \xE2\x98\x85"; // ★ default track
	}
	return t;
}

QColor McFileCardDelegate::badgeColor(const QString& codecType)
{
	if (codecType == "video")    return { 0xa0, 0x50, 0x00 };
	if (codecType == "audio")    return { 0x10, 0x6a, 0xc0 };
	if (codecType == "subtitle") return { 0x1a, 0x86, 0x4a };
	return { 0x60, 0x60, 0x60 };
}

int McFileCardDelegate::drawBadge(QPainter* p, int x, int y, int h,
								   const QString& text, const QColor& bg, const QFont& font,
								   bool removed, bool hasTip, const QColor& cardBg)
{
	const int w = QFontMetrics(font).horizontalAdvance(text) + 2 * kBadgePad;
	const QRect r(x, y, w, h);
	p->save();
	p->setRenderHint(QPainter::Antialiasing);

	const QColor actualBg = removed ? bg.darker(180) : bg;
	p->setBrush(actualBg);

	if (hasTip && cardBg.isValid()) {
		const qreal luma = 0.299 * cardBg.redF()
						 + 0.587 * cardBg.greenF()
						 + 0.114 * cardBg.blueF();
		QColor border = actualBg.lighter(luma < 0.5 ? 210 : 165);
		p->setPen(QPen(border, 1));
		p->drawRoundedRect(QRectF(r).adjusted(0.5, 0.5, -0.5, -0.5), 3, 3);
	} else {
		p->setPen(Qt::NoPen);
		p->drawRoundedRect(r, 3, 3);
	}

	p->setPen(removed ? QColor(0xff, 0xff, 0xff, 90) : Qt::white);
	p->setFont(font);
	p->drawText(r, Qt::AlignCenter, text);

	if (removed) {
		const int mid = r.top() + r.height() / 2;
		p->setRenderHint(QPainter::Antialiasing, false);
		p->setPen(QPen(QColor(0xe8, 0x30, 0x30, 200), 2));
		p->drawLine(r.left() + 3, mid, r.right() - 3, mid);
	}

	p->restore();
	return w;
}

int McFileCardDelegate::drawBadgeRow(QPainter* p, QRect rowRect,
									  const QList<StreamRecord>& tracks,
									  const QFont& badgeFont,
									  const QColor& cardBg,
									  const QSet<int>& forcedRemovals) const
{
	if (tracks.isEmpty()) return 0;

	const QColor      color = badgeColor(tracks.first().codecType);
	const QFontMetrics fm(badgeFont);
	const int maxX = rowRect.right();
	int       x    = rowRect.left();
	int       row  = 0;

	for (int i = 0; i < tracks.size(); ++i) {
		const StreamRecord& s       = tracks.at(i);
		const QString       text    = buildBadgeText(s);
		const int           bW      = fm.horizontalAdvance(text) + 2 * kBadgePad;
		const bool          removed = forcedRemovals.contains(s.streamIndex);
		const bool          isLast  = (i == tracks.size() - 1);

		if (x > rowRect.left() && x + bW > maxX) {
			row++;
			x = rowRect.left();
		}
		const int bY = rowRect.top() + row * (kBadgeH + kRowGap);
		const bool hasTip = !s.title.isEmpty() || s.isDefault || s.isHearingImpaired;
		x += drawBadge(p, x, bY, kBadgeH, text, color, badgeFont,
		               removed, hasTip, cardBg);
		if (!isLast) x += kBadgeGap;
	}
	return row + 1;
}

// ── Interactive helpers ────────────────────────────────────────────────────────

QRect McFileCardDelegate::playButtonRect(const QRect& contentRect)
{
	const int bY = contentRect.top() + kFolderH + kFolderGap + (kHeaderH - kBadgeH) / 2;
	return QRect(contentRect.right() - kPlayBtnW, bY, kPlayBtnW, kBadgeH);
}

bool McFileCardDelegate::editorEvent(QEvent*, QAbstractItemModel*,
									  const QStyleOptionViewItem&, const QModelIndex&)
{
	// All interaction is handled via the viewport eventFilter — see constructor.
	return false;
}

bool McFileCardDelegate::hitTestInteractive(const QPoint& pos, const QRect& itemRect)
{
	const QRect content = itemRect.adjusted(kPosterW + kPosterGap, kPadV, -kPadH, -kPadBottom);
	return playButtonRect(content).contains(pos);
}

bool McFileCardDelegate::eventFilter(QObject* obj, QEvent* event)
{
	if (!m_view || obj != m_view->viewport())
		return false;

	switch (event->type()) {
	case QEvent::MouseMove: {
		const auto* me  = static_cast<const QMouseEvent*>(event);
		const QPoint    pos = me->position().toPoint();
		m_lastMousePos      = pos;

		const QModelIndex cur = m_view->indexAt(pos);
		if (QPersistentModelIndex(cur) != m_lastHoveredIndex) {
			if (m_lastHoveredIndex.isValid())
				m_view->viewport()->update(m_view->visualRect(m_lastHoveredIndex));
			m_lastHoveredIndex = QPersistentModelIndex(cur);
		}
		if (cur.isValid()) {
			m_view->viewport()->update(m_view->visualRect(cur));
			m_view->viewport()->setCursor(
			    hitTestInteractive(pos, m_view->visualRect(cur))
			        ? Qt::PointingHandCursor : Qt::ArrowCursor);
		} else {
			m_view->viewport()->setCursor(Qt::ArrowCursor);
		}
		break;
	}
	case QEvent::Leave:
		m_lastMousePos = QPoint(-1, -1);
		if (m_lastHoveredIndex.isValid()) {
			m_view->viewport()->update(m_view->visualRect(m_lastHoveredIndex));
			m_lastHoveredIndex = QPersistentModelIndex();
		}
		m_view->viewport()->setCursor(Qt::ArrowCursor);
		break;

	default: break;
	}
	return false; // never consume — selection still proceeds normally
}

bool McFileCardDelegate::handlePress(const QPoint& pos, const QRect& itemRect,
                                      const QFont& /*viewFont*/, const QModelIndex& index)
{
	const QRect content = itemRect.adjusted(
	    kPosterW + kPosterGap, kPadV, -kPadH, -kPadBottom);

	if (playButtonRect(content).contains(pos)) {
		emit playRequested(index);
		return true;
	}
	return false;
}

bool McFileCardDelegate::helpEvent(QHelpEvent* event, QAbstractItemView* view,
									const QStyleOptionViewItem& option,
									const QModelIndex& index)
{
	if (event->type() != QEvent::ToolTip)
		return QStyledItemDelegate::helpEvent(event, view, option, index);

	const auto streams = index.data(McFileListModel::StreamsRole).value<QList<StreamRecord>>();
	const QRect content = option.rect.adjusted(
		kPosterW + kPosterGap, kPadV, -kPadH, -kPadBottom);

	// Play button tooltip
	if (playButtonRect(content).contains(event->pos())) {
		const QString tip = ExternalTools::instance().isVlcAvailable()
			? tr("Play in VLC")
			: tr("Play  (VLC not detected — will open in default player)");
		QToolTip::showText(event->globalPos(), tip, view);
		return true;
	}

	QFont badgeFont = option.font;
	badgeFont.setPointSizeF(option.font.pointSizeF() * 0.82);
	const QFontMetrics fm(badgeFont);

	int y = content.top() + kFolderH + kFolderGap + kHeaderH + kSepGap;
	const int contentW = content.width();

	for (const QString& type : {QStringLiteral("video"), QStringLiteral("audio"), QStringLiteral("subtitle")}) {
		QList<StreamRecord> group;
		for (const auto& s : streams)
			if (s.codecType == type) group << s;
		if (group.isEmpty()) continue;

		// Compute wrapped height for this type
		int numRows = 1, rx = 0;
		for (const auto& s : group) {
			const int bW = fm.horizontalAdvance(buildBadgeText(s)) + 2 * kBadgePad;
			if (rx > 0 && rx + bW > contentW) { numRows++; rx = 0; }
			rx += bW + kBadgeGap;
		}
		const int typeH = numRows * kBadgeH + (numRows - 1) * kRowGap;
		const QRect typeRect(content.left(), y, contentW, typeH);

		if (typeRect.contains(event->pos())) {
			int x = content.left(), row = 0;
			for (int i = 0; i < group.size(); ++i) {
				const StreamRecord& s   = group.at(i);
				const QString       txt = buildBadgeText(s);
				const int           bW  = fm.horizontalAdvance(txt) + 2 * kBadgePad;
				if (x > content.left() && x + bW > content.right()) { row++; x = content.left(); }
				const int by = y + row * (kBadgeH + kRowGap);
				if (QRect(x, by, bW, kBadgeH).contains(event->pos())) {
					static const RegexClassifier clf;
					const auto cls = clf.classify(s.title, s.language, s.codecName);
					QStringList lines;
					if (!s.title.isEmpty())  lines << s.title;
					if (s.isDefault)         lines << tr("★ Default track");
					if (s.isHearingImpaired) lines << tr("SDH / Hearing impaired");
					if (cls.type != TrackType::Main) {
						QString typeName;
						switch (cls.type) {
						case TrackType::Commentary:     typeName = tr("Commentary");      break;
						case TrackType::Sdh:            typeName = tr("SDH");             break;
						case TrackType::HearingImpaired:typeName = tr("Hearing impaired");break;
						case TrackType::Forced:         typeName = tr("Forced subtitle"); break;
						case TrackType::Signs:          typeName = tr("Signs / songs");   break;
						default: break;
						}
						if (!typeName.isEmpty())
							lines << tr("%1  (%2% confidence)").arg(typeName).arg(qRound(cls.confidence * 100));
					}
					if (!lines.isEmpty())
						QToolTip::showText(event->globalPos(), lines.join('\n'), view);
					else
						QToolTip::hideText();
					return true;
				}
				x += bW + kBadgeGap;
			}
		}
		y += typeH + kRowGap;
	}

	QToolTip::hideText();
	return true;
}

// ── QStyledItemDelegate overrides ──────────────────────────────────────────────

QSize McFileCardDelegate::sizeHint(const QStyleOptionViewItem& option,
									const QModelIndex& index) const
{
	const int w = m_view ? m_view->viewport()->width() : option.rect.width();
	// Only bust the cache on genuine width changes (≥20 px). A scrollbar appearing
	// or disappearing shifts the viewport by ~14 px (scrollbar width) and must NOT
	// invalidate 3500-entry caches on every modal-dialog open/close cycle.
	if (m_cacheWidth == 0 || std::abs(w - m_cacheWidth) >= 20) {
		m_sizeCache.clear();
		m_cacheWidth = w;
	}
	const int row = index.row();
	const auto cached = m_sizeCache.constFind(row);
	if (cached != m_sizeCache.constEnd())
		return *cached;

	const auto streams = index.data(McFileListModel::StreamsRole).value<QList<StreamRecord>>();

	// Rebuild font metrics only when the view font changes (rare).
	{
		QFont badgeFont = option.font;
		badgeFont.setPointSizeF(option.font.pointSizeF() * 0.82);
		if (badgeFont != m_badgeFont) {
			m_badgeFont = badgeFont;
			m_badgeFm   = QFontMetrics(badgeFont);
		}
	}
	const QFontMetrics& fm = m_badgeFm;
	// Use m_cacheWidth (not w) so that heights are consistent with the cache bucket.
	// QListView ignores the returned width in ListMode — only the height matters.
	const int contentW = m_cacheWidth - kPosterW - kPosterGap - kPadH;

	int totalRows = 0;
	for (const QString& type : {QStringLiteral("video"), QStringLiteral("audio"), QStringLiteral("subtitle")}) {
		QList<StreamRecord> group;
		for (const auto& s : streams)
			if (s.codecType == type) group << s;
		if (group.isEmpty()) continue;

		int rows = 1, x = 0;
		for (const auto& s : group) {
			const int bW = fm.horizontalAdvance(buildBadgeText(s)) + 2 * kBadgePad;
			if (x > 0 && x + bW > contentW) { rows++; x = 0; }
			x += bW + kBadgeGap;
		}
		totalRows += rows;
	}

	const int trackH = totalRows > 0 ? totalRows * kBadgeH + (totalRows - 1) * kRowGap : 0;
	const int h      = kPadV + kFolderH + kFolderGap + kHeaderH + kSepGap + trackH + kPadBottom;
	const QSize result{m_cacheWidth, h < 60 ? 60 : h};
	m_sizeCache.insert(row, result);
	return result;
}

void McFileCardDelegate::clearSizeCache()
{
	m_sizeCache.clear();
}

void McFileCardDelegate::invalidateSizeCacheFor(qint64)
{
	m_sizeCache.clear();
}

void McFileCardDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
								const QModelIndex& index) const
{
	const auto file    = index.data(McFileListModel::FileRole).value<FileRecord>();
	const auto streams = index.data(McFileListModel::StreamsRole).value<QList<StreamRecord>>();

	painter->save();

	// ── Background ────────────────────────────────────────────────────────────
	const bool      sel = option.state & QStyle::State_Selected;
	const QPalette& pal = option.palette;
	const QColor    bg  = sel
		? pal.color(QPalette::Highlight)
		: (index.row() % 2 == 0
			? pal.color(QPalette::Base)
			: pal.color(QPalette::AlternateBase));

	painter->fillRect(option.rect, bg);

	painter->setPen(QPen(pal.color(QPalette::Mid), 1));
	painter->drawLine(option.rect.bottomLeft(), option.rect.bottomRight());

	// ── Poster column — always kPosterW wide, flush with card edges ───────────
	// Cards without a poster leave this column as the card background so all
	// content columns align across every row in the list.
	const QRect posterRect(option.rect.left(), option.rect.top(),
						   kPosterW, option.rect.height());
	const QString posterPath    = index.data(McFileListModel::PosterRole).toString();
	const int     posterVersion = index.data(McFileListModel::PosterVersionRole).toInt();
	if (!posterPath.isEmpty()) {
		QPixmap pm;
		const QString cacheKey = QStringLiteral("poster:%1:%2:%3")
									 .arg(posterPath).arg(option.rect.height()).arg(posterVersion);
		if (!QPixmapCache::find(cacheKey, &pm)) {
			const QPixmap src(posterPath);
			if (!src.isNull()) {
				pm = src.scaled(kPosterW, option.rect.height(),
								Qt::KeepAspectRatio,
								Qt::SmoothTransformation);
				QPixmapCache::insert(cacheKey, pm);
			}
		}
		if (!pm.isNull()) {
			const int px = posterRect.left() + (kPosterW   - pm.width())  / 2;
			const int py = posterRect.top()  + (option.rect.height() - pm.height()) / 2;
			painter->drawPixmap(px, py, pm);
		}
	}

	// ── Content area — starts after poster column, same for every card ────────
	// kPosterGap (8) ≈ kPadV (7): keeps left/top spacing visually consistent.
	const QRect content = option.rect.adjusted(
		kPosterW + kPosterGap, kPadV, -kPadH, -kPadBottom);

	const QColor textColor = sel ? pal.color(QPalette::HighlightedText) : pal.color(QPalette::Text);
	const QColor dimColor  = sel ? textColor.darker(140) : pal.color(QPalette::PlaceholderText);

	// Clip to the content area (excluding poster column).
	painter->setClipRect(QRect(option.rect.left() + kPosterW, option.rect.top(),
							   option.rect.width() - kPosterW, option.rect.height()));

	// ── Folder label — small dim text above filename ──────────────────────────
	{
		QFont folderFont = option.font;
		folderFont.setPointSizeF(option.font.pointSizeF() * 0.76);
		const QString folderName = QFileInfo(file.path).dir().dirName();
		const QRect   folderRect(content.left(), content.top(), content.width(), kFolderH);
		painter->setFont(folderFont);
		painter->setPen(dimColor);
		painter->drawText(folderRect, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
		                  folderName);
	}

	// ── Header: filename (bold, left) + duration + size + ▶ (right) ─────────
	{
		QFont fnFont = option.font;
		fnFont.setBold(true);

		QFont metaFont = option.font;
		metaFont.setPointSizeF(option.font.pointSizeF() * 0.87);

		const QRect hdr(content.left(), content.top() + kFolderH + kFolderGap,
		                content.width(), kHeaderH);

		// ▶ play button pill — highlight only when mouse is directly over the button
		const QRect playBtn = playButtonRect(content);
		const bool  hovered = playBtn.contains(m_lastMousePos);
		drawBadge(painter, playBtn.left(), playBtn.top(), playBtn.height(),
				  "\xE2\x96\xB6",   // ▶
				  hovered ? QColor(0x58, 0x58, 0x58) : QColor(0x38, 0x38, 0x38),
				  metaFont);

		// Meta (duration + size) left of the play button
		QString meta;
		if (file.durationSec > 0)  meta  = formatDuration(file.durationSec);
		if (file.sizeBytes    > 0) meta += (meta.isEmpty() ? QString() : "    ") + formatSize(file.sizeBytes);

		int metaW = 0;
		if (!meta.isEmpty()) {
			metaW = QFontMetrics(metaFont).horizontalAdvance(meta) + 8;
			const int metaX = playBtn.left() - kBadgeGap - metaW;
			painter->setFont(metaFont);
			painter->setPen(dimColor);
			painter->drawText(QRect(metaX, hdr.top(), metaW, hdr.height()),
							  Qt::AlignRight | Qt::AlignVCenter, meta);
		}

		// Filename takes remaining space
		const int rightEdge = playBtn.left() - kBadgeGap - metaW;
		painter->setFont(fnFont);
		painter->setPen(textColor);
		painter->drawText(QRect(hdr.left(), hdr.top(), rightEdge - hdr.left() - 4, hdr.height()),
						  Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
						  file.filename);
	}

	// ── Track badge rows ──────────────────────────────────────────────────────
	const auto overrides = index.data(McFileListModel::OverridesRole).value<QSet<int>>();

	QFont badgeFont = option.font;
	badgeFont.setPointSizeF(option.font.pointSizeF() * 0.82);

	int y = content.top() + kFolderH + kFolderGap + kHeaderH + kSepGap;

	const auto drawGroup = [&](const QString& type) {
		QList<StreamRecord> group;
		for (const auto& s : streams)
			if (s.codecType == type) group << s;
		if (group.isEmpty()) return;
		const int rows = drawBadgeRow(painter, QRect(content.left(), y, content.width(), kBadgeH),
		                              group, badgeFont, bg, overrides);
		y += rows * (kBadgeH + kRowGap);
	};

	drawGroup("video");
	drawGroup("audio");
	drawGroup("subtitle");

	painter->restore();
}

} // namespace Mc
