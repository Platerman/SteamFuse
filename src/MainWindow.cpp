#include "MainWindow.h"
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDesktopServices>
#include <QUrl>
#include <QProcess>
#include <QTimer>
#include <QStringList>
#include <QDir>
#include <QMessageBox>
#include <QFile>
#include <QTextStream>
#include <QTextEdit>

MainWindow::MainWindow(QWidget *parent)
: QMainWindow(parent), injectProcess(nullptr), pgrepProcess(nullptr),
killProcess(nullptr), pollTimer(nullptr), timeoutTimer(nullptr), waitingForProcess(false), pgrepPending(false)
{
    QWidget *central = new QWidget(this);
    setCentralWidget(central);
    setWindowTitle("Injector");
    resize(300, 150);
    QVBoxLayout *layout = new QVBoxLayout(central);

    QPushButton *injectBtn = new QPushButton("Inject", this);
    QPushButton *startInjectBtn = new QPushButton("Start and Inject", this);
    QPushButton *killBtn = new QPushButton("Kill", this);
    QPushButton *setupBtn = new QPushButton("Setup Batch", this);

    layout->addWidget(injectBtn);
    layout->addWidget(startInjectBtn);
    layout->addWidget(killBtn);
    layout->addWidget(setupBtn);

    logEdit = new QTextEdit(this);
    logEdit->setReadOnly(true);
    logEdit->setMinimumHeight(150);
    logEdit->setMaximumHeight(200);

    QHBoxLayout *logLayout = new QHBoxLayout;
    logLayout->addWidget(logEdit, 1);
    QPushButton *clearBtn = new QPushButton("Clear", this);
    clearBtn->setFixedWidth(60);
    connect(clearBtn, &QPushButton::clicked, this, &MainWindow::clearLog);
    logLayout->addWidget(clearBtn);
    layout->addLayout(logLayout);

    connect(injectBtn, &QPushButton::clicked, this, &MainWindow::onInject);
    connect(startInjectBtn, &QPushButton::clicked, this, &MainWindow::onStartAndInject);
    connect(killBtn, &QPushButton::clicked, this, &MainWindow::onKill);
    connect(setupBtn, &QPushButton::clicked, this, &MainWindow::onSetupBatch);

    injectProcess = new QProcess(this);
    pgrepProcess = new QProcess(this);
    killProcess = new QProcess(this);

    connect(injectProcess, &QProcess::readyReadStandardOutput, this, &MainWindow::appendOutput);
    connect(injectProcess, &QProcess::readyReadStandardError, this, &MainWindow::appendError);
    connect(injectProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &MainWindow::onInjectFinished);

    connect(pgrepProcess, &QProcess::finished, this, &MainWindow::onPgrepFinished);

    pollTimer = new QTimer(this);
    pollTimer->setInterval(1000);
    connect(pollTimer, &QTimer::timeout, this, &MainWindow::checkProcess);

    timeoutTimer = new QTimer(this);
    timeoutTimer->setSingleShot(true);
    timeoutTimer->setInterval(60000);
    connect(timeoutTimer, &QTimer::timeout, this, &MainWindow::onTimeout);
}

MainWindow::~MainWindow()
{
}

void MainWindow::onInject()
{
    logEdit->append("[INFO] Checking for tf_win64.exe...");
    waitingForProcess = true;
    pollTimer->start();
    timeoutTimer->start();
}

void MainWindow::onStartAndInject()
{
    QDesktopServices::openUrl(QUrl("steam://rungameid/440"));
    waitingForProcess = true;
    pollTimer->start();
    timeoutTimer->start();
}

void MainWindow::onKill()
{
    killProcessNow();
}

void MainWindow::onPgrepFinished(int exitCode, QProcess::ExitStatus status)
{
    pgrepPending = false;
    bool running = false;
    QString pid;
    if (status == QProcess::NormalExit && exitCode == 0) {
        QString output = pgrepProcess->readAllStandardOutput().trimmed();
        if (!output.isEmpty()) {
            running = true;
            pid = output;
        }
    }

    if (running && waitingForProcess) {
        pollTimer->stop();
        timeoutTimer->stop();
        waitingForProcess = false;
        logEdit->append("[INFO] Found tf_win64.exe (PID: " + pid + ")");
        logEdit->append("[INFO] Waiting 5 seconds for game to fully load...");
        QTimer::singleShot(5000, this, &MainWindow::runInject);
    }
    // If not running and still waiting, pollTimer will call checkProcess again
}

void MainWindow::onTimeout()
{
    pollTimer->stop();
    waitingForProcess = false;
    logEdit->append("[WARNING] Timeout waiting for tf_win64.exe to start.");
}

void MainWindow::checkProcess()
{
    if (pgrepPending)
        return;
    pgrepPending = true;
    pgrepProcess->start("pgrep", QStringList() << "-f" << "tf_win64.exe");
}

void MainWindow::onInjectFinished(int exitCode, QProcess::ExitStatus /*status*/)
{
    logEdit->append("=== Process finished ===");
    logEdit->append("Exit code: " + QString::number(exitCode));
    if (exitCode != 0) {
        QMessageBox::warning(this, "Injection failed",
                             "The injection process exited with code " + QString::number(exitCode) +
                             "\nCheck the log for details.");
    }
}

