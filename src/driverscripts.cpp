#include "driverscripts.h"

#include <KLocalizedString>

namespace
{
QString shellQuote(QString value)
{
    value.replace(QLatin1Char('\''), QLatin1String("'\"'\"'"));
    return QStringLiteral("'") + value + QStringLiteral("'");
}

QString echoLine(const QString &text)
{
    return QStringLiteral("echo ") + shellQuote(text);
}

QString echoExpandedLine(const QString &text)
{
    QString escaped = text;
    escaped.replace(QLatin1Char('\\'), QLatin1String("\\\\"));
    escaped.replace(QLatin1Char('"'), QLatin1String("\\\""));
    escaped.replace(QLatin1Char('`'), QLatin1String("\\`"));
    return QStringLiteral("echo \"") + escaped + QStringLiteral("\"");
}

QString dateEcho(const QString &label)
{
    return echoLine(label) + QStringLiteral(" \"$(date)\"");
}

QStringList commonHeader(const QString &logPath, const QString &locale)
{
    QStringList lines;
    lines << QStringLiteral("#!/bin/bash");
    lines << QStringLiteral("set -e");
    lines << QStringLiteral("set -o pipefail");
    lines << QStringLiteral("LOG=") + shellQuote(logPath);
    lines << QStringLiteral("export LANG=") + shellQuote(locale);
    lines << QStringLiteral("export LC_ALL=") + shellQuote(locale);
    lines << QStringLiteral("rm -f \"$LOG\" 2>/dev/null || true");
    lines << QStringLiteral(": > \"$LOG\"");
    lines << QStringLiteral("chmod 666 \"$LOG\" 2>/dev/null || true");
    lines << QStringLiteral("exec > >(tee -a \"$LOG\") 2>&1");
    lines << echoLine(QStringLiteral("=== Miryu Hardware Manager ==="));
    lines << dateEcho(i18n("Started at"));
    return lines;
}

QStringList akmodsLines()
{
    QStringList lines;
    lines << echoLine(i18n("Checking akmods support package..."));
    lines << QStringLiteral("if ! rpm -q akmods-evernight >/dev/null 2>&1; then");
    lines << QStringLiteral("  ") + echoLine(i18n("akmods-evernight is not installed, installing it now..."));
    lines << QStringLiteral("  \"$PKG_MGR\" install -y --nogpgcheck akmods-evernight");
    lines << QStringLiteral("fi");
    return lines;
}

QStringList mokLines(const QString &password)
{
    QStringList lines;
    lines << QString();
    lines << QStringLiteral("CERT_DIR=\"/etc/pki/akmods/certs\"");
    lines << QStringLiteral("PRIV_DIR=\"/etc/pki/akmods/private\"");
    lines << QStringLiteral("CERT_FILE=\"$CERT_DIR/public_key.der\"");
    lines << QStringLiteral("KEY_FILE=\"$PRIV_DIR/private_key.priv\"");
    lines << QStringLiteral("STATE_DIR=\"/var/lib/miryu-hardware-manager\"");
    lines << QStringLiteral("MOK_IMPORT_STATE=\"$STATE_DIR/mok-import.sha256\"");
    lines << QStringLiteral("PASSWORD=") + shellQuote(password);
    lines << QString();
    lines << QStringLiteral("if [ -f \"$CERT_FILE\" ]; then");
    lines << QStringLiteral("  ")
                 + echoLine(i18n("MOK certificate already exists, skipping registration."));
    lines << QStringLiteral("else");
    lines << akmodsLines();
    lines << QString();
    lines << QStringLiteral("if [ -f \"$CERT_FILE\" ] && [ -f \"$KEY_FILE\" ]; then");
    lines << QStringLiteral("  ") + echoLine(i18n("Certificate and key files already exist, skipping generation."));
    lines << QStringLiteral("else");
    lines << QStringLiteral("  mkdir -p \"$CERT_DIR\" \"$PRIV_DIR\"");
    lines << QStringLiteral("  KMODGENCA=$(command -v kmodgenca || true)");
    lines << QStringLiteral("  if [ -z \"$KMODGENCA\" ] && [ -x /usr/bin/kmodgenca ]; then");
    lines << QStringLiteral("    KMODGENCA=/usr/bin/kmodgenca");
    lines << QStringLiteral("  fi");
    lines << QStringLiteral("  if [ -z \"$KMODGENCA\" ]; then");
    lines << QStringLiteral("    ")
                 + echoLine(i18n("ERROR: kmodgenca was not found after installing akmods-evernight."));
    lines << QStringLiteral("    exit 1");
    lines << QStringLiteral("  fi");
    lines << QStringLiteral("  LOCALE_WRAPPER_DIR=$(mktemp -d /tmp/miryu-hardware-mok-locale.XXXXXX)");
    lines << QStringLiteral("  cat > \"$LOCALE_WRAPPER_DIR/locale\" <<'EOF'");
    lines << QStringLiteral("#!/bin/bash");
    lines << QStringLiteral("if [ \"$1\" = \"country_ab2\" ]; then");
    lines << QStringLiteral("  echo \"US\"");
    lines << QStringLiteral("  exit 0");
    lines << QStringLiteral("fi");
    lines << QStringLiteral("exec /usr/bin/locale \"$@\"");
    lines << QStringLiteral("EOF");
    lines << QStringLiteral("  chmod +x \"$LOCALE_WRAPPER_DIR/locale\"");
    lines << QStringLiteral("  ")
                 + echoLine(i18n("Generating new signing key and certificate using kmodgenca..."));
    lines << QStringLiteral("  set +e");
    lines << QStringLiteral("  PATH=\"$LOCALE_WRAPPER_DIR:$PATH\" \"$KMODGENCA\" -a -f");
    lines << QStringLiteral("  KMODGENCA_EXIT=$?");
    lines << QStringLiteral("  set -e");
    lines << QStringLiteral("  rm -rf \"$LOCALE_WRAPPER_DIR\"");
    lines << QStringLiteral("  if [ $KMODGENCA_EXIT -ne 0 ]; then");
    lines << QStringLiteral("    ") + echoLine(i18n("ERROR: kmodgenca failed."));
    lines << QStringLiteral("    exit 1");
    lines << QStringLiteral("  fi");
    lines << QStringLiteral("  if [ ! -f \"$CERT_FILE\" ] || [ ! -f \"$KEY_FILE\" ]; then");
    lines << QStringLiteral("    ") + echoLine(i18n("ERROR: kmodgenca did not create certificate files."));
    lines << QStringLiteral("    exit 1");
    lines << QStringLiteral("  fi");
    lines << QStringLiteral("  ") + echoLine(i18n("Certificate and key files generated successfully."));
    lines << QStringLiteral("fi");
    lines << QString();
    lines << echoLine(i18n("Checking if certificate is already enrolled..."));
    lines << QStringLiteral("set +e");
    lines << QStringLiteral("MOK_TEST_OUTPUT=$(LC_ALL=C.UTF-8 mokutil --test-key \"$CERT_FILE\" 2>&1)");
    lines << QStringLiteral("set -e");
    lines << QStringLiteral("echo \"$MOK_TEST_OUTPUT\"");
    lines << QStringLiteral("if echo \"$MOK_TEST_OUTPUT\" | grep -qi \"is already enrolled\"; then");
    lines << QStringLiteral("  ") + echoLine(i18n("Certificate is already enrolled in MOK, skipping import."));
    lines << QStringLiteral("  rm -f \"$MOK_IMPORT_STATE\" 2>/dev/null || true");
    lines << QStringLiteral("else");
    lines << QStringLiteral("  CERT_SHA256=$(sha256sum \"$CERT_FILE\" | awk '{print $1}')");
    lines << QStringLiteral("  if [ -f \"$MOK_IMPORT_STATE\" ] && grep -qx \"$CERT_SHA256\" \"$MOK_IMPORT_STATE\"; then");
    lines << QStringLiteral("    ")
                 + echoLine(i18n("MOK import request for this certificate was already submitted."));
    lines << QStringLiteral("  else");
    lines << QStringLiteral("    ") + echoLine(i18n("Certificate is not enrolled in MOK, importing it now..."));
    lines << QStringLiteral("    set +e");
    lines << QStringLiteral("    printf \"%s\\n%s\\n\" \"$PASSWORD\" \"$PASSWORD\" | LC_ALL=C.UTF-8 mokutil --import \"$CERT_FILE\" 2>&1 | tee /tmp/miryu-mok-import.log");
    lines << QStringLiteral("    MOK_EXIT=${PIPESTATUS[1]}");
    lines << QStringLiteral("    set -e");
    lines << QStringLiteral("    if [ $MOK_EXIT -eq 0 ]; then");
    lines << QStringLiteral("      mkdir -p \"$STATE_DIR\"");
    lines << QStringLiteral("      echo \"$CERT_SHA256\" > \"$MOK_IMPORT_STATE\"");
    lines << QStringLiteral("      ") + echoLine(i18n("MOK import request submitted successfully."));
    lines << QStringLiteral("    elif grep -qi -e \"already\" -e \"exists\" /tmp/miryu-mok-import.log; then");
    lines << QStringLiteral("      mkdir -p \"$STATE_DIR\"");
    lines << QStringLiteral("      echo \"$CERT_SHA256\" > \"$MOK_IMPORT_STATE\"");
    lines << QStringLiteral("      ") + echoLine(i18n("MOK import request already exists."));
    lines << QStringLiteral("    else");
    lines << QStringLiteral("      ")
                 + echoExpandedLine(i18n("MOK import failed with exit code %1. Error output:",
                                         QStringLiteral("$MOK_EXIT")));
    lines << QStringLiteral("      cat /tmp/miryu-mok-import.log");
    lines << QStringLiteral("      exit 1");
    lines << QStringLiteral("    fi");
    lines << QStringLiteral("  fi");
    lines << QStringLiteral("fi");
    lines << QStringLiteral("fi");
    return lines;
}

// Remove every installed NVIDIA driver related package.  Querying rpm first
// avoids dnf aborting the whole transaction when one glob pattern matches
// nothing, and guarantees that helper packages such as nvidia-modprobe or
// nvidia-persistenced do not survive a driver version switch.
QStringList nvidiaCleanupLines(const QString &message)
{
    QStringList lines;
    lines << echoLine(message);
    lines << QStringLiteral("OLD_NVIDIA=$(rpm -qa --queryformat '%{NAME}\\n' "
                            "| grep -i nvidia "
                            "| grep -v -e '^nvidia-gpu-firmware$' "
                            "-e '^nvidia-texture-tools$' "
                            "-e '^nvidia-vaapi-driver$' "
                            "-e '^nvidia-container' "
                            "-e '^libnvidia-container' "
                            "-e '^nvidia-driver-manager$' "
                            "|| true)");
    lines << QStringLiteral("if [ -n \"$OLD_NVIDIA\" ]; then");
    lines << QStringLiteral("  echo \"$OLD_NVIDIA\" | sed 's/^/  - /'");
    lines << QStringLiteral("  \"$PKG_MGR\" remove -y $OLD_NVIDIA 2>/dev/null || true");
    lines << QStringLiteral("else");
    lines << QStringLiteral("  ") + echoLine(i18n("No previous NVIDIA driver packages found."));
    lines << QStringLiteral("fi");
    return lines;
}

QStringList installFooter(const QString &message)
{
    QStringList lines;
    lines << dateEcho(i18n("Finished at"));
    lines << echoLine(message);
    return lines;
}
}

