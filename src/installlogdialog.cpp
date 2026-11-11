#include "installlogdialog.h"

#include <QCloseEvent>
#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QStandardPaths>
#include <QTextCursor>
#include <QTemporaryDir>
#include <QTimer>
#include <QVBoxLayout>

#include <KLocalizedString>

#include <csignal>
#include <sys/types.h>
#include <unistd.h>

namespace
{
const char *const kHelperPath = "/usr/libexec/miryu-hardware-manager-helper";

// Collect every process that belongs to the session created for our child.
// The helper is started with setsid(), so the session id is the pid returned by
// QProcess and no unrelated process (for example another dnf5 started by the
// user) can match.
QList<qint64> processesInSession(qint64 sessionId)
{
    QList<qint64> result;
    const QDir proc(QStringLiteral("/proc"));
    const QStringList entries = proc.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &entry : entries) {
        bool ok = false;
        const qint64 pid = entry.toLongLong(&ok);
        if (!ok) {
            continue;
        }
        QFile file(proc.filePath(entry + QStringLiteral("/stat")));
        if (!file.open(QIODevice::ReadOnly)) {
            continue;
        }
        const QByteArray stat = file.readAll();
        const int commEnd = stat.lastIndexOf(')');
        if (commEnd < 0) {
            continue;
        }
        // Fields after the command name: state, ppid, pgrp, session, ...
        const QList<QByteArray> fields = stat.mid(commEnd + 2).split(' ');
        if (fields.size() < 4) {
            continue;
        }
        if (fields.at(3).toLongLong() == sessionId) {
            result.append(pid);
        }
    }
    return result;
}

void signalProcessTree(qint64 rootPid, int signal)
{
    if (rootPid <= 0) {
        return;
    }
    // The helper is the session and process group leader, so the negative pid
    // reaches pkexec, the shell wrapper, dnf5 and the akmod build children.
    ::kill(static_cast<pid_t>(-rootPid), signal);
    for (const qint64 pid : processesInSession(rootPid)) {
        if (pid != static_cast<qint64>(::getpid())) {
            ::kill(static_cast<pid_t>(pid), signal);
        }
    }
}
} // namespace

InstallLogDialog::InstallLogDialog(const QString &title, QWidget *parent)
    : QDialog(parent)
{
    buildUi(title);
}

InstallLogDialog::~InstallLogDialog()
{
    if (m_process && m_process->state() != QProcess::NotRunning) {
        signalProcessTree(m_process->processId(), SIGKILL);
        m_process->kill();
        m_process->waitForFinished(2000);
    }
    delete m_tempDir;
}

void InstallLogDialog::buildUi(const QString &title)
{
    setWindowTitle(title);
    resize(760, 480);
    setAttribute(Qt::WA_DeleteOnClose);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(10);

    m_statusLabel = new QLabel(i18n("Preparing..."), this);
    m_statusLabel->setWordWrap(true);
    layout->addWidget(m_statusLabel);

    m_progress = new QProgressBar(this);
    m_progress->setRange(0, 0);
    m_progress->setTextVisible(false);
    m_progress->setMaximumHeight(8);
    m_progress->hide();
    layout->addWidget(m_progress);

    m_logEdit = new QPlainTextEdit(this);
    m_logEdit->setReadOnly(true);
    m_logEdit->setLineWrapMode(QPlainTextEdit::NoWrap);
    QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    font.setPointSize(9);
    m_logEdit->setFont(font);
    layout->addWidget(m_logEdit, 1);

    auto *buttonRow = new QHBoxLayout;
    buttonRow->addStretch();
    m_cancelButton = new QPushButton(i18n("Cancel"), this);
    m_cancelButton->hide();
    buttonRow->addWidget(m_cancelButton);
    m_closeButton = new QPushButton(i18n("Close"), this);
    m_closeButton->setEnabled(false);
    buttonRow->addWidget(m_closeButton);
    layout->addLayout(buttonRow);

    connect(m_cancelButton, &QPushButton::clicked, this, &InstallLogDialog::cancel);
    connect(m_closeButton, &QPushButton::clicked, this, &QDialog::accept);
}

