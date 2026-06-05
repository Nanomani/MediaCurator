#pragma once
#include <QObject>

namespace Mc {
class JobQueue : public QObject {
    Q_OBJECT
public:
    explicit JobQueue(QObject* parent = nullptr);
    void enqueue(qint64 jobId);
    void start();
    void pause();
    void cancel();
    bool isRunning() const { return m_running; }
signals:
    void jobStarted(qint64 jobId);
    void jobFinished(qint64 jobId, bool success);
    void allFinished();
private:
    QList<qint64> m_queue;
    bool m_running = false;
};
} // namespace Mc
