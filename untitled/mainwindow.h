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
    explicit MainWindow(int userId, QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_pushButton_clicked();
    void onNewConnection();
    void onReadyRead();
    void onClientConnected();
    void onClientDisconnected();

private:
    Ui::MainWindow *ui;
    int userId;
    QTcpServer *server;
    QTcpSocket *clientSocket;
    QTcpSocket *incomingSocket;

    const QString chatFile = "/home/astra/untitled/chat.txt";
    const QString imagesDir = "/home/astra/untitled/images";
    const QString encodedImagesDir = "/home/astra/untitled/encoded_images";

    bool encodeMessage(const QString &message, QString &outputPath);
    QString decodeMessage(const QString &imagePath);
    QString textToBits(const QString &text) const;
    QString bitsToText(const QString &bits) const;
    QString getRandomImage() const;

    void initWorkspace();
    void saveToHistory(const QString &message);
    void addMessageToChat(const QString &message);
    QString getCurrentTime() const;
};

#endif // MAINWINDOW_H