QString DriverScripts::nvidiaInstall(const NvidiaInstallOptions &options)
{
    QStringList lines = commonHeader(options.logPath, options.locale);
    lines << QStringLiteral("PKG_MGR=") + shellQuote(options.packageManager);
    lines << echoLine(i18n("Package manager: %1", options.packageManager));
    lines << QString();

    if (options.configureMok) {
        lines << echoLine(i18n("Starting MOK configuration..."));
        for (const QString &mokLine : mokLines(options.mokPassword)) {
            lines << mokLine;
        }
        lines << QString();
    } else {
        lines << akmodsLines();
        lines << QString();
    }

    if (options.allowerasing) {
        for (const QString &line : nvidiaCleanupLines(
                 i18n("Removing all previous NVIDIA driver packages..."))) {
            lines << line;
        }
        lines << QString();
    }

    // The akmod package is mandatory and determines success.  The CUDA
    // subpackage is optional: on some Fedora releases it is not published
    // for legacy branches, and a "No match" error must not abort the whole
    // driver installation.
    const QString eraseFlag = options.allowerasing ? QStringLiteral("--allowerasing ")
                                                   : QString();
    const QString akmodPackage = options.packages.value(0);
    const QString cudaPackage = options.packages.value(1);

    lines << echoLine(i18n("Installing NVIDIA driver packages..."));
    lines << QStringLiteral("\"$PKG_MGR\" install -y ") + eraseFlag + shellQuote(akmodPackage);
    if (!cudaPackage.isEmpty()) {
        lines << QStringLiteral("\"$PKG_MGR\" install -y ") + eraseFlag + shellQuote(cudaPackage)
                     + QStringLiteral(" || ")
                     + echoLine(i18n("Optional CUDA package \"%1\" could not be installed; "
                                     "continuing without it.",
                                     cudaPackage));
    }

    // Verify the outcome explicitly so the exit code always reflects whether
    // the driver is really installed, no matter how dnf reported its result.
    lines << QString();
    lines << echoLine(i18n("Verifying installation..."));
    lines << QStringLiteral("if ! rpm -q ") + shellQuote(akmodPackage)
                 + QStringLiteral(" >/dev/null 2>&1; then");
    lines << QStringLiteral("  ")
                 + echoLine(i18n("ERROR: package \"%1\" was not installed.", akmodPackage));
    lines << QStringLiteral("  exit 1");
    lines << QStringLiteral("fi");

    const QStringList footer = installFooter(i18n("NVIDIA driver installation completed successfully."));
    for (const QString &line : footer) {
        lines << line;
    }
    return lines.join(QLatin1Char('\n'));
}

