#include "BatchSetupDialog.h"
#include <QFormLayout>
#include <QPushButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QRegularExpression>

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

    driveCombo = new QComboBox(this);
    driveCombo->addItems({"C:", "D:", "Z:"});
    connect(driveCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &BatchSetupDialog::updateWindowsPaths);

    QPushButton *autoDetectBtn = new QPushButton("Auto Detect", this);
    connect(autoDetectBtn, &QPushButton::clicked, this, &BatchSetupDialog::autoDetect);

    injectorWinEdit = new QLineEdit(this);
    injectorWinEdit->setReadOnly(true);
    dllWinEdit = new QLineEdit(this);
    dllWinEdit->setReadOnly(true);

    layout->addRow("Injector EXE:", injectorEdit);
    layout->addRow("", injectorBrowse);
    layout->addRow("DLL:", dllEdit);
    layout->addRow("", dllBrowse);
    layout->addRow("Drive letter (fallback):", driveCombo);
    layout->addRow("", autoDetectBtn);
    layout->addRow("Windows path (Injector):", injectorWinEdit);
    layout->addRow("Windows path (DLL):", dllWinEdit);

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
        autoDetect();
    }
}

void BatchSetupDialog::browseDll()
{
    QString path = QFileDialog::getOpenFileName(this, "Select DLL", QDir::homePath(), "DLL Files (*.dll)");
    if (!path.isEmpty()) {
        dllEdit->setText(path);
        checkPathWarning(path, dllEdit);
        autoDetect();
    }
}

void BatchSetupDialog::autoDetect()
{
    QString injPath = injectorEdit->text().trimmed();
    QString dllPath = dllEdit->text().trimmed();
    if (injPath.isEmpty() || dllPath.isEmpty()) {
        QMessageBox::warning(this, "Error", "Both files must be selected first.");
        return;
    }

    // Detect drives independently
    QString injDrive = detectDriveFromPath(injPath);
    QString dllDrive = detectDriveFromPath(dllPath);

    // If detection fails, use the combo as fallback
    if (injDrive.isEmpty())
        injDrive = driveCombo->currentText();
    if (dllDrive.isEmpty())
        dllDrive = driveCombo->currentText();

    // Update the combo to the first detected drive (for visual feedback)
    QString firstDrive = detectDriveFromPath(injPath);
    if (firstDrive.isEmpty())
        firstDrive = detectDriveFromPath(dllPath);
    if (firstDrive.isEmpty())
        firstDrive = "Z:";
    int index = driveCombo->findText(firstDrive);
    if (index >= 0)
        driveCombo->setCurrentIndex(index);

    // Convert with the selected drives
    if (!injPath.isEmpty())
        injectorWinEdit->setText(convertToWindowsPath(injPath, injDrive));
    else
        injectorWinEdit->clear();

    if (!dllPath.isEmpty())
        dllWinEdit->setText(convertToWindowsPath(dllPath, dllDrive));
    else
        dllWinEdit->clear();
}

void BatchSetupDialog::updateWindowsPaths()
{
    // This is called when driveCombo changes – we update only if the fields are not empty,
    // but we should re‑detect anyway because the combo is a fallback.
    // We'll use the combo as fallback for both.
    QString fallbackDrive = driveCombo->currentText();
    QString injPath = injectorEdit->text().trimmed();
    QString dllPath = dllEdit->text().trimmed();

    if (!injPath.isEmpty()) {
        QString drive = detectDriveFromPath(injPath);
        if (drive.isEmpty())
            drive = fallbackDrive;
        injectorWinEdit->setText(convertToWindowsPath(injPath, drive));
    } else {
        injectorWinEdit->clear();
    }

    if (!dllPath.isEmpty()) {
        QString drive = detectDriveFromPath(dllPath);
        if (drive.isEmpty())
            drive = fallbackDrive;
        dllWinEdit->setText(convertToWindowsPath(dllPath, drive));
    } else {
        dllWinEdit->clear();
    }
}

QString BatchSetupDialog::convertToWindowsPath(const QString &linuxPath, const QString &fallbackDrive)
{
    // Try to detect the drive from the path itself first
    QString detectedDrive = detectDriveFromPath(linuxPath);
    QString drive = detectedDrive.isEmpty() ? fallbackDrive : detectedDrive;

    QString path = linuxPath;
    path.replace('/', '\\');
    QString driveFolder = "drive_" + drive.left(1).toLower();
    int pos = path.lastIndexOf("\\" + driveFolder + "\\", -1, Qt::CaseInsensitive);
    if (pos >= 0) {
        path = path.mid(pos + driveFolder.length() + 2);
        return drive + "\\" + path;
    }
    // Fallback: treat the whole path as relative to the drive root (e.g., Z:\home\...)
    // But we should strip the leading slash if present to avoid double slash
    if (path.startsWith('\\'))
        path = path.mid(1);
    return drive + "\\" + path;
}

QString BatchSetupDialog::detectDriveFromPath(const QString &linuxPath)
{
    // Use a regex to find "/drive_" followed by a single letter
    QRegularExpression re(R"((/|\\\\)drive_([a-zA-Z]))");
    QRegularExpressionMatch match = re.match(linuxPath);
    if (match.hasMatch()) {
        QString driveLetter = match.captured(2);
        return driveLetter.toUpper() + ":";
    }
    return QString();
}

void BatchSetupDialog::checkPathWarning(const QString &linuxPath, QLineEdit *fieldEdit)
{
    if (!linuxPath.contains("drive_")) {
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
            if (fieldEdit == injectorEdit)
                injectorWinEdit->clear();
            else if (fieldEdit == dllEdit)
                dllWinEdit->clear();
            // Do not clear the other field.
        } else {
            // Continue with the chosen drive (Z: will be used as fallback)
            updateWindowsPaths();
        }
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
    out << "\"%INJ_PATH%\" \"%DLL_PATH%\" \"%EXE_PATH%\"\r\n";
    file.close();

    QMessageBox::information(this, "Success", "Batch file created successfully.");
    accept();
}
