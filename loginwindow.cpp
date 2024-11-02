#include "loginwindow.hpp"

// Qt
#include <QCryptographicHash>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QRegularExpression>
#include <QPropertyAnimation>
#include <QGraphicsColorizeEffect>
#include <QParallelAnimationGroup>

LoginWindow::LoginWindow(QWidget *parent)
    : QWidget{parent}
{
    // Postavljanje svih grafickih elemenata
    setupUI();
    // Za trenutni prozor dodeljujemo ime, velicinu
    this->setWindowTitle("Login-Register");
    this->setFixedSize(800,300);

    if (!connectToDatabase())
    {
        QMessageBox::critical(this, "Database error", "Database connection failed");
        return;
    }
}

// Postavljanje grafickih elemenata
void LoginWindow::setupUI()
{
    // Metoda setStyleSheet za graficko ulepsavanje gde se kao argument prosledjuje string
    // Argument je u stvari kod koji lici na CSS koji mozemo izvrsiti u nasem Qt okruzenju
    this->setStyleSheet(
        "QWidget {"
            "background-color: #2e3440;"
            "border-radius: 15px;"
            "font-family: 'Arial', 'sans-serif';"
            "font-size: 12pt;"
            "color: #d8dee9;"
        "}"

        "QLineEdit {"
            "border: 2px solid #4c566a;"
            "border-radius: 10px;"
            "padding: 5px;"
            "font-size: 11pt;"
            "background-color: #3b4252;"
            "color: #eceff4;"

        "}"

        "QLabel {"
            "font-size: 11pt;"
            "color: #d8dee9;"
        "}"

        "QPushButton {"
            "background-color: #5e81ac;"
            "color: #eceff4;"
            "border-radius: 10px;"
            "padding: 10px 10px;"
            "font-size: 11pt;"
        "}"
        "QPushButton:hover {"
            "background-color: #81a1c1;"
        "}"
        "QPushButton:pressed {"
            "background-color: #4c566a;"
        "}"
        );

    // Glavni prikaz prozora za login-register
    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(20);

    // Da bi prikazali sliku potrebno je i za nju kreirati element
    QLabel *imageLabel = new QLabel(this);
    imageLabel->setFixedSize(300,300);
    // Postavljamo sliku u QLabel, tako da imamo ,,glatku" transformaciju i ocuvanje odnosa svih stranica
    // Skalira  QPixmap objekat tako da odgovara velicini QLabel-a
    imageLabel->setPixmap(QPixmap(":/images/images/7176459-removebg-preview.png").scaled(imageLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    imageLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(imageLabel);

    // --------------------------------
    // Forma
    QGridLayout *formLayout = new QGridLayout();
    formLayout->setVerticalSpacing(20);
    formLayout->setHorizontalSpacing(15);

    // Username
    QLabel *userLabel = new QLabel("Username:", this);
    userEdit = new QLineEdit(this);
    formLayout->addWidget(userLabel, 0, 0);
    formLayout->addWidget(userEdit, 0, 1);

    // Password
    QLabel *passwordLabel = new QLabel("Password:", this);
    passwordEdit = new QLineEdit(this);
    passwordEdit->setEchoMode(QLineEdit::Password);
    formLayout->addWidget(passwordLabel, 1,0);
    formLayout->addWidget(passwordEdit, 1, 1);

    // Email i confirmpassword ce biti vidljivi samo tokom registracije
    emailLabel = new QLabel("Email:",this);
    emailEdit = new QLineEdit(this);
    emailLabel->setVisible(false);
    emailEdit->setVisible(false);
    formLayout->addWidget(emailLabel, 2, 0);
    formLayout->addWidget(emailEdit, 2, 1);

    // Confirm password
    confirmPasswordLabel = new QLabel("Confirm Password:",this);
    confirmPasswordEdit = new QLineEdit(this);
    confirmPasswordEdit->setEchoMode(QLineEdit::Password);
    confirmPasswordLabel->setVisible(false);
    confirmPasswordEdit->setVisible(false);
    formLayout->addWidget(confirmPasswordLabel, 3, 0);
    formLayout->addWidget(confirmPasswordEdit, 3, 1);

    // Zelimo da odaljimo sa spacer-om, koji je fleksibilan (dise), za red 4 (koji je poslednji u formi) da bi udaljio dugmice
    formLayout->setRowStretch(4,1);

    // Potrebno je sada napraviti i postaviti dugmice
    loginButton = new QPushButton("Login", this);
    registerButton = new QPushButton("Register", this);
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(10);
    // Ovo je za horizontalni spacer
    buttonLayout->addStretch();
    buttonLayout->addWidget(loginButton);
    buttonLayout->addWidget(registerButton);

    // Na layout dodajemo layout :)
    formLayout->addLayout(buttonLayout, 5, 1, 1, 2); // Dugmici imaju svoj span preko dve kolone

    // Konacno postavljanje formLayout-a na desnu stranu
    mainLayout->addLayout(formLayout);

    // Povezivanje signala i slotova
    connect(loginButton, &QPushButton::clicked, this, &LoginWindow::onLoginClicked);
    connect(registerButton, &QPushButton::clicked, this, &LoginWindow::onRegisterClicked);

}

// Konekcija na nasu bazu podataka (u ovom slucaju lokalna)
bool LoginWindow::connectToDatabase()
{
    // Mozda bi kao poblji primer posluzio konekcija na bazu koja nije lokalna
    // Postoji i stranica https://www.db4free.net/ gde mozemo kreirati besplatno nalog i bazu
    // Jedini problem jeste taj da smo limitirani na velicinu baze, isto tako postoji mogucnost da se posle nekog vremena i sama obrise usled neaktivnosti
    db = QSqlDatabase::addDatabase("QMYSQL");   // U pitanju je mysql baza (unosimo kao argument)
    db.setHostName("localhost");
    db.setDatabaseName("code_version");
    db.setUserName("codeAdmin");    // Bilo bi pametno i kreirati drugog korisnika sa manjim privilegijama, kao sto je ucinjeno ovde
    db.setPassword("#Dqqj0157");

    // U slucaju da nije uspela konekcija na bazu, ispisace se poruka greske
    if (!db.open())
    {
        // .lastError se nalazi u QSqlError biblioteci!
        qDebug() << "Nije uspela konekcija na bazu: " << db.lastError();
        return false;
    }
    return true;
}

// Provera da li korisnik postoji u bazi sa datim username
bool LoginWindow::isUserRegistered(const QString &username)
{
    // Pravimo objekat koji ce biti u stvari nas upit
    QSqlQuery query;

    // .prepare postavlja zeljeni upit iz argumenta, ali ga jos uvek ne izvrsava!
    query.prepare("SELECT id FROM users WHERE username = :username");
    // U upitu se moze naci i :username gde se :username, kasnije zamenjuje pravim
    // .bindValue pronalazi sve vrednosti koje pored svog imena sa leve strane imaju ':'
    query.bindValue(":username", username);

    // Tek sa .exec izvrasavamo nas upit
    // .next pomera internal pointer od query-a na sledeci result set (true ako postoji nesto)
    if (query.exec() && query.next())
    {
        return true;
    }
    return false;
}

// Upisujemo u bazu novog korisnika
bool LoginWindow::registerUser(const QString& username, const QString &email, const QString &password)
{
    if (isUserRegistered(username))
    {
        QMessageBox::warning(this, "Registration Error", "User already exists!");
        return  false;
    }

    // Kada cuvamo podatke u bazi, nije dovoljno zapisati ih recimo odmah sa dobijenim vrednostima
    // Narocito kada je u pitanju password, treba obezbediti korisnika
    // Vrednosti password-a se cuvaju u hash obliku
    // Koristimo standardnu biblioteku: kao prvi unos jeste vrednost koju zelimo da hash-ujemo, a drugi argument je algoritam koji koristimo
    // Mozemo izabrati (MD4, MD5, Sha1, Sha3, Sha3_512...)
    QString hashedPassword = QString(QCryptographicHash::hash(password.toUtf8(), QCryptographicHash::Sha256).toHex());

    QSqlQuery query;
    query.prepare("INSERT INTO users (username, email, password) VALUES (:username, :email, :password)");
    query.bindValue(":username", username);
    query.bindValue(":email", email);
    query.bindValue(":password", hashedPassword);   // Ne obicnu nego koju smo pretvorili u hash

    if (!query.exec())
    {
        qDebug() << "Greska prilikom registracije: " << query.lastError();
        QMessageBox::critical(this, "Registration Error", "Failed to register user!");
        return false;
    }

    QMessageBox::information(this, "Registration Success", "User registered successfully");
    return true;
}

#include "mainwindow.hpp"

// Ispitivanje da li postoji korisnik u bazi i da li je korisnik uneo tacno podatke
bool LoginWindow::loginUser(const QString &username, const QString &password)
{
    QSqlQuery query;
    query.prepare("SELECT password FROM users WHERE username = :username");
    query.bindValue(":username", username);

    if (!query.exec() || !query.next())
    {
        QMessageBox::warning(this, "Login Error", "Invaild username or password");
        return false;
    }

    // .value(0) jeste i kolona koju smo selektovali, tacnije password
    QString storedPassword = query.value(0).toString();

    if (storedPassword == QString(QCryptographicHash::hash(password.toUtf8(), QCryptographicHash::Sha256).toHex()))
    {
        // Ukoliko je sve bilo uspesno otvorice se nas glavni prozor
        MainWindow *mainWindow = new MainWindow();
        mainWindow->show();

        // Prozor koji je bio aktivan zatvaramo
        this->close();
        return true;
    }
    else
    {
        QMessageBox::warning(this, "Login Error", "Invalid Username or Password!");
        return false;
    }
}

// Slot za proveru logovanja
void LoginWindow::onLoginClicked()
{
    QString temp_username = userEdit->text();
    QString temp_password = passwordEdit->text();
    if (loginUser(temp_username, temp_password))
    {
        QMessageBox::information(this, "Login Success", "Logged in successfully!");
    }
}

// Slot za proveru registracije
void LoginWindow::onRegisterClicked()
{
    qInfo() << "Rezultat: ";
    if (emailLabel->isVisible())
    {
        QString temp_username = userEdit->text();
        QString temp_email = emailEdit->text();
        QString temp_password = passwordEdit->text();
        QString temp_confirmPassword = confirmPasswordEdit->text();

        if (temp_password != temp_confirmPassword)
        {
            QMessageBox::warning(this, "Registration Error", "Passwords do not match!");
            animateFieldError(confirmPasswordEdit);
            animateFieldError(passwordEdit);
            return;
        }

        // Email validation
        QRegularExpression emailRegex("^[\\w\\.=-]+@[\\w\\.-]+\\.[\\w]{2,3}$");
        // ^[\\w\\.=-]+ -> email pocinje sa jednim ili vise slova, tacka, jednako ili crtica
        // @[\\w\\.-]+ -> ovo se odnosi na domen, mora imati barem jedno slovo, tacku ili crticu
        // \\.[\\w]{2,3}$ -> osigurava da se email zavrsava sa tackom i nakon nje postoje 2 ili 3 slova kao sufiks
        if (!emailRegex.match(temp_email).hasMatch()) {
            QMessageBox::warning(this, "Registration Error", "Invalid email format.");
            animateFieldError(emailEdit);
            return;
        }

        // Password validation
        if (temp_password.length() < 6) {
            animateFieldError(passwordEdit);
            QMessageBox::warning(this, "Registration Error", "Password must be at least 6 characters long.");
            return;
        }

        if (registerUser(temp_username, temp_email, temp_password))
        {
            // TODO
        }
    }
    else
    {
        emailLabel->setVisible(true);
        emailEdit->setVisible(true);
        confirmPasswordLabel->setVisible(true);
        confirmPasswordEdit->setVisible(true);

    }
}

// Metoda gde vrsimo pomeranje polja lineEdit-a u zavisnosti od toga da li su podaci tacno uneti
void LoginWindow::animateFieldError(QWidget *field) {
    // Animacija - deluje kao da dise
    QPropertyAnimation *pulseAnimation = new QPropertyAnimation(field, "geometry");
    QRect originalGeometry = field->geometry();
    pulseAnimation->setDuration(500);
    pulseAnimation->setStartValue(originalGeometry);
    pulseAnimation->setKeyValueAt(0.5, originalGeometry.adjusted(-5, -5, 5, 5));
    pulseAnimation->setEndValue(originalGeometry);
    pulseAnimation->setEasingCurve(QEasingCurve::InOutQuad);

    // Deo animacije za promenu boje
    QGraphicsColorizeEffect *colorEffect = new QGraphicsColorizeEffect(field);
    field->setGraphicsEffect(colorEffect);
    QPropertyAnimation *colorAnimation = new QPropertyAnimation(colorEffect, "color");
    colorAnimation->setDuration(1000);
    colorAnimation->setStartValue(QColor(Qt::red));
    colorAnimation->setEndValue(QColor(Qt::transparent));

    // Sada je potrebno sve animacije grupisati u jednu celinu
    QParallelAnimationGroup *animationGroup = new QParallelAnimationGroup(field);
    animationGroup->addAnimation(pulseAnimation);
    animationGroup->addAnimation(colorAnimation);
    animationGroup->start(QAbstractAnimation::DeleteWhenStopped);
}


// Metoda za vracanje na podrazumevane postavke register izgleda
void LoginWindow::resetFieldStyles() {
    QList<QWidget *> fields = {userEdit, emailEdit, passwordEdit, confirmPasswordEdit};
    for (QWidget *field : fields) {
        field->setGraphicsEffect(nullptr);
        field->setStyleSheet("QLineEdit {"
                             "  border: 2px solid #cccccc;"
                             "  border-radius: 10px;"
                             "  padding: 8px;"
                             "  font-size: 10pt;"
                             "  background-color: #ffffff;"
                             "}");
    }
}











