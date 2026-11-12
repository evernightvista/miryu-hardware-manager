#include "driverutils.h"

#include <QFile>
#include <QLocale>
#include <QProcess>
#include <QStandardPaths>

#include <KLocalizedString>

namespace
{
QString runAndRead(const QString &program, const QStringList &arguments, int timeoutMs)
{
    QProcess process;
    process.start(program, arguments);
    if (!process.waitForFinished(timeoutMs)) {
        process.kill();
        process.waitForFinished(1000);
        return {};
    }
    return QString::fromUtf8(process.readAllStandardOutput());
}

// rpm prints one line per matching package.  When several packages (for
// example the multilib x86_64 and i686 builds) are installed, the raw output
// contains the same version more than once.  Return a single version instead
// of concatenating every match.
QString singleVersion(const QString &output)
{
    const QStringList lines = output.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        const QString version = line.trimmed();
        if (!version.isEmpty()) {
            return version;
        }
    }
    return {};
}
}

QString DriverUtils::logFilePath()
{
    return QStringLiteral("/tmp/miryu-hardware-manager.log");
}

QString DriverUtils::installLocale()
{
    QString locale;
    const QList<QString> variables = {QStringLiteral("LC_ALL"),
                                      QStringLiteral("LC_MESSAGES"),
                                      QStringLiteral("LANG")};
    for (const QString &variable : variables) {
        locale = qEnvironmentVariable(variable.toLatin1().constData());
        if (!locale.isEmpty()) {
            break;
        }
    }

    if (locale.isEmpty() || locale == QLatin1String("C") || locale == QLatin1String("POSIX")) {
        locale = QLocale::system().name();
    }

    if (locale.isEmpty() || locale == QLatin1String("C") || locale == QLatin1String("POSIX")
        || locale.startsWith(QLatin1String("en_US"))) {
        return QStringLiteral("C.UTF-8");
    }

    if (!locale.contains(QLatin1Char('.'))) {
        locale += QLatin1String(".UTF-8");
    }
    return locale;
}

QString DriverUtils::packageManager()
{
    if (!QStandardPaths::findExecutable(QStringLiteral("dnf5")).isEmpty()) {
        return QStringLiteral("dnf5");
    }
    return QStringLiteral("dnf");
}

bool DriverUtils::isHyperV()
{
    const QString detected =
        runAndRead(QStringLiteral("systemd-detect-virt"), {}, 3000).trimmed();
    if (detected == QLatin1String("microsoft")) {
        return true;
    }

    QFile vendorFile(QStringLiteral("/sys/class/dmi/id/sys_vendor"));
    QFile productFile(QStringLiteral("/sys/class/dmi/id/product_name"));
    const bool hasVendor = vendorFile.open(QIODevice::ReadOnly | QIODevice::Text);
    const bool hasProduct = productFile.open(QIODevice::ReadOnly | QIODevice::Text);
    if (!hasVendor || !hasProduct) {
        return false;
    }

    const QString vendor = QString::fromUtf8(vendorFile.readAll()).trimmed();
    const QString product = QString::fromUtf8(productFile.readAll()).trimmed();
    return vendor.contains(QLatin1String("Microsoft"), Qt::CaseInsensitive)
        && product.contains(QLatin1String("Virtual Machine"), Qt::CaseInsensitive);
}

QStringList DriverUtils::gpuNames()
{
    if (isHyperV()) {
        return {i18n("Hyper-V Graphics")};
    }

    QStringList names;
    const QString output = runAndRead(QStringLiteral("lspci"), {QStringLiteral("-nn")}, 5000);
    if (output.isEmpty()) {
        return names;
    }

    const QStringList lines = output.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        if (!line.contains(QLatin1String("VGA"), Qt::CaseInsensitive)
            && !line.contains(QLatin1String("3D controller"), Qt::CaseInsensitive)
            && !line.contains(QLatin1String("Display controller"), Qt::CaseInsensitive)) {
            continue;
        }

        const int separator = line.indexOf(QLatin1String(": "));
        const QString description = separator >= 0 ? line.mid(separator + 2).trimmed() : line.trimmed();
        // Keep one entry per physical GPU.  Identical models (for example two
        // cards of the same type) must still be counted as separate GPUs so the
        // UI can offer the list of installed graphics cards.
        if (!description.isEmpty()) {
            names.append(description);
        }
    }
    return names;
}

