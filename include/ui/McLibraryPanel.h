#pragma once
#include <QWidget>

namespace Mc {
/** Left panel: folder tree and scan button. Phase 2. */
class McLibraryPanel : public QWidget {
	Q_OBJECT
public:
	explicit McLibraryPanel(QWidget* parent = nullptr);
signals:
	void scanRequested(const QString& path);
};
} // namespace Mc
