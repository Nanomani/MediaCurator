#pragma once
#include <QObject>
#include <QString>
#include <QVariant>

namespace Mc {
class AppSettings : public QObject {
	Q_OBJECT
public:
	static AppSettings& instance();
	QVariant value(const QString& key, const QVariant& defaultValue = {}) const;
	void setValue(const QString& key, const QVariant& value);
private:
	AppSettings() = default;
};
} // namespace Mc
