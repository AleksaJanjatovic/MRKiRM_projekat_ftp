#include "FTPProxy.h"
#include <QNetworkProxy>
#include <QtMath>
#include <QThread>
#include <QCoreApplication>

quint16 FTPProxy::clientControlPort = CLIENT_CONTROL_PORT;
quint16 FTPProxy::clientDataPort = CLIENT_DATA_PORT;
quint16 FTPProxy::serverDataPort = 0;
QHostAddress* FTPProxy::serverAddress = nullptr;
QHostAddress* FTPProxy::clientAddress = nullptr;

/*****CONTROL PARSER*****/
ControlParser::ControlParser(QObject *parent) : QObject(parent)
{

}

//SLOTS//
void ControlParser::activateControlLineThread()
{
    controlThread = new QThread();
    moveToThread(controlThread);
    connect(controlThread, SIGNAL(started()), this, SLOT(activateControlLine()));
    controlThread->start();
}

void ControlParser::activateControlLine()
{
    controlBuffer = new QByteArray();
    //STARTING CLIENT CONTROL
    qInfo("%s", FTPProxy::returnInfoMessage(FTPProxy::STARTING_CLIENT_CONTROL_CONNECTION));

    controlTcpProxyToClient = new QTcpServer(this);
    if(!controlTcpProxyToClient->listen(*FTPProxy::clientAddress, FTPProxy::clientControlPort))
    {
        qFatal("%s", controlTcpProxyToClient->errorString().toStdString().c_str());
        return;
    }

    qInfo("\t%s", FTPProxy::returnInfoMessage(FTPProxy::WAITING_FOR_CLIENT_CONTROL_CONNECTIONS));
    if(!(clientControlConnected = controlTcpProxyToClient->waitForNewConnection(-1)))
    {
        qFatal("%s", (controlTcpProxyToClient->errorString().toStdString().c_str()));
        return;
    }
    clientControlSocket = controlTcpProxyToClient->nextPendingConnection();
    connect(clientControlSocket, SIGNAL(readyRead()), this, SLOT(parseClientToServerControl()));
    connect(clientControlSocket, SIGNAL(disconnected()), this, SLOT(disconnectClientControlLine()));
    qInfo("%s", FTPProxy::returnInfoMessage(FTPProxy::CLIENT_CONTROL_CONFIRMATION));

    //STARTING SERVER CONTROL;
    qInfo("%s", FTPProxy::returnInfoMessage(FTPProxy::STARTING_SERVER_CONTROL_CONNECTION));
    serverControlSocket = new QTcpSocket(this);
    serverControlSocket->connectToHost(*FTPProxy::serverAddress, FTPProxy::SERVER_CONTROL_PORT);

    qInfo("\t%s", FTPProxy::FTPProxy::returnInfoMessage(FTPProxy::WAITING_FOR_SERVER_CONTROL_RESPONSE));
    if(!(serverControlConnected = serverControlSocket->waitForConnected(10000)))
    {
        qFatal("%s", (serverControlSocket->errorString()).toStdString().c_str());
        return;
    }
    connect(serverControlSocket, SIGNAL(readyRead()), this, SLOT(parseServerToClientControl()));
    connect(serverControlSocket, SIGNAL(disconnected()), this, SLOT(disconnectServerControlLine()));
    qInfo("%s", FTPProxy::FTPProxy::returnInfoMessage(FTPProxy::SERVER_CONTROL_CONFIRMATION));
}

void ControlParser::parseClientToServerControl()
{
    *controlBuffer = clientControlSocket->readAll();
    controlByteNumberClientToServer += controlBuffer->size();

    serverControlSocket->write(*controlBuffer);
    controlByteNumberClientToServer -= controlBuffer->size();

    qInfo("%s", FTPProxy::returnInfoMessage(FTPProxy::CLIENT_TO_SERVER_CONTROL_PARSED));
    qInfo("%s", controlBuffer->toStdString().c_str());
    controlBuffer->clear();
}

void ControlParser::parseServerToClientControl()
{
    *controlBuffer = serverControlSocket->readAll();
    processServerResponse(*controlBuffer);
    controlByteNumberServerToClient += controlBuffer->size();

    clientControlSocket->write(*controlBuffer);
    controlByteNumberServerToClient -= controlBuffer->size();

    qInfo("%s", FTPProxy::returnInfoMessage(FTPProxy::SERVER_TO_CLIENT_CONTROL_PARSED));
    qInfo("%s", controlBuffer->toStdString().c_str());
    controlBuffer->clear();
}

void ControlParser::disconnectServerControlLine()
{
    serverControlConnected = false;
    qInfo("%s", FTPProxy::returnInfoMessage(FTPProxy::SERVER_CONTROL_DISCONNECTED));
}

void ControlParser::disconnectClientControlLine()
{
    clientControlConnected = false;
    clientControlSocket->disconnectFromHost();

    clientControlSocket->deleteLater();
    serverControlSocket->deleteLater();
    controlTcpProxyToClient->deleteLater();

    qInfo("%s", FTPProxy::returnInfoMessage(FTPProxy::CLIENT_CONTROL_DISCONNECTED));
    moveToThread(QCoreApplication::instance()->thread());
    controlThread->quit();
    emit restartSession();
}

