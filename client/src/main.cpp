#include "ftpgui.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    FTPGUI w;
    w.show();
    return a.exec();
}
