#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QFileInfo>
#include <QRandomGenerator>
#include <QDateTime>
#include <QMessageBox>
#include <QDir>
#include <QDataStream>
#include <QScrollBar>
#include <QHBoxLayout>

MainWindow::MainWindow(int userId, QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow), userId(userId)
{
    ui->setupUi(this);
    initWorkspace();
    loadHistory();

    server = new QTcpServer(this);
    int port = (userId == 1) ? 12345 : 54321;
    if (!server->listen(QHostAddress::Any, port)) {
        QMessageBox::critical(this, "Ошибка", QString("Не удалось запустить сервер на порту %1").arg(port));
    }

    connect(server, &QTcpServer::newConnection, this, &MainWindow::onNewConnection);

    clientSocket = new QTcpSocket(this);
    connect(clientSocket, &QTcpSocket::connected, this, &MainWindow::onClientConnected);
    connect(clientSocket, &QTcpSocket::disconnected, this, &MainWindow::onClientDisconnected);

    ui->lineEdit->setPlaceholderText("Введите сообщение...");
    connect(ui->pushButton, &QPushButton::clicked, this, &MainWindow::on_pushButton_clicked);
    connect(ui->lineEdit, &QLineEdit::returnPressed, this, &MainWindow::on_pushButton_clicked);
}

MainWindow::~MainWindow()
{
    if (server) {
        server->close();
        delete server;
    }

    if (clientSocket) {
        clientSocket->disconnectFromHost();
        if (clientSocket->state() != QAbstractSocket::UnconnectedState) {
            clientSocket->waitForDisconnected();
        }
        delete clientSocket;
    }

    for (QTcpSocket* socket : socketBlockSizes.keys()) {
        socket->disconnectFromHost();
        socket->deleteLater();
    }

    delete ui;
}

QString MainWindow::getChatHistoryFile() const
{
    return userId == 1 ? chatHistory1 : chatHistory2;
}

void MainWindow::initWorkspace()
{
    QDir().mkpath(imagesDir);
    QDir().mkpath(encodedImagesDir);

    QString historyFile = getChatHistoryFile();
    if (!QFile::exists(historyFile)) {
        QFile file(historyFile);
        file.open(QIODevice::WriteOnly);
        file.close();
    }
}

void MainWindow::loadHistory()
{
    QFile file(getChatHistoryFile());
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        while (!in.atEnd()) {
            QString line = in.readLine();
            if (line.contains("|||")) {
                QStringList parts = line.split("|||");
                if (parts.size() >= 2) {
                    bool isMyMessage = parts[0].contains("Я:");
                    QString messageText = parts[0].replace("Я:", "").replace("Собеседник:", "").trimmed();
                    addMessageToChat(messageText, isMyMessage);
                }
            }
        }
        file.close();
    }
}

void MainWindow::onNewConnection()
{
    QTcpSocket *socket = server->nextPendingConnection();
    connect(socket, &QTcpSocket::readyRead, this, &MainWindow::onReadyRead);
    connect(socket, &QTcpSocket::disconnected, this, &MainWindow::onSocketDisconnected);
    socketBlockSizes[socket] = 0;
}

void MainWindow::onReadyRead()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket || !socketBlockSizes.contains(socket)) return;

    QDataStream in(socket);
    in.setVersion(QDataStream::Qt_5_15);

    quint32 &blockSize = socketBlockSizes[socket];

    while (socket->bytesAvailable() > 0) {
        if (blockSize == 0) {
            if (socket->bytesAvailable() < sizeof(quint32))
                return;
            in >> blockSize;
        }

        if (socket->bytesAvailable() < blockSize)
            return;

        QByteArray imageData;
        in >> imageData;
        blockSize = 0;

        QString tempImagePath = encodedImagesDir + "/received_" +
                              QString::number(QDateTime::currentSecsSinceEpoch()) + ".png";
        QFile file(tempImagePath);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(imageData);
            file.close();

            QString decodedMessage = decodeMessage(tempImagePath);
            if (!decodedMessage.isEmpty()) {
                addMessageToChat(decodedMessage, false);
                saveToHistory("Собеседник: " + decodedMessage + " ||| " + getCurrentTime());
            }
        }
    }
}

void MainWindow::onClientConnected()
{
    qDebug() << "Connected to server!";
}

void MainWindow::onClientDisconnected()
{
    qDebug() << "Disconnected from server!";
}

void MainWindow::onSocketDisconnected()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (socket) {
        socketBlockSizes.remove(socket);
        socket->deleteLater();
    }
}

bool MainWindow::encodeMessage(const QString &message, QString &outputPath)
{
    QString imagePath = getRandomImage();
    if (imagePath.isEmpty()) {
        qDebug() << "No images found in directory";
        return false;
    }

    QImage image(imagePath);
    if (image.isNull()) {
        qDebug() << "Failed to load image";
        return false;
    }

    QString fullMessage = QString::number(message.length()) + '|' + message;
    QString bits = textToBits(fullMessage);

    if (bits.length() > image.width() * image.height() * 3) {
        qDebug() << "Message too large for image";
        return false;
    }

    int bitIndex = 0;
    for (int y = 0; y < image.height() && bitIndex < bits.length(); y++) {
        for (int x = 0; x < image.width() && bitIndex < bits.length(); x++) {
            QRgb pixel = image.pixel(x, y);
            int r = qRed(pixel);
            int g = qGreen(pixel);
            int b = qBlue(pixel);

            if (bitIndex < bits.length()) {
                r = (r & 0xFE) | bits[bitIndex++].digitValue();
            }
            if (bitIndex < bits.length()) {
                g = (g & 0xFE) | bits[bitIndex++].digitValue();
            }
            if (bitIndex < bits.length()) {
                b = (b & 0xFE) | bits[bitIndex++].digitValue();
            }

            image.setPixel(x, y, qRgb(r, g, b));
        }
    }

    outputPath = encodedImagesDir + "/encoded_" + QFileInfo(imagePath).fileName();
    if (!image.save(outputPath)) {
        qDebug() << "Failed to save encoded image";
        return false;
    }

    return true;
}

