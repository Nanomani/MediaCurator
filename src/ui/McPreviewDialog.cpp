#include "ui/McPreviewDialog.h"

#include <QColor>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QTableWidget>
#include <QVBoxLayout>

namespace Mc {

// ── Default (stub) constructor — keeps Phase2Stubs usage valid ────────────────

McPreviewDialog::McPreviewDialog(QWidget* parent)
	: QDialog(parent)
{}

// ── Full constructor ──────────────────────────────────────────────────────────

McPreviewDialog::McPreviewDialog(const FileDecision& decision, QWidget* parent)
	: QDialog(parent)
{
	setupUi(decision);
}

void McPreviewDialog::setupUi(const FileDecision& decision)
{
	setWindowTitle(tr("Preview — %1").arg(decision.file.filename));
	setMinimumSize(860, 480);
	resize(1000, 560);

	auto* root = new QVBoxLayout(this);
	root->setContentsMargins(8, 8, 8, 8);
	root->setSpacing(8);

	// ── File info bar ─────────────────────────────────────────────────────────
	const qint64 estimatedSaving = decision.estimatedSavingBytes();
	QString infoText = tr("<b>%1</b>").arg(decision.file.filename);
	if (estimatedSaving > 0) {
		const double mb = estimatedSaving / 1048576.0;
		infoText += tr("  ·  Estimated saving: <b>%1 MB</b>")
		                .arg(mb >= 1024.0
		                     ? QString::number(mb / 1024.0, 'f', 2) + " GB"
		                     : QString::number(mb, 'f', 1) + " MB");
	}
	auto* infoLabel = new QLabel(infoText, this);
	infoLabel->setTextFormat(Qt::RichText);
	root->addWidget(infoLabel);

	// ── Two-panel layout ──────────────────────────────────────────────────────
	auto* panels = new QHBoxLayout;
	panels->setSpacing(8);

	// Before
	auto* beforeGroup = new QGroupBox(tr("Before  (%1 tracks)").arg(decision.tracks.size()), this);
	auto* beforeLayout = new QVBoxLayout(beforeGroup);
	beforeLayout->setContentsMargins(4, 4, 4, 4);

	auto* beforeTable = new QTableWidget(0, 5, beforeGroup);
	beforeTable->setHorizontalHeaderLabels({
		tr("Type"), tr("Codec"), tr("Language"), tr("Bitrate"), tr("Decision / Reason")
	});
	beforeTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
	beforeTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
	beforeTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
	beforeTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
	beforeTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
	beforeTable->horizontalHeader()->resizeSection(0, 72);
	beforeTable->horizontalHeader()->resizeSection(1, 100);
	beforeTable->horizontalHeader()->resizeSection(2, 68);
	beforeTable->horizontalHeader()->resizeSection(3, 72);
	beforeTable->setSelectionBehavior(QAbstractItemView::SelectRows);
	beforeTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
	beforeTable->setAlternatingRowColors(true);
	beforeTable->setShowGrid(false);
	beforeTable->verticalHeader()->setVisible(false);
	beforeTable->verticalHeader()->setDefaultSectionSize(22);
	populateBeforeTable(beforeTable, decision);
	beforeLayout->addWidget(beforeTable);
	panels->addWidget(beforeGroup);

	// After
	const int keptCount = decision.tracks.size() - decision.removalCount();
	auto* afterGroup = new QGroupBox(tr("After  (%1 tracks)").arg(keptCount), this);
	auto* afterLayout = new QVBoxLayout(afterGroup);
	afterLayout->setContentsMargins(4, 4, 4, 4);

	auto* afterTable = new QTableWidget(0, 4, afterGroup);
	afterTable->setHorizontalHeaderLabels({
		tr("Type"), tr("Codec"), tr("Language"), tr("Bitrate")
	});
	afterTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
	afterTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
	afterTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
	afterTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
	afterTable->horizontalHeader()->resizeSection(0, 72);
	afterTable->horizontalHeader()->resizeSection(1, 100);
	afterTable->horizontalHeader()->resizeSection(2, 68);
	afterTable->setSelectionBehavior(QAbstractItemView::SelectRows);
	afterTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
	afterTable->setAlternatingRowColors(true);
	afterTable->setShowGrid(false);
	afterTable->verticalHeader()->setVisible(false);
	afterTable->verticalHeader()->setDefaultSectionSize(22);
	populateAfterTable(afterTable, decision);
	afterLayout->addWidget(afterTable);
	panels->addWidget(afterGroup);

	root->addLayout(panels, 1);

	// ── Button box ────────────────────────────────────────────────────────────
	auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	root->addWidget(buttons);
}

// ── Table population ──────────────────────────────────────────────────────────

void McPreviewDialog::populateBeforeTable(QTableWidget* table, const FileDecision& decision)
{
	table->setRowCount(0);
	for (const TrackDecision& td : decision.tracks) {
		const int row = table->rowCount();
		table->insertRow(row);

		auto* typeItem    = new QTableWidgetItem(td.stream.codecType);
		auto* codecItem   = new QTableWidgetItem(td.stream.codecName.isEmpty()
		                                         ? tr("—") : td.stream.codecName);
		auto* langItem    = new QTableWidgetItem(td.stream.language.isEmpty()
		                                         ? tr("und") : td.stream.language);
		auto* bitrateItem = new QTableWidgetItem(formatBitrate(td.stream.bitRate));

		const QString decText = decisionText(td.decision)
		    + (td.reason.isEmpty() ? QString() : tr(" — ") + td.reason);
		auto* decItem = new QTableWidgetItem(decText);
		decItem->setForeground(decisionColor(td.decision));

		bitrateItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

		// Strike-through the whole row for removed tracks
		if (td.decision == Decision::Remove) {
			for (auto* item : { typeItem, codecItem, langItem, bitrateItem }) {
				QFont f = item->font();
				f.setStrikeOut(true);
				item->setFont(f);
				item->setForeground(QColor(0x99, 0x99, 0x99));
			}
		}

		table->setItem(row, 0, typeItem);
		table->setItem(row, 1, codecItem);
		table->setItem(row, 2, langItem);
		table->setItem(row, 3, bitrateItem);
		table->setItem(row, 4, decItem);
	}
}

void McPreviewDialog::populateAfterTable(QTableWidget* table, const FileDecision& decision)
{
	table->setRowCount(0);
	for (const TrackDecision& td : decision.tracks) {
		if (td.decision == Decision::Remove) continue;

		const int row = table->rowCount();
		table->insertRow(row);

		auto* typeItem    = new QTableWidgetItem(td.stream.codecType);
		auto* codecItem   = new QTableWidgetItem(td.stream.codecName.isEmpty()
		                                         ? tr("—") : td.stream.codecName);
		auto* langItem    = new QTableWidgetItem(td.stream.language.isEmpty()
		                                         ? tr("und") : td.stream.language);
		auto* bitrateItem = new QTableWidgetItem(formatBitrate(td.stream.bitRate));
		bitrateItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

		table->setItem(row, 0, typeItem);
		table->setItem(row, 1, codecItem);
		table->setItem(row, 2, langItem);
		table->setItem(row, 3, bitrateItem);
	}
}

// ── Helpers ───────────────────────────────────────────────────────────────────

QString McPreviewDialog::formatBitrate(qint64 bps)
{
	if (bps <= 0)              return {};
	if (bps >= 1'000'000)      return QStringLiteral("%1 Mbps").arg(bps / 1'000'000.0, 0, 'f', 1);
	if (bps >= 1'000)          return QStringLiteral("%1 kbps").arg(bps / 1'000.0,     0, 'f', 0);
	return QStringLiteral("%1 bps").arg(bps);
}

QString McPreviewDialog::decisionText(Decision d)
{
	switch (d) {
	case Decision::Keep:   return tr("Keep");
	case Decision::Remove: return tr("Remove");
	case Decision::Unsure: return tr("Unsure");
	}
	return {};
}

QColor McPreviewDialog::decisionColor(Decision d)
{
	switch (d) {
	case Decision::Keep:   return { 0x1a, 0x86, 0x4a };   // green
	case Decision::Remove: return { 0xcc, 0x22, 0x22 };   // red
	case Decision::Unsure: return { 0xc0, 0x80, 0x00 };   // amber
	}
	return {};
}

} // namespace Mc
