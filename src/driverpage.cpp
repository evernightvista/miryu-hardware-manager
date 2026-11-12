#include "driverpage.h"

#include "driverscripts.h"
#include "driverutils.h"
#include "installlogdialog.h"

#include <QComboBox>
#include <QFile>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStringList>
#include <QStyle>
#include <QVBoxLayout>

#include <KGuiItem>
#include <KLocalizedString>
#include <KMessageBox>

namespace
{
const char *const kLogitechPackage = "akmod-logitech-trueforce";
const char *const kRazerPackage = "akmod-openrazer";
const char *const kMokHelperPath = "/usr/libexec/miryu-hardware-manager-mok-helper";

QLabel *makeBadge(const QString &text, const QString &background, const QString &foreground, int fontSize)
{
    auto *label = new QLabel(text);
    label->setAlignment(Qt::AlignCenter);
    label->setFixedSize(56, 56);
    label->setStyleSheet(QStringLiteral("background-color:%1;color:%2;border-radius:12px;"
                                        "font-weight:700;font-size:%3px;")
                             .arg(background, foreground)
                             .arg(fontSize));
    return label;
}

QLabel *makeIconBadge(const QStringList &iconNames, const QString &fallbackText,
                      const QString &fallbackColor = QStringLiteral("#76b900"),
                      const QString &fallbackTextColor = QStringLiteral("#ffffff"))
{
    QIcon icon;
    for (const QString &iconName : iconNames) {
        icon = QIcon::fromTheme(iconName);
        if (!icon.isNull()) {
            break;
        }
    }

    if (icon.isNull()) {
        return makeBadge(fallbackText, fallbackColor, fallbackTextColor, 18);
    }

    auto *label = new QLabel;
    label->setAlignment(Qt::AlignCenter);
    label->setFixedSize(56, 56);
    label->setPixmap(icon.pixmap(40, 40));
    return label;
}

bool askConfirmation(QWidget *parent, QMessageBox::Icon icon, const QString &title,
                     const QString &text, bool defaultYes)
{
    KMessageBox::Options options = KMessageBox::Notify;
    if (!defaultYes) {
        options |= KMessageBox::Dangerous;
    }

    const KGuiItem yesItem(i18n("Yes"));
    const KGuiItem noItem(i18n("No"));
    const int result = icon == QMessageBox::Warning
        ? KMessageBox::warningTwoActions(parent, text, title, yesItem, noItem, QString(), options)
        : KMessageBox::questionTwoActions(parent, text, title, yesItem, noItem, QString(), options);
    return result == KMessageBox::PrimaryAction;
}

QLabel *makeMuted(const QString &text)
{
    auto *label = new QLabel(text);
    label->setProperty("muted", true);
    label->setWordWrap(true);
    return label;
}

void setInstallState(QLabel *label, bool installed)
{
    label->setProperty("state", installed ? QStringLiteral("ok") : QStringLiteral("none"));
    label->setText(installed ? i18n("Installed") : i18n("Not installed"));
    label->style()->unpolish(label);
    label->style()->polish(label);
}
}

DriverPage::DriverPage(QWidget *parent)
    : QWidget(parent)
{
    buildUi();
    refreshStatus();
}

void DriverPage::buildUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(28, 24, 28, 24);
    layout->setSpacing(16);

    layout->addWidget(buildNvidiaCard());

    auto *packageRow = new QHBoxLayout;
    packageRow->setSpacing(16);

    QFrame *logitechCard = buildPackageCard({QStringLiteral("gnome-games")},
                                            QStringLiteral("Log"),
                                            QStringLiteral("#00a3e0"),
                                            QStringLiteral("#ffffff"),
                                            i18n("Logitech TrueForce Driver"),
                                            QString::fromLatin1(kLogitechPackage),
                                            &m_logitechStatus,
                                            &m_logitechInstall,
                                            &m_logitechRemove);
    QFrame *razerCard = buildPackageCard({QStringLiteral("keyboard")},
                                         QStringLiteral("Razer"),
                                         QStringLiteral("#1e1e1e"),
                                         QStringLiteral("#44d62c"),
                                         i18n("Razer Driver"),
                                         QString::fromLatin1(kRazerPackage),
                                         &m_razerStatus,
                                         &m_razerInstall,
                                         &m_razerRemove);

    packageRow->addWidget(logitechCard, 1);
    packageRow->addWidget(razerCard, 1);
    layout->addLayout(packageRow);

    layout->addStretch();

    auto *bottomRow = new QHBoxLayout;
    bottomRow->addStretch();
    m_mokButton = new QPushButton(i18n("Register Secure Boot MOK"), this);
    m_mokButton->setIcon(QIcon::fromTheme(QStringLiteral("system-lock-screen")));
    bottomRow->addWidget(m_mokButton);
    layout->addLayout(bottomRow);

    connect(m_installButton, &QPushButton::clicked, this, &DriverPage::onInstallNvidia);
    connect(m_revertButton, &QPushButton::clicked, this, &DriverPage::onRevertNouveau);
    connect(m_mokButton, &QPushButton::clicked, this, &DriverPage::onMokClicked);

    connect(m_logitechInstall, &QPushButton::clicked, this, [this] {
        onPackageAction(QString::fromLatin1(kLogitechPackage), i18n("Logitech TrueForce driver"), true);
    });
    connect(m_logitechRemove, &QPushButton::clicked, this, [this] {
        onPackageAction(QString::fromLatin1(kLogitechPackage), i18n("Logitech TrueForce driver"), false);
    });
    connect(m_razerInstall, &QPushButton::clicked, this, [this] {
        onPackageAction(QString::fromLatin1(kRazerPackage), i18n("Razer driver"), true);
    });
    connect(m_razerRemove, &QPushButton::clicked, this, [this] {
        onPackageAction(QString::fromLatin1(kRazerPackage), i18n("Razer driver"), false);
    });
}

