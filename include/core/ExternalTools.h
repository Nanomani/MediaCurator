#pragma once
#include <QObject>
#include <QString>

namespace Mc {
/**
 * ExternalTools — locates and validates ffprobe, mkvmerge, mkvpropedit.
 * Searches in: tools/<platform>/ relative to the executable, then PATH.
 */
class ExternalTools : public QObject {
    Q_OBJECT
public:
    static ExternalTools& instance();

    QString ffprobePath()     const;
    QString mkvmergePath()    const;
    QString mkvpropeditPath() const;

    bool    validateAll();
    QString ffprobeVersion()  const;
    QString mkvmergeVersion() const;

private:
    ExternalTools() = default;
    QString findTool(const QString& name) const;
    mutable QString m_ffprobePath;
    mutable QString m_mkvmergePath;
    mutable QString m_mkvpropeditPath;
};
} // namespace Mc
