#ifndef BASICINFOPAGE_H
#define BASICINFOPAGE_H

#include <QWidget>
#include <QStringList>

class QEvent;
class QLabel;
class QFrame;
class QToolButton;

class BasicInfoPage : public QWidget
{
    Q_OBJECT

public:
    explicit BasicInfoPage(QWidget *parent = nullptr);
    void refresh();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void buildUi();
    void positionGpuMoreButton();
    QFrame *createCard(const QString &title, const QStringList &iconNames, bool smallValue,
                       QLabel **valueLabel, QLabel **detailLabel);

    QLabel *m_osNameLabel = nullptr;
    QLabel *m_productLabel = nullptr;
    QLabel *m_cpuValue = nullptr;
    QLabel *m_cpuDetail = nullptr;
    QLabel *m_memoryValue = nullptr;
    QLabel *m_memoryDetail = nullptr;
    QLabel *m_storageValue = nullptr;
    QLabel *m_storageDetail = nullptr;
    QLabel *m_gpuValue = nullptr;
    QLabel *m_gpuDetail = nullptr;
    QFrame *m_gpuCard = nullptr;
    QToolButton *m_gpuMoreButton = nullptr;
    QStringList m_gpuNames;
};

#endif // BASICINFOPAGE_H
