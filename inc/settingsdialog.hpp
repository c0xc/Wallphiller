#ifndef SETTINGSDIALOG_HPP
#define SETTINGSDIALOG_HPP

#include "wallphiller.hpp"

class Wallphiller;

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:

    SettingsDialog(QWidget *parent = 0);

private:

    Wallphiller
    *wallphiller;

    QRadioButton
    *opt_auto;

    QRadioButton
    *opt_command;

    QLineEdit
    *txt_command;

    QDialogButtonBox
    *button_box;

    QString
    new_routine;

    QString
    new_command;

    QSpinBox
    *txt_cache_limit;

    int
    new_cache_limit;

    QSpinBox
    *txt_interval_value;

    int
    new_interval_value;

    QComboBox
    *cmb_interval_unit;

    QString
    new_interval_unit;

private slots:

    void
    enableRoutineAuto(bool checked = true);

    void
    enableRoutineCommand(bool checked = true);

    void
    checkRoutineCommand();

    void
    checkIntervalValue(int value);

    void
    checkIntervalUnit(int index);

    void
    checkCacheLimit(int limit);

    void
    saveAndClose();

    void
    cancel();

};

#endif
