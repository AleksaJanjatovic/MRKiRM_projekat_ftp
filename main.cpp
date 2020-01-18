#include <QCoreApplication>
#include "FTPProxy.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    FTPProxy ftpProxy("127.0.0.1", "127.0.0.1", CLIENT_CONTROL_PORT, CLIENT_DATA_PORT);
    ftpProxy.connectClientControl();
    ftpProxy.connectServerControl();
    return a.exec();
}
