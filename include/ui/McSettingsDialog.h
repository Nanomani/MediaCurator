#pragma once
#include <QDialog>

namespace Mc {
/** User profile editor and external tools config. Phase 2. */
class McSettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit McSettingsDialog(QWidget* parent = nullptr);
};
} // namespace Mc
