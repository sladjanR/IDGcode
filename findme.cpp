#include "findme.hpp"

FindMe::FindMe(QWidget *parent) :
    QDialog{parent},
    findLineEdit(new QLineEdit(this)),
    findButton(new QPushButton("Find",this))
{
    layout = new QVBoxLayout(this);
    layout->addWidget(findLineEdit);
    layout->addWidget(findButton);

    setLayout(layout);
    setWindowFilePath("Find in text");

    // Povezemo u konstruktoru signal i slot
    connect(findButton, &QPushButton::clicked, this, &FindMe::onFindClicked);
}

QString FindMe::findText() const
{
    return findLineEdit->text();
}

void FindMe::onFindClicked()
{
    emit findRequested(findText());
    accept();
}