QString DriverUtils::installedNvidiaPackage()
{
    // Only look up the NVIDIA package names instead of scanning the whole RPM
    // database.  `rpm -qa` has to format every installed header and can block
    // for several seconds, which froze the log window right after a denied
    // polkit prompt.  Name patterns are resolved through the RPM name index
    // and return almost instantly.
    const QString output = runAndRead(QStringLiteral("rpm"),
                                      {QStringLiteral("-q"), QStringLiteral("--queryformat"),
                                       QStringLiteral("%{NAME}\n"),
                                       QStringLiteral("akmod-nvidia*"),
                                       QStringLiteral("kmod-nvidia*")},
                                      3000);
    const QStringList packages = output.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &package : packages) {
        if (package.startsWith(QLatin1String("akmod-nvidia")) && !package.contains(QLatin1String("runtime"))) {
            return package;
        }
    }
    for (const QString &package : packages) {
        if (package.startsWith(QLatin1String("kmod-nvidia"))) {
            return package;
        }
    }
    return {};
}

QString DriverUtils::installedNvidiaVersion()
{
    const QString package = installedNvidiaPackage();
    if (package.isEmpty()) {
        return {};
    }

    const QString version = singleVersion(
        runAndRead(QStringLiteral("rpm"),
                   {QStringLiteral("-q"), QStringLiteral("--queryformat"),
                    QStringLiteral("%{VERSION}\n"), package},
                   5000));
    return version.isEmpty() ? package : version;
}

QString DriverUtils::mesaDriverVersion()
{
    // %{VERSION} already excludes the release and the .fcXX distribution tag.
    return singleVersion(
        runAndRead(QStringLiteral("rpm"),
                   {QStringLiteral("-q"), QStringLiteral("--queryformat"),
                    QStringLiteral("%{VERSION}\n"), QStringLiteral("mesa-dri-drivers")},
                   5000));
}

QList<QPair<QString, QString>> DriverUtils::availableNvidiaDrivers()
{
    return {
        {i18n("NVIDIA Latest (open kernel module)"), QStringLiteral("akmod-nvidia")},
        {QStringLiteral("NVIDIA 580"), QStringLiteral("akmod-nvidia-580xx")},
        {QStringLiteral("NVIDIA 470"), QStringLiteral("akmod-nvidia-470xx")},
        {QStringLiteral("NVIDIA 390"), QStringLiteral("akmod-nvidia-390xx")},
    };
}

QString DriverUtils::cudaPackageFor(const QString &akmodPackage)
{
    if (akmodPackage == QLatin1String("akmod-nvidia")) {
        return QStringLiteral("xorg-x11-drv-nvidia-cuda");
    }

    QString version = akmodPackage;
    version.remove(QLatin1String("akmod-nvidia-"));
    version.remove(QLatin1String("kmod-nvidia-"));
    if (version.isEmpty()) {
        return QStringLiteral("xorg-x11-drv-nvidia-cuda");
    }
    return QStringLiteral("xorg-x11-drv-nvidia-") + version + QStringLiteral("-cuda");
}

bool DriverUtils::hasSecureBoot()
{
    // The Secure Boot state cannot change while the application is running, so
    // query mokutil only once.  This keeps refresh operations off the costly
    // (and potentially blocking) mokutil call path.
    static const bool secureBoot = [] {
        const QString mokutil = QStandardPaths::findExecutable(QStringLiteral("mokutil"));
        if (mokutil.isEmpty()) {
            return false;
        }
        const QString output = runAndRead(mokutil, {QStringLiteral("--sb-state")}, 3000);
        return output.contains(QLatin1String("SecureBoot enabled"), Qt::CaseInsensitive);
    }();
    return secureBoot;
}

bool DriverUtils::hasMokCertificate()
{
    const QString certificate = QStringLiteral("/etc/pki/akmods/certs/public_key.der");
    if (QFile::exists(certificate)) {
        return true;
    }

    // /etc/pki/akmods may not be traversable for regular users, so fall back to
    // a dedicated polkit helper that performs the check as root.
    const QString helper = QStringLiteral(
        "/usr/libexec/miryu-hardware-manager-mok-cert-helper");
    const QString policy = QStringLiteral(
        "/usr/share/polkit-1/actions/org.miryugaming.hardwaremanager.policy");
    if (!QFile::exists(helper) || !QFile::exists(policy)) {
        return false;
    }

    const QString output = runAndRead(QStringLiteral("pkexec"), {helper}, 5000);
    return output.contains(QStringLiteral("present"));
}

bool DriverUtils::isPackageInstalled(const QString &package)
{
    QProcess process;
    process.start(QStringLiteral("rpm"), {QStringLiteral("-q"), package});
    process.waitForFinished(3000);
    return process.exitCode() == 0 && process.exitStatus() == QProcess::NormalExit;
}

QString DriverUtils::displayServer()
{
    const QByteArray sessionType = qgetenv("XDG_SESSION_TYPE");
    if (sessionType == "wayland") {
        return QStringLiteral("Wayland");
    }
    if (sessionType == "x11") {
        return QStringLiteral("X11");
    }
    if (!qgetenv("WAYLAND_DISPLAY").isEmpty()) {
        return QStringLiteral("Wayland");
    }
    if (!qgetenv("DISPLAY").isEmpty()) {
        return QStringLiteral("X11");
    }
    return i18n("Unknown");
}
