#ifndef DRIVERSCRIPTS_H
#define DRIVERSCRIPTS_H

#include <QString>
#include <QStringList>

struct NvidiaInstallOptions {
    QString logPath;
    QString locale;
    QString packageManager;
    QStringList packages;
    bool allowerasing = false;
    bool configureMok = false;
    QString mokPassword;
};

namespace DriverScripts
{
QString nvidiaInstall(const NvidiaInstallOptions &options);
QString mokSetup(const QString &logPath, const QString &locale, const QString &packageManager,
                 const QString &password);
QString packageAction(const QString &logPath, const QString &locale, const QString &packageManager,
                      const QString &package, bool install, bool nogpgcheck = false,
                      bool configureMok = false, const QString &mokPassword = QString());
QString nouveauRestore(const QString &logPath, const QString &locale, const QString &packageManager);
}

#endif // DRIVERSCRIPTS_H
