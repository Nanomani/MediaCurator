#pragma once
#include "core/DatabaseManager.h"
#include <QDialog>
#include <QList>
#include <QSet>

namespace Mc {

/**
 * McJobReviewDialog — shown when mkvmerge completed but reported a "track not found"
 * warning, indicating the file's track layout may have shifted since the job was
 * created. Presents the source file's current tracks (top) and the proposed output
 * tracks (bottom) using the same badge-pill style as the job/library cards, so the
 * user can visually verify that the right tracks were removed before committing.
 *
 * Result codes (via QDialog::done / QDialog::result):
 *   QDialog::Accepted (1) — commit the .tmp output as the final file
 *   2                     — discard .tmp and re-analyze from current file state
 *   QDialog::Rejected (0) — discard .tmp and leave job as failed
 */
class McJobReviewDialog : public QDialog
{
	Q_OBJECT
public:
	explicit McJobReviewDialog(
		qint64                      jobId,
		const QString&              filename,
		const QString&              warningText,
		const QString&              commandArgsJson,
		const QList<StreamRecord>&  sourceStreams,
		const QList<StreamRecord>&  outputStreams,
		QWidget*                    parent = nullptr);

	qint64 reviewJobId() const { return m_jobId; }

	void done(int result) override;

private:
	qint64 m_jobId;
};

} // namespace Mc