QString MainWindow::decodeMessage(const QString &imagePath)
{
    QImage image(imagePath);
    if (image.isNull()) {
        qDebug() << "Failed to load image";
        return "";
    }

    QString bits;
    for (int y = 0; y < image.height(); y++) {
        for (int x = 0; x < image.width(); x++) {
            QRgb pixel = image.pixel(x, y);
            bits += QString::number(qRed(pixel) & 1);
            bits += QString::number(qGreen(pixel) & 1);
            bits += QString::number(qBlue(pixel) & 1);
        }
    }

    QString fullMessage = bitsToText(bits);

    int separatorPos = fullMessage.indexOf('|');
    if (separatorPos == -1) {
        qDebug() << "Invalid message format";
        return "";
    }

    bool ok;
    int length = fullMessage.left(separatorPos).toInt(&ok);
    if (!ok || length <= 0 || separatorPos + 1 + length > fullMessage.length()) {
        qDebug() << "Invalid message length";
        return "";
    }

    return fullMessage.mid(separatorPos + 1, length);
}

QString MainWindow::textToBits(const QString &text) const
{
    QString bits;
    QByteArray utf8 = text.toUtf8();
    for (char c : utf8) {
        bits += QString("%1").arg((unsigned char)c, 8, 2, QLatin1Char('0'));
    }
    return bits;
}

QString MainWindow::bitsToText(const QString &bits) const
{
    QByteArray data;
    for (int i = 0; i < bits.length(); i += 8) {
        QString byte = bits.mid(i, 8);
        if (byte.length() < 8) break;
        data.append(static_cast<char>(byte.toInt(nullptr, 2)));
    }
    return QString::fromUtf8(data);
}

QString MainWindow::getRandomImage() const
{
    QDir dir(imagesDir);
    QStringList images = dir.entryList({"*.jpg", "*.png", "*.bmp"}, QDir::Files);

    if (images.isEmpty()) {
        qDebug() << "No images found in" << imagesDir;
        return "";
    }

    return dir.filePath(images[QRandomGenerator::global()->bounded(images.size())]);
}

void MainWindow::addMessageToChat(const QString &message, bool isMyMessage)
{
    QWidget *messageWidget = new QWidget();
    QHBoxLayout *outerLayout = new QHBoxLayout(messageWidget);
    outerLayout->setContentsMargins(5, 2, 5, 2);

    QWidget *contentWidget = new QWidget();
    QVBoxLayout *innerLayout = new QVBoxLayout(contentWidget);
    innerLayout->setContentsMargins(0, 0, 0, 0);
    innerLayout->setSpacing(2);

    QLabel *textLabel = new QLabel(message);
    textLabel->setWordWrap(true);
    textLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    textLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    textLabel->setMaximumWidth(ui->listWidget->width() * 0.7);
    textLabel->setStyleSheet(
        isMyMessage
        ? "QLabel { background: #DCF8C6; border-radius: 10px; padding: 8px; margin-right: 5px; }"
        : "QLabel { background: #E1F3FB; border-radius: 10px; padding: 8px; margin-left: 5px; }"
    );

    QLabel *timeLabel = new QLabel(getCurrentTime() + (isMyMessage ? " • Вы" : " • Собеседник"));
    timeLabel->setStyleSheet("color: #777777; font-size: 9px;");
    timeLabel->setAlignment(isMyMessage ? Qt::AlignRight : Qt::AlignLeft);

    innerLayout->addWidget(textLabel);
    innerLayout->addWidget(timeLabel);

    if (isMyMessage) {
        outerLayout->addStretch();
        outerLayout->addWidget(contentWidget);
    } else {
        outerLayout->addWidget(contentWidget);
        outerLayout->addStretch();
    }

    QListWidgetItem *item = new QListWidgetItem(ui->listWidget);
    item->setSizeHint(contentWidget->sizeHint());
    ui->listWidget->setItemWidget(item, messageWidget);
    ui->listWidget->scrollToBottom();
}

void MainWindow::saveToHistory(const QString &message)
{
    QFile file(getChatHistoryFile());
    if (file.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&file);
        out << message << "\n";
        file.close();
    }
}

QString MainWindow::getCurrentTime() const
{
    return QTime::currentTime().toString("hh:mm");
}

void MainWindow::on_pushButton_clicked()
{
    QString message = ui->lineEdit->text().trimmed();
    if (message.isEmpty()) return;

    QString imagePath;
    if (!encodeMessage(message, imagePath)) {
        QMessageBox::warning(this, "Ошибка", "Не удалось закодировать сообщение");
        return;
    }

    QFile file(imagePath);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "Ошибка", "Не удалось открыть файл изображения");
        return;
    }
    QByteArray imageData = file.readAll();
    file.close();

    int targetPort = (userId == 1) ? 54321 : 12345;
    clientSocket->connectToHost("127.0.0.1", targetPort);

    if (!clientSocket->waitForConnected(1000)) {
        QMessageBox::warning(this, "Ошибка", "Не удалось подключиться к собеседнику");
        return;
    }

    QByteArray block;
    QDataStream out(&block, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_5_15);
    out << quint32(imageData.size());
    out << imageData;

    clientSocket->write(block);
    clientSocket->flush();
    clientSocket->disconnectFromHost();

    addMessageToChat(message, true);
    saveToHistory("Я: " + message + " ||| " + getCurrentTime());
    ui->lineEdit->clear();
}