QFrame *DriverPage::buildNvidiaCard()
{
    auto *card = new QFrame(this);
    card->setObjectName(QStringLiteral("card"));

    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(20, 18, 20, 18);
    layout->setSpacing(14);

    auto *headerRow = new QHBoxLayout;
    headerRow->setSpacing(14);

    headerRow->addWidget(makeIconBadge({QStringLiteral("nvidia"), QStringLiteral("nvidia-settings")},
                                       QStringLiteral("NV")));

    auto *titleBox = new QVBoxLayout;
    titleBox->setSpacing(2);
    auto *title = new QLabel(i18n("NVIDIA Drivers"), card);
    title->setObjectName(QStringLiteral("sectionTitle"));
    titleBox->addWidget(title);
    m_currentDriverLabel = makeMuted(i18n("Current driver: detecting..."));
    titleBox->addWidget(m_currentDriverLabel);
    headerRow->addLayout(titleBox, 1);
    layout->addLayout(headerRow);

    auto *controls = new QHBoxLayout;
    controls->setSpacing(10);

    m_driverCombo = new QComboBox(card);
    m_driverCombo->setMinimumWidth(270);
    const QList<QPair<QString, QString>> drivers = DriverUtils::availableNvidiaDrivers();
    for (const auto &driver : drivers) {
        m_driverCombo->addItem(driver.first, driver.second);
        m_driverCombo->setItemData(m_driverCombo->count() - 1, driver.second, Qt::ToolTipRole);
    }
    controls->addWidget(m_driverCombo);
    controls->addStretch();

    m_installButton = new QPushButton(i18n("Install / Switch"), card);
    controls->addWidget(m_installButton);

    m_revertButton = new QPushButton(i18n("Revert to Nouveau"), card);
    controls->addWidget(m_revertButton);

    layout->addLayout(controls);

    m_sessionLabel = makeMuted(QString());
    layout->addWidget(m_sessionLabel);

    return card;
}

QFrame *DriverPage::buildPackageCard(const QStringList &iconNames, const QString &fallbackText,
                                     const QString &fallbackColor, const QString &fallbackTextColor,
                                     const QString &title, const QString &packageName,
                                     QLabel **statusLabel, QPushButton **installButton,
                                     QPushButton **removeButton)
{
    auto *card = new QFrame(this);
    card->setObjectName(QStringLiteral("card"));
    card->setMinimumHeight(160);

    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(18, 16, 18, 16);
    layout->setSpacing(12);

    auto *headerRow = new QHBoxLayout;
    headerRow->setSpacing(12);

    headerRow->addWidget(makeIconBadge(iconNames, fallbackText, fallbackColor, fallbackTextColor));

    auto *titleBox = new QVBoxLayout;
    titleBox->setSpacing(2);
    auto *titleLabel = new QLabel(title, card);
    titleLabel->setObjectName(QStringLiteral("sectionTitle"));
    titleBox->addWidget(titleLabel);
    titleBox->addWidget(makeMuted(packageName));
    headerRow->addLayout(titleBox, 1);

    auto *status = new QLabel(card);
    status->setProperty("state", QStringLiteral("none"));
    headerRow->addWidget(status);
    layout->addLayout(headerRow);

    layout->addStretch();

    auto *buttonRow = new QHBoxLayout;
    buttonRow->setSpacing(8);
    buttonRow->addStretch();
    auto *install = new QPushButton(i18n("Install"), card);
    auto *remove = new QPushButton(i18n("Uninstall"), card);
    buttonRow->addWidget(install);
    buttonRow->addWidget(remove);
    layout->addLayout(buttonRow);

    *statusLabel = status;
    *installButton = install;
    *removeButton = remove;
    return card;
}

