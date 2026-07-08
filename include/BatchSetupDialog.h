#ifndef BATCHSETUPDIALOG_H
#define BATCHSETUPDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QComboBox>

class BatchSetupDialog : public QDialog
{
    Q_OBJECT
public:
    explicit BatchSetupDialog(QWidget *parent = nullptr);

private slots:
    void browseInjector();
    void browseDll();
    void createBatch();
    void autoDetect();
    void updateWindowsPaths();

private:
    QLineEdit *injectorEdit;
    QLineEdit *dllEdit;
    QLineEdit *injectorWinEdit;
    QLineEdit *dllWinEdit;
    QComboBox *driveCombo;
    QString batchPath;
    QString prefixRoot;

    QString convertToWindowsPath(const QString &linuxPath, const QString &fallbackDrive);
    QString detectDriveFromPath(const QString &linuxPath);
    void checkPathWarning(const QString &linuxPath, QLineEdit *fieldEdit);
};

#endif