void MainWindow::appendOutput()
{
    QString data = injectProcess->readAllStandardOutput();
    if (!data.isEmpty()) {
        logEdit->append(data);
    }
}

void MainWindow::appendError()
{
    QString data = injectProcess->readAllStandardError();
    if (!data.isEmpty()) {
        logEdit->append("[ERROR] " + data);
    }
}

void MainWindow::clearLog()
{
    logEdit->clear();

}

void MainWindow::runInject()
{
    if (injectProcess->state() == QProcess::Running)
        return;

    // Verify tf_win64.exe is still running
    QProcess check;
    check.start("pgrep", QStringList() << "-f" << "tf_win64.exe");
    if (!check.waitForFinished(2000) || check.exitCode() != 0) {
        logEdit->append("[ERROR] tf_win64.exe is not running. Aborting injection.");
        QMessageBox::warning(this, "Process not found", "tf_win64.exe is not running. Please start the game first.");
        return;
    }

    if (!isBatchSafe()) {
        QMessageBox::StandardButton reply;
        reply = QMessageBox::warning(this, "Unsafe Batch Configuration",
                                     "The batch file contains paths that are outside the Wine prefix or do not exist.\n"
                                     "Injection may fail.\n\n"
                                     "Do you want to continue anyway?",
                                     QMessageBox::Yes | QMessageBox::No);
        if (reply != QMessageBox::Yes)
            return;
    }

    logEdit->clear();

    QString batPath = QDir::homePath() + "/.local/share/Steam/steamapps/compatdata/440/pfx/drive_c/Injection.bat";
    logEdit->append("=== Running injection ===");
    logEdit->append("Command: protontricks-launch --appid 440 \"" + batPath + "\"");

    QFile batchFile(batPath);
    if (batchFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        logEdit->append("=== Batch file content ===");
        QTextStream in(&batchFile);
        while (!in.atEnd()) {
            logEdit->append(in.readLine());
        }
        batchFile.close();
    } else {
        logEdit->append("[ERROR] Cannot open batch file for reading.");
    }

    injectProcess->setWorkingDirectory(QDir::homePath());

    QStringList args;
    args << "--appid" << "440" << batPath;
    injectProcess->start("protontricks-launch", args);
}

void MainWindow::killProcessNow()
{
    killProcess->start("pkill", QStringList() << "-9" << "-f" << "tf_win64.exe");
    // No wait – the process will terminate asynchronously
}

void MainWindow::onSetupBatch()
{
    QString batchPath = QDir::homePath() + "/.local/share/Steam/steamapps/compatdata/440/pfx/drive_c/Injection.bat";
    if (QFile::exists(batchPath)) {
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this, "Configuration Exists",
                                      "The configuration is already created, continue?",
                                      QMessageBox::Ok | QMessageBox::Cancel);
        if (reply != QMessageBox::Ok)
            return;
    }
    BatchSetupDialog dialog(this);
    dialog.exec();
}

bool MainWindow::isBatchSafe()
{
    QString batchPath = QDir::homePath() + "/.local/share/Steam/steamapps/compatdata/440/pfx/drive_c/Injection.bat";
    if (!QFile::exists(batchPath))
        return false;

    QFile file(batchPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    QTextStream in(&file);
    QString injPath, dllPath;
    while (!in.atEnd()) {
        QString line = in.readLine();
        if (line.startsWith("set INJ_PATH="))
            injPath = line.mid(13).trimmed();
        else if (line.startsWith("set DLL_PATH="))
            dllPath = line.mid(13).trimmed();
    }
    file.close();

    if (injPath.isEmpty() || dllPath.isEmpty())
        return false;

    QString prefixRoot = QDir::homePath() + "/.local/share/Steam/steamapps/compatdata/440/pfx/";
    auto toLinuxPath = [&](const QString &winPath) -> QString {
        QString path = winPath;
        path.replace('\\', '/');
        if (path.startsWith("C:")) {
            path.remove(0, 2);
            if (path.startsWith('/'))
                path = path.mid(1);
            return prefixRoot + "drive_c/" + path;
        } else if (path.startsWith("D:")) {
            path.remove(0, 2);
            if (path.startsWith('/'))
                path = path.mid(1);
            return prefixRoot + "drive_d/" + path;
        } else if (path.startsWith("Z:")) {
            path.remove(0, 2);
            if (path.startsWith('/'))
                path = path.mid(1);
            return "/" + path;
        }
        return QString();
    };

    QString linuxInj = toLinuxPath(injPath);
    QString linuxDll = toLinuxPath(dllPath);
    if (linuxInj.isEmpty() || linuxDll.isEmpty())
        return false;

    QFileInfo injInfo(linuxInj);
    QFileInfo dllInfo(linuxDll);
    bool injExists = injInfo.exists() && injInfo.isFile();
    bool dllExists = dllInfo.exists() && dllInfo.isFile();
    bool injInside = linuxInj.startsWith(prefixRoot);
    bool dllInside = linuxDll.startsWith(prefixRoot);

    return (injExists && dllExists && injInside && dllInside);
}