void InstallLogDialog::startScript(const QString &script, const QString &logPath,
                                   const QString &helperPath)
{
    m_logPath = logPath;

    m_tempDir = new QTemporaryDir(QDir::tempPath()
                                  + QStringLiteral("/miryu-hardware-manager-XXXXXX"));
    if (!m_tempDir->isValid()) {
        appendText(i18n("Error: Cannot create temporary directory.") + QLatin1Char('\n'));
        m_statusLabel->setText(i18n("Failed to start the installation."));
        m_closeButton->setEnabled(true);
        failEarly();
        return;
    }

    const QString scriptPath = m_tempDir->path() + QStringLiteral("/task.sh");
    QFile scriptFile(scriptPath);
    if (!scriptFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        appendText(i18n("Error: Cannot write temporary script.") + QLatin1Char('\n'));
        m_statusLabel->setText(i18n("Failed to start the installation."));
        m_closeButton->setEnabled(true);
        failEarly();
        return;
    }
    scriptFile.write(script.toUtf8());
    scriptFile.setPermissions(QFileDevice::ExeOwner | QFileDevice::ReadOwner);
    scriptFile.close();

    appendText(i18n("Log file: %1", m_logPath) + QLatin1Char('\n') + QLatin1Char('\n'));

    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::MergedChannels);
    m_decoder.resetState();

    // Run the privileged command in its own session/process group so that a
    // user-initiated cancellation can stop the whole tree (helper, bash and
    // the package manager), not just pkexec.
    m_process->setChildProcessModifier([] {
        ::setsid();
    });

    connect(m_process, &QProcess::readyReadStandardOutput, this, &InstallLogDialog::onReadyRead);
    connect(m_process, &QProcess::readyReadStandardError, this, &InstallLogDialog::onReadyRead);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &InstallLogDialog::onProcessFinished);

    QStringList arguments;
    const QString selectedHelper = helperPath.isEmpty() ? QString::fromLatin1(kHelperPath) : helperPath;
    if (QFile::exists(selectedHelper)) {
        arguments << selectedHelper << scriptPath;
    } else {
        arguments << QStringLiteral("bash") << scriptPath;
    }

    m_process->start(QStringLiteral("pkexec"), arguments);
    if (!m_process->waitForStarted(5000)) {
        appendText(i18n("Failed to start pkexec.") + QLatin1Char('\n'));
        m_statusLabel->setText(i18n("Failed to start the installation."));
        // waitForStarted() spins an event loop: the process may already have
        // finished and cleared m_process while we were waiting.
        if (m_process) {
            m_process->deleteLater();
            m_process = nullptr;
        }
        m_closeButton->setEnabled(true);
        failEarly();
        return;
    }

    m_running = true;
    setRunning(true);
    m_statusLabel->setText(i18n("Running..."));
}

QString InstallLogDialog::logPath() const
{
    return m_logPath;
}

bool InstallLogDialog::isRunning() const
{
    return m_running;
}

void InstallLogDialog::setRunning(bool running)
{
    m_running = running;
    m_progress->setVisible(running);
    m_cancelButton->setVisible(running);
    m_cancelButton->setEnabled(running);
    m_closeButton->setEnabled(!running);
}

void InstallLogDialog::cancel()
{
    if (!m_running || !m_process) {
        return;
    }

    m_cancelled = true;
    m_cancelButton->setEnabled(false);
    m_statusLabel->setText(i18n("Cancelling..."));
    appendText(QLatin1Char('\n')
               + i18n("Cancellation requested by the user.") + QLatin1Char('\n'));

    const qint64 pid = m_process->processId();
    if (pid <= 0) {
        m_process->kill();
        return;
    }

    // Kill only the process tree of this installation.  A plain "killall dnf5"
    // would also terminate package transactions started elsewhere.
    signalProcessTree(pid, SIGTERM);
    QTimer::singleShot(5000, this, [this, pid] {
        // The helper may already be gone while a stray dnf5 or akmod child
        // survived the polite SIGTERM.
        if ((m_process && m_process->state() != QProcess::NotRunning)
            || !processesInSession(pid).isEmpty()) {
            signalProcessTree(pid, SIGKILL);
        }
    });
}

void InstallLogDialog::failEarly()
{
    if (m_finished) {
        return;
    }
    m_finished = true;
    QTimer::singleShot(0, this, [this] {
        emit finished(false, false);
    });
}

void InstallLogDialog::onReadyRead()
{
    if (!m_process) {
        return;
    }
    const QByteArray data = m_process->readAllStandardOutput();
    if (data.isEmpty()) {
        return;
    }
    appendText(m_decoder(data));
}

void InstallLogDialog::onProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    if (m_finished) {
        return;
    }
    m_finished = true;

    const bool success = !m_cancelled && (status == QProcess::NormalExit && exitCode == 0);

    appendText(QLatin1Char('\n')
               + i18n("Process finished with exit code %1.", exitCode)
               + QLatin1Char('\n'));

    setRunning(false);
    if (m_cancelled) {
        m_statusLabel->setText(i18n("Installation cancelled by the user."));
    } else {
        m_statusLabel->setText(success ? i18n("Completed successfully.")
                                       : i18n("Failed. Please check the log."));
    }

    if (m_process) {
        m_process->deleteLater();
        m_process = nullptr;
    }
    if (m_tempDir) {
        delete m_tempDir;
        m_tempDir = nullptr;
    }

    emit finished(success, m_cancelled);
}

void InstallLogDialog::appendText(const QString &text)
{
    if (text.isEmpty()) {
        return;
    }
    QTextCursor cursor(m_logEdit->document());
    cursor.movePosition(QTextCursor::End);
    cursor.insertText(text);
    m_logEdit->setTextCursor(cursor);
    m_logEdit->ensureCursorVisible();
}

void InstallLogDialog::reject()
{
    if (m_running) {
        return;
    }
    QDialog::reject();
}

void InstallLogDialog::closeEvent(QCloseEvent *event)
{
    if (m_running) {
        event->ignore();
        return;
    }
    QDialog::closeEvent(event);
}
