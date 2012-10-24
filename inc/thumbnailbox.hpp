#ifndef THUMBNAILBOX_HPP
#define THUMBNAILBOX_HPP

#include <QFrame>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollBar>
#include <QLabel>
#include <QTimer>
#include <QFileInfo>
#include <QDir>
#include <QMap>
#include <QPixmap>
#include <QMouseEvent>

class Thumb;

class ThumbnailBox : public QFrame
{
    Q_OBJECT

public:

    ThumbnailBox(QWidget *parent);

signals:

    void
    resized();

    void
    selectionChanged();

    void
    itemSelected(int index);

    void
    itemSelected(const QString &path);

    void
    updated();

private:

    bool
    updating_thumbnails;

    int
    _index;

    QFileInfoList
    _list;

    double
    _size;

    QStringList
    _filter;

    bool
    _showdirs;

    bool
    _isclickable;

    QString
    _path;

    QMap<QString, QPixmap>
    _pixcache;

    QPixmap
    (*_pixsource)(QString);

    QWidget
    *thumbcontainer;

    QHBoxLayout
    *thumbcontainerlayout;

    QWidget
    *thumbarea;

    QScrollBar
    *scrollbar;

    QTimer
    *updatetimer;

private slots:

    void
    resizeEvent(QResizeEvent *event);

public:

    QPixmap
    getPixmap(QString file);

    void
    clearCache();

    int
    availableWidth();

    int
    availableHeight();

    QFileInfoList&
    items();

    QStringList
    list();

    int
    count();

    double
    thumbSize();

    int
    thumbWidth();

    int
    index();

    bool
    isSelected();

    QFileInfo
    item(int index = -1);

    QString
    itemPath(int index = -1);

    bool
    isFirst();

    bool
    isLast();

    QString
    path();

    bool
    directoriesVisible();

    bool
    itemsClickable();

public slots:

    void
    updateThumbnails();

    void
    updateThumbnails(int unused);

    void
    scheduleUpdateThumbnails();

    void
    setThumbSize(double percent);

    void
    setThumbSize(int percent);

    void
    setThumbWidth(int width);

    void
    select(int index);

    void
    unselect();

    void
    selectPrevious();

    void
    selectNext();

    void
    setNameFilter(QStringList filter);

    void
    reload();

    bool
    setList(const QStringList &paths);

    bool
    navigateTo(const QString &path);

    bool
    navigate2(QString path); //for cool people only

    void
    setDirectoriesVisible(bool enable);

    void
    setItemsClickable(bool enable);

    //MOC says:
    //error: no matching function for call to
    //‘ThumbnailBox::setPixmapSource(QPixmap&)’
    //note: no known conversion for argument 1 from ‘QPixmap’ to
    //‘QPixmap (*)(QString)’
    #if !defined(Q_MOC_RUN)
    void
    setPixmapSource(QPixmap(*pixsource)(QString));
    #endif

    void
    setPixmapSource();

};

class Thumb : public QFrame
{
    Q_OBJECT

public:
    
    Thumb(QWidget *parent = 0);

    void
    setIndex(int index);

protected:

    void
    mousePressEvent(QMouseEvent *event);

private:

    int
    index;

signals:

    void
    clicked(int index);

};

#endif
