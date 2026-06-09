#pragma once
#include <QDialog>

namespace Mc {

/**
 * McLegendDialog — Help-menu legend explaining the track badge pills:
 * codec abbreviations, channel layouts, language flags, and status icons.
 * Badges are rendered with the same code that paints the cards, so the
 * legend always matches the real look.
 */
class McLegendDialog : public QDialog
{
	Q_OBJECT
public:
	explicit McLegendDialog(QWidget* parent = nullptr);
};

} // namespace Mc
