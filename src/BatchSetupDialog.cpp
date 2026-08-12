#include "BatchSetupDialog.h"
#include <QFormLayout>
#include <QPushButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QHBoxLayout>
#include <QRegularExpression>
#include <QFileInfo>

BatchSetupDialog::BatchSetupDialog(QWidget *parent)
: QDialog(parent)
{
    setWindowTitle("Setup Batch");
    QFormLayout *layout = new QFormLayout(this);

    injectorEdit = new QLineEdit(this);
    QPushButton *injectorBrowse = new QPushButton("Browse...", this);
    connect(injectorBrowse, &QPushButton::clicked, this, &BatchSetupDialog::browseInjector);

    dllEdit = new QLineEdit(this);
    QPushButton *dllBrowse = new QPushButton("Browse...", this);
    connect(dllBrowse, &QPushButton::clicked, this, &BatchSetupDialog::browseDll);

    injectorWinEdit = new QLineEdit(this);
    injectorWinEdit->setReadOnly(true);
    dllWinEdit = new QLineEdit(this);
    dllWinEdit->setReadOnly(true);

    layout->addRow("Injector EXE:", injectorEdit);
    layout->addRow("", injectorBrowse);
    layout->addRow("DLL:", dllEdit);
    layout->addRow("", dllBrowse);
    layout->addRow("Windows path (Injector):", injectorWinEdit);
    layout->addRow("Windows path (DLL):", dllWinEdit);

    // Update Windows paths whenever a Linux path changes
    connect(injectorEdit, &QLineEdit::textChanged, this, &BatchSetupDialog::updateWindowsPaths);
    connect(dllEdit, &QLineEdit::textChanged, this, &BatchSetupDialog::updateWindowsPaths);

    QPushButton *createBtn = new QPushButton("Create", this);
    QPushButton *cancelBtn = new QPushButton("Cancel", this);
    QHBoxLayout *btnLayout = new QHBoxLayout;
    btnLayout->addWidget(createBtn);
    btnLayout->addWidget(cancelBtn);
    layout->addRow(btnLayout);

    connect(createBtn, &QPushButton::clicked, this, &BatchSetupDialog::createBatch);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    QString home = QDir::homePath();
    batchPath = home + "/.local/share/Steam/steamapps/compatdata/440/pfx/drive_c/Injection.bat";
    prefixRoot = home + "/.local/share/Steam/steamapps/compatdata/440/pfx/";
}

void BatchSetupDialog::browseInjector()
{
    QString path = QFileDialog::getOpenFileName(this, "Select Injector EXE", QDir::homePath(), "Executables (*.exe)");
    if (!path.isEmpty()) {
        injectorEdit->setText(path);
        checkPathWarning(path, injectorEdit);
        // updateWindowsPaths will be called via textChanged signal
    }
}

void BatchSetupDialog::browseDll()
{
    QString path = QFileDialog::getOpenFileName(this, "Select DLL", QDir::homePath(), "DLL Files (*.dll)");
    if (!path.isEmpty()) {
        dllEdit->setText(path);
        checkPathWarning(path, dllEdit);
        // updateWindowsPaths will be called via textChanged signal
    }
}

void BatchSetupDialog::updateWindowsPaths()
{
    QString injPath = injectorEdit->text().trimmed();
    QString dllPath = dllEdit->text().trimmed();

    if (!injPath.isEmpty())
        injectorWinEdit->setText(convertToWindowsPath(injPath));
    else
        injectorWinEdit->clear();

    if (!dllPath.isEmpty())
        dllWinEdit->setText(convertToWindowsPath(dllPath));
    else
        dllWinEdit->clear();
}

