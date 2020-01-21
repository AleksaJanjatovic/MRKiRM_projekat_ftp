#include "FTPProxy.h"
#include <QNetworkProxy>
#include <QtMath>
#include <QThread>

quint16 FTPProxy::clientControlPort = CLIENT_CONTROL_PORT;
quint16 FTPProxy::clientDataPort = CLIENT_DATA_PORT;
quint16 FTPProxy::serverDataPort = 0;

FTPProxy::FTPProxy(QObject *parent) : QObject(parent)
{

}

FTPProxy::FTPProxy(const char* serverAddress, const char* clientAddress, const quint16 clControlPort = CLIENT_CONTROL_PORT, const quint16 clDataPort = CLIENT_DATA_PORT)
{
    this->serverAddress = new QHostAddress(serverAddress);
    this->clientAddress = new QHostAddress(clientAddress);
    controlBuffer = new QByteArray();
    dataBuffer = new QByteArray();
    clientControlPort = clControlPort ;
    clientDataPort = clDataPort;
    infoOutputLock = new QReadWriteLock();
    connect(this, SIGNAL(ftpDataConnectionOpeningSignal()), this, SLOT(activateDataLineSlot()));
    dataThread = new QThread();

}

bool FTPProxy::connectServerControl() {
    qInfo("%s", returnInfoMessage(STARTING_SERVER_CONTROL_CONNECTION));
    serverControlSocket = new QTcpSocket(this);
    serverControlSocket->connectToHost(*serverAddress, SERVER_CONTROL_PORT);

    qInfo("\t%s", returnInfoMessage(WAITING_FOR_SERVER_CONTROL_RESPONSE));
    if(!(serverControlConnected = serverControlSocket->waitForConnected(10000))) {
        qFatal("%s", (serverControlSocket->errorString()).toStdString().c_str());
        return false;
    }
    connect(serverControlSocket, SIGNAL(readyRead()), this, SLOT(parseServerToClientControls()));

    qInfo("%s", returnInfoMessage(SERVER_CONTROL_CONFIRMATION));
    return true;
}

bool FTPProxy::connectClientControl() {
    qInfo("%s", returnInfoMessage(STARTING_CLIENT_CONTROL_CONNECTION));

    controlTcpProxyToClient = new QTcpServer(this);
    if(!controlTcpProxyToClient->listen(*clientAddress, clientControlPort)) {
        qFatal("%s", controlTcpProxyToClient->errorString().toStdString().c_str());
        return false;
    }
    qInfo("\t%s", returnInfoMessage(WAITING_FOR_CLIENT_CONTROL_CONNECTIONS));
    if(!(clientControlConnected = controlTcpProxyToClient->waitForNewConnection(-1))) {
        qFatal("%s", (controlTcpProxyToClient->errorString().toStdString().c_str()));
        return false;
    }
    clientControlSocket = controlTcpProxyToClient->nextPendingConnection();
    connect(clientControlSocket, SIGNAL(readyRead()), this, SLOT(parseClientToServerControls()));
    qInfo("%s", returnInfoMessage(CLIENT_CONTROL_CONFIRMATION));
    return true;
}

