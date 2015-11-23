#include "settingsdialog.hpp"

SettingsDialog::SettingsDialog(QWidget *parent)
              : QDialog(parent)
{
    wallphiller = qobject_cast<Wallphiller*>(parent);

    //Layout
    QVBoxLayout *vbox_main = new QVBoxLayout;
    setLayout(vbox_main);
    setWindowTitle(tr("Settings"));

    //Selection frame
    QGroupBox *grp_routine = new QGroupBox(tr("Wallpaper change routine"));
    vbox_main->addWidget(grp_routine);
    QVBoxLayout *vbox_routine = new QVBoxLayout;
    grp_routine->setLayout(vbox_routine);

    //Radio buttons
    QString auto_title = tr("Auto");
    QString auto_title_info;
    switch (wallphiller->desktopEnvironment())
    {
        case DE::Gnome:
        auto_title_info = "Gnome";
        break;

        case DE::Mate:
        auto_title_info = "Mate";
        break;

        case DE::Cinnamon:
        auto_title_info = "Cinnamon";
        break;

        case DE::KDE:
        auto_title_info = "KDE";
        break;

        case DE::XFCE:
        auto_title_info = "XFCE";
        break;

        case DE::LXDE:
        auto_title_info = "LXDE";
        break;

        case DE::Windows:
        auto_title_info = "Windows";
        break;

        case DE::None:
        auto_title_info = tr("not detected");
        break;
    }
    if (!auto_title_info.isEmpty())
    {
        auto_title += " (" + auto_title_info + ")";
    }
    opt_auto = new QRadioButton(auto_title);
    vbox_routine->addWidget(opt_auto);
    opt_command = new QRadioButton(tr("Custom command"));
    vbox_routine->addWidget(opt_command);

    //Radio button actions
    connect(opt_auto,
            SIGNAL(toggled(bool)),
            SLOT(enableRoutineAuto(bool)));
    connect(opt_command,
            SIGNAL(toggled(bool)),
            SLOT(enableRoutineCommand(bool)));

    //Custom command field
    txt_command = new QLineEdit;
    txt_command->setPlaceholderText(tr("command --option %f"));
    txt_command->setToolTip(tr(
        "Enter a custom command that will be run to change your wallpaper. "
        "%f will be replaced with a local file path, "
        "%u will be replaced with the path formatted as url - both quoted."));
    txt_command->setEnabled(false);
    vbox_routine->addWidget(txt_command);
    QString saved_command = wallphiller->changeRoutineCommand();
    connect(txt_command,
            SIGNAL(editingFinished()),
            SLOT(checkRoutineCommand()));
    txt_command->setText(saved_command);

    //Saved routine
    QString saved_routine = wallphiller->changeRoutine();
    if (saved_routine == "command")
    {
        opt_command->setChecked(true);
    }
    else
    {
        opt_auto->setChecked(true);
    }

    //Interval frame
    QGroupBox *grp_interval = new QGroupBox(tr("Interval"));
    vbox_main->addWidget(grp_interval);
    QFormLayout *interval_form_layout = new QFormLayout;
    grp_interval->setLayout(interval_form_layout);

    //Interval field
    QHBoxLayout *hbox_interval = new QHBoxLayout;
    interval_form_layout->addRow(tr("Interval"), hbox_interval);
    txt_interval_value = new QSpinBox;
    txt_interval_value->setMinimum(0);
    txt_interval_value->setMaximum(999);
    new_interval_value = wallphiller->intervalValue();
    txt_interval_value->setValue(new_interval_value);
    hbox_interval->addWidget(txt_interval_value);
    connect(txt_interval_value,
            SIGNAL(valueChanged(int)),
            SLOT(checkIntervalValue(int)));
    cmb_interval_unit = new QComboBox;
    cmb_interval_unit->addItem(tr("startup/manual"), "ONCE");
    cmb_interval_unit->addItem(tr("minutes"), "MINUTES");
    cmb_interval_unit->addItem(tr("hours"), "HOURS");
    cmb_interval_unit->addItem(tr("New York minutes"), "NYMINUTES");
    cmb_interval_unit->addItem(tr("nanocenturies"), "NANOCENTURIES");
    cmb_interval_unit->setToolTip(tr(
        "Define how often your wallpaper should be changed automatically. "
        "Select \"startup\" to only have it changed once "
        "on program startup."));
    cmb_interval_unit->setCurrentIndex(
        cmb_interval_unit->findData(wallphiller->intervalUnit()));
    hbox_interval->addWidget(cmb_interval_unit);
    connect(cmb_interval_unit,
            SIGNAL(currentIndexChanged(int)),
            SLOT(checkIntervalUnit(int)));
    checkIntervalUnit(cmb_interval_unit->currentIndex());

    //Cache frame
    QGroupBox *grp_cache = new QGroupBox(tr("Cache"));
    vbox_main->addWidget(grp_cache);
    QFormLayout *cache_layout = new QFormLayout;
    grp_cache->setLayout(cache_layout);

    //Cache limit
    txt_cache_limit = new QSpinBox;
    txt_cache_limit->setMinimum(1);
    txt_cache_limit->setMaximum(10 * 1024); //10 GB limit
    new_cache_limit = wallphiller->cacheLimit(); //limit in MB
    txt_cache_limit->setValue(new_cache_limit);
    txt_cache_limit->setToolTip(tr(
        "Enter the total amount of memory (in MB) that may be used "
        "for the previews. At least 10 MB are recommended. "
        "A low value will save some memory while the program is running."));
    cache_layout->addRow(tr("Cache limit (MB)"), txt_cache_limit);
    connect(txt_cache_limit,
            SIGNAL(valueChanged(int)),
            SLOT(checkCacheLimit(int)));

    //Horizontal line
    QFrame *hline = new QFrame;
    hline->setFrameShape(QFrame::HLine);
    vbox_main->addWidget(hline);

    //Buttons
    button_box = new QDialogButtonBox;
    vbox_main->addWidget(button_box);
    QPushButton *btn_ok = button_box->addButton(QDialogButtonBox::Ok);
    connect(btn_ok, SIGNAL(clicked()), SLOT(saveAndClose()));
    QPushButton *btn_cancel =
        button_box->addButton(QDialogButtonBox::Cancel);
    connect(btn_cancel, SIGNAL(clicked()), SLOT(cancel()));

    //Quit on close (don't keep running invisibly by default)
    setAttribute(Qt::WA_DeleteOnClose); //quit on close

}

