#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "QVector"
#include <string>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <thread>
using namespace std;

QString nameus;

// Ожидание подключений и возврат ip сервера клиенту
void Udp()
{
    char fileName[100];
        int sd;
        socklen_t len;

        struct sockaddr_in servaddr,cliaddr;
        QHostAddress address;

        sd = socket(AF_INET, SOCK_DGRAM, 0);

        bzero(&servaddr, sizeof(servaddr));

        int broadcastEnable=1;
        setsockopt(sd, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable));
        servaddr.sin_family = AF_INET;
        servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
        servaddr.sin_port = htons(8001);

        memset(&(servaddr.sin_zero),'\0',8);
        bind(sd, (struct sockaddr *)&servaddr, sizeof(servaddr));
        len=sizeof(cliaddr);

        while(1)
        {
            recvfrom(sd,fileName, 1024, 0, (struct sockaddr *)&cliaddr, &len);

            QTcpSocket socket;
            socket.connectToHost("8.8.8.8", 53);
            if (socket.waitForConnected()) {
                    address = socket.localAddress();
            }

            bool conversionOK = false;
            QHostAddress ip4Address(address.toIPv4Address(&conversionOK));
            QString ip4String;
            if (conversionOK)
            {
                ip4String = ip4Address.toString();
            }

            char digs[100];
            string p = ip4String.toStdString();

            strcpy(digs, p.c_str());

            sendto(sd, digs, sizeof(digs), 0,(struct sockaddr *)&cliaddr, sizeof(struct sockaddr));
        }
}

// Запуск сервера и прослушки
MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    std::thread Thread(Udp);
    Thread.detach();


    m_server = new QTcpServer();

    if(m_server->listen(QHostAddress::Any, 8080))
    {
       connect(this, &MainWindow::newMessage, this, &MainWindow::displayMessage);
       connect(m_server, &QTcpServer::newConnection, this, &MainWindow::newConnection);
    }
    else
    {
        QMessageBox::critical(this,"QTCPServer",QString("Невозможно запустить сервер: %1.").arg(m_server->errorString()));
        exit(EXIT_FAILURE);
    }
}

MainWindow::~MainWindow()
{
    foreach (QTcpSocket* socket, connection_set)
    {
        socket->close();
        socket->deleteLater();
    }

    m_server->close();
    m_server->deleteLater();

    delete ui;
}

// Подключение нового клиента
void MainWindow::newConnection()
{
    while (m_server->hasPendingConnections())
        appendToSocketList(m_server->nextPendingConnection());
}

void MainWindow::appendToSocketList(QTcpSocket* socket)
{
    connection_set.insert(socket);
    connect(socket, &QTcpSocket::readyRead, this, &MainWindow::readSocket);
    connect(socket, &QTcpSocket::disconnected, this, &MainWindow::discardSocket);
    connect(socket, &QAbstractSocket::errorOccurred, this, &MainWindow::displayError);
    displayMessage(QString("Сервер >> клиент %1 успешно подключен").arg(socket->socketDescriptor()));
}

