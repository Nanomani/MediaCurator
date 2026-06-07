#include "ui/McBulkSummaryDialog.h"
#include "ui/McJobListModel.h"
#include "core/DatabaseManager.h"

#include <algorithm>

#include <QColor>
#include <QDialogButtonBox>
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMap>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>

namespace Mc {

// ── Mini proportional bar ─────────────────────────────────────────────────────

class MiniBar : public QWidget
{
public:
	MiniBar(int value, int max, const QColor& color, QWidget* parent = nullptr)
		: QWidget(parent), m_value(value), m_max(max), m_color(color)
	{
		setFixedHeight(9);
		setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	}
protected:
	void paintEvent(QPaintEvent*) override
	{
		QPainter p(this);
		p.setRenderHint(QPainter::Antialiasing);
		p.setPen(Qt::NoPen);
		p.setBrush(QColor(128, 128, 128, 45));
		p.drawRoundedRect(rect(), 3, 3);
		if (m_max > 0 && m_value > 0) {
			QRect fill = rect();
			fill.setWidth(qMax(10, int(rect().width() * double(m_value) / m_max)));
			p.setBrush(m_color);
			p.drawRoundedRect(fill, 3, 3);
		}
	}
private:
	int    m_value;
	int    m_max;
	QColor m_color;
};

// ── Collapsible section ───────────────────────────────────────────────────────

class CollapsibleSection : public QWidget
{
public:
	CollapsibleSection(const QString& title, bool startExpanded, QWidget* parent = nullptr)
		: QWidget(parent), m_title(title)
	{
		auto* vbox = new QVBoxLayout(this);
		vbox->setContentsMargins(0, 0, 0, 0);
		vbox->setSpacing(0);

		m_toggle = new QPushButton(this);
		m_toggle->setFlat(true);
		m_toggle->setCursor(Qt::PointingHandCursor);
		m_toggle->setStyleSheet(
		    "QPushButton { text-align: left; padding: 4px 2px; font-weight: bold; border: none; }"
		    "QPushButton:hover { color: palette(highlight); }");

		m_container = new QWidget(this);
		m_rows = new QVBoxLayout(m_container);
		m_rows->setContentsMargins(0, 4, 0, 6);
		m_rows->setSpacing(0);

		vbox->addWidget(m_toggle);
		vbox->addWidget(m_container);

		setExpanded(startExpanded);

		connect(m_toggle, &QPushButton::clicked, this, [this]() {
			setExpanded(!m_expanded);
		});
	}

	QVBoxLayout* rowLayout() { return m_rows; }

private:
	void setExpanded(bool expanded)
	{
		m_expanded = expanded;
		m_container->setVisible(expanded);
		m_toggle->setText((expanded ? QStringLiteral("▼  ") : QStringLiteral("▶  ")) + m_title);
	}

	QPushButton* m_toggle    = nullptr;
	QWidget*     m_container = nullptr;
	QVBoxLayout* m_rows      = nullptr;
	QString      m_title;
	bool         m_expanded  = true;
};

// ── Stats struct ──────────────────────────────────────────────────────────────

struct BulkStats {
	int    filesAffected        = 0;
	int    audioRemoved         = 0;
	int    subtitleRemoved      = 0;
	int    videoRemoved         = 0;
	qint64 estimatedSavedBytes  = 0;

	int reasonLanguage   = 0;
	int reasonRedundancy = 0;
	int reasonSdh        = 0;
	int reasonCommentary = 0;
	int reasonMjpeg      = 0;
	int reasonOther      = 0;

