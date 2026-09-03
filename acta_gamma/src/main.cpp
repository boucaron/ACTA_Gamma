#include <QApplication>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QMutex>
#include <QTextStream>
#include <QtGlobal>
#include "mainwindow.h"

namespace {
    // UR #16: persist Qt log records (including DB qWarning/qCritical calls,
    // which otherwise only reach the console) to a log file in the app dir.
    QFile *g_logFile = nullptr;
    QTextStream *g_logStream = nullptr;
    QMutex g_logMutex;
    // Original (console) handler, so output is not swallowed by ours.
    void (*g_defaultHandler)(QtMsgType, const QMessageLogContext &, const QString &) = nullptr;

    void logHandler(QtMsgType type, const QMessageLogContext &context, const QString &message)
    {
        if (g_defaultHandler)
            g_defaultHandler(type, context, message);

        QMutexLocker locker(&g_logMutex);
        if (!g_logStream || !g_logStream->device()->isWritable())
            return;

        static const char *kLevels[] = { "DBG", "INF", "WRN", "CRT", "FAT" };
        const char *level = (type >= 0 && type < 5) ? kLevels[type] : "??";

        *g_logStream
            << QDateTime::currentDateTime().toString(Qt::ISODate)
            << ' ' << level << ' ';
        if (context.file)
            *g_logStream << context.file << ':' << context.line << ' ';
        else
            *g_logStream << "main.cpp ";
        *g_logStream << message << Qt::endl;
        g_logStream->flush();
    }

    void setupMessageLogging()
    {
        const QString path =
            QDir::cleanPath(QCoreApplication::applicationDirPath()) +
            QStringLiteral("/acta_gamma.log");

        const QIODevice::OpenMode mode = QIODevice::Append | QIODevice::Text;
        QFile *file = new QFile(path);
        if (!file->open(mode)) {
            // Read-only/unwritable app dir: fall back to the user's home
            // directory so the log still has somewhere to go.
            delete file;
            const QString fallback =
                QDir::cleanPath(QDir::homePath()) + QStringLiteral("/acta_gamma.log");
            file = new QFile(fallback);
            if (!file->open(mode)) {
                delete file;
                return;
            }
        }
        g_logFile = file;
        g_logStream = new QTextStream(g_logFile);

        // Keep the original (console) handler so output is not swallowed.
        // qInstallMessageHandler returns the previously installed handler.
        g_defaultHandler = qInstallMessageHandler(nullptr);
        qInstallMessageHandler(&logHandler);
    }
}

int main(int argc, char *argv[])
{
    // Application identity (UR #11): required so QSettings has a stable
    // home for preferences (H6: database path; P3: window state).
    QCoreApplication::setOrganizationName(QStringLiteral("boucaron"));
    QCoreApplication::setApplicationName(QStringLiteral("ACTA Gamma"));

    QApplication app(argc, argv);

    // File-backed log for Qt records (UR #16).
    setupMessageLogging();

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
