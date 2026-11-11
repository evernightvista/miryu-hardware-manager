#ifndef SYSTEMINFO_H
#define SYSTEMINFO_H

#include <QString>
#include <QStringList>

struct SystemInfoData {
    QString osName;
    QString productName;
    QString cpuModel;
    int cpuCores = 0;
    int cpuThreads = 0;
    double memoryInstalledGb = 0.0;
    double memoryTotalGb = 0.0;
    double memoryAvailableGb = 0.0;
    QString memoryType;
    QString memorySpeed;
    double storageTotalGb = 0.0;
    double storageAvailableGb = 0.0;
    QString gpuName;
    QStringList gpuNames;
    QString gpuDriverVersion;
};

class SystemInfo
{
public:
    static SystemInfoData collect();

private:
    static QString readOsPrettyName();
    static QString readProductName();
    static void readCpuInfo(SystemInfoData &data);
    static void readMemoryInfo(SystemInfoData &data);
    static void readStorageInfo(SystemInfoData &data);
};

#endif // SYSTEMINFO_H
