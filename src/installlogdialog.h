#ifndef INSTALLLOGDIALOG_H
#define INSTALLLOGDIALOG_H

#include <QDialog>
#include <QProcess>
#include <QString>
#include <QStringDecoder>

class QLabel;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QTemporaryDir;

class InstallLogDialog : public QDialog
{
    Q_OBJECT

public:
    explicit InstallLogDialog(const QString &title, QWidget *parent = nullptr);
    ~InstallLogDialog() override;

    void startScript(const QString &script, const QString &logPath,
                     const QString &helperPath = QString());
    QString logPath() const;
    bool isRunning() const;

signals:
    void finished(bool success);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void buildUi(const QString &title);
    void appendText(const QString &text);
    void setRunning(bool running);
    void onReadyRead();
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);
    // Report failure for startup errors that never reach QProcess, so callers
    // always get a finished() signal and can notify the user.
    void failEarly();

    QPlainTextEdit *m_logEdit = nullptr;
    QLabel *m_statusLabel = nullptr;
    QProgressBar *m_progress = nullptr;
    QPushButton *m_closeButton = nullptr;
    QProcess *m_process = nullptr;
    QTemporaryDir *m_tempDir = nullptr;
    QString m_logPath;
    QStringDecoder m_decoder{QStringDecoder::Utf8};
    bool m_running = false;
};

#endif // INSTALLLOGDIALOG_H
