#ifndef FTPPROXY_H
#define FTPPROXY_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QHostAddress>

#define DATA_COMMAND_NUMBER 4
#define DATA_COMMAND_LENGTH 5
#define CLIENT_CONTROL_PORT 15024
#define CLIENT_DATA_PORT 15034

class FTPProxy : public QObject
{
    Q_OBJECT
public:
    explicit FTPProxy(QObject *parent = nullptr);
    FTPProxy(const char* serverAddress, const char* clientAddress, const quint16 clControlPort, const quint16 clDataPort);
    static const quint16 SERVER_CONTROL_PORT = 21;
    static quint16 clientControlPort;
    static quint16 clientDataPort;
    static quint16 serverDataPort;

    bool connectServerControl();
    bool connectClientControl();

signals:
    void ftpDataCommandSignal(quint16 dataPort);
private slots:
    void parseServerToClientControls();
    void parseClientToServerControls();
//    void activateDataLine(quint16 dataPort);

private:
    QByteArray *controlBuffer,
        *dataBuffer;
    QHostAddress *serverAddress,
        *clientAddress;
    QTcpServer *controlTcpServerToClient,
        *dataTcpServerToClient;
    QTcpSocket *clientControlSocket,
        *clientDataSocket,
        *serverControlSocket,
        *serverDataSocket;
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
        CLIENT_TO_SERVER_CONTROL_PARSED,
        DATA_LINE_PASSIVE_STATE
    };

    const char* returnInfoMessage(FTPProxyInfo infoMessage);
    const char ftpDataCommands[DATA_COMMAND_NUMBER][DATA_COMMAND_LENGTH] = {"STOR", "LIST", "RETR", "PORT"};
    const quint16 ATTEMPT_TIME_BASE = 10;
    const quint8 MAX_NUMBER_OF_FAILED_ATTEMPTS = 10;
    const quint8 PACKET_COMMAND_OFFSET = 0x18;
    const quint8 PACKET_PORT_OFFSET = 0x25;

    bool checkIfDataCommand(QByteArray& packet);
    bool checkIfPassiveCommand(QByteArray& packet);
    quint16 extractDataPortFromPacketEPSV(QByteArray& packet);
    quint16 extractDataPortFromPacket(QByteArray& packet);
};

#endif // FTPPROXY_H
