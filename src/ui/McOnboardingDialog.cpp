#include "ui/McOnboardingDialog.h"
#include "ui/McCardDelegate.h"
#include "ui/SvgIcon.h"

#include <QApplication>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QPalette>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace Mc {

namespace {

// Tinted functional icon (matches the "ButtonText/theme-aware" icons used elsewhere).
QLabel* iconLabel(QWidget* parent, const QString& resourcePath, int size)
{
	auto* lbl = new QLabel(parent);
	lbl->setPixmap(McCardDelegate::renderSvgIcon(resourcePath, parent->palette().color(QPalette::Highlight),
	                                             size, parent->devicePixelRatioF()));
	lbl->setAlignment(Qt::AlignHCenter);
	return lbl;
}

// The app icon has intentional brand colors — render it as-is, never tinted.
QLabel* appIconLabel(QWidget* parent, int size)
{
	auto* lbl = new QLabel(parent);
	lbl->setPixmap(qApp->windowIcon().pixmap(size, size));
	lbl->setAlignment(Qt::AlignHCenter);
	return lbl;
}

QLabel* sampleBadge(QWidget* parent, const QString& text, const QString& codecType,
                    const QString& flagLang = {})
{
	auto* lbl = new QLabel(parent);
	lbl->setPixmap(McCardDelegate::badgePixmap(text, codecType, parent->font(),
	                                           parent->devicePixelRatioF(), flagLang));
	return lbl;
}

// One onboarding page: icon, bold title, body text, and an optional row of extra widgets
// (e.g. sample badges) placed between the body and the bottom stretch.
QWidget* buildPage(QWidget* parent, QLabel* icon, const QString& title,
                   const QString& bodyHtml, const QList<QWidget*>& extras = {})
{
	auto* page = new QWidget(parent);
	auto* v = new QVBoxLayout(page);
	v->setContentsMargins(24, 20, 24, 8);
	v->setSpacing(12);

	v->addStretch(1);
	v->addWidget(icon);

	auto* titleLbl = new QLabel(title, page);
	QFont tf = titleLbl->font();
	tf.setPointSizeF(tf.pointSizeF() * 1.3);
	tf.setBold(true);
	titleLbl->setFont(tf);
	titleLbl->setAlignment(Qt::AlignHCenter);
	v->addWidget(titleLbl);

	auto* bodyLbl = new QLabel(bodyHtml, page);
	bodyLbl->setTextFormat(Qt::RichText);
	bodyLbl->setWordWrap(true);
	bodyLbl->setAlignment(Qt::AlignHCenter);
	v->addWidget(bodyLbl);

	if (!extras.isEmpty()) {
		auto* row = new QHBoxLayout;
		row->addStretch(1);
		for (QWidget* w : extras)
			row->addWidget(w);
		row->addStretch(1);
		v->addLayout(row);
	}

	v->addStretch(2);
	return page;
}

} // anonymous namespace

McOnboardingDialog::McOnboardingDialog(QWidget* parent)
	: QDialog(parent)
{
	setWindowTitle(tr("Welcome to MediaCurator"));
	setMinimumSize(460, 420);

	auto* root = new QVBoxLayout(this);
	root->setContentsMargins(0, 0, 0, 12);
	root->setSpacing(10);

	m_stack = new QStackedWidget(this);
	root->addWidget(m_stack, 1);

	m_stack->addWidget(buildPage(m_stack, appIconLabel(m_stack, 56),
		tr("Welcome to MediaCurator"),
		tr("MediaCurator scans your personal video library, reads every track in every file, "
		   "and finds the audio, subtitle, and codec tracks you don't need — then removes them "
		   "losslessly with mkvmerge, so picture and kept-audio quality never change."
		   "<br><br>Nothing is touched until you explicitly approve it — every scan and every "
		   "removal is preview-first.")));

	m_stack->addWidget(buildPage(m_stack, iconLabel(m_stack, QStringLiteral(":/icons/manage_search.svg"), 56),
		tr("Tell it what you care about"),
		tr("Open <b>Tools → Settings</b> to set the languages you understand (kept audio and "
		   "subtitle tracks), your preferred codec order, and how to handle commentary, SDH, "
		   "and forced subtitles. The movie's original-language audio track is always kept, "
		   "no matter what you choose."),
		{ sampleBadge(m_stack, QStringLiteral("H.265"), QStringLiteral("video")),
		  sampleBadge(m_stack, QStringLiteral("DD+  5.1"), QStringLiteral("audio"), QStringLiteral("eng")),
		  sampleBadge(m_stack, QStringLiteral("SRT"), QStringLiteral("subtitle"), QStringLiteral("eng")) }));

	m_stack->addWidget(buildPage(m_stack, iconLabel(m_stack, QStringLiteral(":/icons/playlist_add_check.svg"), 56),
		tr("Reclaim disk space"),
		tr("Use <b>File → Scan</b> to add a folder or refresh your library. Flagged tracks show "
		   "up struck-through on each card — see <b>View → Legend</b> for what every badge and "
		   "icon means. Run <b>Tools → Analyze</b> to preview the savings, then queue the job: "
		   "the Job Queue panel and status bar track how much space you've reclaimed.")));

	auto* footer = new QHBoxLayout;
	footer->setContentsMargins(16, 0, 16, 0);

	m_stepLabel = new QLabel(this);
	footer->addWidget(m_stepLabel);
	footer->addStretch(1);

	auto* skipBtn = new QPushButton(tr("Skip"), this);
	connect(skipBtn, &QPushButton::clicked, this, &QDialog::accept);
	footer->addWidget(skipBtn);

	m_btnBack = new QPushButton(tr("< Back"), this);
	connect(m_btnBack, &QPushButton::clicked, this, [this] { goToPage(m_stack->currentIndex() - 1); });
	footer->addWidget(m_btnBack);

	m_btnNext = new QPushButton(this);
	m_btnNext->setDefault(true);
	connect(m_btnNext, &QPushButton::clicked, this, [this] {
		if (m_stack->currentIndex() + 1 >= m_pageCount)
			accept();
		else
			goToPage(m_stack->currentIndex() + 1);
	});
	footer->addWidget(m_btnNext);

	root->addLayout(footer);

	m_pageCount = m_stack->count();
	goToPage(0);
}

void McOnboardingDialog::goToPage(int index)
{
	m_stack->setCurrentIndex(index);
	m_stepLabel->setText(tr("Step %1 of %2").arg(index + 1).arg(m_pageCount));
	m_btnBack->setEnabled(index > 0);
	m_btnNext->setText(index + 1 >= m_pageCount ? tr("Get Started") : tr("Next >"));
}

} // namespace Mc
