#include <QCoreApplication>
#include "FTPProxy.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    FTPProxy ftpProxy("127.0.0.1", "127.0.0.1", 15024);
    ftpProxy.connectClientControl();
    ftpProxy.connectServerControl();
    return a.exec();
}
