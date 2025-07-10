#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QFileInfo>
#include <QRandomGenerator>
#include <QDateTime>

MainWindow::MainWindow(int userId, QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow), userId(userId)
{
    ui->setupUi(this);
    initWorkspace();

    // Настройка сервера (для приёма сообщений)
    server = new QTcpServer(this);
    int port = (userId == 1) ? 12345 : 54321;  // У каждого пользователя свой порт
    if (!server->listen(QHostAddress::Any, port)) {
        qDebug() << "Server could not start!";
    } else {
        qDebug() << "Server started on port" << port;
    }
    connect(server, &QTcpServer::newConnection, this, &MainWindow::onNewConnection);

    // Настройка клиента (для отправки сообщений)
    clientSocket = new QTcpSocket(this);
    connect(clientSocket, &QTcpSocket::connected, this, &MainWindow::onClientConnected);
    connect(clientSocket, &QTcpSocket::disconnected, this, &MainWindow::onClientDisconnected);

    // Настройка интерфейса
    ui->lineEdit->setPlaceholderText("Введите сообщение...");
    connect(ui->pushButton, &QPushButton::clicked, this, &MainWindow::on_pushButton_clicked);
    connect(ui->lineEdit, &QLineEdit::returnPressed, this, &MainWindow::on_pushButton_clicked);
}

MainWindow::~MainWindow()
{
    delete ui;
    if (server) server->close();
    if (clientSocket) clientSocket->close();
    if (incomingSocket) incomingSocket->close();
}

// === Сетевые методы ===

void MainWindow::onNewConnection()
{
    incomingSocket = server->nextPendingConnection();
    connect(incomingSocket, &QTcpSocket::readyRead, this, &MainWindow::onReadyRead);
    connect(incomingSocket, &QTcpSocket::disconnected, incomingSocket, &QTcpSocket::deleteLater);
    qDebug() << "New connection from" << incomingSocket->peerAddress().toString();
}

