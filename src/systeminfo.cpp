#include "systeminfo.h"

#include "driverutils.h"

#include <QDir>
#include <QFile>
#include <QProcess>
#include <QStandardPaths>
#include <QStorageInfo>

#include <KLocalizedString>

namespace
{
bool isUsableDmiValue(const QString &value)
{
    return !value.isEmpty() && value.compare(QLatin1String("Unknown"), Qt::CaseInsensitive) != 0
        && value.compare(QLatin1String("Other"), Qt::CaseInsensitive) != 0
        && value.compare(QLatin1String("None"), Qt::CaseInsensitive) != 0
        && !value.startsWith(QLatin1Char('<'));
}

double parseDmiCapacityGb(const QString &value)
{
    const QStringList parts = value.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (parts.size() < 2) {
        return 0.0;
    }

    bool ok = false;
    const double amount = parts.at(0).toDouble(&ok);
    if (!ok || amount <= 0.0) {
        return 0.0;
    }

    const QString unit = parts.at(1).toUpper();
    if (unit == QLatin1String("MB")) {
        return amount / 1024.0;
    }
    if (unit == QLatin1String("GB")) {
        return amount;
    }
    if (unit == QLatin1String("TB")) {
        return amount * 1024.0;
    }
    return 0.0;
}

// dmidecode needs root.  When the application runs as a normal user we ask the
// read-only polkit helper for the same information.  The result is cached
// because SMBIOS data does not change while the application is running.
QString dmiMemoryOutput()
{
    static QString cached;
    static bool loaded = false;
    if (loaded) {
        return cached;
    }
    loaded = true;

    const QString dmidecode = QStandardPaths::findExecutable(QStringLiteral("dmidecode"));
    if (!dmidecode.isEmpty()) {
        QProcess process;
        process.start(dmidecode, {QStringLiteral("-t"), QStringLiteral("memory")});
        if (process.waitForFinished(3000) && process.exitCode() == 0) {
            cached = QString::fromUtf8(process.readAllStandardOutput());
            if (!cached.isEmpty()) {
                return cached;
            }
        }
    }

    const QString helper = QStringLiteral("/usr/libexec/miryu-hardware-manager-dmi-helper");
    const QString pkexec = QStandardPaths::findExecutable(QStringLiteral("pkexec"));
    if (!pkexec.isEmpty() && QFile::exists(helper)) {
        QProcess process;
        process.start(pkexec, {helper});
        if (process.waitForFinished(5000) && process.exitCode() == 0) {
            cached = QString::fromUtf8(process.readAllStandardOutput());
        }
    }
    return cached;
}

// The kernel exposes one directory per memory block.  Unlike MemTotal, the sum
// of all blocks reports the memory the firmware handed to the machine, which is
// what KDE and other system information tools display as installed memory.
double installedMemoryFromSysfs()
{
    QFile blockSizeFile(QStringLiteral("/sys/devices/system/memory/block_size_bytes"));
    if (!blockSizeFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return 0.0;
    }

    bool ok = false;
    const qulonglong blockSize =
        QString::fromUtf8(blockSizeFile.readAll()).trimmed().toULongLong(&ok, 16);
    if (!ok || blockSize == 0) {
        return 0.0;
    }

    const QDir memoryDir(QStringLiteral("/sys/devices/system/memory"));
    const QStringList entries = memoryDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    qulonglong blocks = 0;
    for (const QString &entry : entries) {
        if (!entry.startsWith(QLatin1String("memory"))) {
            continue;
        }
        bool numberOk = false;
        entry.mid(6).toInt(&numberOk);
        if (numberOk) {
            ++blocks;
        }
    }

    if (blocks == 0) {
        return 0.0;
    }
    return static_cast<double>(blocks * blockSize) / (1024.0 * 1024.0 * 1024.0);
}
}

