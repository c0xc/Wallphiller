/*
 * Wallphiller
 * ===========
 *
 * A simple tool that changes your desktop wallpaper.
 * Supports several common desktop environments.
 *
 */

#ifndef WALLPHILLER_HPP
#define WALLPHILLER_HPP

#include <iostream>
#include <cassert>
#include <cstdlib>

#if defined(_WIN32)
#include <windows.h>
#endif
#include <csignal>

#include <QApplication>
#include <QMainWindow>
#include <QSplitter>
#include <QGroupBox>
#include <QComboBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QStringListModel>
#include <QListView>
#include <QTextCodec>
#include <QSettings>
#include <QInputDialog>
#include <QMessageBox>
#include <QFileDialog>
#include <QDebug>
#include <QStatusBar>
#include <QShortcut>
#include <QLabel>
#include <QPainter>
#include <QTimer>
#include <QRadioButton>
#include <QDateTime>
#include <QScrollArea>
#include <QUrl>
#include <QSharedMemory>
#include <QBuffer>
#include <QToolButton>
#include <QPixmap>
#include <QImageReader>
#include <QImageWriter>
#include <QDialogButtonBox>
#include <QProcessEnvironment>
#include <QSystemTrayIcon>
#include <QSpinBox>
#include <QFormLayout>

#include "version.hpp"

#include "settingsdialog.hpp"
#include "thumbnailbox.hpp"
#include "playlist.hpp"

enum class DE
{
    None,
    Gnome,
    Mate,
    KDE,
    Cinnamon,
    Xfce,
    Lxde,
    Windows
};

class SettingsDialog;
class ThumbnailBox;

class Wallphiller : public QMainWindow
{
    Q_OBJECT
    
public:

    Wallphiller();

    ~Wallphiller();

private:

    static Wallphiller
    *instanceptr;

    static void
    sig(int signal);

    QSharedMemory
    shared_memory;

    QTimer
    *tmr_check_shared_memory;

    QLineEdit
    *txt_playlist_title;

    QToolButton
    *btn_playlist;

    ThumbnailBox
    *thumbnailbox;

    QSlider
    *sld_thumb_size;

    QPushButton
    *btn_settings;

    QPushButton
    *btn_tray;

    QPushButton
    *btn_hide;

    QPushButton
    *btn_quit;

    QTimer
    *tmr_next_wallpaper;

    int
    _configured_interval_value;

    QString
    _configured_interval_unit;

    QSystemTrayIcon
    *tray_icon;

    int
    _configured_thumbnail_cache_limit;

    void
    setPlaylistMenu();

    QStringList
    _read_formats;

    Playlist
    *_current_playlist;

    int
    _position;

    QStringList
    _sorted_picture_addresses;

    QPointer<SettingsDialog>
    settings_dialog;

    DE
    _de;

    QString
    _change_routine;

    QString
    _change_routine_command;

private slots:

    void
    closeEvent(QCloseEvent *event);

    void
    dragEnterEvent(QDragEnterEvent *event);

    void
    dropEvent(QDropEvent *event);

    void
    checkAndUpdateMemory();

    void
    playlistNameChanged(const QString &name);

public:

    QStringList
    formatFilters() const;

    Playlist*
    playlist() const;

    int
    cacheLimit() const;

    int
    intervalValue() const;

    QString
    intervalUnit() const;

    int
    interval() const;

    int
    position() const;

    QStringList
    sortedAddresses() const;

    DE
    desktopEnvironment() const;

    QString
    changeRoutine() const;

    QString
    changeRoutineCommand() const;

public slots:

    void
    hideInstance();

    void
    showInstance();

    void
    handleTrayClicked(QSystemTrayIcon::ActivationReason reason);

    void
    minimizeToTray();

    void
    openSettingsWindow();

    void
    applyChangeRoutine(const QString &routine, const QString &command);

    void
    applyInterval(int value, QString unit);

    void
    applyCacheLimit(int max_mb);

    void
    generateList();

    void
    setPlaylist(Playlist *playlist, int start_index = 0);

    void
    createPlaylist(const QList<QUrl> &addresses, bool empty = false);

    void
    createPlaylistWithFullDirectory(bool empty = false);

    void
    createPlaylistWithShallowDirectory(bool empty = false);

    void
    createPlaylistWithFiles(bool empty = false);

    void
    unloadPlaylist();

    void
    setWallpaper(const QString &file_path);

    void
    selectWallpaper(int index);

    void
    previous();

    void
    next();

};

#endif
