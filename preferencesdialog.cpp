
// Qt
#include <QFormLayout>
#include <QComboBox>
#include <QPushButton>
#include <QFontDialog>
#include <QColorDialog>
#include <QKeySequenceEdit>
#include <QHBoxLayout>
#include <QTabWidget>

#include "preferencesdialog.hpp"

PreferencesDialog::PreferencesDialog(QWidget *parent) :
    QDialog(parent)
{
    setupUI();

    bFont = false;
    bColor = false;
}

void PreferencesDialog::setupUI()
{
    tabWidget = new QTabWidget(this);

    setupGeneralTab();
    setupAppearanceTab();
    setupShortcutTab();

    okButton = new QPushButton("OK", this);
    cancelButton = new QPushButton("Cancel", this);
    applyButton = new QPushButton("Apply", this);

    QHBoxLayout *buttonLayout = new QHBoxLayout;
    buttonLayout->addStretch();
    buttonLayout->addWidget(okButton);
    buttonLayout->addWidget(cancelButton);
    buttonLayout->addWidget(applyButton);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(tabWidget);
    mainLayout->addLayout(buttonLayout);

    connect(okButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(applyButton, &QPushButton::clicked, this, &PreferencesDialog::applyChanges);

    this->setLayout(mainLayout);
    this->setWindowTitle("Preferences");
    this->resize(400, 300);
}

void PreferencesDialog::setupGeneralTab()
{
    QWidget *generalTab = new QWidget;
    QFormLayout *layout = new QFormLayout;

    languageComboBox = new QComboBox;
    languageComboBox->addItems({"English", "Serbian"});
    layout->addRow("Language:", languageComboBox);

    generalTab->setLayout(layout);
    tabWidget->addTab(generalTab, "General");
}

void PreferencesDialog::setupAppearanceTab()
{
    QWidget *appearanceTab = new QWidget;
    QFormLayout *layout = new QFormLayout;

    QPushButton *fontButton = new QPushButton("Select Font");
    connect(fontButton, &QPushButton::clicked, this, [this]()
            {
                bool ok;
                bFont = true;
                selectedFont = QFontDialog::getFont(&ok, this);
            });
    layout->addRow("Font:", fontButton);

    QPushButton *colorButton = new QPushButton("Select Color");
    connect(colorButton, &QPushButton::clicked, this, [this]()
            {
                bColor = true;
                selectedColor = QColorDialog::getColor(Qt::white, this);
            });
    layout->addRow("Theme Color:", colorButton);

    appearanceTab->setLayout(layout);
    tabWidget->addTab(appearanceTab, "Appearance");
}

void PreferencesDialog::setupShortcutTab()
{
    QWidget *shortcutsTab = new QWidget;
    QFormLayout *layout = new QFormLayout;

    copyShortcutEdit = new QKeySequenceEdit(QKeySequence(Qt::CTRL + Qt::Key_C), this);
    layout->addRow("Copy:", copyShortcutEdit);

    undoShortcutEdit = new QKeySequenceEdit(QKeySequence(Qt::CTRL + Qt::Key_Z), this);
    layout->addRow("Undo:", undoShortcutEdit);

    shortcutsTab->setLayout(layout);
    tabWidget->addTab(shortcutsTab, "Shortcuts");
}

void PreferencesDialog::applyChanges()
{
    QString selectedLanguage = languageComboBox->currentText();
    emit languageChanged(selectedLanguage);
    emit shortcutChanged("Copy", copyShortcutEdit->keySequence());
    emit shortcutChanged("Undo", undoShortcutEdit->keySequence());
    if (bFont)
    {
            emit fontChanged(selectedFont);
    }
    if (bColor)
    {
        emit themeColorChanged(selectedColor);
    }



}