//UTILITIES//
ControlParser::ServerResponse ControlParser::processServerResponse(QByteArray& packet) {
    QString serverResponse;
    serverResponse.push_back(packet.at(0));
    serverResponse.push_back(packet.at(1));
    serverResponse.push_back(packet.at(2));
    quint16 serverResponseCode = serverResponse.toUInt();
    switch (serverResponseCode) {
    case ENTERING_PASSIVE_MODE:
        FTPProxy::serverDataPort = extractDataPortFromPacket(packet);
        convertPassiveModePacket(packet);
        emit dataConnectionOpenningSignal();
        return ENTERING_PASSIVE_MODE;
    case OPENNING_DATA_CHANNEL:
        return OPENNING_DATA_CHANNEL;
    default:
        return RESPONSE_NOT_IMPLEMENTED;
    }
}

quint16 ControlParser::extractDataPortFromPacket(QByteArray& packet)
{
    QString stringForSplitting(packet);
    QStringList stringList = stringForSplitting.split(",");
    QString portSecondPart = stringList.at(5);
    portSecondPart.truncate(portSecondPart.indexOf(')'));
    return portSecondPart.toUInt() + (stringList.at(4).toUInt() << 8);
}

void ControlParser::convertPassiveModePacket(QByteArray& packet)
{
    packet.truncate(27);
    packet.append(FTPProxy::serverAddress->toString().replace('.',',') + ',');
    packet.append(QString::number((FTPProxy::clientDataPort - (FTPProxy::clientDataPort & 0x00FF)) >> 8) + ',');
    packet.append(QString::number(FTPProxy::clientDataPort & 0x00FF) + ")\r\n");
}

/*****DATA PARSER*****/
DataParser::DataParser(QObject* parent) : QObject(parent)
{

}

//SLOTS//
void DataParser::activateDataLineThread()
{
    dataThread = new QThread();
    moveToThread(dataThread);
    connect(dataThread, SIGNAL(started()), this, SLOT(activateDataLine()));
    dataThread->start();
}

void DataParser::activateDataLine()
{
    qInfo("%s", FTPProxy::returnInfoMessage(FTPProxy::ESTABLISHING_DATA_CONNECTION));
    dataTcpProxyToClient = new QTcpServer();

    qInfo("\t%s", FTPProxy::returnInfoMessage(FTPProxy::WAITING_FOR_CLIENT_DATA_CONNECTION));
    dataTcpProxyToClient->listen(*FTPProxy::clientAddress, FTPProxy::clientDataPort);
    if(!(clientDataConnected = dataTcpProxyToClient->waitForNewConnection(-1)))
    {
        qFatal("%s", dataTcpProxyToClient->errorString().toStdString().c_str());
        return;
    }
    clientDataSocket = dataTcpProxyToClient->nextPendingConnection();
    qInfo("\t%s", FTPProxy::returnInfoMessage(FTPProxy::CLIENT_DATA_LINE_CONFIRMATION));

    qInfo("\t%s", FTPProxy::returnInfoMessage(FTPProxy::WAITING_FOR_SERVER_DATA_RESPONSE));
    serverDataSocket = new QTcpSocket();
    serverDataSocket->connectToHost(*FTPProxy::serverAddress, FTPProxy::serverDataPort);
    if(!(serverDataConnected = serverDataSocket->waitForConnected(10000)))
    {
        qFatal("%s", serverDataSocket->errorString().toStdString().c_str());
        return;
    }
    qInfo("\t%s", FTPProxy::returnInfoMessage(FTPProxy::SERVER_DATA_LINE_CONFIRMATION));

    connect(serverDataSocket, SIGNAL(readyRead()), this, SLOT(parseServerToClientData()));
    connect(serverDataSocket, SIGNAL(disconnected()), this, SLOT(disconnectServerDataLine()));
    connect(clientDataSocket, SIGNAL(bytesWritten(qint64)), this, SLOT(disconnectClientDataLine(qint64)));
    connect(clientDataSocket, SIGNAL(readyRead()), this, SLOT(parseClientToServerData()));
    qInfo("%s", FTPProxy::returnInfoMessage(FTPProxy::DATA_LINE_ESTABLISHED));
}

void DataParser::parseClientToServerData()
{
    dataBufferClientToServer = new QByteArray();

    *dataBufferClientToServer = clientDataSocket->readAll();
    dataByteNumberClientToServer += dataBufferClientToServer->size();

    serverDataSocket->write(*dataBufferClientToServer);
    dataByteNumberClientToServer -= dataBufferClientToServer->size();

    qInfo("%s", FTPProxy::returnInfoMessage(FTPProxy::CLIENT_TO_SERVER_DATA_PARSED));
    delete dataBufferClientToServer;
}

