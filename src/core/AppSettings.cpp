#include "core/AppSettings.h"
#include <QSettings>

namespace Mc {

AppSettings& AppSettings::instance()
{
	static AppSettings s;
	return s;
}

QVariant AppSettings::value(const QString& key, const QVariant& defaultValue) const
{
	QSettings s;
	return s.value(key, defaultValue);
}

void AppSettings::setValue(const QString& key, const QVariant& value)
{
	QSettings s;
	s.setValue(key, value);
}

} // namespace Mc
