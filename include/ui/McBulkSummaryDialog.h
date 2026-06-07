#pragma once
#include <QDialog>

namespace Mc {

class McBulkSummaryDialog : public QDialog
{
	Q_OBJECT
public:
	explicit McBulkSummaryDialog(QWidget* parent = nullptr);
};

} // namespace Mc