QString DriverScripts::mokSetup(const QString &logPath, const QString &locale,
                                const QString &packageManager, const QString &password)
{
    QStringList lines = commonHeader(logPath, locale);
    lines << QStringLiteral("PKG_MGR=") + shellQuote(packageManager);
    lines << echoLine(i18n("Package manager: %1", packageManager));
    lines << echoLine(i18n("Starting MOK configuration..."));
    for (const QString &mokLine : mokLines(password)) {
        lines << mokLine;
    }
    lines << QString();

    const QStringList footer = installFooter(i18n("MOK configuration completed successfully."));
    for (const QString &line : footer) {
        lines << line;
    }
    return lines.join(QLatin1Char('\n'));
}

QString DriverScripts::packageAction(const QString &logPath, const QString &locale,
                                     const QString &packageManager, const QString &package, bool install,
                                     bool nogpgcheck, bool configureMok, const QString &mokPassword)
{
    QStringList lines = commonHeader(logPath, locale);
    lines << QStringLiteral("PKG_MGR=") + shellQuote(packageManager);
    lines << QStringLiteral("PACKAGE=") + shellQuote(package);
    lines << echoLine(i18n("Package manager: %1", packageManager));
    if (install) {
        if (configureMok) {
            lines << echoLine(i18n("Starting MOK configuration..."));
            for (const QString &mokLine : mokLines(mokPassword)) {
                lines << mokLine;
            }
            lines << QString();
        }
        lines << echoLine(i18n("Installing %1...", package));
        lines << QStringLiteral("\"$PKG_MGR\" install -y ")
                     + (nogpgcheck ? QStringLiteral("--nogpgcheck ") : QString())
                     + QStringLiteral("\"$PACKAGE\"");
        lines << echoLine(i18n("Package installation completed successfully."));
    } else {
        lines << echoLine(i18n("Removing %1...", package));
        lines << QStringLiteral("\"$PKG_MGR\" remove -y \"$PACKAGE\"");
        lines << echoLine(i18n("Package removal completed successfully."));
    }
    lines << dateEcho(i18n("Finished at"));
    return lines.join(QLatin1Char('\n'));
}

QString DriverScripts::nouveauRestore(const QString &logPath, const QString &locale,
                                      const QString &packageManager)
{
    QStringList lines = commonHeader(logPath, locale);
    lines << QStringLiteral("PKG_MGR=") + shellQuote(packageManager);
    for (const QString &line : nvidiaCleanupLines(i18n("Removing NVIDIA driver packages..."))) {
        lines << line;
    }
    lines << echoLine(i18n("NVIDIA driver packages removed."));
    lines << echoLine(i18n("The open-source Nouveau driver will be used after reboot."));
    lines << dateEcho(i18n("Finished at"));
    return lines.join(QLatin1Char('\n'));
}
