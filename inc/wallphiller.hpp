#ifndef WALLPHILLER_HPP
#define WALLPHILLER_HPP

#include <iostream>

#if defined(_WIN32)
#include <windows.h>
#endif

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

#include <csignal>

#include "version.hpp"

#include "thumbnailbox.hpp"

class ThumbnailBox;

class PreviewLabel : public QLabel
{
    Q_OBJECT

public:

    PreviewLabel(QWidget *parent = 0);

private slots:

    void
    resizeEvent(QResizeEvent *event);

signals:

    void
    resized();

};

class Wallphiller : public QMainWindow
{
    Q_OBJECT
    
public:

    static QString
    wallpaperSetterInfo();

    static void
    setWallpaper(QString file);

    Wallphiller();

    static Wallphiller*
    instanceptr;

private:

    static void
    sig(int signal);

    bool
    _is2ndinstance;

    QGroupBox
    *preview_box;

    QList<QWidget*>
    preview_widgets;

    bool
    _updating_preview_box;

    QTimer
    *preview_update_timer;

    QPushButton
    *btn_pause;

    QPushButton
    *btn_next;

    QPushButton
    *btn_previous;

    QSplitter
    *splitter;

    QScrollArea
    *scrollarea;

    QGroupBox
    *playlist_box;

    QComboBox
    *cmb_playlist;

    QPushButton
    *btn_addlist;

    QPushButton
    *btn_renamelist;

    QPushButton
    *btn_removelist;

    QPushButton
    *btn_activatelist;

    QGroupBox
    *contents_box;

    QStringListModel
    *contents_model;

    QListView
    *contents_list;

    QPushButton
    *btn_addcnt;

    QPushButton
    *btn_addfolder;

    QPushButton
    *btn_removecnt;

    QGroupBox
    *thumbnails_box;

    QLineEdit
    *txt_pathbox;

    ThumbnailBox
    *thumbnailbox;

    QSlider
    *thumbslider;

    QPushButton
    *btn_addfile;

    QGroupBox
    *settings_box;

    QLineEdit
    *txt_intval;

    QComboBox
    *cmb_intval;

    QGroupBox
    *config_box;

    QRadioButton
    *rad_auto;

    QRadioButton
    *rad_command;

    QLabel
    *lbl_autoinfo;

    QLineEdit
    *txt_command;

    QTimer
    *tmr_instance;

    QTimer
    *timer;

    QMap<QString, QStringList>
    _files_cache;

    QString
    _active;

    int
    _interval;

    qint64
    _last;

    bool
    _paused;

    QStringList
    playlists();

    void
    setPlaylists(QStringList lists);

    QStringList
    nameFilter();

    QString
    encodeName(QString name = "");

    QString
    decodeName(QString name = "");

    QString
    activeList();

    bool
    isListActive(QString name = "");

    void
    setListActive(QString name = "");

    QString
    currentList();

    qint64
    lastChange();

    void
    setLastChange(qint64 time);

    bool
    paused();

    void
    setPaused(bool paused);

    QVariant
    playlistSetting(QString name, QString setting);

    void
    setPlaylistSetting(QString name, QString setting, QVariant value);

    QStringList
    fileList(QString name = "");

    void
    resetFileListCache(QString name);

    void
    resetFileListCache();

    int
    position(QString name = "");

    int
    position(QString name, int position);

    int
    position(int position);

    void
    setPosition(QString name, int position);

    void
    setPosition(int position);

    QString
    fileAt(int position);

    int
    interval(QString name);

    int
    intervalNumber(QString name);

    char
    intervalUnit(QString name);

    void
    setInterval(QString name, int interval);

private slots:

    void
    changeEvent(QEvent *event);

    void
    closeEvent(QCloseEvent *event);

    void
    dragEnterEvent(QDragEnterEvent *event);

    void
    dropEvent(QDropEvent *event);

    void
    checkInstance();

    void
    refresh();

    void
    updatePreviewBox();

    void
    scheduleUpdatePreviewBox();

    void
    updateComboBox(QString selected = "");

    void
    selectList();

    void
    selectList(int index);

    void
    selectList(const QString &list);

    void
    addList();

    void
    renameList();

    void
    removeList();

    void
    activateList();

    void
    resetContentsBox();

    void
    addContent(QString content);

    void
    addContent();

    void
    addFolder();

    void
    removeContent();

    void
    navigateToSelected(const QModelIndex &index);

    void
    updateThumbButtons();

    void
    addThumbFile();

    void
    saveInterval();

    void
    saveInterval(int index);

    void
    toggleMode();

    void
    toggleMode(bool checked);

public slots:

    void
    hideInstance();

    void
    showInstance();

    void
    setCurrentWallpaper();

    void
    select(int index);

    void
    previous();

    void
    pause();

    void
    next();

    void
    update();
};

#endif