// Чтение данных от клиента
void MainWindow::readSocket()
{
    QTcpSocket* socket = reinterpret_cast<QTcpSocket*>(sender());

    QTcpSocket* socket1 = socket;

    QByteArray buffer;

    QDataStream socketStream(socket);
    socketStream.setVersion(QDataStream::Qt_5_15);

    socketStream.startTransaction();
    socketStream >> buffer;

    QString temp;
    if(!socketStream.commitTransaction())
        return;

    QString header = buffer.mid(0,128);
    QString fileType = header.split(",")[0].split(":")[1];

    buffer = buffer.mid(128);

    if(fileType=="attachment"){
        QString fileName = header.split(",")[1].split(":")[1];
        QString ext = fileName.split(".")[1];
        QString usr = header.split(",")[2].split(":")[1];
        QString size = header.split(",")[3].split(":")[1].split(";")[0];
        nameus=usr;

            QString filePath = "~/Рабочий стол/build-QTCPServer-Desktop-Debug";

            QFile file(fileName);
            if(file.open(QIODevice::WriteOnly)){
                file.write(buffer);
                QString message = QString("Сервер >> Файл от " + usr + " получен успешно").arg(socket->socketDescriptor()).arg(QString(filePath));
                emit newMessage(message);

                file.close();

                foreach (QTcpSocket* socket,connection_set)
                        {
                            if (socket != socket1)
                            sendAttachment(socket, fileName);
                        }
            }else
                QMessageBox::critical(this,"QTCPServer", "Ошибка при записи файла");

    }
    else if(fileType=="message"){
        QString message = QString("%1 ").arg(QString::fromStdString(buffer.toStdString()));
        emit newMessage(message);
        temp = message;
        foreach (QTcpSocket* socket,connection_set)
            sendMessage(socket, temp);
    }
}

void MainWindow::discardSocket()
{
    QTcpSocket* socket = reinterpret_cast<QTcpSocket*>(sender());
    QSet<QTcpSocket*>::iterator it = connection_set.find(socket);
    if (it != connection_set.end()){
        connection_set.remove(*it);
    }
    
    socket->deleteLater();
}

void MainWindow::displayError(QAbstractSocket::SocketError socketError)
{
    switch (socketError) {
        case QAbstractSocket::RemoteHostClosedError:
        break;
        case QAbstractSocket::HostNotFoundError:
            QMessageBox::information(this, "QTCPServer", "Сервер не найден");
        break;
        case QAbstractSocket::ConnectionRefusedError:
            QMessageBox::information(this, "QTCPServer", "Соединение отключено узлом");
        break;
        default:
            QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
            QMessageBox::information(this, "QTCPServer", QString("Ошибка: %1.").arg(socket->errorString()));
        break;
    }
}

// Отправка сообщения с севера
void MainWindow::sendMessage(QTcpSocket* socket, QString text)
{
    if(socket)
    {
        if(socket->isOpen())
        {
            QString str = text;

            QDataStream socketStream(socket);
            socketStream.setVersion(QDataStream::Qt_5_15);

            QByteArray header;
            header.prepend(QString("fileType:message,fileName:null,fileSize:%1;").arg(str.size()).toUtf8());
            header.resize(128);

            QByteArray byteArray = str.toUtf8();
            byteArray.prepend(header);

            socketStream.setVersion(QDataStream::Qt_5_15);
            socketStream << byteArray;
        }
        else
            QMessageBox::critical(this,"QTCPServer","Невозможно открыть сокет");
    }
    else
        QMessageBox::critical(this,"QTCPServer","Не подключен");
}

// Отправка вложений с сервера
void MainWindow::sendAttachment(QTcpSocket* socket, QString filePath)
{
    if(socket)
    {
        if(socket->isOpen())
        {
            QFile m_file(filePath);
            if(m_file.open(QIODevice::ReadOnly)){

                QFileInfo fileInfo(m_file.fileName());
                QString fileName(fileInfo.fileName());

                QDataStream socketStream(socket);
                socketStream.setVersion(QDataStream::Qt_5_15);

                QByteArray header;
                header.prepend(QString("fileType:attachment,fileName:%1,name:%2,fileSize:%3;").arg(fileName).arg(nameus).arg(m_file.size()).toUtf8());
                header.resize(128);

                QByteArray byteArray = m_file.readAll();
                byteArray.prepend(header);

                socketStream << byteArray;
            }else
                QMessageBox::critical(this,"QTCPClient","Невозможно открыть файл");
        }
        else
            QMessageBox::critical(this,"QTCPServer","Невозможно открыть сокет");
    }
    else
        QMessageBox::critical(this,"QTCPServer","Не подключен");
}

void MainWindow::displayMessage(const QString& str)
{
    ui->textBrowser_receivedMessages->append(str);
}

