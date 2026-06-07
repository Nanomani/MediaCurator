#include "ui/McJobCardDelegate.h"
#include "ui/McJobListModel.h"
#include "classifier/RegexClassifier.h"
#include "core/ExternalTools.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QDir>
#include <QEvent>
#include <QFileInfo>
#include <QFontMetrics>
#include <QHelpEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPersistentModelIndex>
#include <QPixmap>
#include <QPixmapCache>
#include <QStyle>
#include <QStyleOption>
#include <QToolTip>
#include <algorithm>

namespace Mc {

McJobCardDelegate::McJobCardDelegate(QObject* parent)
	: QStyledItemDelegate(parent)
{
	if (auto* view = qobject_cast<QAbstractItemView*>(parent)) {
		m_view = view;
		view->viewport()->installEventFilter(this);
	}
}

// ── Static helpers ─────────────────────────────────────────────────────────────

QString McJobCardDelegate::codecLabel(const StreamRecord& s)
{
	const QString n = s.codecName.toLower();
	if (n == "h264")                   return "H.264";
	if (n == "hevc")                   return "H.265";
	if (n == "av1")                    return "AV1";
	if (n == "vp9")                    return "VP9";
	if (n == "vp8")                    return "VP8";
	if (n == "mpeg4")                  return "MPEG-4";
	if (n == "mpeg2video")             return "MPEG-2";
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
	if (n == "eac3")                   return "DD+";
	if (n == "ac3")                    return "DD";
	if (n == "aac")                    return "AAC";
	if (n == "mp3")                    return "MP3";
	if (n == "flac")                   return "FLAC";
	if (n == "opus")                   return "Opus";
	if (n == "vorbis")                 return "Vorbis";
	if (n.startsWith("pcm_"))          return "PCM";
	if (n == "subrip" || n == "srt")   return "SRT";
	if (n == "ass"    || n == "ssa")   return "ASS";
	if (n == "hdmv_pgs_subtitle")      return "PGS";
	if (n == "dvd_subtitle")           return "VobSub";
	if (n == "webvtt")                 return "VTT";
	return s.codecName.toUpper();
}

QString McJobCardDelegate::channelStr(int ch)
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

QString McJobCardDelegate::buildBadgeText(const StreamRecord& s)
{
	QString t = codecLabel(s);
	if (s.codecType == "video") {
		if (s.width > 0)
			t += QStringLiteral("  %1\xC3\x97%2").arg(s.width).arg(s.height);
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

QColor McJobCardDelegate::badgeColor(const QString& codecType)
{
	if (codecType == "video")    return { 0xa0, 0x50, 0x00 };
	if (codecType == "audio")    return { 0x10, 0x6a, 0xc0 };
	if (codecType == "subtitle") return { 0x1a, 0x86, 0x4a };
	return { 0x60, 0x60, 0x60 };
}

QString McJobCardDelegate::formatSize(qint64 bytes)
{
	const double gb = bytes / 1073741824.0;
	if (gb >= 1.0) return QStringLiteral("%1 GB").arg(gb, 0, 'f', 2);
	return QStringLiteral("%1 MB").arg(bytes / 1048576.0, 0, 'f', 1);
}

QString McJobCardDelegate::formatSaved(qint64 bytes)
{
	if (bytes <= 0) return {};
	return QStringLiteral("-%1").arg(formatSize(bytes));
}

QColor McJobCardDelegate::statusColor(const QString& status)
{
	if (status == "proposed")  return { 0x88, 0x88, 0x88 };
	if (status == "queued")   return { 0xc0, 0x80, 0x00 };
	if (status == "running")   return { 0x10, 0x6a, 0xc0 };
	if (status == "done")      return { 0x1a, 0x86, 0x4a };
	if (status == "failed")    return { 0xcc, 0x22, 0x22 };
	if (status == "cancelled") return { 0x88, 0x88, 0x88 };
	return { 0x60, 0x60, 0x60 };
}

QString McJobCardDelegate::statusLabel(const QString& status)
{
	if (status == "proposed")  return "Proposed";
	if (status == "queued")   return "Queued";
	if (status == "running")   return "Running\xE2\x80\xA6";
	if (status == "done")      return "Done";
	if (status == "failed")    return "Failed";
	if (status == "cancelled") return "Cancelled";
	return status;
}

// ── Badge drawing ──────────────────────────────────────────────────────────────

int McJobCardDelegate::drawBadge(QPainter* p, int x, int y, int h,
								  const QString& text, const QColor& bg,
								  const QFont& font, bool removed,
								  bool hasTip, const QColor& cardBg,
								  bool hovered)
{
	const int w = QFontMetrics(font).horizontalAdvance(text) + 2 * kBadgePad;
	const QRect r(x, y, w, h);

	p->save();
	p->setRenderHint(QPainter::Antialiasing);

	// Pill background — hover lightens, removed dims
	const QColor actualBg = removed
	    ? (hovered ? bg.darker(150) : bg.darker(180))
	    : (hovered ? bg.lighter(130) : bg);
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

	// Label text — faded for removed tracks
	p->setPen(removed ? QColor(0xff, 0xff, 0xff, 90) : Qt::white);
	p->setFont(font);
	p->drawText(r, Qt::AlignCenter, text);

	// Red strike line through the pill centre for removed tracks
	if (removed) {
		const int mid = r.top() + r.height() / 2;
		p->setRenderHint(QPainter::Antialiasing, false);
		p->setPen(QPen(QColor(0xe8, 0x30, 0x30, 160), 2));
		p->drawLine(r.left() + 3, mid, r.right() - 3, mid);
	}

	p->restore();
	return w;
}

int McJobCardDelegate::drawBadgeRow(QPainter* p, QRect rowRect,
									 const QList<StreamRecord>& present,
									 const QList<StreamRecord>& removed,
									 const QFont& badgeFont,
									 const QColor& cardBg,
									 int hoveredStreamIndex) const
{
	if (present.isEmpty() && removed.isEmpty()) return 0;

	const QString type  = present.isEmpty() ? removed.first().codecType
	                                         : present.first().codecType;
	const QColor  color = badgeColor(type);
	const QFontMetrics fm(badgeFont);
	const int maxX = rowRect.right();
	int       x    = rowRect.left();
	int       row  = 0;

	QList<QPair<StreamRecord, bool>> ordered;
	for (const auto& s : present) ordered.append({ s, false });
	for (const auto& s : removed) ordered.append({ s, true  });
	std::sort(ordered.begin(), ordered.end(), [](const auto& a, const auto& b) {
		return a.first.streamIndex < b.first.streamIndex;
	});

	for (int i = 0; i < ordered.size(); ++i) {
		const auto& [s, isRemoved] = ordered[i];
		const QString text  = buildBadgeText(s);
		const int     bW    = fm.horizontalAdvance(text) + 2 * kBadgePad;
		const bool    isLast = (i == ordered.size() - 1);

		if (x > rowRect.left() && x + bW > maxX) {
			row++;
			x = rowRect.left();
		}
		const int bY      = rowRect.top() + row * (kBadgeH + kRowGap);
		const bool isHovered = (s.streamIndex == hoveredStreamIndex);
		const bool hasTip = !s.title.isEmpty() || s.isDefault || s.isHearingImpaired;
		x += drawBadge(p, x, bY, kBadgeH, text, color, badgeFont, isRemoved,
		               hasTip, cardBg, isHovered);
		if (!isLast) x += kBadgeGap;
	}
	return row + 1;
}

// ── Interactive helpers ───────────────────────────────────────────────────────

QRect McJobCardDelegate::playButtonRect(const QRect& contentRect)
{
	const int bY = contentRect.top() + kFolderH + kFolderGap + (kHeaderH - kPlayBtnW) / 2;
	return QRect(contentRect.right() - kPlayBtnW, bY, kPlayBtnW, kPlayBtnW);
}

QRect McJobCardDelegate::imdbButtonRect(const QRect& contentRect)
{
	const int y = contentRect.top() + kFolderH + kFolderGap + kHeaderH + kSepGap;
	return QRect(contentRect.right() - kPlayBtnW, y, kPlayBtnW, kPlayBtnW);
}

int McJobCardDelegate::hitTestBadgeStream(const QPoint& pos, const QRect& itemRect,
                                           const QList<StreamRecord>& all,
                                           const QList<StreamRecord>& kept,
                                           const QFont& baseFont,
                                           bool hasImdbBtn)
{
	const QRect content = itemRect.adjusted(kPosterW + kPosterGap, kPadV, -kPadH, -kPadV);
	QFont badgeFont = baseFont;
	badgeFont.setPointSizeF(baseFont.pointSizeF() * 0.82);
	const QFontMetrics fm(badgeFont);
	const int contentW = content.width() - (hasImdbBtn ? kPlayBtnW + kBadgeGap : 0);

	int y = content.top() + kFolderH + kFolderGap + kHeaderH + kSepGap;

	for (const QString& type : {QStringLiteral("video"), QStringLiteral("audio"), QStringLiteral("subtitle")}) {
		QList<QPair<StreamRecord, bool>> ordered;
		for (const auto& s : all) {
			if (s.codecType != type) continue;
			const bool survives = std::any_of(kept.cbegin(), kept.cend(),
				[&s](const StreamRecord& k) { return k.streamIndex == s.streamIndex; });
			ordered.append({s, !survives});
		}
		if (ordered.isEmpty()) continue;
		std::sort(ordered.begin(), ordered.end(), [](const auto& a, const auto& b) {
			return a.first.streamIndex < b.first.streamIndex;
		});

		// Compute how many rows this type group occupies when wrapped
		int numRows = 1, rx = 0;
		for (const auto& item : ordered) {
			const int bW = fm.horizontalAdvance(buildBadgeText(item.first)) + 2 * kBadgePad;
			if (rx > 0 && rx + bW > contentW) { numRows++; rx = 0; }
			rx += bW + kBadgeGap;
		}
		const int typeH  = numRows * kBadgeH + (numRows - 1) * kRowGap;
		const QRect typeRect(content.left(), y, contentW, typeH);

		// Video badges are not toggleable — skip hit-testing but still advance y
		if (type != "video" && typeRect.contains(pos)) {
			int x = content.left(), row = 0;
			for (const auto& item : ordered) {
				const StreamRecord& s    = item.first;
				const QString       text = buildBadgeText(s);
				const int bW = fm.horizontalAdvance(text) + 2 * kBadgePad;
				if (x > content.left() && x + bW > content.right()) { row++; x = content.left(); }
				const int by = y + row * (kBadgeH + kRowGap);
				if (QRect(x, by, bW, kBadgeH).contains(pos))
					return s.streamIndex;
				x += bW + kBadgeGap;
			}
			return -1;
		}

		y += typeH + kRowGap;
	}
	return -1;
}

bool McJobCardDelegate::eventFilter(QObject* obj, QEvent* event)
{
	if (!m_view || obj != m_view->viewport()) return false;

	switch (event->type()) {
	case QEvent::MouseMove: {
		const auto*  me  = static_cast<const QMouseEvent*>(event);
		const QPoint pos = me->position().toPoint();
		m_lastMousePos = pos;

		const QModelIndex cur = m_view->indexAt(pos);
		if (QPersistentModelIndex(cur) != m_lastHoveredIndex) {
			if (m_lastHoveredIndex.isValid())
				m_view->viewport()->update(m_view->visualRect(m_lastHoveredIndex));
			m_lastHoveredIndex = QPersistentModelIndex(cur);
		}
		if (cur.isValid()) {
			m_view->viewport()->update(m_view->visualRect(cur));

			const QRect vizRect = m_view->visualRect(cur);
			const QRect content = vizRect.adjusted(kPosterW + kPosterGap, kPadV, -kPadH, -kPadV);
			const bool  overPlay = playButtonRect(content).contains(pos);

			const QString imdbId = cur.data(McJobListModel::ImdbIdRole).toString();
			const bool hasImdb   = !imdbId.isEmpty();
			const bool overImdb  = hasImdb && imdbButtonRect(content).contains(pos);

			bool overToggleBadge = false;
			if (!overPlay && !overImdb && cur.data(McJobListModel::StatusRole).toString() == "proposed") {
				const auto all  = cur.data(McJobListModel::AllStreamsRole).value<QList<StreamRecord>>();
				const auto kept = cur.data(McJobListModel::KeptStreamsRole).value<QList<StreamRecord>>();
				overToggleBadge = (hitTestBadgeStream(pos, vizRect, all, kept, m_view->font(), hasImdb) >= 0);
			}

			m_view->viewport()->setCursor(
				(overPlay || overImdb || overToggleBadge) ? Qt::PointingHandCursor : Qt::ArrowCursor);
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
	return false;
}

bool McJobCardDelegate::handlePress(const QPoint& viewportPos, const QRect& itemRect,
                                     const QFont& viewFont, const QModelIndex& index)
{
	if (!index.isValid()) return false;
	const QRect content = itemRect.adjusted(kPosterW + kPosterGap, kPadV, -kPadH, -kPadV);

	if (playButtonRect(content).contains(viewportPos)) {
		emit playRequested(index);
		return true;
	}

	const QString imdbId = index.data(McJobListModel::ImdbIdRole).toString();
	const bool hasImdb = !imdbId.isEmpty();
	if (hasImdb && imdbButtonRect(content).contains(viewportPos)) {
		emit imdbPageRequested(index);
		return true;
	}

	if (index.data(McJobListModel::StatusRole).toString() != "proposed")
		return false;

	const auto all  = index.data(McJobListModel::AllStreamsRole).value<QList<StreamRecord>>();
	const auto kept = index.data(McJobListModel::KeptStreamsRole).value<QList<StreamRecord>>();
	const int streamIdx = hitTestBadgeStream(viewportPos, itemRect, all, kept, viewFont, hasImdb);
	if (streamIdx >= 0) {
		emit streamToggleRequested(index, streamIdx);
		return true;
	}

	return false;
}

bool McJobCardDelegate::editorEvent(QEvent*, QAbstractItemModel*,
									 const QStyleOptionViewItem&,
									 const QModelIndex&)
{
	// All interaction handled via eventFilter + pressed signal — see McJobPanel setup.
	return false;
}

// ── helpEvent (tooltips) ──────────────────────────────────────────────────────

bool McJobCardDelegate::helpEvent(QHelpEvent* event, QAbstractItemView* view,
								   const QStyleOptionViewItem& option,
								   const QModelIndex& index)
{
	if (event->type() != QEvent::ToolTip)
		return QStyledItemDelegate::helpEvent(event, view, option, index);

	const auto all = index.data(McJobListModel::AllStreamsRole).value<QList<StreamRecord>>();
	const QRect content = option.rect.adjusted(kPosterW + kPosterGap, kPadV, -kPadH, -kPadV);

	// IMDb button tooltip
	const QString imdbId = index.data(McJobListModel::ImdbIdRole).toString();
	const bool hasImdb = !imdbId.isEmpty();
	if (hasImdb && imdbButtonRect(content).contains(event->pos())) {
		QToolTip::showText(event->globalPos(),
		    tr("Open IMDb page — %1").arg(imdbId), view);
		return true;
	}

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
		for (const auto& s : all)
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

// ── sizeHint ──────────────────────────────────────────────────────────────────

QSize McJobCardDelegate::sizeHint(const QStyleOptionViewItem& option,
								   const QModelIndex& index) const
{
	const auto all = index.data(McJobListModel::AllStreamsRole).value<QList<StreamRecord>>();

	QFont badgeFont = option.font;
	badgeFont.setPointSizeF(option.font.pointSizeF() * 0.82);
	const QFontMetrics fm(badgeFont);
	const bool hasImdb = !index.data(McJobListModel::ImdbIdRole).toString().isEmpty();
	const int contentW = option.rect.width() - kPosterW - kPosterGap - kPadH
	                   - (hasImdb ? kPlayBtnW + kBadgeGap : 0);

	int totalRows = 0;
	for (const QString& type : {QStringLiteral("video"), QStringLiteral("audio"), QStringLiteral("subtitle")}) {
		QList<StreamRecord> group;
		for (const auto& s : all)
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

	const int trackH     = totalRows > 0 ? totalRows * kBadgeH + (totalRows - 1) * kRowGap : 0;
	const int badgeAreaH = hasImdb ? qMax(trackH, kPlayBtnW) : trackH;
	const int h          = kPadV + kFolderH + kFolderGap + kHeaderH + kSepGap + badgeAreaH + kPadV;
	return { option.rect.width(), h < kPosterW ? kPosterW : h };
}

// ── paint ─────────────────────────────────────────────────────────────────────

void McJobCardDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
							   const QModelIndex& index) const
{
	const QString filename = index.data(McJobListModel::FilenameRole).toString();
	const QString status   = index.data(McJobListModel::StatusRole).toString();
	const qint64  saved    = index.data(McJobListModel::SavedRole).toLongLong();
	const int     progress = index.data(McJobListModel::ProgressRole).toInt();
	const auto    all      = index.data(McJobListModel::AllStreamsRole).value<QList<StreamRecord>>();
	const auto    kept     = index.data(McJobListModel::KeptStreamsRole).value<QList<StreamRecord>>();
	const bool    checked  = (index.data(Qt::CheckStateRole).toInt() == Qt::Checked);

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

	// ── Poster column — flush with card edges, always kPosterW wide ───────────
	const QRect posterRect(option.rect.left(), option.rect.top(),
	                       kPosterW, option.rect.height());
	const QString posterPath = index.data(McJobListModel::PosterRole).toString();
	if (!posterPath.isEmpty()) {
		QPixmap pm;
		const QString cacheKey = QStringLiteral("poster:%1:%2")
		                             .arg(posterPath).arg(option.rect.height());
		if (!QPixmapCache::find(cacheKey, &pm)) {
			const QPixmap src(posterPath);
			if (!src.isNull()) {
				pm = src.scaled(kPosterW, option.rect.height(),
				                Qt::KeepAspectRatio, Qt::SmoothTransformation);
				QPixmapCache::insert(cacheKey, pm);
			}
		}
		if (!pm.isNull()) {
			const int px = posterRect.left() + (kPosterW         - pm.width())  / 2;
			const int py = posterRect.top()  + (option.rect.height() - pm.height()) / 2;
			painter->drawPixmap(px, py, pm);
		}
	}

	// Checkbox for proposed jobs — overlaid bottom-left of poster column
	const bool checkable = (status == "proposed");
	if (checkable) {
		QStyleOptionButton cbOpt;
		cbOpt.rect  = QRect(posterRect.left() + 3,
		                    posterRect.bottom() - 19,
		                    16, 16);
		cbOpt.state = QStyle::State_Enabled
		            | (checked ? QStyle::State_On : QStyle::State_Off);
		QApplication::style()->drawControl(QStyle::CE_CheckBox, &cbOpt, painter);
	}

	// ── Content area — consistent for all cards ───────────────────────────────
	const QRect content = option.rect.adjusted(kPosterW + kPosterGap, kPadV, -kPadH, -kPadV);
	const QColor textColor = sel ? pal.color(QPalette::HighlightedText)
	                             : pal.color(QPalette::Text);

	painter->setClipRect(QRect(option.rect.left() + kPosterW, option.rect.top(),
	                           option.rect.width() - kPosterW, option.rect.height()));

	// ── Folder label ─────────────────────────────────────────────────────────────
	{
		const QString filePath  = index.data(McJobListModel::FilePathRole).toString();
		const QString folderName = QFileInfo(filePath).dir().dirName();
		QFont folderFont = option.font;
		folderFont.setPointSizeF(option.font.pointSizeF() * 0.76);
		const QColor dimColor = sel ? textColor.darker(140)
		                            : pal.color(QPalette::PlaceholderText);
		const QRect folderRect(content.left(), content.top(), content.width(), kFolderH);
		painter->setFont(folderFont);
		painter->setPen(dimColor);
		painter->drawText(folderRect, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, folderName);
	}

	// ── Header: filename (bold) | size | status pill | ▶ ───────────────────────
	{
		QFont fnFont = option.font;
		fnFont.setBold(true);

		QFont metaFont = option.font;
		metaFont.setPointSizeF(option.font.pointSizeF() * 0.87);

		QFont pillFont = option.font;
		pillFont.setPointSizeF(option.font.pointSizeF() * 0.80);

		const QRect hdr(content.left(), content.top() + kFolderH + kFolderGap, content.width(), kHeaderH);
		const QColor dimColor = sel ? textColor.darker(140)
		                            : pal.color(QPalette::PlaceholderText);

		// VLC logo button — right-edge column, aligned with header row
		const QRect playBtn = playButtonRect(content);
		const bool  hovered = playBtn.contains(m_lastMousePos);
		{
			QPixmap vlcLogo;
			if (!QPixmapCache::find("vlc_logo", &vlcLogo)) {
				vlcLogo = QPixmap(":/icons/vlc.svg").scaled(
				    playBtn.width(), playBtn.height(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
				QPixmapCache::insert("vlc_logo", vlcLogo);
			}
			painter->save();
			painter->setOpacity(hovered ? 1.0 : 0.80);
			const int ox = playBtn.left() + (playBtn.width()  - vlcLogo.width())  / 2;
			const int oy = playBtn.top()  + (playBtn.height() - vlcLogo.height()) / 2;
			painter->drawPixmap(ox, oy, vlcLogo);
			painter->restore();
		}

		// Status pill — right-aligned just left of the play button
		QString pillText = statusLabel(status);
		if (status == "running" && progress > 0)
			pillText = QStringLiteral("Running %1%\xE2\x80\xA6").arg(progress);
		if (saved > 0) pillText += QStringLiteral("  %1").arg(formatSaved(saved));
		const int pillW = QFontMetrics(pillFont).horizontalAdvance(pillText) + 2 * kBadgePad;
		const int pillX = playBtn.left() - kBadgeGap - pillW;
		const int pillY = hdr.top() + (hdr.height() - kBadgeH) / 2;
		drawBadge(painter, pillX, pillY, kBadgeH,
		          pillText, statusColor(status), pillFont, false);

		// File size — text just left of the status pill
		const qint64 sizeBytes = index.data(McJobListModel::FileSizeRole).toLongLong();
		int sizeW = 0;
		if (sizeBytes > 0) {
			const QString sizeText = formatSize(sizeBytes);
			sizeW = QFontMetrics(metaFont).horizontalAdvance(sizeText) + 8;
			const int sizeX = pillX - kBadgeGap - sizeW;
			painter->setFont(metaFont);
			painter->setPen(dimColor);
			painter->drawText(QRect(sizeX, hdr.top(), sizeW, hdr.height()),
			                  Qt::AlignRight | Qt::AlignVCenter, sizeText);
		}

		// Filename — fills remaining left space
		const int rightEdge = pillX - kBadgeGap - sizeW;
		painter->setFont(fnFont);
		painter->setPen(textColor);
		painter->drawText(QRect(hdr.left(), hdr.top(), rightEdge - hdr.left() - 4, hdr.height()),
		                  Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
		                  filename);
	}

	// ── Track badge rows: kept normal, removed struck with red line ───────────
	QFont badgeFont = option.font;
	badgeFont.setPointSizeF(option.font.pointSizeF() * 0.82);

	const QString imdbId = index.data(McJobListModel::ImdbIdRole).toString();
	const bool hasImdb   = !imdbId.isEmpty();
	const int badgeAreaW = content.width() - (hasImdb ? kPlayBtnW + kBadgeGap : 0);

	// IMDb logo button — right-edge column, aligned with badge rows
	if (hasImdb) {
		const QRect ir = imdbButtonRect(content);
		QPixmap logo;
		if (!QPixmapCache::find("imdb_logo", &logo)) {
			logo = QPixmap(":/icons/imdb.png").scaled(
			    ir.width(), ir.height(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
			QPixmapCache::insert("imdb_logo", logo);
		}
		painter->save();
		painter->setOpacity(ir.contains(m_lastMousePos) ? 1.0 : 0.75);
		const int ox = ir.left() + (ir.width()  - logo.width())  / 2;
		const int oy = ir.top()  + (ir.height() - logo.height()) / 2;
		painter->drawPixmap(ox, oy, logo);
		painter->restore();
	}

	// Compute which badge (if any) is hovered — only for Proposed items
	const int hoveredStreamIdx = (m_lastMousePos.x() >= 0 && status == "proposed")
	    ? hitTestBadgeStream(m_lastMousePos, option.rect, all, kept, option.font, hasImdb)
	    : -1;

	int y = content.top() + kFolderH + kFolderGap + kHeaderH + kSepGap;

	const auto drawGroup = [&](const QString& type) {
		QList<StreamRecord> present, removed;
		for (const auto& s : all) {
			if (s.codecType != type) continue;
			bool survives = false;
			for (const auto& k : kept)
				if (k.streamIndex == s.streamIndex) { survives = true; break; }
			(survives ? present : removed).append(s);
		}
		if (present.isEmpty() && removed.isEmpty()) return;
		const int rows = drawBadgeRow(painter,
		                              QRect(content.left(), y, badgeAreaW, kBadgeH),
		                              present, removed, badgeFont, bg, hoveredStreamIdx);
		y += rows * (kBadgeH + kRowGap);
	};

	drawGroup("video");
	drawGroup("audio");
	drawGroup("subtitle");

	// Progress bar — drawn last so it renders on top of the poster
	painter->setClipping(false);
	if (status == "running" && progress > 0) {
		const QRect barRect(option.rect.left(), option.rect.bottom() - 2,
		                    option.rect.width(), 3);
		painter->fillRect(barRect, QColor(0x30, 0x30, 0x40));
		const int fillW = barRect.width() * progress / 100;
		if (fillW > 0)
			painter->fillRect(QRect(barRect.left(), barRect.top(), fillW, barRect.height()),
			                  QColor(0x10, 0x6a, 0xc0));
	}

	painter->restore();
}

} // namespace Mc
