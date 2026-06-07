#pragma once
#include <QIcon>

namespace Mc {

// Returns a theme-aware, HiDPI-safe icon from an SVG resource.
// The fill color tracks QPalette::ButtonText for normal/disabled modes.
// Do NOT use this for brand icons (app_icon.svg) that have intentional colors.
QIcon svgIcon(const QString& resourcePath);

} // namespace Mc
