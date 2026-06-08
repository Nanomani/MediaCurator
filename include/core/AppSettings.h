#pragma once
#include <QObject>
#include <QString>
#include <QVariant>
#include <QJsonObject>

namespace Mc {

/**
 * AppSettings — JSON-backed key/value store for application settings.
 *
 * Owns settings.json in AppDataLocation. The file has two top-level sections:
 *   "profile"  — managed by UserProfile (curation policy)
 *   "app"      — everything else (folders, UI flags, panel state)
 *
 * Geometry/splitter state (opaque QByteArray blobs) stay in QSettings (INI).
 *
 * Call load() once at startup before constructing any UI. setValue() persists
 * immediately; save() is called internally.
 */
class AppSettings : public QObject {
	Q_OBJECT
public:
	static AppSettings& instance();

	static QString filePath();

	void load();
	void save();

	// "app" section ────────────────────────────────────────────────────────────
	QVariant value(const QString& key, const QVariant& defaultValue = {}) const;
	void     setValue(const QString& key, const QVariant& value);

	// "profile" section — used by UserProfile ─────────────────────────────────
	QJsonObject profileSection() const;
	void        setProfileSection(const QJsonObject& obj);

	// Raw root — used during migration from old flat-file format
	QJsonObject rawRoot() const { return m_root; }

private:
	AppSettings() = default;
	QJsonObject m_root;
};

} // namespace Mc