QString BatchSetupDialog::convertToWindowsPath(const QString &linuxPath)
{
    // Get canonical absolute path (resolves symlinks)
    QFileInfo fi(linuxPath);
    QString absPath = fi.canonicalFilePath();
    if (absPath.isEmpty()) {
        absPath = fi.absoluteFilePath();  // fallback if file doesn't exist
    }

    // Use prefixRoot to map to C: or D:
    QDir prefixDir(prefixRoot);
    QString canonicalPrefix = prefixDir.canonicalPath();
    if (canonicalPrefix.isEmpty()) {
        canonicalPrefix = QDir::cleanPath(prefixRoot);
    }

    if (absPath.startsWith(canonicalPrefix + "/drive_c/")) {
        QString rel = absPath.mid(canonicalPrefix.length() + 9); // skip "/drive_c/"
        rel.replace('/', '\\');
        return "C:\\" + rel;
    }
    if (absPath.startsWith(canonicalPrefix + "/drive_d/")) {
        QString rel = absPath.mid(canonicalPrefix.length() + 9); // skip "/drive_d/"
        rel.replace('/', '\\');
        return "D:\\" + rel;
    }

    // Outside prefix: map to Z: (Linux root)
    QString path = linuxPath;
    path.replace('/', '\\');
    if (path.startsWith('\\')) {
        path = path.mid(1);
    }
    return "Z:\\" + path;
}

void BatchSetupDialog::checkPathWarning(const QString &linuxPath, QLineEdit *fieldEdit)
{
    QFileInfo fi(linuxPath);
    QString absPath = fi.canonicalFilePath();
    if (absPath.isEmpty())
        absPath = fi.absoluteFilePath();
    QDir prefixDir(prefixRoot);
    QString canonicalPrefix = prefixDir.canonicalPath();
    if (canonicalPrefix.isEmpty())
        canonicalPrefix = QDir::cleanPath(prefixRoot);

    bool insidePrefix = (absPath.startsWith(canonicalPrefix + "/drive_c/") ||
                         absPath.startsWith(canonicalPrefix + "/drive_d/"));

    if (!insidePrefix) {
        QMessageBox::StandardButton reply;
        reply = QMessageBox::warning(this, "Path outside Wine prefix",
                                     "The selected file is outside the Wine prefix.\n"
                                     "It will be mapped to the Z: drive (Linux root).\n"
                                     "For correct operation, please place your files inside the prefix\n"
                                     "e.g., " + prefixRoot + "drive_c/\n\n"
                                     "Do you want to continue anyway?",
                                     QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::No) {
            fieldEdit->clear();
            // No need to clear Windows paths; textChanged signal will update them
        }
        // If Yes, do nothing – the path stays and updateWindowsPaths will be triggered via textChanged
    }
}

void BatchSetupDialog::createBatch()
{
    QString injWin = injectorWinEdit->text().trimmed();
    QString dllWin = dllWinEdit->text().trimmed();
    if (injWin.isEmpty() || dllWin.isEmpty()) {
        QMessageBox::warning(this, "Error", "Both paths must be set.");
        return;
    }

    // Validate that they start with a drive letter
    QRegularExpression driveRe(R"(^[A-Z]:\\)");
    if (!driveRe.match(injWin).hasMatch() || !driveRe.match(dllWin).hasMatch()) {
        QMessageBox::warning(this, "Error", "Windows paths must start with a drive letter (e.g., C:\\...).");
        return;
    }

    QFile file(batchPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Error", "Cannot write batch file.");
        return;
    }

    // Write with \r\n line endings for Windows compatibility
    QTextStream out(&file);
    out.setGenerateByteOrderMark(false);
    out << "@echo off\r\n";
    out << "set INJ_PATH=" << injWin << "\r\n";
    out << "set DLL_PATH=" << dllWin << "\r\n";
    out << "set EXE_PATH=tf_win64.exe\r\n";
    // Change to the directory containing the injector
    out << "%INJ_PATH% %DLL_PATH% %EXE_PATH%\r\n";
    file.close();

    QMessageBox::information(this, "Success", "Batch file created successfully.");
    accept();
}
