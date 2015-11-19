#ifndef THUMBNAILBOX_HPP
#define THUMBNAILBOX_HPP

#include <cassert>

#include <QDebug>
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
#include <QMenu>
#include <QCache>
#include <QPointer>

namespace ThumbnailBoxComponents { class Thumb; }

class ThumbnailBox : public QFrame
{
    Q_OBJECT

public:

    enum class SourceType
    {
        Local,
        LoaderFunction,
        External
    };

    typedef ThumbnailBoxComponents::Thumb Thumb;

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

    void
    clicked(int index);

    void
    clicked(int index, const QPoint &pos);

    void
    rightClicked(int index);

    void
    rightClicked(int index, const QPoint &pos);

    void
    contextMenuRequested(const QPoint &pos);

    void
    menuItemSelected(QAction *action, const QString &item);

    void
    middleClicked(int index);

    void
    middleClicked(int index, const QPoint &pos);

    void
    imageRequested(const QString &path);

    void
    imageCached(const QString &path = "");

private:

    QPalette
    _original_palette;

    bool
    updating_thumbnails;

    int
    _index;

    QStringList
    _list;

    double
    _size;

    bool
    _showdirs;

    bool
    _isclickable;

    QSize
    _max_cache_pix_dimensions;

    QMap<int, QColor>
    _colors;

    QMap<QString, int>
    _file_colors;

    QCache<QString, QImage>
    _pixcache;

    SourceType
    _source_type;

    QImage
    (*_image_loader_function)(const QString&);

    QList<QAction*>
    _actions;

    QWidget
    *thumbcontainer;

    QHBoxLayout
    *thumbcontainerlayout;

    QPointer<QWidget>
    thumbarea;

    QScrollBar
    *scrollbar;

    QMap<int, QPointer<Thumb>>
    _visible_thumbnails_in_viewport;

    int
    availableWidth() const;

    int
    availableHeight() const;

    int
    columnCount() const;

    int
    rowCount() const;

    int
    topRow() const;

    int
    bottomRow() const;

    double
    thumbSize() const;

    int
    thumbWidth() const;

    QList<int>
    visibleIndexes() const;

    QPointer<Thumb>
    thumbAtIndex(int index) const;

    QColor
    fileColor(const QString &file) const;

    QImage
    cachedImage(const QString &file) const;

    QPixmap
    cachedPixmap(const QString &file) const;

    void
    requestImage(const QString &path);

private slots:

    void
    resizeEvent(QResizeEvent *event);

    void
    wheelEvent(QWheelEvent *event);

    void
    showMenu(int index, const QPoint &pos);

    void
    updateThumbnail(int index);

    void
    updateThumbnail(const QString &file);

public:

    SourceType
    sourceType() const;

    QImage
    shrinkImage(const QImage &original_image) const;

    QStringList
    list() const;

    int
    count() const;

    bool
    isValidIndex(int index) const;

    int
    indexOf(const QString &file) const;

    int
    index() const;

    bool
    isSelected() const;

    QString
    itemPath(int index = -1) const;

    QString
    itemTitle(int index = -1) const;

    bool
    isFirst() const;

    bool
    isLast() const;

    bool
    directoriesVisible() const;

    bool
    itemsClickable() const;

    bool
    isMenuEnabled() const;

public slots:

    void
    setFrame(int style = QFrame::Panel | QFrame::Sunken);

    void
    setBackground(const QColor &color);

    void
    setDarkBackground(bool dark = true);

    void
    setPreviewSizeLimit(int wh);

    void
    setItemsClickable(bool enable);

    void
    setCacheLimit(int max_mb);

    void
    addMenuItem(QAction *action);

    void
    removeMenuItem(QAction *action = 0);

    void
    undefineColors();

    void
    defineColor(const QColor &color, int number = 1);

    void
    clearColors(int number = 0);

    void
    setFileColor(const QString &file, int color = 1);

    void
    setFileColors(const QStringList &files, int color = 1);

    void
    clearCache();

    void
    cacheImage(const QString &file, const QImage &image);

    void
    scrollToRow(int row);

    void
    scrollToTop();

    void
    scrollToBottom();

    void
    updateThumbnails();

    void
    updateThumbnails(int unused);

    void
    scheduleUpdateThumbnails(int timeout = 100);

    void
    setThumbSize(double percent);

    void
    setThumbSize(int percent);

    void
    setThumbWidth(int width);

    void
    select(int index, bool send_signal = true);

    void
    unselect();

    void
    selectPrevious();

    void
    selectNext();

    void
    ensureItemVisible(int index);

    //MOC says:
    //error: no matching function for call to
    //‘ThumbnailBox::setPixmapSource(QPixmap&)’
    //note: no known conversion for argument 1 from ‘QPixmap’ to
    //‘QPixmap (*)(QString)’
    #if !defined(Q_MOC_RUN)
    void
    setImageSource(QImage(*loader)(const QString&));
    #endif

    void
    clear();

    bool
    setList(const QStringList &paths, int selected,
        SourceType type = SourceType::Local);

    bool
    setList(const QStringList &paths, SourceType type = SourceType::Local);

    bool
    setList(const QStringList &paths, const QString &selected,
        SourceType type);

    #if !defined(Q_MOC_RUN)
    bool
    setList(const QStringList &remote_paths, QImage(*loader)(const QString&));
    #endif

};

class ThumbnailBoxComponents::Thumb : public QFrame
{
    Q_OBJECT

signals:

    void
    clicked(int index);

    void
    clicked(int index, const QPoint &pos);

    void
    rightClicked(int index);

    void
    rightClicked(int index, const QPoint &pos);

    void
    contextMenuRequested(const QPoint &pos);

    void
    middleClicked(int index);

    void
    middleClicked(int index, const QPoint &pos);

public:
    
    Thumb(int index, QWidget *parent = 0);

public slots:

    void
    setPixmap(const QPixmap &preview);

    void
    setTitle(const QString &title);

protected:

    void
    mousePressEvent(QMouseEvent *event);

private:

    int
    index;

    QLabel
    *lbl_preview;

    QLabel
    *lbl_title;

};

#endif