	QMap<QString, int> audioByLang;
	QMap<QString, int> subtitleByLang;
};

static QString formatSize(qint64 bytes)
{
	if (bytes <= 0) return QStringLiteral("0 B");
	if (bytes >= qint64(1024) * 1024 * 1024)
		return QStringLiteral("%1 GB").arg(bytes / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2);
	if (bytes >= 1024 * 1024)
		return QStringLiteral("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 1);
	return QStringLiteral("%1 KB").arg(bytes / 1024.0, 0, 'f', 0);
}

static QString langDisplayName(const QString& code)
{
	static const QHash<QString, QString> names = {
		{"mul","Multiple"}, {"ara","Arabic"}, {"zho","Chinese"}, {"hrv","Croatian"},
		{"ces","Czech"},    {"dan","Danish"},  {"nld","Dutch"},   {"eng","English"},
		{"fin","Finnish"},  {"fra","French"},  {"deu","German"},  {"ell","Greek"},
		{"heb","Hebrew"},   {"hun","Hungarian"},{"ind","Indonesian"},{"ita","Italian"},
		{"jpn","Japanese"}, {"kor","Korean"},  {"nor","Norwegian"},{"pol","Polish"},
		{"por","Portuguese"},{"ron","Romanian"},{"rus","Russian"}, {"srp","Serbian"},
		{"slk","Slovak"},   {"spa","Spanish"}, {"swe","Swedish"}, {"tha","Thai"},
		{"tur","Turkish"},  {"ukr","Ukrainian"},{"vie","Vietnamese"},
	};
	const QString n = names.value(code);
	return n.isEmpty() ? code : QStringLiteral("%1 (%2)").arg(n, code);
}

static BulkStats computeStats()
{
	auto& db = DatabaseManager::instance();
	const auto jobs = db.allJobs();

	BulkStats s;

	for (const JobRecord& job : jobs) {
		if (job.status != QStringLiteral("proposed")) continue;
		++s.filesAffected;

		// Determine removed streams from kept-stream delta
		const auto allStreams  = db.streamsForFile(job.fileId);
		const auto keptStreams = McJobListModel::computeKeptStreams(allStreams, job.commandArgsJson);

		const auto fileOpt = db.fileById(job.fileId);
		const double durationSec = fileOpt ? fileOpt->durationSec : 0.0;

		QSet<int> keptIdx;
		for (const auto& sr : keptStreams) keptIdx.insert(sr.streamIndex);

		for (const StreamRecord& sr : allStreams) {
			if (keptIdx.contains(sr.streamIndex)) continue;
			if (sr.codecType == QStringLiteral("audio")) {
				++s.audioRemoved;
				if (!sr.language.isEmpty() && sr.language != QStringLiteral("und"))
					++s.audioByLang[sr.language];
			} else if (sr.codecType == QStringLiteral("subtitle")) {
				++s.subtitleRemoved;
				if (!sr.language.isEmpty() && sr.language != QStringLiteral("und"))
					++s.subtitleByLang[sr.language];
			} else if (sr.codecType == QStringLiteral("video")) {
				++s.videoRemoved;
			}

			// Estimate bytes freed: bitRate is in bits/sec; subtitle streams
			// rarely carry a bitrate, so fall back to a 256 KB flat estimate.
			if (sr.bitRate > 0 && durationSec > 0.0)
				s.estimatedSavedBytes += qint64(sr.bitRate / 8.0 * durationSec);
			else if (sr.codecType == QStringLiteral("subtitle"))
				s.estimatedSavedBytes += 256 * 1024;
		}

		// Bucket removal reasons from description text
		for (const QString& line : job.descriptionText.split('\n', Qt::SkipEmptyParts)) {
			// Each line: "  [TYPE] codec (lang) ... — reason"
			const int dash = line.indexOf(QStringLiteral(" — "));
			const QString reason = (dash >= 0) ? line.mid(dash + 3) : QString();

			if (reason.contains(QStringLiteral("MJPEG"), Qt::CaseInsensitive)
					|| reason.contains(QStringLiteral("cover-art"), Qt::CaseInsensitive)) {
				++s.reasonMjpeg;
			} else if (reason.contains(QStringLiteral("Redundant"), Qt::CaseInsensitive)) {
				++s.reasonRedundancy;
			} else if (reason.contains(QStringLiteral("SDH"), Qt::CaseInsensitive)
					|| reason.contains(QStringLiteral("superseded by"), Qt::CaseInsensitive)) {
				++s.reasonSdh;
			} else if (reason.contains(QStringLiteral("Commentary"), Qt::CaseInsensitive)) {
				++s.reasonCommentary;
			} else if (reason.contains(QStringLiteral("not understood"), Qt::CaseInsensitive)
					|| reason.contains(QStringLiteral("not in understood"), Qt::CaseInsensitive)
					|| reason.contains(QStringLiteral("not original"), Qt::CaseInsensitive)) {
				++s.reasonLanguage;
			} else {
				++s.reasonOther;
			}
		}
	}

	return s;
}

// ── Layout helpers ─────────────────────────────────────────────────────────────

static QFrame* separator(QWidget* parent)
{
	auto* f = new QFrame(parent);
	f->setFrameShape(QFrame::HLine);
	f->setFrameShadow(QFrame::Sunken);
	return f;
}

static QWidget* savingsCell(const QString& number, const QString& label, QWidget* parent)
{
	auto* w      = new QWidget(parent);
	auto* layout = new QVBoxLayout(w);
	layout->setContentsMargins(12, 8, 12, 8);
	layout->setSpacing(2);

	auto* numLbl = new QLabel(number, w);
	QFont f = numLbl->font();
	f.setPointSizeF(f.pointSizeF() * 2.8);
	f.setBold(true);
	numLbl->setFont(f);
	numLbl->setAlignment(Qt::AlignHCenter);
	QPalette np = numLbl->palette();
	np.setColor(QPalette::WindowText, np.color(QPalette::Highlight));
	numLbl->setPalette(np);

	auto* txtLbl = new QLabel(label, w);
	txtLbl->setAlignment(Qt::AlignHCenter);
	QFont sf = txtLbl->font();
	sf.setPointSizeF(sf.pointSizeF() * 0.88);
	txtLbl->setFont(sf);
	QPalette pal = txtLbl->palette();
	pal.setColor(QPalette::WindowText, pal.color(QPalette::WindowText).darker(130));
	txtLbl->setPalette(pal);

	layout->addWidget(numLbl);
	layout->addWidget(txtLbl);
	return w;
}

static QWidget* statCell(const QString& number, const QString& label, QWidget* parent)
{
	auto* w      = new QWidget(parent);
	auto* layout = new QVBoxLayout(w);
	layout->setContentsMargins(12, 8, 12, 8);
	layout->setSpacing(2);

	auto* numLbl = new QLabel(number, w);
	QFont f = numLbl->font();
	f.setPointSizeF(f.pointSizeF() * 2.2);
	f.setBold(true);
	numLbl->setFont(f);
	numLbl->setAlignment(Qt::AlignHCenter);

	auto* txtLbl = new QLabel(label, w);
	txtLbl->setAlignment(Qt::AlignHCenter);
	QFont sf = txtLbl->font();
	sf.setPointSizeF(sf.pointSizeF() * 0.88);
	txtLbl->setFont(sf);
	QPalette pal = txtLbl->palette();
	pal.setColor(QPalette::WindowText, pal.color(QPalette::WindowText).darker(130));
	txtLbl->setPalette(pal);

	layout->addWidget(numLbl);
	layout->addWidget(txtLbl);
	return w;
}

static void addBarRow(QVBoxLayout* layout, const QString& label, int value, int max,
                      const QColor& color, QWidget* parent)
{
	auto* row    = new QWidget(parent);
	auto* hbox   = new QHBoxLayout(row);
	hbox->setContentsMargins(0, 2, 0, 2);
	hbox->setSpacing(8);

	auto* nameLbl = new QLabel(label, row);
	nameLbl->setFixedWidth(200);
	nameLbl->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

	auto* bar = new MiniBar(value, max, color, row);

	auto* countLbl = new QLabel(QString::number(value), row);
	countLbl->setFixedWidth(40);
	countLbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
	QFont bf = countLbl->font();
	bf.setBold(true);
	countLbl->setFont(bf);

	hbox->addWidget(nameLbl);
	hbox->addWidget(bar, 1);
	hbox->addWidget(countLbl);
	layout->addWidget(row);
}

// ── Dialog ────────────────────────────────────────────────────────────────────

McBulkSummaryDialog::McBulkSummaryDialog(QWidget* parent)
	: QDialog(parent)
{
	setWindowTitle(tr("Analysis Summary"));
	setMinimumSize(640, 440);
	resize(780, 600);
	setAttribute(Qt::WA_DeleteOnClose);

	const BulkStats s = computeStats();
	const int totalTracks = s.audioRemoved + s.subtitleRemoved + s.videoRemoved;

	auto* root   = new QVBoxLayout(this);
	auto* scroll = new QScrollArea(this);
	scroll->setWidgetResizable(true);
	scroll->setFrameShape(QFrame::NoFrame);
	auto* body   = new QWidget(scroll);
	auto* layout = new QVBoxLayout(body);
	layout->setSpacing(8);
	layout->setContentsMargins(16, 12, 16, 12);
	scroll->setWidget(body);
	root->addWidget(scroll, 1);

	// ── Stat cells ────────────────────────────────────────────────────────────
	if (s.filesAffected == 0) {
		layout->addWidget(new QLabel(tr("No proposed jobs found — run Analyze Library first."), body));
		layout->addStretch();
	} else {
		auto* statRow    = new QWidget(body);
		auto* statLayout = new QHBoxLayout(statRow);
		statLayout->setContentsMargins(0, 0, 0, 0);
		statLayout->setSpacing(0);

		statLayout->addWidget(statCell(QString::number(s.filesAffected), tr("files\naffected"),  statRow));
		statLayout->addWidget(statCell(QString::number(totalTracks),     tr("tracks\nremoved"),  statRow));
		if (s.audioRemoved > 0)
			statLayout->addWidget(statCell(QString::number(s.audioRemoved),    tr("audio\nremoved"),    statRow));
		if (s.subtitleRemoved > 0)
			statLayout->addWidget(statCell(QString::number(s.subtitleRemoved), tr("subtitle\nremoved"), statRow));
		if (s.videoRemoved > 0)
			statLayout->addWidget(statCell(QString::number(s.videoRemoved),    tr("MJPEG\nremoved"),    statRow));

		// Stretch pushes the savings to the far right so it reads like an equation:
		//   [remove these tracks]  ——  =  ——  [est. savings]
		statLayout->addStretch(1);

		if (s.estimatedSavedBytes > 0) {
			auto* eqLbl = new QLabel(QStringLiteral("="), statRow);
			QFont eqFont = eqLbl->font();
			eqFont.setPointSizeF(eqFont.pointSizeF() * 2.2);
			eqFont.setBold(true);
			eqLbl->setFont(eqFont);
			eqLbl->setAlignment(Qt::AlignVCenter | Qt::AlignHCenter);
			eqLbl->setContentsMargins(8, 0, 8, 0);
			QPalette eqPal = eqLbl->palette();
			eqPal.setColor(QPalette::WindowText, eqPal.color(QPalette::PlaceholderText));
			eqLbl->setPalette(eqPal);
			statLayout->addWidget(eqLbl);

			statLayout->addWidget(savingsCell(formatSize(s.estimatedSavedBytes), tr("est.\nsavings"), statRow));
		}

		layout->addWidget(statRow);
		layout->addWidget(separator(body));

		// ── Removal reasons ───────────────────────────────────────────────────
		const int maxReason = std::max({s.reasonLanguage, s.reasonRedundancy,
		                               s.reasonSdh, s.reasonCommentary,
		                               s.reasonMjpeg, s.reasonOther});
		if (maxReason > 0) {
			auto* sec = new CollapsibleSection(tr("Removal reasons"), true, body);
			layout->addWidget(sec);

			const QColor colLang ("#4a8ccc");
			const QColor colRed  ("#6aaa60");
			const QColor colSdh  ("#c08040");
			const QColor colCom  ("#9060b0");
			const QColor colMjpeg("#888888");
			const QColor colOther("#666666");

			if (s.reasonLanguage   > 0) addBarRow(sec->rowLayout(), tr("Language policy"),        s.reasonLanguage,   maxReason, colLang,  sec);
			if (s.reasonRedundancy > 0) addBarRow(sec->rowLayout(), tr("Codec redundancy"),       s.reasonRedundancy, maxReason, colRed,   sec);
			if (s.reasonSdh        > 0) addBarRow(sec->rowLayout(), tr("SDH (no regular track)"), s.reasonSdh,        maxReason, colSdh,   sec);
			if (s.reasonCommentary > 0) addBarRow(sec->rowLayout(), tr("Commentary"),              s.reasonCommentary, maxReason, colCom,   sec);
			if (s.reasonMjpeg      > 0) addBarRow(sec->rowLayout(), tr("MJPEG cover art"),         s.reasonMjpeg,      maxReason, colMjpeg, sec);
			if (s.reasonOther      > 0) addBarRow(sec->rowLayout(), tr("Other"),                   s.reasonOther,      maxReason, colOther, sec);
			layout->addWidget(separator(body));
		}

		// ── Audio by language ─────────────────────────────────────────────────
		if (!s.audioByLang.isEmpty()) {
			const int maxAL = *std::max_element(s.audioByLang.cbegin(), s.audioByLang.cend());
			QList<QPair<int,QString>> sorted;
			for (auto it = s.audioByLang.cbegin(); it != s.audioByLang.cend(); ++it)
				sorted.append({ it.value(), it.key() });
			std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b){ return a.first > b.first; });

			const bool bigList = sorted.size() > 5;
			auto* sec = new CollapsibleSection(
			    tr("Audio removed by language  (%1)").arg(sorted.size()),
			    !bigList, body);
			layout->addWidget(sec);
			for (const auto& [count, lang] : sorted)
				addBarRow(sec->rowLayout(), langDisplayName(lang), count, maxAL, QColor("#4a8ccc"), sec);
			layout->addWidget(separator(body));
		}

		// ── Subtitle by language ──────────────────────────────────────────────
		if (!s.subtitleByLang.isEmpty()) {
			const int maxSL = *std::max_element(s.subtitleByLang.cbegin(), s.subtitleByLang.cend());
			QList<QPair<int,QString>> sorted;
			for (auto it = s.subtitleByLang.cbegin(); it != s.subtitleByLang.cend(); ++it)
				sorted.append({ it.value(), it.key() });
			std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b){ return a.first > b.first; });

			const bool bigList = sorted.size() > 5;
			auto* sec = new CollapsibleSection(
			    tr("Subtitles removed by language  (%1)").arg(sorted.size()),
			    !bigList, body);
			layout->addWidget(sec);
			for (const auto& [count, lang] : sorted)
				addBarRow(sec->rowLayout(), langDisplayName(lang), count, maxSL, QColor("#c08040"), sec);
		}
	}

	layout->addStretch();

	// ── Buttons ───────────────────────────────────────────────────────────────
	auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	root->addWidget(buttons);
}

} // namespace Mc
