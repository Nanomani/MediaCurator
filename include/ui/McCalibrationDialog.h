#pragma once
#include <QDialog>

namespace Mc {

// Shows the per-codec accuracy ratios accumulated from completed jobs.
// Use "Copy as CSV" to export the raw data, or "Copy Suggested Code" to get
// ready-to-paste constexpr lines for the FallbackBps namespace in TrackDecision.h.
class McCalibrationDialog : public QDialog
{
	Q_OBJECT
public:
	explicit McCalibrationDialog(QWidget* parent = nullptr);
};

} // namespace Mc
