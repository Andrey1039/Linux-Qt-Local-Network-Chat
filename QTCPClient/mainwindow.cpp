#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <string>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fstream>
#define BUFFSIZE 4096

using namespace std;

// Подключение к серверу по UDP
QString Udp(){
    socklen_t len;   
    QString ad;
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in serveraddr;
    memset(&serveraddr, 0, sizeof(serveraddr));
    bzero(&serveraddr,sizeof(serveraddr));

    int broadcastEnable=1;
    setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable));

    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(8001);
    serveraddr.sin_addr.s_addr = inet_addr("192.168.1.255");

    string filename = "";
    char buff[BUFFSIZE] = {0};
    strcpy(buff, filename.c_str());
    if (sendto(sockfd, buff, BUFFSIZE, 0, (struct sockaddr *)&serveraddr, sizeof(struct sockaddr)) == -1)
    {
        exit(1);
    }

    recvfrom(sockfd, buff, BUFFSIZE,0,(struct sockaddr *)&serveraddr, &len);

    QString string1(buff);

    close(sockfd);
    return string1;
}

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    QString ip = Udp();

    socket = new QTcpSocket(this);

    connect(this, &MainWindow::newMessage, this, &MainWindow::displayMessage);
    connect(socket, &QTcpSocket::readyRead, this, &MainWindow::readSocket);
    connect(socket, &QTcpSocket::disconnected, this, &MainWindow::discardSocket);
    connect(socket, &QAbstractSocket::errorOccurred, this, &MainWindow::displayError);

    socket->connectToHost(ip,8080);

    if(!socket->waitForConnected())
    {
        QMessageBox::critical(this,"QTCPClient", QString("Ошибка: %1.").arg(socket->errorString()));
        exit(EXIT_FAILURE);
    }
}

MainWindow::~MainWindow()
{
    if(socket->isOpen())
        socket->close();
    delete ui;
}

// Получение данных от сервера
void MainWindow::readSocket()
{
    QByteArray buffer;

    QDataStream socketStream(socket);
    socketStream.setVersion(QDataStream::Qt_5_15);

    socketStream.startTransaction();
    socketStream >> buffer;

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

        QString filePath = QFileDialog::getSaveFileName(this, tr("Save File"), QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)+"/"+fileName, QString("File (*.%1)").arg(ext));

        QFile file(filePath);
        if(file.open(QIODevice::WriteOnly)){
            file.write(buffer);
            QString message = QString("Вы >> получили файл от "+ usr);
            emit newMessage(message);
        }else
            QMessageBox::critical(this,"QTCPServer", "Ошибка при записи файла");
    }
    else if(fileType=="message"){
        QString message = QString("%1 ").arg(QString::fromStdString(buffer.toStdString()));
        emit newMessage(message);
    }
}

void MainWindow::discardSocket()
{
    socket->deleteLater();
    socket=nullptr;
}

void MainWindow::displayError(QAbstractSocket::SocketError socketError)
{
    switch (socketError) {
        case QAbstractSocket::RemoteHostClosedError:
        break;
        case QAbstractSocket::HostNotFoundError:
            QMessageBox::information(this, "QTCPClient", "Сервер не найден");
        break;
        case QAbstractSocket::ConnectionRefusedError:
            QMessageBox::information(this, "QTCPClient", "Соединение разорвано узлом");
        break;
        default:
            QMessageBox::information(this, "QTCPClient", QString("Ошибка: %1.").arg(socket->errorString()));
        break;
    }
}

// Отправка сообщения на сервер
void MainWindow::on_pushButton_sendMessage_clicked()
{
    if(socket)
    {
        if(socket->isOpen())
        {
            QString str = ui-> lineEdit_nik -> text() +" >> " + ui->lineEdit_message->text();

            QDataStream socketStream(socket);
            socketStream.setVersion(QDataStream::Qt_5_15);

            QByteArray header;
            header.prepend(QString("fileType:message,fileName:null,fileSize:%1;").arg(str.size()).toUtf8());
            header.resize(128);

            QByteArray byteArray = str.toUtf8();
            byteArray.prepend(header);

            socketStream << byteArray;

            ui->lineEdit_message->clear();
        }
        else
            QMessageBox::critical(this,"QTCPClient","Невозможно открыть сокет");
    }
    else
        QMessageBox::critical(this,"QTCPClient","Не подключен");
}

// Отправка файла на сервер
void MainWindow::on_pushButton_sendAttachment_clicked()
{
    if(socket)
    {
        if(socket->isOpen())
        {
            QString filePath = QFileDialog::getOpenFileName(this, ("Select an attachment"), QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation), ("File (*.*)"));

            if(filePath.isEmpty()){
                QMessageBox::critical(this,"QTCPClient","Вы не выбрали файл");
                return;
            }

            QFile m_file(filePath);
            if(m_file.open(QIODevice::ReadOnly)){

                QFileInfo fileInfo(m_file.fileName());
                QString fileName(fileInfo.fileName());

                QDataStream socketStream(socket);
                socketStream.setVersion(QDataStream::Qt_5_15);

                QByteArray header;
                header.prepend(QString("fileType:attachment,fileName:%1, name:%2,fileSize:%3;").arg(fileName).arg(ui->lineEdit_nik->text()).arg(m_file.size()).toUtf8());
                header.resize(128);

                QByteArray byteArray = m_file.readAll();
                byteArray.prepend(header);

                socketStream.setVersion(QDataStream::Qt_5_15);
                socketStream << byteArray;

                QString message = QString("Вы >> файл успешно отправлен");
                emit newMessage(message);

            }else
                QMessageBox::critical(this,"QTCPClient","Невозможно прочитать файл");
        }
        else
            QMessageBox::critical(this,"QTCPClient","Невозможно открыть сокет");
    }
    else
        QMessageBox::critical(this,"QTCPClient","Не подключен");
}

void MainWindow::displayMessage(const QString& str)
{
    ui->textBrowser_receivedMessages->append(str);
}
