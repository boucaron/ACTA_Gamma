#include <QApplication>
#include <QCoreApplication>
#include <QFile>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    // Application identity (UR #11): required so QSettings has a stable
    // home for preferences (H6: database path; P3: window state).
    QCoreApplication::setOrganizationName(QStringLiteral("boucaron"));
    QCoreApplication::setApplicationName(QStringLiteral("ACTA Gamma"));

    QApplication app(argc, argv);

    // Light stylesheet (P2 / UR #30): accent/hover/disabled states and
    // panel section headers; color-only, no geometry rules. Loaded from
    // the Qt resource so it ships with the binary.
    QFile qss(QStringLiteral(":/assets/style.qss"));
    if (qss.open(QIODevice::ReadOnly | QIODevice::Text))
        app.setStyleSheet(QString::fromUtf8(qss.readAll()));

    MainWindow w;
    w.show();
    return app.exec();
}