void DriverPage::refreshStatus()
{
    const QString installedPackage = DriverUtils::installedNvidiaPackage();
    if (!installedPackage.isEmpty()) {
        const QSignalBlocker blocker(m_driverCombo);
        for (int index = 0; index < m_driverCombo->count(); ++index) {
            if (m_driverCombo->itemData(index).toString() == installedPackage) {
                m_driverCombo->setCurrentIndex(index);
                break;
            }
        }
    }

    const QString version = DriverUtils::installedNvidiaVersion();
    m_currentDriverLabel->setText(version.isEmpty()
                                      ? i18n("Current driver: not installed")
                                      : i18n("Current driver: %1", version));

    const QString secureBoot = DriverUtils::hasSecureBoot()
        ? i18n("Secure Boot: enabled")
        : i18n("Secure Boot: disabled");
    m_sessionLabel->setText(i18n("Display server: %1", DriverUtils::displayServer())
                            + QStringLiteral("  ·  ") + secureBoot);

    m_revertButton->setEnabled(!installedPackage.isEmpty());

    const bool logitechInstalled = DriverUtils::isPackageInstalled(QString::fromLatin1(kLogitechPackage));
    setInstallState(m_logitechStatus, logitechInstalled);
    m_logitechInstall->setEnabled(!logitechInstalled);
    m_logitechRemove->setEnabled(logitechInstalled);

    const bool razerInstalled = DriverUtils::isPackageInstalled(QString::fromLatin1(kRazerPackage));
    setInstallState(m_razerStatus, razerInstalled);
    m_razerInstall->setEnabled(!razerInstalled);
    m_razerRemove->setEnabled(razerInstalled);
}

void DriverPage::onInstallNvidia()
{
    const QString selectedPackage = m_driverCombo->currentData().toString();
    if (selectedPackage.isEmpty()) {
        return;
    }

    const QString installedPackage = DriverUtils::installedNvidiaPackage();
    if (installedPackage == selectedPackage) {
        KMessageBox::information(this,
                             i18n("The driver package \"%1\" is already installed.\n"
                                  "No further action is required.",
                                  selectedPackage),
                             i18n("Already Installed"));
        return;
    }

    bool allowerasing = false;
    if (!installedPackage.isEmpty()) {
        const bool replace = askConfirmation(
            this, QMessageBox::Warning, i18n("Different Driver Installed"),
            i18n("The package \"%1\" is currently installed.\n"
                 "Installing \"%2\" will replace it.\n\nContinue?",
                 installedPackage, selectedPackage),
            false);
        if (!replace) {
            return;
        }
        allowerasing = true;
    }

    bool configureMok = false;
    QString mokPassword;
    if (DriverUtils::hasSecureBoot() && !DriverUtils::hasMokCertificate()) {
        const bool setupMok = askConfirmation(
            this, QMessageBox::Question, i18n("Secure Boot Detected"),
            i18n("Secure Boot is enabled. The NVIDIA kernel module must be signed.\n"
                 "Configure a Machine Owner Key (MOK) now?"),
            true);
        if (setupMok) {
            mokPassword = promptMokPassword();
            configureMok = !mokPassword.isEmpty();
        }
    }

    const QStringList packages = {selectedPackage,
                                  DriverUtils::cudaPackageFor(selectedPackage)};

    NvidiaInstallOptions options;
    options.logPath = DriverUtils::logFilePath();
    options.locale = DriverUtils::installLocale();
    options.packageManager = DriverUtils::packageManager();
    options.packages = packages;
    options.allowerasing = allowerasing;
    options.configureMok = configureMok;
    options.mokPassword = mokPassword;

    const QString logPath = DriverUtils::logFilePath();
    runScript(i18n("NVIDIA Driver Installation Log"), DriverScripts::nvidiaInstall(options),
              [this, configureMok, mokPassword, logPath](bool success, bool cancelled, QWidget *dialog) {
                  if (cancelled) {
                      showCancelled(logPath, dialog);
                      refreshStatus();
                      return;
                  }
                  if (!success) {
                      KMessageBox::error(dialog,
                               i18n("The installation did not complete. "
                                    "Please check the log:\n%1",
                                    logPath),
                               i18n("Installation Failed"));
                      refreshStatus();
                      return;
                  }
                  if (configureMok) {
                      showMokInstructions(mokPassword, dialog);
                  }
                  QString message = i18n("The NVIDIA driver was installed successfully. "
                                         "Open System Monitor and wait for the cc1 processes "
                                         "and the subsequent dnf5 process to finish, "
                                         "then reboot to start using it.\n\n"
                                         "Log saved to: %1",
                                         logPath);
                  if (!QFile::exists(QStringLiteral("/usr/bin/nvidia-smi"))) {
                      message += QLatin1String("\n\n")
                                 + i18n("nvidia-smi is not installed. The CUDA package for "
                                        "this driver branch is not available, so the nvidia-smi "
                                        "monitoring tool is missing.");
                  }
                  KMessageBox::information(dialog, message, i18n("Installation Complete"));
                  refreshStatus();
              });
}

