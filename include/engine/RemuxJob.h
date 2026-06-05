#pragma once
#include <QObject>
#include <QString>
#include <QStringList>

namespace Mc {
class RemuxJob : public QObject {
    Q_OBJECT
public:
    explicit RemuxJob(qint64 jobId, const QString& mkvmergePath,
                      const QStringList& args, QObject* parent = nullptr);
    void run();
signals:
    void progressLine(const QString& line);
    void finished(int exitCode, const QString& log);
private:
    qint64      m_jobId;
    QString     m_mkvmergePath;
    QStringList m_args;
};
} // namespace Mc
