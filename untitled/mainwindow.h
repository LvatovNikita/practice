#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTcpServer>
#include <QTcpSocket>
#include <QHash>
#include <QImage>
#include <QListWidgetItem>
#include <QLabel>
#include <QVBoxLayout>

namespace Ui {
class MainWindow;
}

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
    void onSocketDisconnected();

private:
    Ui::MainWindow *ui;
    int userId;
    QTcpServer *server;
    QTcpSocket *clientSocket;
    QHash<QTcpSocket*, quint32> socketBlockSizes;

    const QString chatHistory1 = "/home/astra/untitled/chat_history_user1.txt";
    const QString chatHistory2 = "/home/astra/untitled/chat_history_user2.txt";
    const QString imagesDir = "/home/astra/untitled/images";
    const QString encodedImagesDir = "/home/astra/untitled/encoded_images";

    bool encodeMessage(const QString &message, QString &outputPath);
    QString decodeMessage(const QString &imagePath);
    QString textToBits(const QString &text) const;
    QString bitsToText(const QString &bits) const;
    QString getRandomImage() const;

    void initWorkspace();
    void saveToHistory(const QString &message);
    void loadHistory();
    void addMessageToChat(const QString &message, bool isMyMessage);
    QString getCurrentTime() const;
    QString getChatHistoryFile() const;
};

#endif // MAINWINDOW_H