const char* FTPProxy::returnInfoMessage(FTPProxyInfo infoMessage) {
    QReadLocker readLocker(infoOutputLock);
    switch(infoMessage) {
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

void FTPProxy::parseClientToServerControls() {
    //QThread::msleep(5);
    *controlBuffer = clientControlSocket->readAll();
    serverControlSocket->write(*controlBuffer);
    qDebug(*controlBuffer);
    qInfo("%s", returnInfoMessage(CLIENT_TO_SERVER_CONTROL_PARSED));
    controlBuffer->clear();
    serverControlSocket->flush();
}

void FTPProxy::parseServerToClientControls() {
    //QThread::msleep(5);
    *controlBuffer = serverControlSocket->readAll();
    qDebug(*controlBuffer);
    processServerResponse(*controlBuffer);
    clientControlSocket->write(*controlBuffer);
    qInfo("%s", returnInfoMessage(SERVER_TO_CLIENT_CONTROL_PARSED));
    controlBuffer->clear();
    clientControlSocket->flush();
}

bool FTPProxy::checkIfDataCommand(QByteArray& packet) {
    QString dataCommandStr;
    int i;
    for (i = 0; i < 4; i++)
        dataCommandStr.push_back(packet.at(PACKET_COMMAND_OFFSET + i));

    for (i = 0; i < 4; i++)
        if (!dataCommandStr.compare(dataCommandStr))
            return true;
    return false;
}

FTPProxy::FTPServerRepsonse FTPProxy::processServerResponse(QByteArray& packet) {
    QString serverResponse;
    serverResponse.push_back(packet.at(0));
    serverResponse.push_back(packet.at(1));
    serverResponse.push_back(packet.at(2));
    quint16 serverResponseCode = serverResponse.toUInt();
    switch (serverResponseCode) {
    case FTPProxy::ENTERING_PASSIVE_MODE:
        serverDataPort = extractDataPortFromPacket(packet);
        convertPassiveModePacket(packet);
        emit ftpDataConnectionOpeningSignal();
        return ENTERING_PASSIVE_MODE;
    case FTPProxy::OPENNING_DATA_CHANNEL:
        return OPENNING_DATA_CHANNEL;
    default:
        return RESPONSE_NOT_IMPLEMENTED;
    }
}

bool FTPProxy::checkIfPassiveCommand(QByteArray& packet) {
    QString pasv("227");
    QString packetCommand;
    packetCommand.push_back(packet.at(0));
    packetCommand.push_back(packet.at(1));
    packetCommand.push_back(packet.at(2));
    if (!packetCommand.compare(pasv))
        return true;
    return false;
}

quint16 FTPProxy::extractDataPortFromPacketEPSV(QByteArray& packet) {
    QString portArray;
    for(int i = 0; i < 5; i++)
        if(portArray.at(PACKET_PORT_OFFSET + i) != '|')
            portArray.push_back(packet.at(PACKET_PORT_OFFSET + i));
        else break;
    return portArray.toUInt();
}

quint16 FTPProxy::extractDataPortFromPacket(QByteArray& packet) {
    QString stringForSplitting(packet);
    QStringList stringList = stringForSplitting.split(",");
    QString portSecondPart = stringList.at(5);
    portSecondPart.truncate(portSecondPart.indexOf(')'));
    return portSecondPart.toUInt() + (stringList.at(4).toUInt() << 8);
}

void FTPProxy::activateDataLineThread() {
    qInfo("%s", returnInfoMessage(ESTABLISHING_DATA_CONNECTION));
    dataTcpProxyToClient = new QTcpServer();

    qInfo("\t%s", returnInfoMessage(WAITING_FOR_CLIENT_DATA_CONNECTION));
    dataTcpProxyToClient->listen(*clientAddress, clientDataPort);
    if(!(clientDataConnected = dataTcpProxyToClient->waitForNewConnection(-1))) {
        qFatal("%s", dataTcpProxyToClient->errorString().toStdString().c_str());
        return;
    }
    clientDataSocket = dataTcpProxyToClient->nextPendingConnection();
    qInfo("\t%s", returnInfoMessage(CLIENT_DATA_LINE_CONFIRMATION));

    qInfo("\t%s", returnInfoMessage(WAITING_FOR_SERVER_DATA_RESPONSE));
    serverDataSocket = new QTcpSocket();
    serverDataSocket->connectToHost(*serverAddress, serverDataPort);
    if(!(serverDataConnected = serverDataSocket->waitForConnected(10000))) {
        qFatal("%s", serverDataSocket->errorString().toStdString().c_str());
        return;
    }
    qInfo("\t%s", returnInfoMessage(SERVER_DATA_LINE_CONFIRMATION));

    qInfo("%s", returnInfoMessage(DATA_LINE_ESTABLISHED));
    connect(serverDataSocket, SIGNAL(readyRead()), this, SLOT(parseServerToClientData()));
    connect(clientDataSocket, SIGNAL(readyRead()), this, SLOT(parseClientToServerData()));
}

void FTPProxy::activateDataLineSlot() {
    //moveToThread(dataThread);
    connect(dataThread, SIGNAL(started()), this, SLOT(activateDataLineThread()));
    dataThread->start();
}

void FTPProxy::convertPassiveModePacket(QByteArray& packet) {
    packet.truncate(27);
    packet.append(serverAddress->toString().replace('.',',') + ',');
    packet.append(QString::number((clientDataPort - (clientDataPort & 0x00FF)) >> 8) + ',');
    packet.append(QString::number(clientDataPort & 0x00FF) + ")\r\n");
}

void FTPProxy::parseClientToServerData() {
    *dataBuffer = clientDataSocket->readAll();
    serverDataSocket->write(*dataBuffer);
    qInfo("%s", returnInfoMessage(CLIENT_TO_SERVER_DATA_PARSED));
    serverDataSocket->flush();
    dataBuffer->clear();
}

void FTPProxy::parseServerToClientData() {
    *dataBuffer = serverDataSocket->readAll();
    clientDataSocket->write(*dataBuffer);
    qInfo("%s", returnInfoMessage(SERVER_TO_CLIENT_DATA_PARSED));
    clientDataSocket->flush();
    dataBuffer->clear();
}
