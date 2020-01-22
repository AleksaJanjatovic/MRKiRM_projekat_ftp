#ifndef FTPPROXY_H
#define FTPPROXY_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QHostAddress>
#include <QReadWriteLock>
#include <QWaitCondition>
#include <QSemaphore>
#include <QMutex>
#include <QThread>

#define DATA_COMMAND_NUMBER 4
#define DATA_COMMAND_LENGTH 5
#define CLIENT_CONTROL_PORT 50001
#define CLIENT_DATA_PORT 50101

/*****CONTROL PARSER*****/
class ControlParser : public QObject
{
    Q_OBJECT
    public:
        explicit ControlParser(QObject *parent = nullptr);
    signals:
        void dataConnectionOpenningSignal();
        void controlLineDisconnectedSignal();
        void restartSession();
    public slots:
        void activateControlLineThread();
    private slots:
        void activateControlLine();
        void parseServerToClientControl();
        void parseClientToServerControl();
        void disconnectServerControlLine();
        void disconnectClientControlLine();

    private:
        QThread* controlThread;
        QHostAddress *serverAddress,
            *clientAddress;
        QByteArray* controlBuffer;
        QTcpServer* controlTcpProxyToClient;
        QTcpSocket* clientControlSocket,
            *serverControlSocket;
        quint64 controlByteNumberServerToClient,
            controlByteNumberClientToServer;
        bool clientControlConnected,
            serverControlConnected;

        enum ServerResponse {
            ENTERING_PASSIVE_MODE = 227,
            OPENNING_DATA_CHANNEL = 150,
            RESPONSE_NOT_IMPLEMENTED = 0
        };
        bool checkIfDataCommand(QByteArray& packet);
        quint16 extractDataPortFromPacket(QByteArray& packet);
        void convertPassiveModePacket(QByteArray& packet);
        ServerResponse processServerResponse(QByteArray& packet);
};

/*****DATA PARSER*****/
class DataParser : public QObject {
        Q_OBJECT
    public:
        explicit DataParser(QObject *parent = nullptr);
    signals:
        void dataLineDisconnectedSignal();
    public slots:
        void activateDataLineThread();
    private slots:
        void activateDataLine();
        void parseServerToClientData();
        void parseClientToServerData();
        void disconnectServerDataLine();
        void disconnectClientDataLine(qint64 dummy);
    private:

        QThread* dataThread;
        QByteArray *dataBufferServerToClient,
            *dataBufferClientToServer;
        QHostAddress *serverAddress,
            *clientAddress;
        QTcpServer *dataTcpProxyToClient;
        QTcpSocket *clientDataSocket,
            *serverDataSocket;
        bool serverDataConnected,
            clientDataConnected;
        QAtomicInt dataByteNumberClientToServer,
            dataByteNumberServerToClient;
};

/*****FTP Proxy*****/
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
    static QHostAddress* clientAddress,
        *serverAddress;

    enum FTPProxyInfo {
        STARTING_SERVER_CONTROL_CONNECTION,
        WAITING_FOR_SERVER_CONTROL_RESPONSE,
        STARTING_CLIENT_CONTROL_CONNECTION,
        WAITING_FOR_CLIENT_CONTROL_CONNECTIONS,
        SERVER_CONTROL_CONFIRMATION,
        CLIENT_CONTROL_CONFIRMATION,
        SERVER_TO_CLIENT_CONTROL_PARSED,
        CLIENT_TO_SERVER_CONTROL_PARSED,
        ESTABLISHING_DATA_CONNECTION,
        CLIENT_DATA_LINE_CONFIRMATION,
        WAITING_FOR_SERVER_DATA_RESPONSE,
        WAITING_FOR_CLIENT_DATA_CONNECTION,
        SERVER_DATA_LINE_CONFIRMATION,
        SERVER_TO_CLIENT_DATA_PARSED,
        CLIENT_TO_SERVER_DATA_PARSED,
        DATA_LINE_ESTABLISHED,
        CONTROL_LINE_ESTABLISHED,
        SERVER_DATA_LINE_DISCONNECTED,
        CLIENT_DATA_LINE_DISCONNECTED,
        SERVER_CONTROL_DISCONNECTED,
        CLIENT_CONTROL_DISCONNECTED
    };
    static const char* returnInfoMessage(FTPProxyInfo infoMessage);

signals:
    void dataConnectionOpenningSignal();
    void controlConnectionOpenningSignal();
public slots:
    void start();

private:
    ControlParser* controlParser;
    DataParser* dataParser;
    QReadWriteLock *infoOutputLock;
    QWaitCondition sessionNotOver;
    QMutex sessionMutex;
};

#endif // FTPPROXY_H