void DriverPage::onRevertNouveau()
{
    const bool revert = askConfirmation(
        this, QMessageBox::Question, i18n("Revert to Nouveau"),
        i18n("This will remove the NVIDIA driver packages and switch back to the "
             "Nouveau driver.\n\nContinue?"),
        false);
    if (!revert) {
        return;
    }

    const QString logPath = DriverUtils::logFilePath();
    runScript(i18n("Nouveau Restore Log"),
              DriverScripts::nouveauRestore(logPath, DriverUtils::installLocale(),
                                            DriverUtils::packageManager()),
              [this, logPath](bool success, bool cancelled, QWidget *dialog) {
                  if (cancelled) {
                      showCancelled(logPath, dialog);
                  } else if (success) {
                      KMessageBox::information(dialog,
                                           i18n("The NVIDIA driver packages were removed. "
                                                "Reboot to use the Nouveau driver."),
                                           i18n("Uninstall Complete"));
                  } else {
                      KMessageBox::error(dialog,
                               i18n("The operation did not complete. "
                                    "Please check the log:\n%1",
                                    logPath),
                               i18n("Operation Failed"));
                  }
                  refreshStatus();
              });
}

void DriverPage::onMokClicked()
{
    if (!DriverUtils::hasSecureBoot()) {
        KMessageBox::error(this,
                       i18n("Secure Boot on your computer firmware is disabled "
                            "or the firmware does not support Secure Boot."),
                       i18n("Secure Boot MOK"));
        return;
    }

    if (DriverUtils::hasMokCertificate()) {
        KMessageBox::information(this,
                             i18n("You have already registered the Secure Boot MOK."),
                             i18n("Secure Boot MOK"));
        return;
    }

    const QString supportPackage = QStringLiteral("akmods-evernight");
    if (DriverUtils::isPackageInstalled(supportPackage)) {
        registerMok();
        return;
    }

    const bool installSupport = askConfirmation(
        this, QMessageBox::Question, i18n("Install akmods-evernight"),
        i18n("akmods-evernight is required to generate and enroll the Secure Boot MOK.\n"
             "It is not installed yet. Install it first and then continue with MOK registration?"),
        true);
    if (!installSupport) {
        return;
    }

    const QString logPath = DriverUtils::logFilePath();
    runScript(i18n("%1 - Installation Log", supportPackage),
              DriverScripts::packageAction(logPath, DriverUtils::installLocale(),
                                            DriverUtils::packageManager(), supportPackage, true, true),
              [this, logPath](bool success, bool cancelled, QWidget *dialog) {
                  if (cancelled) {
                      showCancelled(logPath, dialog);
                      return;
                  }
                  if (!success) {
                      KMessageBox::error(dialog,
                               i18n("The operation did not complete. "
                                    "Please check the log:\n%1",
                                    logPath),
                               i18n("Operation Failed"));
                      return;
                  }
                  registerMok();
              });
}

void DriverPage::registerMok()
{
    const QString password = promptMokPassword();
    if (password.isEmpty()) {
        return;
    }

    const QString logPath = DriverUtils::logFilePath();
    runScript(i18n("MOK Configuration Log"),
              DriverScripts::mokSetup(logPath, DriverUtils::installLocale(),
                                      DriverUtils::packageManager(), password),
              [this, password, logPath](bool success, bool cancelled, QWidget *dialog) {
                  if (cancelled) {
                      showCancelled(logPath, dialog);
                  } else if (success) {
                      showMokInstructions(password, dialog);
                  } else {
                      KMessageBox::error(dialog,
                               i18n("The operation did not complete. "
                                    "Please check the log:\n%1",
                                    logPath),
                               i18n("Operation Failed"));
                  }
              },
              QString::fromLatin1(kMokHelperPath));
}

