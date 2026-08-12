#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QProcess>
#include <QTimer>
#include <QTextEdit>
#include "BatchSetupDialog.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onInject();
    void onStartAndInject();
    void onKill();
    void checkProcess();
    void onInjectFinished(int exitCode, QProcess::ExitStatus status);
    void onTimeout();
    void onSetupBatch();
    void clearLog();
    bool isBatchSafe();
    void onPgrepFinished(int exitCode, QProcess::ExitStatus status);

private:
    QProcess *injectProcess;
    QProcess *pgrepProcess;
    QProcess *killProcess;
    QTimer *pollTimer;
    QTimer *timeoutTimer;
    bool waitingForProcess;
    bool pgrepPending;
    QTextEdit *logEdit;
    void runInject();
    void killProcessNow();
    void appendOutput();
    void appendError();
};

#endif
