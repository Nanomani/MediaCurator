#pragma once
#include <QWidget>

namespace Mc {
/** Filter/rule builder panel. Phase 2. */
class McFilterPanel : public QWidget {
	Q_OBJECT
public:
	explicit McFilterPanel(QWidget* parent = nullptr);
signals:
	void filterChanged(const QString& filterExpression);
};
} // namespace Mc
