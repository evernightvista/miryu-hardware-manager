#include <QApplication>
#include <QIcon>
#include <KAboutData>
#include <KCrash>
#include <KLocalizedString>

#include "mainwindow.h"

static const char *const kStyleSheet = R"(
QFrame#appHeader {
    background-color: palette(base);
    border-bottom: 1px solid palette(mid);
}
QLabel#appTitle {
    font-size: 21px;
    font-weight: 600;
}
QPushButton#tabPill {
    border: none;
    border-radius: 16px;
    padding: 7px 22px;
    background-color: transparent;
    color: palette(window-text);
}
QPushButton#tabPill:hover {
    background-color: palette(midlight);
}
QPushButton#tabPill:checked {
    background-color: palette(highlight);
    color: palette(highlighted-text);
    font-weight: 600;
}
QFrame#card {
    background-color: palette(base);
    border: 1px solid palette(mid);
    border-radius: 14px;
}
QLabel#osName {
    font-size: 23px;
    font-weight: 600;
}
QLabel#sectionTitle {
    font-size: 16px;
    font-weight: 600;
}
QLabel#metricValue {
    font-size: 20px;
    font-weight: 600;
}
QLabel#metricValueSmall {
    font-size: 15px;
    font-weight: 600;
}
QLabel[muted="true"] {
    color: #8a8f98;
}
QLabel[state="ok"] {
    color: #2f9e44;
    font-weight: 600;
}
QLabel[state="none"] {
    color: #8a8f98;
}
)";

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("miryu-hardware-manager"));
    app.setOrganizationDomain(QStringLiteral("miryu.gaming"));
    app.setStyleSheet(QString::fromLatin1(kStyleSheet));

    QIcon appIcon = QIcon::fromTheme(QStringLiteral("yast-hardware"));
    if (appIcon.isNull()) {
        appIcon = QIcon::fromTheme(QStringLiteral("computer"));
    }
    app.setWindowIcon(appIcon);

    KLocalizedString::setApplicationDomain("miryu-hardware-manager");

    KAboutData aboutData(
        QStringLiteral("miryu-hardware-manager"),
        i18n("Miryu Hardware Manager"),
        QStringLiteral("45.0.0"),
        i18n("View hardware information and manage device drivers"),
        KAboutLicense::GPL_V3,
        i18n("(C) 2026 Miryu"));

    KAboutData::setApplicationData(aboutData);

    KCrash::initialize();

    MainWindow window;
    window.show();
    return app.exec();
}
