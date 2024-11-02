#ifndef FINDME_HPP
#define FINDME_HPP

// Qt
#include <QPushButton>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QDialog>

class FindMe : public QDialog
{
    Q_OBJECT
private:
    // Vertikalan layout ce biti koriscen
    QVBoxLayout *layout;
    QLineEdit *findLineEdit;
    QPushButton *findButton;

public:
    explicit FindMe(QWidget *parent = nullptr);

    QString findText() const;

signals:
    // U zavisnosti od teksta koji je prosledjen vrsimo pretragu
    void findRequested(const QString &text);

private slots:
    void onFindClicked();
};

#endif // FINDME_HPP
