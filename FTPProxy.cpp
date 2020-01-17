#include "FTPProxy.h"
#include <QNetworkProxy>
#include <QtMath>
#include <QThread>

quint16 FTPProxy::clientControlPort = 15024;

FTPProxy::FTPProxy(QObject *parent) : QObject(parent)
{

}

FTPProxy::FTPProxy(const char* serverAddress, const char* clientAddress, const quint16 clPort = 1024)
{
    this->serverAddress = new QHostAddress(serverAddress);
    this->clientAddress = new QHostAddress(clientAddress);
    controlBuffer = new QByteArray();
    clientControlPort = clPort;

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
    }
    return "None";
}

void FTPProxy::parseClientToServerControls() {
    QThread::msleep(20);
    *controlBuffer = clientControlSocket->readAll();
    serverControlSocket->write(*controlBuffer);
    controlBuffer->clear();
    serverControlSocket->flush();
    qInfo("Client To Server Controls Accepted");
}

void FTPProxy::parseServerToClientControls() {
    QThread::msleep(20);
    *controlBuffer = serverControlSocket->readAll();
    clientControlSocket->write(*controlBuffer);
    controlBuffer->clear();
    clientControlSocket->flush();
    qInfo("Server To Client Controls Accepted");
}
