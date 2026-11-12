#include "basicinfopage.h"

#include "systeminfo.h"

#include <QEvent>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLocale>
#include <QMenu>
#include <QToolButton>
#include <QVBoxLayout>

#include <KLocalizedString>

static QString formatGb(double value)
{
    return QLocale().toString(value, 'f', 1);
}

static QIcon findThemedIcon(const QStringList &names)
{
    for (const QString &name : names) {
        const QIcon icon = QIcon::fromTheme(name);
        if (!icon.isNull()) {
            return icon;
        }
    }
    return {};
}

BasicInfoPage::BasicInfoPage(QWidget *parent)
    : QWidget(parent)
{
    buildUi();
    refresh();
}

void BasicInfoPage::buildUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(28, 24, 28, 24);
    layout->setSpacing(20);

    auto *headerRow = new QHBoxLayout;
    headerRow->setSpacing(18);

    auto *systemIcon = new QLabel(this);
    const QIcon icon = findThemedIcon({QStringLiteral("computer"), QStringLiteral("preferences-system")});
    if (!icon.isNull()) {
        systemIcon->setPixmap(icon.pixmap(64, 64));
    }
    systemIcon->setAlignment(Qt::AlignTop);
    headerRow->addWidget(systemIcon);

    auto *titleBox = new QVBoxLayout;
    titleBox->setSpacing(4);

    m_osNameLabel = new QLabel(this);
    m_osNameLabel->setObjectName(QStringLiteral("osName"));
    m_osNameLabel->setWordWrap(true);
    titleBox->addWidget(m_osNameLabel);

    m_productLabel = new QLabel(this);
    m_productLabel->setProperty("muted", true);
    m_productLabel->setWordWrap(true);
    titleBox->addWidget(m_productLabel);

    headerRow->addLayout(titleBox, 1);
    layout->addLayout(headerRow);

    auto *grid = new QGridLayout;
    grid->setSpacing(16);

    QFrame *cpuCard = createCard(i18n("Processor"),
                                 {QStringLiteral("cpu"), QStringLiteral("application-x-executable")},
                                 false, &m_cpuValue, &m_cpuDetail);
    QFrame *memoryCard = createCard(i18n("Memory"),
                                    {QStringLiteral("memory"), QStringLiteral("media-flash")},
                                    false, &m_memoryValue, &m_memoryDetail);
    QFrame *storageCard = createCard(i18n("Storage"),
                                     {QStringLiteral("drive-harddisk"), QStringLiteral("drive-harddisk-solidstate")},
                                     false, &m_storageValue, &m_storageDetail);
    QFrame *gpuCard = createCard(i18n("Graphics Card"),
                                 {QStringLiteral("video-display"), QStringLiteral("computer")},
                                 true, &m_gpuValue, &m_gpuDetail);

    // The button is an overlay pinned to the bottom-right corner of the card.
    // Keeping it out of the vertical layout guarantees it stays visible even
    // when a long GPU name fills the card.
    m_gpuCard = gpuCard;
    m_gpuDetail->setContentsMargins(0, 0, 28, 0);
    m_gpuMoreButton = new QToolButton(gpuCard);
    m_gpuMoreButton->setText(QStringLiteral("…"));
    m_gpuMoreButton->setAutoRaise(true);
    m_gpuMoreButton->setCursor(Qt::PointingHandCursor);
    m_gpuMoreButton->setFixedSize(24, 24);
    m_gpuMoreButton->setToolTip(i18n("Show all graphics cards"));
    m_gpuMoreButton->setStyleSheet(QStringLiteral(
        "QToolButton { border: none; border-radius: 6px; padding: 0;"
        " font-size: 18px; font-weight: 700; color: palette(text); }"
        "QToolButton:hover { background-color: palette(midlight); }"));
    m_gpuMoreButton->hide();
    gpuCard->installEventFilter(this);

    connect(m_gpuMoreButton, &QToolButton::clicked, this, [this] {
        QMenu menu(this);
        for (const QString &name : m_gpuNames) {
            menu.addAction(name);
        }
        menu.exec(m_gpuMoreButton->mapToGlobal(QPoint(0, -menu.sizeHint().height())));
    });

    grid->addWidget(cpuCard, 0, 0);
    grid->addWidget(memoryCard, 0, 1);
    grid->addWidget(storageCard, 0, 2);
    grid->addWidget(gpuCard, 0, 3);
    for (int column = 0; column < 4; ++column) {
        grid->setColumnStretch(column, 1);
    }

    layout->addLayout(grid);
    layout->addStretch();
}

