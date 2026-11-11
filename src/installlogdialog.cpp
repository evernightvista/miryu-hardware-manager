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

namespace
{
const char *const kHelperPath = "/usr/libexec/miryu-hardware-manager-helper";
}

InstallLogDialog::InstallLogDialog(const QString &title, QWidget *parent)
    : QDialog(parent)
{
    buildUi(title);
}

InstallLogDialog::~InstallLogDialog()
{
    if (m_process && m_process->state() != QProcess::NotRunning) {
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
    m_closeButton = new QPushButton(i18n("Close"), this);
    m_closeButton->setEnabled(false);
    buttonRow->addWidget(m_closeButton);
    layout->addLayout(buttonRow);

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
        m_process->deleteLater();
        m_process = nullptr;
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
    m_closeButton->setEnabled(!running);
}

void InstallLogDialog::failEarly()
{
    QTimer::singleShot(0, this, [this] {
        emit finished(false);
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
    const bool success = (status == QProcess::NormalExit && exitCode == 0);

    appendText(QLatin1Char('\n')
               + i18n("Process finished with exit code %1.", exitCode)
               + QLatin1Char('\n'));

    setRunning(false);
    m_statusLabel->setText(success ? i18n("Completed successfully.")
                                   : i18n("Failed. Please check the log."));

    if (m_process) {
        m_process->deleteLater();
        m_process = nullptr;
    }
    if (m_tempDir) {
        delete m_tempDir;
        m_tempDir = nullptr;
    }

    emit finished(success);
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

void InstallLogDialog::closeEvent(QCloseEvent *event)
{
    if (m_running) {
        event->ignore();
        return;
    }
    QDialog::closeEvent(event);
}
