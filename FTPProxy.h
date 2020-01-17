#ifndef FTPPROXY_H
#define FTPPROXY_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QHostAddress>

class FTPProxy : public QObject
{
    Q_OBJECT
public:
    explicit FTPProxy(QObject *parent = nullptr);
    FTPProxy(const char* serverAddress, const char* clientAddress, const quint16 clientControlPort);
    static const quint16 SERVER_CONTROL_PORT = 21;
    static const quint16 SERVER_DATA_PORT = 20;
    static const quint16 CLIENT_DATA_PORT = 15025;
    static quint16 clientControlPort;
    static quint16 serverDataPort;

    bool connectServerControl();
    bool connectClientControl();

signals:
    void ftpDataCommand(quint16 dataPort);
private slots:
    void parseServerToClientControls();
    void parseClientToServerControls();
    void activateDataLine(quint16 dataPort);

private:
    QByteArray *controlBuffer;
    QHostAddress *serverAddress,
        *clientAddress;
    QTcpServer *controlTcpServerToClient,
        *dataTcpServerToClient;
    QTcpSocket *clientControlSocket,
        *clientDataSocket,
        *serverControlSocket,
        *serverDataSocket;
    const quint16 ATTEMPT_TIME_BASE = 10;
    const quint8 MAX_NUMBER_OF_FAILED_ATTEMPTS = 10;
    bool clientControlConnected,
        clientDataConnected,
        serverControlConnected,
        serverDataConnected;

    enum FTPProxyInfo {
        STARTING_SERVER_CONTROL_CONNECTION,
        WAITING_FOR_SERVER_CONTROL_RESPONSE,
        STARTING_CLIENT_CONTROL_CONNECTION,
        WAITING_FOR_CLIENT_CONNECTIONS,
        SERVER_CONTROL_CONFIRMATION,
        CLIENT_CONTROL_CONFIRMATION,
        SERVER_TO_CLIENT_CONTROL_PARSED,
        CLIENT_TO_SERVER_CONTROL_PARSED
    };

    const char* returnInfoMessage(FTPProxyInfo infoMessage);
    const char ftpDataCommands[4][5] = {"STOR", "LIST", "RETR", "PORT"};
};

#endif // FTPPROXY_H
