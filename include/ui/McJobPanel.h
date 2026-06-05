#pragma once
#include <QWidget>

namespace Mc {
/** Bottom panel: job queue and dry-run/run controls. Phase 3. */
class McJobPanel : public QWidget {
    Q_OBJECT
public:
    explicit McJobPanel(QWidget* parent = nullptr);
};
} // namespace Mc
