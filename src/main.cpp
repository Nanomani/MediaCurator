#include "ui/McMainWindow.h"
#include "core/DatabaseManager.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QMessageBox>
#include <QStandardPaths>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("MediaCurator");
    app.setApplicationVersion("0.1.0");
    app.setOrganizationName("MediaCurator");
    app.setOrganizationDomain("mediacurator.app");

    // Open the database
    if (!Mc::DatabaseManager::instance().open()) {
        QMessageBox::critical(nullptr, "MediaCurator",
            "Failed to open database. The application cannot start.");
        return 1;
    }

    Mc::McMainWindow window;
    window.show();

    return app.exec();
}