void DataParser::parseServerToClientData()
{
    dataBufferServerToClient = new QByteArray();

    *dataBufferServerToClient = serverDataSocket->readAll();
    dataByteNumberServerToClient += dataBufferServerToClient->size();

    clientDataSocket->write(*dataBufferServerToClient);
    dataByteNumberServerToClient -= dataBufferServerToClient->size();

    qInfo("%s", FTPProxy::returnInfoMessage(FTPProxy::SERVER_TO_CLIENT_DATA_PARSED));
    delete dataBufferServerToClient;
}

void DataParser::disconnectServerDataLine()
{
    serverDataConnected = false;
    qInfo("%s", FTPProxy::returnInfoMessage(FTPProxy::SERVER_DATA_LINE_DISCONNECTED));
}

void DataParser::disconnectClientDataLine(qint64 dummy)
{
    if (!serverDataConnected && dataByteNumberServerToClient == 0)
    {
        clientDataConnected = false;
        clientDataSocket->disconnectFromHost();
        dataTcpProxyToClient->deleteLater();
        serverDataSocket->deleteLater();
        clientDataSocket->deleteLater();
        qInfo("%s", FTPProxy::returnInfoMessage(FTPProxy::CLIENT_DATA_LINE_DISCONNECTED));
        moveToThread(QCoreApplication::instance()->thread());
        dataThread->quit();
    }
}

/*****FTP Proxy*****/
FTPProxy::FTPProxy(QObject *parent) : QObject(parent)
{

}

FTPProxy::FTPProxy(const char* serverAddress, const char* clientAddress, const quint16 clControlPort = CLIENT_CONTROL_PORT, const quint16 clDataPort = CLIENT_DATA_PORT)
{
    this->serverAddress = new QHostAddress(serverAddress);
    this->clientAddress = new QHostAddress(clientAddress);
    clientControlPort = clControlPort ;
    clientDataPort = clDataPort;

    controlParser = new ControlParser();
    dataParser = new DataParser();

    connect(controlParser, SIGNAL(dataConnectionOpenningSignal()), dataParser, SLOT(activateDataLineThread()));
    connect(controlParser, SIGNAL(restartSession()), this, SLOT(start()));
}

//SLOTS//
void FTPProxy::start()
{
    controlParser->activateControlLineThread();
}

//UTILITIES//
const char* FTPProxy::returnInfoMessage(FTPProxyInfo infoMessage)
{
    QReadWriteLock infoOutputLock;
    QReadLocker readLocker(&infoOutputLock);
    switch(infoMessage)
    {
    case FTPProxy::SERVER_DATA_LINE_DISCONNECTED:
        return "Server To Proxy Data Line Disconnected.";
    case FTPProxy::CLIENT_DATA_LINE_DISCONNECTED:
        return "Client To Proxy Data Line Disconnected.";
    case FTPProxy::SERVER_CONTROL_DISCONNECTED:
        return "Server To Proxy Control Disconnected.";
    case FTPProxy::CLIENT_CONTROL_DISCONNECTED:
        return "Client To Proxy Control Disconnected.";
    case FTPProxy::WAITING_FOR_SERVER_DATA_RESPONSE:
        return "Waiting For Server Data Line Response...";
    case FTPProxy::DATA_LINE_ESTABLISHED:
        return "Data Line Established!!!";
    case FTPProxy::CONTROL_LINE_ESTABLISHED:
        return "Control Line Established";
    case FTPProxy::SERVER_TO_CLIENT_DATA_PARSED:
        return "Data Packet From Server To Client Parsed";
    case FTPProxy::CLIENT_TO_SERVER_DATA_PARSED:
        return "Data Packet From Client To Server Parsed";
    case FTPProxy::ESTABLISHING_DATA_CONNECTION:
        return "Starting Data Line Connection.";
    case FTPProxy::CLIENT_DATA_LINE_CONFIRMATION:
        return "Client Data Line Confirmed!";
    case FTPProxy::WAITING_FOR_CLIENT_DATA_CONNECTION:
        return "Waiting For Client Data Line Connections...";
    case FTPProxy::SERVER_DATA_LINE_CONFIRMATION:
        return "Server Data Line Confirmed!";
    case FTPProxy::CLIENT_CONTROL_CONFIRMATION:
        return "Client Control Connection Confirmed!";
    case FTPProxy::STARTING_SERVER_CONTROL_CONNECTION:
        return "Starting Server Control Connection.";
    case FTPProxy::WAITING_FOR_SERVER_CONTROL_RESPONSE:
        return "Waiting For Server Control Connection Response...";
    case FTPProxy::SERVER_CONTROL_CONFIRMATION:
        return "Server Control Connection Confirmed!";
    case FTPProxy::STARTING_CLIENT_CONTROL_CONNECTION:
        return "Starting Client Control Connection.";
    case FTPProxy::WAITING_FOR_CLIENT_CONTROL_CONNECTIONS:
        return "Waiting For Client Control Connections...";
    case FTPProxy::CLIENT_TO_SERVER_CONTROL_PARSED:
        return "Client To Server Controls Accepted";
    case FTPProxy::SERVER_TO_CLIENT_CONTROL_PARSED:
        return "Server To Client Controls Accepted";
    }
    return "None";
}
