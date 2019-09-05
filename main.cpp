#include "mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setApplicationName("ReflowOvenController");
    a.setOrganizationName("Robin Gingras");
    a.setOrganizationDomain("robingingras.com");
    MainWindow w;
    w.show();

    return a.exec();
}