QString SystemInfo::readOsPrettyName()
{
    QFile file(QStringLiteral("/etc/os-release"));
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QStringList lines = QString::fromUtf8(file.readAll()).split(QLatin1Char('\n'));
        for (const QString &rawLine : lines) {
            const QString line = rawLine.trimmed();
            if (!line.startsWith(QStringLiteral("PRETTY_NAME="))) {
                continue;
            }
            QString value = line.section(QLatin1Char('='), 1).trimmed();
            if (value.size() >= 2
                && ((value.startsWith(QLatin1Char('"')) && value.endsWith(QLatin1Char('"')))
                    || (value.startsWith(QLatin1Char('\'')) && value.endsWith(QLatin1Char('\''))))) {
                value = value.mid(1, value.size() - 2);
            }
            if (!value.isEmpty()) {
                return value;
            }
        }
    }
    return i18n("Unknown");
}

QString SystemInfo::readProductName()
{
    QString product;
    const QString dmidecode = QStandardPaths::findExecutable(QStringLiteral("dmidecode"));
    if (!dmidecode.isEmpty()) {
        QProcess process;
        process.start(dmidecode, {QStringLiteral("-s"), QStringLiteral("system-product-name")});
        if (process.waitForFinished(2500) && process.exitCode() == 0) {
            product = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
        }
    }

    if (product.isEmpty() || product == QLatin1String("System Product Name")) {
        QFile sysfs(QStringLiteral("/sys/devices/virtual/dmi/id/product_name"));
        if (sysfs.open(QIODevice::ReadOnly | QIODevice::Text)) {
            product = QString::fromUtf8(sysfs.readAll()).trimmed();
        }
    }

    if (product.isEmpty() || product == QLatin1String("System Product Name")) {
        return i18n("Unknown");
    }
    return product;
}

void SystemInfo::readCpuInfo(SystemInfoData &data)
{
    QFile file(QStringLiteral("/proc/cpuinfo"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }

    int coresPerSocket = 0;
    int threads = 0;
    const QStringList lines = QString::fromUtf8(file.readAll()).split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        if (line.startsWith(QLatin1String("processor"))) {
            ++threads;
        } else if (data.cpuModel.isEmpty() && line.startsWith(QLatin1String("model name"))) {
            data.cpuModel = line.section(QLatin1Char(':'), 1).trimmed();
        } else if (coresPerSocket == 0 && line.startsWith(QLatin1String("cpu cores"))) {
            coresPerSocket = line.section(QLatin1Char(':'), 1).trimmed().toInt();
        }
    }

    data.cpuThreads = threads;
    data.cpuCores = coresPerSocket > 0 ? coresPerSocket : threads;

    if (!data.cpuModel.isEmpty()) {
        data.cpuModel.remove(QLatin1String("(R)"));
        data.cpuModel.remove(QLatin1String("(TM)"));
        data.cpuModel.remove(QLatin1String(" CPU"));
        const int at = data.cpuModel.indexOf(QLatin1String(" @"));
        if (at > 0) {
            data.cpuModel.truncate(at);
        }
        data.cpuModel = data.cpuModel.simplified();
    }
    if (data.cpuModel.isEmpty()) {
        data.cpuModel = i18n("Unknown");
    }
}

