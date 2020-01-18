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
    clientControlPort = clControlPort ;
    clientDataPort = clDataPort;
    infoOutputLock = new QReadWriteLock();
    connect(this, SIGNAL(ftpDataCommandSignal()), this, SLOT(activateDataLineSlot()));
    dataThread = new QThread();
}

bool FTPProxy::connectServerControl() {
    quint16 numberOfAttempts = 0;

    /* SERVER CONTROL CONNECTION */
    qInfo("%s", returnInfoMessage(STARTING_SERVER_CONTROL_CONNECTION));
    serverControlSocket = new QTcpSocket(this);
    //serverControlSocket->setProxy(QNetworkProxy::NoProxy);
    serverControlSocket->connectToHost(*serverAddress, SERVER_CONTROL_PORT);

    qInfo("\t%s", returnInfoMessage(WAITING_FOR_SERVER_CONTROL_RESPONSE));
    while(!(serverControlConnected = serverControlSocket->waitForConnected(ATTEMPT_TIME_BASE * pow(2, numberOfAttempts)))) {
        if(++numberOfAttempts >= MAX_NUMBER_OF_FAILED_ATTEMPTS)
            break;
    }
    if (!serverControlConnected) {
        qFatal("%s", (serverControlSocket->errorString()).toStdString().c_str());
        return false;
    }
    connect(serverControlSocket, SIGNAL(readyRead()), this, SLOT(parseServerToClientControls()));

    qInfo("%s", returnInfoMessage(SERVER_CONTROL_CONFIRMATION));
    return true;
}

bool FTPProxy::connectClientControl() {
    qInfo("%s", returnInfoMessage(STARTING_CLIENT_CONTROL_CONNECTION));
//    clientCon
    controlTcpServerToClient = new QTcpServer(this);
   //Ovo bi trebalo zakomentarisati
    clientControlSocket = new QTcpSocket(this);
    controlTcpServerToClient->listen(*clientAddress, clientControlPort);

    qInfo("\t%s", returnInfoMessage(WAITING_FOR_CLIENT_CONNECTIONS));
    while(!(clientControlConnected = controlTcpServerToClient->waitForNewConnection()));
    clientControlSocket = controlTcpServerToClient->nextPendingConnection();
    connect(clientControlSocket, SIGNAL(readyRead()), this, SLOT(parseClientToServerControls()));
    qInfo("%s", returnInfoMessage(CLIENT_CONTROL_CONFIRMATION));
    return true;
}

const char* FTPProxy::returnInfoMessage(FTPProxyInfo infoMessage) {
    QReadLocker readLocker(infoOutputLock);
    switch(infoMessage) {
    case FTPProxy::SERVER_TO_CLIENT_DATA_PARSED:
        return "Data Packet From Server To Client Parsed";
    case FTPProxy::CLIENT_TO_SERVER_DATA_PARSED:
        return "Data Packet From Client To Server Parsed";
    case FTPProxy::ESTABLISHING_DATA_CONNECTION:
        return "Starting Data Line Connection.";
    case FTPProxy::SERVER_TO_PROXY_DATA_LINE_CONNECTED:
        return "Server To Proxy Data Line Confirmed!";
    case FTPProxy::PROXY_TO_CLIENT_DATA_LINE_REQUEST_SENT:
        return "Waiting For Client Data Line Connection Response...";
    case FTPProxy::PROXY_TO_CLIENT_DATA_LINE_CONFIRMED:
        return "Proxy To Client Data Line Confirmed!";
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
    case FTPProxy::WAITING_FOR_CLIENT_CONNECTIONS:
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
    if(checkIfPassiveCommand(*controlBuffer)) {
        serverDataPort = extractDataPortFromPacket(*controlBuffer);
        convertPassiveModePacket(*controlBuffer);
        emit ftpDataCommandSignal();
    }
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
    serverDataSocket = new QTcpSocket();
    dataTcpServerToClient = new QTcpServer();
    dataTcpServerToClient->listen(*clientAddress, clientDataPort);
  //  while(!(serverDataConnected = dataTcpServerToClient->waitForNewConnection()));
    serverDataSocket = dataTcpServerToClient->nextPendingConnection();


    qInfo("\t%s", returnInfoMessage(SERVER_TO_PROXY_DATA_LINE_CONNECTED));
    clientDataSocket = new QTcpSocket();
    clientDataSocket->connectToHost(*serverAddress, serverDataPort);

    qInfo("\t%s", returnInfoMessage(PROXY_TO_CLIENT_DATA_LINE_REQUEST_SENT));
    if(!(clientDataConnected = clientDataSocket->waitForConnected(10000))) {
        qFatal("%s", clientDataSocket->errorString().toStdString().c_str());
        return;
    }
    connect(serverDataSocket, SIGNAL(readyRead()), this, SLOT(parseServerToClientData()));
    connect(clientDataSocket, SIGNAL(readyRead()), this, SLOT(parseClientToServerData()));
    qInfo("\t%s", returnInfoMessage(PROXY_TO_CLIENT_DATA_LINE_CONFIRMED));
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
    packet.append(QString::number(clientDataPort & 0x00FF) + ')');
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
    qInfo("%s", returnInfoMessage(CLIENT_TO_SERVER_DATA_PARSED));
    clientDataSocket->flush();
    dataBuffer->clear();
}
