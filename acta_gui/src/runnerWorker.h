#pragma once

// In-process runner worker (M1 / UR #45): runs the acta_runner execution
// pipeline (run_execution() in acta_runner/src/run.c) on a background
// thread instead of spawning the acta_runner binary.
//
//  - The worker opens its OWN acta_db handle inside the thread
//    (SQLite connections are not shareable across threads); the GUI
//    thread keeps the app's handle and polls it — WAL makes the
//    concurrent reader work, exactly as with the spawned process.
//  - The worker emits finished() with the process-style exit code from
//    acta_runner/include/runner.h plus a human-readable failure message
//    read from the execution's last error log line (no stderr parsing).
//
// Threading contract: create the worker (parent-less), moveToThread()
// it into a QThread, connect QThread::started ->
// RunnerWorker::runInThread (queued across the thread boundary), then
// start the thread. (Reparenting to the QThread would NOT move the
// worker: a QThread object lives on its creating thread, so the
// worker would keep the GUI thread's affinity and runInThread() would
// dispatch into the GUI event loop, freezing the UI for the whole
// run.) Because the worker is parent-less, the owner must post
// deleteLater() to the worker's queue before quit() + wait().
// runInThread() is the thread's only task; quit() + wait() let it run
// to completion or cooperative cancel. A cancel (requestCancel())
// aborts the in-flight HTTP call (curl abort callback) and the runner
// pipeline transitions the row pending|running -> cancelled, so a
// stuck "running" row must never be left behind either way.

#include <QObject>
#include <QString>

class RunnerWorker : public QObject {
    Q_OBJECT
public:
    explicit RunnerWorker(int executionId, const QString &dbPath,
                          int timeoutSec = 300,
                          QObject *parent = nullptr);

    // The database file the worker opens its own handle on.
    QString dbPath() const { return m_dbPath; }
    int executionId() const { return m_executionId; }

    // Cooperative cancel of the in-flight pipeline (the "Cancel"
    // meaning of the Run button): sets the runner's process-global
    // cancel flag, safe to call from the GUI thread.
    void requestCancel();
    // Clear the cancel flag (done automatically by runInThread before
    // and after the run).
    void resetCancel();

    // Queued-invocation entry point; must run in the worker thread.
public slots:
    void runInThread();

signals:
    // Pipeline finished: exit code per runner.h (0 = success) and a
    // failure message (empty on success or when no log message
    // exists).
    void finished(int exitCode, const QString &message);

private:
    int m_executionId;
    QString m_dbPath;
    int m_timeoutSec;
};