void MainWindow::onReadyRead() {
    QDataStream in(incomingSocket);
    in.setVersion(QDataStream::Qt_5_15);

    // Если размер данных ещё не известен, читаем его
    static quint32 blockSize = 0;
    if (blockSize == 0) {
        if (incomingSocket->bytesAvailable() < sizeof(quint32))
            return;
        in >> blockSize;
    }

    // Проверяем, что все данные получены
    if (incomingSocket->bytesAvailable() < blockSize)
        return;

    // Читаем само изображение
    QByteArray imageData;
    in >> imageData;
    blockSize = 0;  // Сбрасываем для следующего сообщения

    // Сохраняем изображение
    QString tempImagePath = encodedImagesDir + "/received_" +
                          QString::number(QDateTime::currentSecsSinceEpoch()) + ".png";
    QFile file(tempImagePath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(imageData);
        file.close();

        // Декодируем сообщение
        QString decodedMessage = decodeMessage(tempImagePath);
        if (!decodedMessage.isEmpty()) {
            addMessageToChat("Собеседник: " + decodedMessage);
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

// === Стеганографические методы ===

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

    // Добавляем заголовок с длиной сообщения
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

    // Извлекаем длину сообщения
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

// === Вспомогательные методы ===

void MainWindow::on_pushButton_clicked() {
    QString message = ui->lineEdit->text().trimmed();
    if (message.isEmpty()) return;

    // Кодируем сообщение в изображение
    QString imagePath;
    if (!encodeMessage(message, imagePath)) {
        qDebug() << "Encoding failed";
        return;
    }

    // Читаем изображение как бинарные данные
    QFile file(imagePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Cannot open image file";
        return;
    }
    QByteArray imageData = file.readAll();
    file.close();

    // Подключаемся к серверу другого пользователя
    int targetPort = (userId == 1) ? 54321 : 12345;
    clientSocket->connectToHost("127.0.0.1", targetPort);

    // Ждём подключения (таймаут 1 секунда)
    if (!clientSocket->waitForConnected(1000)) {
        qDebug() << "Connection failed!";
        return;
    }

    // Отправляем РАЗМЕР данных (4 байта) + сами данные
    QByteArray block;
    QDataStream out(&block, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_5_15);
    out << quint32(imageData.size());  // Размер изображения
    out << imageData;                 // Сами данные

    clientSocket->write(block);
    clientSocket->flush();
    clientSocket->disconnectFromHost();

    // Выводим сообщение в чат
    addMessageToChat("Я: " + message);
    ui->lineEdit->clear();
}

void MainWindow::addMessageToChat(const QString &message) {
    // Убираем префиксы из текста
    QString messageText = message;
    messageText.remove("Я: ").remove("Собеседник: ");

    // Создаем виджет сообщения
    QWidget *messageWidget = new QWidget(ui->listWidget);
    QHBoxLayout *outerLayout = new QHBoxLayout(messageWidget);
    outerLayout->setContentsMargins(2, 5, 2, 5);  // Увеличили вертикальные отступы
    // Контейнер для сообщения и времени
    QWidget *contentWidget = new QWidget();
    QVBoxLayout *innerLayout = new QVBoxLayout(contentWidget);
    innerLayout->setContentsMargins(0, 0, 0, 0);
    innerLayout->setSpacing(2);

    // Лейбл для текста
    QLabel *textLabel = new QLabel(messageText);
    textLabel->setWordWrap(true);
    textLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    // Рассчитываем ширину сообщения
    QFontMetrics fm(textLabel->font());
    int textWidth = fm.horizontalAdvance(messageText);
    int maxWidth = ui->listWidget->width() * 0.7;  // Максимум 70% ширины
    int minWidth = ui->listWidget->width() * 0.10; // Минимум 10% ширины
    int optimalWidth = qBound(minWidth, textWidth + 20, maxWidth);

    // Устанавливаем параметры текста
    textLabel->setMinimumWidth(minWidth);
    textLabel->setMaximumWidth(maxWidth);
    textLabel->setFixedWidth(optimalWidth);

    // --- ОСНОВНОЕ ИСПРАВЛЕНИЕ ---
    // Удаляем фиксированную высоту, чтобы QLabel мог растягиваться по содержимому
    textLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
    // ----------------------------

    textLabel->setStyleSheet(
        message.startsWith("Я: ")
            ? "QLabel {"
              "background: #DCF8C6;"
              "border-radius: 10px;"
              "padding: 8px;"
              "min-height: 20px;"  // Гарантированная минимальная высота
              "}"
            : "QLabel {"
              "background: #E1F3FB;"
              "border-radius: 10px;"
              "padding: 8px;"
              "min-height: 20px;"  // Гарантированная минимальная высота
              "}"
    );

    // Лейбл для времени
    QLabel *timeLabel = new QLabel(
        message.startsWith("Я: ")
            ? QString("%1 • Вы").arg(getCurrentTime())
            : QString("%1 • Собеседник").arg(getCurrentTime())
    );
    timeLabel->setStyleSheet("color: #777777; font-size: 9px; margin-top: 2px;");
    timeLabel->setAlignment(
        message.startsWith("Я: ") ? Qt::AlignRight : Qt::AlignLeft
    );

    innerLayout->addWidget(textLabel);
    innerLayout->addWidget(timeLabel);

    // Выравнивание в зависимости от отправителя
    if (message.startsWith("Я: ")) {
        outerLayout->addStretch();  // Прижимаем к правому краю
        outerLayout->addWidget(contentWidget);
    } else {
        outerLayout->addWidget(contentWidget);
        outerLayout->addStretch();  // Прижимаем к левому краю
    }

    // Рассчитываем оптимальный размер
    textLabel->adjustSize();         // Обновляем размер после всех изменений
    contentWidget->adjustSize();
    messageWidget->adjustSize();

    // Создаём элемент списка
    QListWidgetItem *item = new QListWidgetItem(ui->listWidget);
    item->setSizeHint(contentWidget->sizeHint());
    ui->listWidget->setItemWidget(item, messageWidget);

    ui->listWidget->scrollToBottom();
}

void MainWindow::saveToHistory(const QString &message)
{
    QFile file(chatFile);
    if (file.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&file);
        out << "user|||" << message << "|||" << getCurrentTime() << "\n";
        file.close();
    }
}

QString MainWindow::getCurrentTime() const
{
    return QTime::currentTime().toString("hh:mm");
}

void MainWindow::initWorkspace()
{
    QDir().mkpath(imagesDir);
    QDir().mkpath(encodedImagesDir);

    if (!QFile::exists(chatFile)) {
        QFile file(chatFile);
        if (file.open(QIODevice::WriteOnly)) {
            file.close();
        }
    }
}
