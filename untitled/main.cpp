#include "mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // Создаём два окна для двух пользователей
    MainWindow user1(1);  // Пользователь 1 (сервер на порту 12345)
    MainWindow user2(2);  // Пользователь 2 (сервер на порту 54321)

    user1.setWindowTitle("Чат (Пользователь 1)");
    user2.setWindowTitle("Чат (Пользователь 2)");

    user1.show();
    user2.show();

    return a.exec();
}
