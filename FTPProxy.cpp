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
    switch(infoMessage) {
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
    case FTPProxy::DATA_LINE_PASSIVE_STATE:
        return "Control Packet Accepted For Passive Data Line State.";
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
    clientControlSocket->write(*controlBuffer);
    qDebug(*controlBuffer);
    qInfo("%s", returnInfoMessage(SERVER_TO_CLIENT_CONTROL_PARSED));
    if(checkIfPassiveCommand(*controlBuffer)) {
        serverDataPort = extractDataPortFromPacket(*controlBuffer);
    }
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
    QString portString[2] = {stringList.at(5), stringList.at(4)};
    return portString[0].toUInt() + (portString[1].toUInt() >> 8);
}
