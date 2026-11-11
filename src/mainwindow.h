#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class BasicInfoPage;
class DriverPage;
class QPushButton;
class QStackedWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    void buildUi();

    BasicInfoPage *m_basicPage = nullptr;
    DriverPage *m_driverPage = nullptr;
    QStackedWidget *m_stack = nullptr;
    QPushButton *m_basicTab = nullptr;
    QPushButton *m_driverTab = nullptr;
};

#endif // MAINWINDOW_H
