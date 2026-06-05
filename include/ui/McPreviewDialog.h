#pragma once
#include <QDialog>

namespace Mc {
/** Shows before/after track list for a single file. Phase 2. */
class McPreviewDialog : public QDialog {
    Q_OBJECT
public:
    explicit McPreviewDialog(QWidget* parent = nullptr);
};
} // namespace Mc
