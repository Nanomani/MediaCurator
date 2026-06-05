#pragma once

#include <QMainWindow>
#include <QLabel>
#include <QProgressBar>
#include <QThread>

namespace Mc {

class McTrackTableModel;
class ScanWorker;

class McMainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit McMainWindow(QWidget* parent = nullptr);
    ~McMainWindow() override;

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onScanFolder();
    void onScanProgress(int current, int total, const QString& currentFile);
    void onScanFinished(int scanned, int added, int updated, int failed);
    void onRefreshView();
    void onAbout();
    void onSettings();

private:
    void setupUi();
    void setupMenuBar();
    void setupStatusBar();
    void createScanWorker(const QString& folderPath);
    void setScanningState(bool scanning);

    McTrackTableModel* m_tableModel = nullptr;
    QLabel*            m_statusLabel = nullptr;
    QProgressBar*      m_progressBar = nullptr;
    QThread*           m_scanThread  = nullptr;
    ScanWorker*        m_scanWorker  = nullptr;
};

} // namespace Mc
