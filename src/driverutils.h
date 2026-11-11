#ifndef DRIVERUTILS_H
#define DRIVERUTILS_H

#include <QList>
#include <QMap>
#include <QPair>
#include <QString>
#include <QStringList>

namespace DriverUtils
{
QString logFilePath();
QString installLocale();
QString packageManager();

bool isHyperV();
QStringList gpuNames();
QString installedNvidiaPackage();
QString installedNvidiaVersion();
QString mesaDriverVersion();
QList<QPair<QString, QString>> availableNvidiaDrivers();
QString cudaPackageFor(const QString &akmodPackage);

bool hasSecureBoot();
bool hasMokCertificate();
bool isPackageInstalled(const QString &package);
QString displayServer();
}

#endif // DRIVERUTILS_H
