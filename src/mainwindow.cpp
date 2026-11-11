#include "mainwindow.h"

#include "basicinfopage.h"
#include "driverpage.h"

#include <QApplication>
#include <QButtonGroup>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

#include <KLocalizedString>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(i18n("Miryu Hardware Manager"));
    resize(1120, 720);
    setMinimumSize(940, 620);
    buildUi();
}

void MainWindow::buildUi()
{
    auto *central = new QWidget(this);
    setCentralWidget(central);

    auto *rootLayout = new QVBoxLayout(central);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    auto *header = new QFrame(central);
    header->setObjectName(QStringLiteral("appHeader"));
    auto *headerLayout = new QVBoxLayout(header);
    headerLayout->setContentsMargins(24, 14, 24, 12);
    headerLayout->setSpacing(12);

    auto *titleRow = new QHBoxLayout;
    titleRow->setSpacing(12);

    auto *iconLabel = new QLabel(header);
    QIcon appIcon = QApplication::windowIcon();
    if (appIcon.isNull()) {
        appIcon = QIcon::fromTheme(QStringLiteral("computer"));
    }
    iconLabel->setPixmap(appIcon.pixmap(40, 40));
    titleRow->addWidget(iconLabel);

    auto *titleLabel = new QLabel(i18n("Miryu Hardware Manager"), header);
    titleLabel->setObjectName(QStringLiteral("appTitle"));
    titleRow->addWidget(titleLabel);
    titleRow->addStretch();
    headerLayout->addLayout(titleRow);

    auto *tabRow = new QHBoxLayout;
    tabRow->setSpacing(8);

    m_basicTab = new QPushButton(i18n("Basic Information"), header);
    m_basicTab->setObjectName(QStringLiteral("tabPill"));
    m_basicTab->setCheckable(true);
    m_basicTab->setChecked(true);

    m_driverTab = new QPushButton(i18n("Driver Management"), header);
    m_driverTab->setObjectName(QStringLiteral("tabPill"));
    m_driverTab->setCheckable(true);

    auto *tabGroup = new QButtonGroup(this);
    tabGroup->setExclusive(true);
    tabGroup->addButton(m_basicTab);
    tabGroup->addButton(m_driverTab);

    tabRow->addWidget(m_basicTab);
    tabRow->addWidget(m_driverTab);
    tabRow->addStretch();
    headerLayout->addLayout(tabRow);

    rootLayout->addWidget(header);

    m_stack = new QStackedWidget(central);
    m_basicPage = new BasicInfoPage(m_stack);
    m_driverPage = new DriverPage(m_stack);
    m_stack->addWidget(m_basicPage);
    m_stack->addWidget(m_driverPage);
    rootLayout->addWidget(m_stack, 1);

    connect(m_basicTab, &QPushButton::clicked, this, [this] {
        m_stack->setCurrentWidget(m_basicPage);
    });
    connect(m_driverTab, &QPushButton::clicked, this, [this] {
        m_stack->setCurrentWidget(m_driverPage);
    });
}
