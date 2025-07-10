#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTcpServer>
#include <QTcpSocket>
#include <QLabel>
#include <QVBoxLayout>
#include <QListWidgetItem>
#include <QImage>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QTime>
#include <QDebug>
#include <QTimer>
#include <random>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(int userId, QWidget *parent = nullptr); // Добавляем ID пользователя
    ~MainWindow();

private slots:
    void on_pushButton_clicked();
    void onNewConnection();     // Для сервера: новый клиент подключился
    void onReadyRead();         // Для сервера: данные получены
    void onClientConnected();   // Для клиента: успешное подключение к серверу
    void onClientDisconnected(); // Для клиента: отключение от сервера

private:
    Ui::MainWindow *ui;
    int userId;                 // 1 или 2 (идентификатор пользователя)
    QTcpServer *server;         // Сервер для приёма сообщений
    QTcpSocket *clientSocket;   // Сокет для отправки сообщений
    QTcpSocket *incomingSocket; // Сокет для приёма сообщений

    // Пути к файлам и папкам
    const QString chatFile = "/home/astra/untitled/chat.txt";
    const QString imagesDir = "/home/astra/untitled/images";
    const QString encodedImagesDir = "/home/astra/untitled/encoded_images";

    // Стеганографические методы
    bool encodeMessage(const QString &message, QString &outputPath);
    QString decodeMessage(const QString &imagePath);
    QString textToBits(const QString &text) const;
    QString bitsToText(const QString &bits) const;
    QString getRandomImage() const;

    // Вспомогательные методы
    void initWorkspace();
    void saveToHistory(const QString &message);
    void addMessageToChat(const QString &message);
    QString getCurrentTime() const;
};

#endif // MAINWINDOW_H