QFrame *BasicInfoPage::createCard(const QString &title, const QStringList &iconNames, bool smallValue,
                                  QLabel **valueLabel, QLabel **detailLabel)
{
    auto *card = new QFrame(this);
    card->setObjectName(QStringLiteral("card"));
    card->setMinimumHeight(178);
    card->setMaximumHeight(200);

    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(18, 16, 18, 16);
    layout->setSpacing(8);

    auto *titleRow = new QHBoxLayout;
    titleRow->setSpacing(8);

    const QIcon icon = findThemedIcon(iconNames);
    if (!icon.isNull()) {
        auto *iconLabel = new QLabel(card);
        iconLabel->setPixmap(icon.pixmap(18, 18));
        titleRow->addWidget(iconLabel);
    }

    auto *titleLabel = new QLabel(title, card);
    titleLabel->setObjectName(QStringLiteral("sectionTitle"));
    titleRow->addWidget(titleLabel);
    titleRow->addStretch();
    layout->addLayout(titleRow);

    auto *value = new QLabel(card);
    value->setObjectName(smallValue ? QStringLiteral("metricValueSmall") : QStringLiteral("metricValue"));
    value->setWordWrap(true);
    value->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(value);

    auto *detail = new QLabel(card);
    detail->setProperty("muted", true);
    detail->setWordWrap(true);
    layout->addWidget(detail);

    layout->addStretch();

    *valueLabel = value;
    *detailLabel = detail;
    return card;
}

void BasicInfoPage::positionGpuMoreButton()
{
    if (!m_gpuCard || !m_gpuMoreButton) {
        return;
    }
    m_gpuMoreButton->move(m_gpuCard->width() - m_gpuMoreButton->width() - 14,
                          m_gpuCard->height() - m_gpuMoreButton->height() - 12);
    m_gpuMoreButton->raise();
}

bool BasicInfoPage::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_gpuCard && event->type() == QEvent::Resize) {
        positionGpuMoreButton();
    }
    return QWidget::eventFilter(watched, event);
}

void BasicInfoPage::refresh()
{
    const SystemInfoData data = SystemInfo::collect();

    m_osNameLabel->setText(data.osName);
    m_productLabel->setText(i18n("Device: %1", data.productName));

    m_cpuValue->setText(data.cpuModel);
    if (data.cpuCores > 0 && data.cpuThreads > 0) {
        m_cpuDetail->setText(i18n("%1 cores, %2 threads", data.cpuCores, data.cpuThreads));
    } else {
        m_cpuDetail->setText(i18n("Not available"));
    }

    const double installedGb =
        data.memoryInstalledGb > 0.0 ? data.memoryInstalledGb : data.memoryTotalGb;
    m_memoryValue->setText(i18n("%1 GB", formatGb(installedGb)));
    QStringList memoryDetails;
    memoryDetails << i18n("%1 GB usable, %2 GB available",
                          formatGb(data.memoryTotalGb), formatGb(data.memoryAvailableGb));
    QStringList memoryParts;
    if (data.memoryType != i18n("Not available")) {
        memoryParts << data.memoryType;
    }
    if (data.memorySpeed != i18n("Not available")) {
        memoryParts << data.memorySpeed;
    }
    if (!memoryParts.isEmpty()) {
        memoryDetails << memoryParts.join(QStringLiteral(" · "));
    }
    m_memoryDetail->setText(memoryDetails.join(QLatin1Char('\n')));

    m_storageValue->setText(i18n("%1 GB", formatGb(data.storageTotalGb)));
    m_storageDetail->setText(i18n("%1 GB available", formatGb(data.storageAvailableGb)));

    m_gpuValue->setText(data.gpuName);
    m_gpuDetail->setText(i18n("Driver version: %1", data.gpuDriverVersion));

    m_gpuNames = data.gpuNames;
    m_gpuMoreButton->setVisible(m_gpuNames.size() > 1);
    positionGpuMoreButton();
}
