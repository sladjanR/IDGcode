#ifndef PREFERENCESDIALOG_HPP
#define PREFERENCESDIALOG_HPP

#include <QDialog>
#include <QColor>
#include <QFont>
#include <QKeySequence>

class QTabWidget;
class QComboBox;
class QKeySequenceEdit;

class PreferencesDialog : public QDialog
{
    Q_OBJECT

public:
    PreferencesDialog(QWidget *parent = nullptr);

signals:
    void languageChanged(const QString &language);
    void fontChanged(const QFont &font);
    void themeColorChanged(const QColor &color);
    void shortcutChanged(const QString &action, const QKeySequence &shortcut);

private slots:
    void applyChanges();

private:
    void setupUI();
    void setupGeneralTab();
    void setupAppearanceTab();
    void setupShortcutTab();

    QTabWidget *tabWidget;
    QPushButton *okButton, *cancelButton, *applyButton;

    bool bFont;
    bool bColor;
    QComboBox *languageComboBox;
    QFont selectedFont;
    QColor selectedColor;
    QKeySequenceEdit *copyShortcutEdit;
    QKeySequenceEdit *undoShortcutEdit;
};

#endif // PREFERENCESDIALOG_HPP