void SystemInfo::readMemoryInfo(SystemInfoData &data)
{
    QFile meminfo(QStringLiteral("/proc/meminfo"));
    if (meminfo.open(QIODevice::ReadOnly | QIODevice::Text)) {
        double totalKb = 0.0;
        double availableKb = 0.0;
        const QStringList lines = QString::fromUtf8(meminfo.readAll()).split(QLatin1Char('\n'));
        for (const QString &line : lines) {
            if (line.startsWith(QLatin1String("MemTotal:"))) {
                totalKb = line.section(QLatin1Char(':'), 1).trimmed().section(QLatin1Char(' '), 0, 0).toDouble();
            } else if (line.startsWith(QLatin1String("MemAvailable:"))) {
                availableKb = line.section(QLatin1Char(':'), 1).trimmed().section(QLatin1Char(' '), 0, 0).toDouble();
            }
        }
        data.memoryTotalGb = totalKb / 1024.0 / 1024.0;
        data.memoryAvailableGb = availableKb / 1024.0 / 1024.0;
    }

    double installedFromDmi = 0.0;
    const QString dmiOutput = dmiMemoryOutput();
    if (!dmiOutput.isEmpty()) {
        const QStringList lines = dmiOutput.split(QLatin1Char('\n'));
        for (const QString &rawLine : lines) {
            const QString line = rawLine.trimmed();
            if (line.startsWith(QLatin1String("Size:"))) {
                const QString value = line.section(QLatin1Char(':'), 1).trimmed();
                if (value.compare(QLatin1String("No Module Installed"), Qt::CaseInsensitive) != 0) {
                    installedFromDmi += parseDmiCapacityGb(value);
                }
            } else if (line.startsWith(QLatin1String("Type:"))) {
                const QString value = line.section(QLatin1Char(':'), 1).trimmed();
                if (data.memoryType.isEmpty() && isUsableDmiValue(value)) {
                    data.memoryType = value;
                }
            } else if (line.startsWith(QLatin1String("Speed:"))) {
                const QString value = line.section(QLatin1Char(':'), 1).trimmed();
                if (data.memorySpeed.isEmpty() && isUsableDmiValue(value)) {
                    data.memorySpeed = value;
                }
            } else if (line.startsWith(QLatin1String("Configured Memory Speed:"))) {
                const QString value = line.section(QLatin1Char(':'), 1).trimmed();
                if (data.memorySpeed.isEmpty() && isUsableDmiValue(value)) {
                    data.memorySpeed = value;
                }
            }
        }
    }

    const double installedFromSysfs = installedMemoryFromSysfs();
    if (installedFromDmi > 0.0) {
        data.memoryInstalledGb = installedFromDmi;
    } else if (installedFromSysfs > 0.0) {
        data.memoryInstalledGb = installedFromSysfs;
    } else {
        data.memoryInstalledGb = data.memoryTotalGb;
    }
}

void SystemInfo::readStorageInfo(SystemInfoData &data)
{
    const QStorageInfo storage = QStorageInfo::root();
    if (storage.isValid() && storage.isReady()) {
        data.storageTotalGb = storage.bytesTotal() / 1024.0 / 1024.0 / 1024.0;
        data.storageAvailableGb = storage.bytesAvailable() / 1024.0 / 1024.0 / 1024.0;
    }
}

SystemInfoData SystemInfo::collect()
{
    SystemInfoData data;
    data.osName = readOsPrettyName();
    data.productName = readProductName();
    readCpuInfo(data);
    readMemoryInfo(data);
    readStorageInfo(data);

    data.gpuNames = DriverUtils::gpuNames();
    if (data.gpuNames.isEmpty()) {
        data.gpuName = i18n("Not available");
    } else {
        data.gpuName = data.gpuNames.first();
        for (const QString &name : data.gpuNames) {
            if (name.contains(QLatin1String("NVIDIA"), Qt::CaseInsensitive)) {
                data.gpuName = name;
                break;
            }
        }
    }

    // Only report the NVIDIA driver version when an NVIDIA GPU is present.
    // Reporting it for Intel/AMD/Hyper-V graphics would be misleading.
    const bool hasNvidiaGpu = data.gpuName.contains(QLatin1String("NVIDIA"), Qt::CaseInsensitive);
    if (hasNvidiaGpu) {
        data.gpuDriverVersion = DriverUtils::installedNvidiaVersion();
        if (data.gpuDriverVersion.isEmpty()) {
            data.gpuDriverVersion = i18n("Not installed");
        }
    } else {
        // Non-NVIDIA hardware is driven by Mesa, so report the version of the
        // mesa-dri-drivers package instead of pretending the driver is missing.
        data.gpuDriverVersion = DriverUtils::mesaDriverVersion();
        if (data.gpuDriverVersion.isEmpty()) {
            data.gpuDriverVersion = i18n("Not available");
        }
    }
    if (data.memoryType.isEmpty()) {
        data.memoryType = i18n("Not available");
    }
    if (data.memorySpeed.isEmpty()) {
        data.memorySpeed = i18n("Not available");
    }

    return data;
}
