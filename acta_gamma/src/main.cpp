#include <QApplication>
#include <QCoreApplication>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    // Application identity (UR #11): required so QSettings has a stable
    // home for preferences (H6: database path; P3: window state).
    QCoreApplication::setOrganizationName(QStringLiteral("boucaron"));
    QCoreApplication::setApplicationName(QStringLiteral("ACTA Gamma"));

    QApplication app(argc, argv);
    MainWindow w;
    w.show();
    return app.exec();
}
