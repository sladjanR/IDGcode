#ifndef LOGINWINDOW_HPP
#define LOGINWINDOW_HPP

#include <QSqlDatabase>
#include <QWidget>

// Qt
class QLineEdit;
class QPushButton;
class QLabel;

class LoginWindow : public QWidget
{
    Q_OBJECT
private:
    // Ne mozemo forward deklaraciju jer u pitanju nije pokazivac za QSqlDatabase
    QSqlDatabase db;

    QLineEdit *userEdit;
    QLineEdit *passwordEdit;
    QLineEdit *emailEdit;
    QLineEdit *confirmPasswordEdit;
    QPushButton *loginButton;
    QPushButton *registerButton;
    QLabel *emailLabel;
    QLabel *confirmPasswordLabel;


public:
    explicit LoginWindow(QWidget *parent = nullptr);

signals:

private slots:
    void onLoginClicked();
    void onRegisterClicked();

private:
    void setupUI();
    bool connectToDatabase();
    bool isUserRegistered(const QString &username);
    bool registerUser(const QString &username, const QString &email, const QString &password);
    bool loginUser(const QString &username, const QString &password);
    void animateFieldError(QWidget *field);
    void resetFieldStyles();
};

#endif // LOGINWINDOW_HPP
