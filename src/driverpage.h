#ifndef DRIVERPAGE_H
#define DRIVERPAGE_H

#include <QWidget>
#include <QString>
#include <QStringList>

#include <functional>

class QComboBox;
class QFrame;
class QLabel;
class QPushButton;
class InstallLogDialog;

class DriverPage : public QWidget
{
    Q_OBJECT

public:
    explicit DriverPage(QWidget *parent = nullptr);
    void refreshStatus();

private:
    void buildUi();
    QFrame *buildNvidiaCard();
    QFrame *buildPackageCard(const QStringList &iconNames, const QString &fallbackText,
                             const QString &fallbackColor, const QString &fallbackTextColor,
                             const QString &title, const QString &packageName,
                             QLabel **statusLabel, QPushButton **installButton,
                             QPushButton **removeButton);

    void onInstallNvidia();
    void onRevertNouveau();
    void onMokClicked();
    void registerMok();
    void onPackageAction(const QString &package, const QString &displayName, bool install);

    QString promptMokPassword();
    void showMokInstructions(const QString &password, QWidget *parent = nullptr);
    void showCancelled(const QString &logPath, QWidget *parent = nullptr);
    // The finishing callback also receives the log dialog, so that result
    // messages can be shown on top of it instead of behind it.
    void runScript(const QString &title, const QString &script,
                   const std::function<void(bool success, bool cancelled, QWidget *dialog)> &onFinished,
                   const QString &helperPath = QString());

    QLabel *m_currentDriverLabel = nullptr;
    QLabel *m_sessionLabel = nullptr;
    QComboBox *m_driverCombo = nullptr;
    QPushButton *m_installButton = nullptr;
    QPushButton *m_revertButton = nullptr;
    QPushButton *m_mokButton = nullptr;

    QLabel *m_logitechStatus = nullptr;
    QPushButton *m_logitechInstall = nullptr;
    QPushButton *m_logitechRemove = nullptr;
    QLabel *m_razerStatus = nullptr;
    QPushButton *m_razerInstall = nullptr;
    QPushButton *m_razerRemove = nullptr;
};

#endif // DRIVERPAGE_H