void DriverPage::onPackageAction(const QString &package, const QString &displayName, bool install)
{
    bool configureMok = false;
    QString mokPassword;
    if (install && DriverUtils::hasSecureBoot() && !DriverUtils::hasMokCertificate()) {
        const bool setupMok = askConfirmation(
            this, QMessageBox::Question, i18n("Secure Boot Detected"),
            i18n("Secure Boot is enabled. The %1 kernel module must be signed.\n"
                 "Configure a Machine Owner Key (MOK) now?", displayName),
            true);
        if (setupMok) {
            mokPassword = promptMokPassword();
            configureMok = !mokPassword.isEmpty();
        }
    }

    if (!install) {
        const bool remove = askConfirmation(
            this, QMessageBox::Question, i18n("Uninstall Driver"),
            i18n("Remove the %1 from this system?", displayName),
            false);
        if (!remove) {
            return;
        }
    }

    const QString logPath = DriverUtils::logFilePath();
    const QString title = install ? i18n("%1 - Installation Log", displayName)
                                  : i18n("%1 - Removal Log", displayName);

    runScript(title,
              DriverScripts::packageAction(logPath, DriverUtils::installLocale(),
                                            DriverUtils::packageManager(), package, install, false,
                                            configureMok, mokPassword),
              [this, logPath, configureMok, mokPassword, install, displayName](bool success, bool cancelled,
                                                                              QWidget *dialog) {
                  if (cancelled) {
                      showCancelled(logPath, dialog);
                      refreshStatus();
                      return;
                  }
                  if (!success) {
                      KMessageBox::error(dialog,
                               i18n("The operation did not complete. "
                                    "Please check the log:\n%1",
                                    logPath),
                               i18n("Operation Failed"));
                      refreshStatus();
                      return;
                  }
                  if (configureMok) {
                      showMokInstructions(mokPassword, dialog);
                  }
                  if (install) {
                      KMessageBox::information(dialog,
                                           i18n("%1 was installed successfully. "
                                                "Open System Monitor and wait for the cc1 processes "
                                                "and the subsequent dnf5 process to finish, "
                                                "then reboot to start using it.",
                                                displayName),
                                           i18n("Installation Complete"));
                  } else {
                      KMessageBox::information(dialog,
                                           i18n("%1 was removed successfully.", displayName),
                                           i18n("Uninstall Complete"));
                  }
                  refreshStatus();
              });
}

QString DriverPage::promptMokPassword()
{
    bool ok = false;
    const QString password = QInputDialog::getText(
        this, i18n("Set MOK Password"),
        i18n("Enter a password for the Machine Owner Key (MOK).\n"
             "You will need to enter it after reboot:"),
        QLineEdit::Password, QString(), &ok);
    if (!ok || password.isEmpty()) {
        return {};
    }

    const QString confirmation = QInputDialog::getText(
        this, i18n("Confirm MOK Password"),
        i18n("Re-enter the password to confirm:"),
        QLineEdit::Password, QString(), &ok);
    if (!ok || password != confirmation) {
        KMessageBox::error(this,
                       i18n("The passwords do not match. "
                            "MOK configuration was skipped."),
                       i18n("Password Mismatch"));
        return {};
    }
    return password;
}

void DriverPage::showMokInstructions(const QString &password, QWidget *parent)
{
    KMessageBox::information(
        parent ? parent : this,
        i18n("After rebooting, the MOK management screen will appear.\n"
             "Choose \"Enroll MOK\" and enter this password:\n\n%1",
             password),
        i18n("MOK Configuration Complete"));
}

void DriverPage::showCancelled(const QString &logPath, QWidget *parent)
{
    KMessageBox::information(parent ? parent : this,
                             i18n("The operation was cancelled before it completed. "
                                  "Changes already made may be partially applied. "
                                  "Please check the log:\n%1",
                                  logPath),
                             i18n("Operation Cancelled"));
}

void DriverPage::runScript(const QString &title, const QString &script,
                           const std::function<void(bool success, bool cancelled, QWidget *dialog)> &onFinished,
                           const QString &helperPath)
{
    auto *dialog = new InstallLogDialog(title, this);
    connect(dialog, &InstallLogDialog::finished, this,
            [onFinished, dialog](bool success, bool cancelled) {
                // The dialog is passed on as parent so that result messages are
                // shown above the log window instead of behind it.
                onFinished(success, cancelled, dialog);
            });
    dialog->show();
    dialog->startScript(script, DriverUtils::logFilePath(), helperPath);
}
