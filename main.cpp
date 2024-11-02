#include "loginwindow.hpp"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    LoginWindow loginRegister;
    loginRegister.show();

    return a.exec();
}