void
SettingsDialog::enableRoutineAuto(bool checked)
{
    new_routine = "auto";
    txt_command->setEnabled(false);
}

void
SettingsDialog::enableRoutineCommand(bool checked)
{
    //Show annoying warning to scare the user
    if (checked && QMessageBox::warning(this,
        tr("Custom change command selected"),
        tr(
        "You have chosen to use a custom change command. "
        "This command will be executed everytime the wallpaper is changed. "
        "So be careful what you type in there."),
        QMessageBox::Ok | QMessageBox::Cancel,
        QMessageBox::Ok) != QMessageBox::Ok)
    {
        //We scared the user, got back to automatic
        QTimer::singleShot(0, opt_auto, SLOT(click()));
        return;
    }

    new_routine = "command";
    txt_command->setEnabled(true);

}

void
SettingsDialog::checkRoutineCommand()
{
    new_command = txt_command->text();
}

void
SettingsDialog::checkIntervalValue(int value)
{
    new_interval_value = value;
}

void
SettingsDialog::checkIntervalUnit(int index)
{
    QString selected_unit = cmb_interval_unit->itemData(index).toString();
    bool is_once = selected_unit == "ONCE";
    txt_interval_value->setEnabled(!is_once);
    if (is_once) txt_interval_value->setValue(0);
    new_interval_unit = selected_unit;
}

void
SettingsDialog::checkCacheLimit(int limit)
{
    new_cache_limit = limit;
}

void
SettingsDialog::saveAndClose()
{
    //Check settings
    if (new_routine == "command")
    {
        //Custom command specified

        //Command must not be empty
        if (new_command.isEmpty())
        {
            QMessageBox::critical(this,
                tr("Custom command incomplete"),
                tr(
                "You've specified an empty custom change command."));
            return;
        }

        //Command should at least contain the file argument
        //We don't even check if there's a typo like "-f %f-o...",
        //we just want to make sure that the user didn't forget the file.
        //This is not the right place to do sanity checks
        //because the user could still modify the config file.
        if (!new_command.contains("%f") && !new_command.contains("%u"))
        {
            QMessageBox::critical(this,
                tr("Custom command incomplete"),
                tr(
                "You've specified a custom change command, "
                "but you seem to have forgotten the file argument (%f)."));
            return;
        }
    }

    //Save and close
    wallphiller->applyChangeRoutine(new_routine, new_command);
    wallphiller->applyInterval(new_interval_value, new_interval_unit);
    wallphiller->applyCacheLimit(new_cache_limit);
    close();
}

void
SettingsDialog::cancel()
{
    close();
}

