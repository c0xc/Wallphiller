#include "main.hpp"

int main(int argc, char *argv[])
{
    QApplication qapp(argc, argv);
    qapp.setApplicationName(PROGRAM);
    Wallphiller *mainwindow = new Wallphiller;
    mainwindow->show();
    return qapp.exec();
}

