#include "main.hpp"

int main(int argc, char *argv[])
{
    QApplication qapp(argc, argv);
    qapp.setApplicationName(PROGRAM);
    //std::cout << PROGRAM << " " << GITVERSION << std::endl;
    //QApplication::processEvents();
    Wallphiller *mainwindow = new Wallphiller;
    mainwindow->show();
    return qapp.exec();
}

