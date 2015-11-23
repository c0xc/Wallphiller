#include "thumbnailbox.hpp"

/*! \class ThumbnailBox
 *
 * \brief ThumbnailBox is a Qt widget that shows thumbnails of files.
 *
 * A list of (image) files can be provided, see setList().
 * This list is essentially a list of address strings.
 * Different image source types can be defined.
 * The default type is Local, which means previews are loaded directly
 * by this module. This works for local image files only.
 * Every address must be a local file path.
 * If this type is not used, this module will never attempt to open
 * an image file directly because other types allow remote sources.
 *
 * A loader function (reference) can be provided (type LoaderFunction).
 * In this case, this function is called whenever a preview is needed.
 * The function must take the image address and return a QImage object.
 * The function should probably use a cache because an instant response
 * is expected.
 *
 * If External is used as source type, the parent module (or something else)
 * is responsible for loading the images.
 * This ThumbnailBox object will merely emit a signal (carrying the address)
 * whenever an image is needed (imageRequested()).
 * The parent module is expected to catch this signal, load the image
 * and send it to the cacheImage() slot.
 * It can be loaded in the background to prevent the gui from freezing.
 *
 * As long as any type other than Local is used,
 * image addresses could be remote urls.
 *
 * Loaded previews are cached.
 *
 * Loaded previews may be shrunk to save memory.
 *
 * It's very important that a ThumbnailBox object will never load
 * all images in the list in memory at the same time, except for small lists.
 * Even if hundreds of images are in the list, the number of thumbnail
 * widgets will be just enough to fill the viewport properly.
 * The cache is limited too, so that Jon Doe can scroll through his
 * collection of 20k high-def pictures without the program
 * using a Firefox-typical amount of memory.
 *
 */

ThumbnailBox::ThumbnailBox(QWidget *parent)
            : QFrame(parent),
              updating_thumbnails(false),
              _index(-1),
              _size(.3),
              _showdirs(false),
              _isclickable(true),
              _max_cache_pix_dimensions(200, 200),
              _pixcache(500 * 1024), //500 KB
              _source_type(SourceType::Local),
              _image_loader_function(0)
{
    //Copy original palette (may be changed, see setDarkBackground())
    _original_palette = palette();

    //Main layout
    QHBoxLayout *hbox;
    hbox = new QHBoxLayout;
    setLayout(hbox);

    //Thumbnail container widget
    thumbcontainer = new QWidget;
    thumbcontainerlayout = new QHBoxLayout;
    thumbcontainerlayout->setContentsMargins(QMargins());
    thumbcontainerlayout->setSpacing(2);
    //This is where the thumbnail area will be...
    thumbcontainer->setLayout(thumbcontainerlayout);
    hbox->addWidget(thumbcontainer);

    //Scrollbar
    scrollbar = new QScrollBar(Qt::Vertical);
    scrollbar->setTracking(true);
    scrollbar->setMinimum(0);
    scrollbar->setMaximum(0);
    connect(scrollbar,
            SIGNAL(valueChanged(int)),
            SLOT(updateThumbnails(int)));
    hbox->addWidget(scrollbar);

    //Update view on resize
    connect(this, SIGNAL(resized()), SLOT(updateThumbnails()));

    //Scroll to item when selected
    connect(this, SIGNAL(itemSelected(int)), SLOT(ensureItemVisible(int)));

    //Context menu
    connect(this,
            SIGNAL(rightClicked(int, const QPoint&)),
            SLOT(showMenu(int, const QPoint&)));

}

int
ThumbnailBox::availableWidth()
const
{
    return thumbcontainer->width();
}

int
ThumbnailBox::availableHeight()
const
{
    return thumbcontainer->height();
}

int
ThumbnailBox::columnCount()
const
{
    int cols;
    int padding = 5;
    int availablewidth = availableWidth();
    int thumbsize = thumbWidth();
    int thumbwidth = thumbsize;
    cols = availablewidth / (thumbwidth + padding); //2.9 -> 2
    return cols;
}

int
ThumbnailBox::rowCount()
const
{
    int rows;
    int padding = 5;
    int availableheight = availableHeight();
    int thumbsize = thumbWidth();
    int thumbheight = thumbsize;
    rows = availableheight / (thumbheight + padding); //2.9 -> 2
    return rows;
}

int
ThumbnailBox::topRow()
const
{
    int scrollpos = scrollbar->value();
    return scrollpos;
}

int
ThumbnailBox::bottomRow()
const
{
    return topRow() + (rowCount() - 1);
}

double
ThumbnailBox::thumbSize()
const
{
    return _size;
}

int
ThumbnailBox::thumbWidth()
const
{
    int availablewidth = availableWidth();
    int width = availablewidth;
    width *= thumbSize();
    if (width < 30) width = 30; //should be a const

    return width;
}

QList<int>
ThumbnailBox::visibleIndexes()
const
{
    return _visible_thumbnails_in_viewport.keys();
}

QPointer<ThumbnailBox::Thumb>
ThumbnailBox::thumbAtIndex(int index)
const
{
    QPointer<Thumb> thumb; //nullpointer
    if (_visible_thumbnails_in_viewport.contains(index))
        thumb = _visible_thumbnails_in_viewport.value(index);
    return thumb;
}

QColor
ThumbnailBox::fileColor(const QString &file)
const
{
    QColor color;
    if (_file_colors.contains(file))
    {
        int number = _file_colors[file];
        if (_colors.contains(number)) color = _colors[number];
    }
    return color;
}

QImage
ThumbnailBox::cachedImage(const QString &file)
const
{
    //Get cached image or create empty image if not cached
    //Cached image (heap) is (shallow) copied locally first (stack)
    //Cached image could be deleted at any point (managed by cache)
    QImage image;
    if (_pixcache.contains(file))
        image = QImage(*_pixcache.object(file)); //local shallow copy

    return image;
}

QPixmap
ThumbnailBox::cachedPixmap(const QString &file)
const
{
    //Get cached image
    QImage image = cachedImage(file);

    //Convert to QPixmap (if found, empty otherwise)
    QPixmap pixmap;
    if (!image.isNull())
        pixmap.convertFromImage(image);

    return pixmap;
}

void
ThumbnailBox::requestImage(const QString &path)
{
    //Request image (identified by path)
    //Response -> compress -> cacheImage() -> thumbnail drawn
    //If the compressed image doesn't fit in the cache,
    //it will not be displayed. Again, it will not be displayed.
    //This might be due to an undersized cache or oversized preview limit.
    //At the time of writing, the default preview limit is 200x200 px
    //and the cache limit is 500 KB, tested images are 150-200 KB in size.

    QImage image;
    switch (sourceType())
    {
        case SourceType::Local:
        //Load image directly from file (path points to file)
        //TODO run in background, non-blocking!
        image.load(path);
        cacheImage(path, image);
        break;

        case SourceType::LoaderFunction:
        //Call external function which returns QImage
        //Loader should have a cache or else this will block the gui
        if (_image_loader_function)
            image = _image_loader_function(path);
        cacheImage(path, image);
        break;

        case SourceType::External:
        //Request image from external loader (path is uri)
        //Response will be sent to cacheImage() by parent module
        //This is async by design
        emit imageRequested(path);
        break;

    }

}

void
ThumbnailBox::resizeEvent(QResizeEvent *event)
{
    if (event->oldSize().isValid())
        emit resized();

    QFrame::resizeEvent(event);
}

void
ThumbnailBox::wheelEvent(QWheelEvent *event)
{
    int degrees = event->delta() / 8;
    int steps = degrees / 15;

    if (event->orientation() != Qt::Vertical) return;
    if (!scrollbar->isEnabled()) return;

    scrollbar->setValue(scrollbar->value() - steps);

    event->accept();
}

void
ThumbnailBox::showMenu(int index, const QPoint &pos)
{
    if (!isMenuEnabled()) return;

    QString item = itemPath(index);
    if (item.isEmpty()) return;

    QString title = itemTitle(index);

    QMenu menu;
    QAction *action = new QAction(title, &menu);
    QFont font = action->font();
    font.setBold(true);
    action->setFont(font);
    action->setEnabled(false);
    menu.addAction(action);
    foreach (QAction *action, _actions)
    {
        menu.addAction(action);
    }

    emit menuItemSelected(menu.exec(pos), item);
}

void
ThumbnailBox::updateThumbnail(int index)
{
    //Check if thumbnail visible
    if (!visibleIndexes().contains(index))
        return; //it's not

    //Get thumbnail
    QPointer<Thumb> thumb = thumbAtIndex(index);

    //Get preview
    QString path = itemPath(index); //path, uri
    QPixmap cached_pixmap = cachedPixmap(path);

    //Redraw thumbnail
    thumb->setPixmap(cached_pixmap);

}

void
ThumbnailBox::updateThumbnail(const QString &file)
{
    int index = indexOf(file);
    updateThumbnail(index);
}

/*!
 * Returns the type which defines how image previews are loaded.
 * They will be loaded by this class if this type is set to Local (default).
 */
ThumbnailBox::SourceType
ThumbnailBox::sourceType()
const
{
    return _source_type;
}

/*!
 * Shrinks a copy of original_image if it's bigger
 * than the preview size limit.
 */
QImage
ThumbnailBox::shrinkImage(const QImage &original_image)
const
{
    //Configured maximum size (dimensions)
    QSize max_size(_max_cache_pix_dimensions);

    //Original size
    QSize original_size = original_image.size();

    //Resize image (if necessary)
    QImage image(original_image); //shallow copy
    if (max_size.isValid())
    {
        if (original_size.width() > max_size.width() ||
            original_size.height() > max_size.height())
        {
            //Image is bigger, shrink it
            image = image.scaled(max_size, Qt::KeepAspectRatio);
        }
    }

    return image;
}

/*!
 * Returns the current list of file addresses.
 */
QStringList
ThumbnailBox::list()
const
{
    return _list;
}

/*!
 * Returns the total number of thumbnails.
 */
int
ThumbnailBox::count()
const
{
    return list().size();
}

/*!
 * Returns whether index is a valid thumbnail index.
 */
bool
ThumbnailBox::isValidIndex(int index)
const
{
    return (index >= 0 && index < count());
}

/*!
 * Returns the index of this file.
 * Returns -1 if not found.
 */
int
ThumbnailBox::indexOf(const QString &file)
const
{
    return list().indexOf(file);
}

/*!
 * Returns the index of the selected thumbnail.
 * If no thumbnail is selected, -1 is returned.
 */
int
ThumbnailBox::index()
const
{
    int index = _index;
    if (index < 0) index = -1;
    if (index >= count()) index = -1;
    return index;
}

/*!
 * Returns true if a thumbnail is selected or false otherwise.
 */
bool
ThumbnailBox::isSelected()
const
{
    return (index() != -1);
}

/*!
 * Returns the item address of the thumbnail at given index.
 * If source type Local is used, this will be a local file path.
 */
QString
ThumbnailBox::itemPath(int index)
const
{
    QString path;
    if (index == -1) index = this->index();
    if (isValidIndex(index)) path = list().at(index);
    return path;
}

/*!
 * Returns the title that is shown on the thumbnail.
 * By default, this is the file name, which is extracted from the address.
 * It will be the full address if the file name can't be extracted.
 */
QString
ThumbnailBox::itemTitle(int index)
const
{
    QString path = itemPath(index); //might not exist or look weird
    QString title = QFileInfo(path).baseName();
    if (title.isEmpty())
        title = path; //use full address rather than empty title
    return title;
}

/*!
 * Returns true if the first thumbnail is currently selected.
 */
bool
ThumbnailBox::isFirst()
const
{
    return (index() == 0);
}

/*!
 * Returns true if the last thumbnail is currently selected.
 */
bool
ThumbnailBox::isLast()
const
{
    return (index() == count() - 1);
}

/*!
 * Returns true if directories are shown as thumbnails or false otherwise.
 */
bool
ThumbnailBox::directoriesVisible()
const
{
    return _showdirs;
}

/*!
 * Returns true if thumbnails are clickable or false otherwise.
 * Clickable thumbnails can be selected by clicking on them,
 * this triggers a signal.
 */
bool
ThumbnailBox::itemsClickable()
const
{
    return _isclickable;
}

/*!
 * Returns whether the thumbnail context menu is configured.
 */
bool
ThumbnailBox::isMenuEnabled()
const
{
    return _actions.size();
}

/*!
 * Sets the frame style.
 */
void
ThumbnailBox::setFrame(int style)
{
    setFrameStyle(style);
}

/*!
 * Sets the background color to color.
 */
void
ThumbnailBox::setBackground(const QColor &color)
{
    QPalette palette = this->palette();
    palette.setColor(QPalette::Background, color);
    setAutoFillBackground(true);
    setPalette(palette);
}

/*!
 * Sets a dark background if dark is true,
 * resets the default background otherwise.
 */
void
ThumbnailBox::setDarkBackground(bool dark)
{
    if (dark)
    {
        setBackground(Qt::darkGray);
    }
    else
    {
        setBackground(_original_palette.color(QPalette::Background));
    }
}

/*!
 * Sets the maximum dimensions of the cached image previews.
 * Both width and height will be set to wh.
 * Bigger images will be shrunk to save memory.
 */
void
ThumbnailBox::setPreviewSizeLimit(int wh)
{
    if (wh < 0) wh = 0;
    _max_cache_pix_dimensions.setWidth(wh);
    _max_cache_pix_dimensions.setHeight(wh);
}

/*!
 * Sets thumbnails to be clickable or not.
 * Clickable thumbnails can be selected manually.
 */
void
ThumbnailBox::setItemsClickable(bool enable)
{
    _isclickable = enable;

    updateThumbnails();
}

/*!
 * Sets the cache limit in MB.
 * Cached images will be dropped when this limit is exceeded.
 * Unless another cache is used, this limit should be increased
 * to reduce the number of image requests and improve performance.
 *
 * Images that don't fit in the cache will be dropped and not be displayed.
 */
void
ThumbnailBox::setCacheLimit(int max_mb)
{
    if (max_mb < 0) max_mb = 1; //need cache, enforce minimum size of 1 MB
    int max_bytes = max_mb * 1024 * 1024;
    _pixcache.setMaxCost(max_bytes);
}

/*!
 * Adds action to the thumbnail context menu.
 * The ownership of action is not transferred.
 * Adding the first action will automatically create the menu.
 */
void
ThumbnailBox::addMenuItem(QAction *action)
{
    if (!action) return;
    if (!_actions.contains(action))
    {
        _actions << action;
    }
}

/*!
 * Removes given action from the thumbnail context menu.
 * Removing the only action will disable the menu.
 */
void
ThumbnailBox::removeMenuItem(QAction *action)
{
    if (action) _actions.removeAll(action);
    else _actions.clear();
}

/*!
 * Removes previously defined custom thumbnail colors.
 */
void
ThumbnailBox::undefineColors()
{
    _colors.clear();
}

/*!
 * Defines a custom thumbnail color, identified by number, to be used later.
 * This will not change the color of any thumbnail.
 * The identifier number must not be 0.
 */
void
ThumbnailBox::defineColor(const QColor &color, int number)
{
    if (!number) return;
    _colors[number] = color;
}

/*!
 * Resets the thumbnail color of all thumbnails
 * using the pre-defined color number.
 * If number is omitted, all thumbnails are reset.
 */
void
ThumbnailBox::clearColors(int number)
{
    if (!number)
    {
        _file_colors.clear();
    }
    else
    {
        foreach (QString file, _file_colors.keys(number))
        {
            _file_colors.remove(file);
        }
    }
}

/*!
 * Sets the thumbnail color of file to the pre-defined color color.
 */
void
ThumbnailBox::setFileColor(const QString &file, int color)
{
    if (!color) _file_colors.remove(file);
    else _file_colors[file] = color;
}

/*!
 * Resets all thumbnails set to this color and
 * sets the given list of thumbnails to the same color instead.
 */
void
ThumbnailBox::setFileColors(const QStringList &files, int color)
{
    clearColors(color);
    foreach (QString file, files)
    {
        setFileColor(file, color);
    }
}

/*!
 * Deletes all cached images.
 * This will not cause thumbnails to be redrawn immediately.
 */
void
ThumbnailBox::clearCache()
{
    _pixcache.clear();
}

/*!
 * Receives and caches the image for the given file.
 * The thumbnail is then redrawn to display this new image.
 *
 * A small version of this image is cached, which may cause
 * older cache items to be dropped if the limit is exceeded.
 */
void
ThumbnailBox::cacheImage(const QString &file, const QImage &image)
{
    //Put copy of QImage object (on heap) in cache (then managed by cache)
    //Image is shrunk before its cached (original one likely exceeds limit)
    QImage compressed_image = shrinkImage(image);
    QImage *cached_image = new QImage(compressed_image); //on heap!
    int size = cached_image->byteCount(); //size in bytes of compressed image
    _pixcache.insert(file, cached_image, size); //ownership goes to cache
    if (!_pixcache.contains(file))
        return; //not cached (too big?), stop
    emit imageCached(file);

    //Draw image on thumbnail widget (if thumbnail visible)
    //Updating the whole thumbnail area would be overkill
    updateThumbnail(file);

}

/*!
 * Scrolls to row.
 */
void
ThumbnailBox::scrollToRow(int row)
{
    scrollbar->setValue(row);
}

/*!
 * Scrolls to the top.
 */
void
ThumbnailBox::scrollToTop()
{
    scrollbar->setValue(0);
}

/*!
 * Scrolls to the bottom.
 */
void
ThumbnailBox::scrollToBottom()
{
    scrollbar->setValue(scrollbar->maximum());
}

/*!
 * Updates and redraws the thumbnails.
 */
void
ThumbnailBox::updateThumbnails()
{
    //Prepare number of thumbnails and related information
    int count = this->count(); //total number of files
    int thumbwidth;
    int thumbheight;
    int padding = 5;
    int cols;
    int rows;
    int totalrows; //row count (incl. hidden)
    int totalhiddenrows;
    int maxthumbs; //maximum visible thumbs
    int visiblethumbs; //visible thumbs needed
    int scrollpos;
    int hiddenrows; //rows hidden ABOVE viewport
    int hiddenthumbs; //thumbs hidden ABOVE viewport

    //Prevent update when disabled (loading)
    if (!isEnabled()) return;

    //Prevent second call
    if (updating_thumbnails) return;
    updating_thumbnails = true;

    //Dimensions
    int thumbsize = thumbWidth();
    thumbwidth = thumbsize;
    if (thumbwidth < 30) thumbwidth = 30;
    thumbheight = thumbwidth;

    //Numbers
    cols = columnCount(); //2.9 -> 2
    rows = rowCount(); //2.9 -> 2
    if (!cols) cols = 1;
    if (!rows) rows = 1;
    maxthumbs = cols * rows;
    visiblethumbs = count;
    if (visiblethumbs > maxthumbs) visiblethumbs = maxthumbs;
    totalrows = count / cols; if (count % cols) totalrows++; //2.1 -> 3
    totalhiddenrows = totalrows - rows;
    if (totalhiddenrows < 0) totalhiddenrows = 0;

    //Recreational activities (what)
    //Everytime an update is triggered,
    //the thumbnail area widget is recreated
    //and filled with the visible thumbs, which are also recreated.
    //This is important for large directories (> 1000 items),
    //because creating 1000 thumbs is inefficient/stupid/sigsegv.

    //Clear list of visible thumbnails (recreated below)
    _visible_thumbnails_in_viewport.clear();

    //Recreate thumbnail area
    //The 2013 easter egg:
    //if (thumbarea) delete thumbarea; //SIGSEGV (we found an easter egg)
    if (thumbarea)
    {
        thumbarea->hide();
        thumbarea->deleteLater();
    }
    thumbarea = new QWidget;
    thumbcontainerlayout->insertWidget(0, thumbarea);
    thumbarea->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    scrollbar->setPageStep(rows);
    QVBoxLayout *vbox_rows = new QVBoxLayout;
    thumbarea->setLayout(vbox_rows);

    //Scrollbar position
    if (totalhiddenrows >= 0) scrollbar->setMaximum(totalhiddenrows);
    scrollpos = scrollbar->value();
    hiddenrows = scrollpos;
    hiddenthumbs = hiddenrows * cols;

    //Create thumbnails
    for (int i = 0; i < rows; i++)
    {
        //Row
        QHBoxLayout *hbox_row = new QHBoxLayout;
        hbox_row->setSpacing(padding);
        for (int j = 0; j < cols; j++)
        {
            //Column
            int relindex = i * cols + j;
            int absindex = relindex + hiddenthumbs;
            if (absindex >= count) break; //no more thumbs, row not filled
            int index = this->index();

            //Item title
            QString title = itemTitle(absindex);

            //Create item thumbnail object
            Thumb *thumb = new Thumb(absindex);
            thumb->setEnabled(itemsClickable());
            connect(thumb,
                    SIGNAL(clicked(int)),
                    SLOT(select(int)));
            thumb->setFixedSize(QSize(thumbwidth, thumbheight));
            thumb->setFrameStyle(QFrame::Panel | QFrame::Raised);
            if (absindex == index) thumb->setFrameShadow(QFrame::Sunken);
            thumb->setLineWidth(3);
            thumb->setToolTip(title);
            QColor clr_bg = fileColor(itemPath(absindex));
            if (clr_bg.isValid())
            {
                thumb->setAutoFillBackground(true);
                QPalette palette = thumb->palette();
                palette.setColor(QPalette::Window, clr_bg);
                thumb->setPalette(palette);
            }
            hbox_row->addWidget(thumb);

            //Add to list of visible thumbnails
            _visible_thumbnails_in_viewport[absindex] = thumb;

            //Set title
            thumb->setTitle(title);

            //Load image (if available)
            QString path = itemPath(absindex); //path, uri
            QPixmap cached_pixmap = cachedPixmap(path);
            if (!cached_pixmap.isNull())
            {
                //Got it, draw it
                thumb->setPixmap(cached_pixmap); //from internal cache
            }
            else
            {
                //Not cached, request it
                //It will be drawn later
                //Request should be processed in background (ideally)
                requestImage(path);
            }

            //Connect thumbnail signals
            connect(thumb,
                    SIGNAL(clicked(int)),
                    SIGNAL(clicked(int)));
            connect(thumb,
                    SIGNAL(clicked(int, const QPoint&)),
                    SIGNAL(clicked(int, const QPoint&)));
            connect(thumb,
                    SIGNAL(rightClicked(int)),
                    SIGNAL(rightClicked(int)));
            connect(thumb,
                    SIGNAL(rightClicked(int, const QPoint&)),
                    SIGNAL(rightClicked(int, const QPoint&)));
            connect(thumb,
                    SIGNAL(contextMenuRequested(const QPoint&)),
                    SIGNAL(contextMenuRequested(const QPoint&)));
            connect(thumb,
                    SIGNAL(middleClicked(int)),
                    SIGNAL(middleClicked(int)));
            connect(thumb,
                    SIGNAL(middleClicked(int, const QPoint&)),
                    SIGNAL(middleClicked(int, const QPoint&)));

        }

        //Add row to layout
        hbox_row->addStretch(1);
        vbox_rows->addLayout(hbox_row);
    }
    vbox_rows->addStretch(1);

    //Let the world know
    emit updated();

    //Done
    updating_thumbnails = false;
}

/*!
 * This is a convenience function.
 */
void
ThumbnailBox::updateThumbnails(int unused)
{
    updateThumbnails();
}

/*!
 * This is a convenience function.
 */
void
ThumbnailBox::scheduleUpdateThumbnails(int timeout)
{
    if (timeout < 0) timeout = 0;
    QTimer::singleShot(timeout, this, SLOT(updateThumbnails()));
}

/*!
 * Sets the size of the thumbnails, relative to the available space.
 * The provided percentage should be a value between 0 and 1.
 */
void
ThumbnailBox::setThumbSize(double percent)
{
    if (percent < 0) percent = 0;
    if (percent > 1) percent = 1;
    _size = percent;

    updateThumbnails();

    setMinimumHeight(thumbWidth() * 1.5);
}

/*!
 * This is a convenience function.
 * The percentage must be an integer between 1 and 100.
 */
void
ThumbnailBox::setThumbSize(int percent)
{
    if (percent < 1) percent = 1; //minimum 1%
    if (percent >= 100) percent = 100; //maximum 100%

    setThumbSize((double)percent / 100);
}

/*!
 * Sets the absolute width of the thumbnails.
 */
void
ThumbnailBox::setThumbWidth(int width)
{
    double percent = 0;
    int availwidth = availableWidth();
    if (availwidth) percent = width / availwidth;
    setThumbSize(percent);
}

/*!
 * Selects the thumbnail at the given index.
 * This will emit the itemSelected() signal unless send_signal is false.
 */
void
ThumbnailBox::select(int index, bool send_signal)
{
    if (!isEnabled()) return;

    if (!isValidIndex(index)) index = -1;
    if (index == this->index()) return; //don't re-select selected item
    _index = index;

    //The 2013 easter egg:
    //We realize that closing the "File not found" message box
    //after clicking on a non-existent thumbnail causes a SIGSEGV.
    //So after one day of debugging, we trace the issue down to this
    //function, where we found the following comment looking in our face:
    ////updateThumbnails(); //Segfault for no apparent reason
    //scheduleUpdateThumbnails();
    //Courtesy of: 29bb83de2f97485b6e02ad9974833b6c151984e0
    //Dated to: Fri Oct 26 21:55:44 2012 +0200
    //That's 5 months old now!
    //!@#$ !!! If something causes a crash, don't just put a timer in there,
    //fix that **** !
    //After increasing the timeout, it appears as if the crash would
    //only occur if a message box is closed *after* the update has finished.
    //So what does this mean?
    //A thumbnail is clicked (QMouseEvent in the backtrace),
    //this function schedules an update,
    //another module shows a message box (which blocks that module),
    //the update runs/finishes (recreating the view),
    //the message box is closed and SIGSEGV.
    //(_ZN11QMetaObject8activateEP7QObjectPKS_iPPv+0x69)
    //All of this points to a signal/slot connection, featuring a pointer
    //to some object in the view (which is recreated, remember?),
    //which is held back while the message box is shown.
    //Interestingly, the crash occurs within this function:
    //Thumb::mousePressEvent(QMouseEvent *event)
    //This function emits signals... Signals that belong to thumbnails...
    //Thumbnails that have been DELETEd by the update function!!!
    //Happy easter everyone!
    updateThumbnails();

    emit selectionChanged();
    if (index != -1 && send_signal)
    {
        emit itemSelected(index);
        emit itemSelected(itemPath(index));
    }
}

/*!
 * Removes the selection.
 * This will also trigger the selectionChanged() signal.
 */
void
ThumbnailBox::unselect()
{
    select(-1);
}

/*!
 * Selects the previous item, if possible.
 */
void
ThumbnailBox::selectPrevious()
{
    if (!isFirst()) select(index() - 1);
}

/*!
 * Selects the next item, if possible.
 */
void
ThumbnailBox::selectNext()
{
    if (!isLast()) select(index() + 1);
}

/*!
 * Ensures that the thumbnail at given index is visible
 * by scrolling to that position.
 */
void
ThumbnailBox::ensureItemVisible(int index)
{
    //Scroll to item, if out of viewport

    if (index < 0 || index >= count()) return;
    int col_count = columnCount();
    int row_item = (double)index / (double)col_count;
    int row_top = topRow();
    int row_bottom = bottomRow();

    if (row_item < row_top)
    {
        //Item above viewport
        //Make item row 2nd from top?
        scrollToRow(row_item);
    }
    else if (row_item > row_bottom)
    {
        //Item below viewport
        //Make item row 2nd from bottom?
        scrollToRow(row_item);
    }
}

/*!
 * Defines a loader function that will be used to load the images.
 * This also sets the loader source type accordingly.
 */
void
ThumbnailBox::setImageSource(QImage(*loader)(const QString&))
{
    _image_loader_function = loader;
    _source_type = SourceType::LoaderFunction;
}

/*!
 * Clears the ThumbnailBox.
 *
 * This will not clear the cache, which has to be cleared separately
 * if it might be obsolete.
 */
void
ThumbnailBox::clear()
{
    //Reset scrollbar (silently)
    bool was_enabled = isEnabled();
    setEnabled(false);
    scrollToTop(); //won't trigger update because disabled
    setEnabled(was_enabled);

    //Reset position
    _index = -1;

    //Clear list
    QStringList &list = _list;
    list.clear();

    //Cache not cleared by default, could be reused

    //Update view (unless disabled)
    updateThumbnails();

    //Notify listeners about new index (-1)
    emit selectionChanged();

}

/*!
 * Fills the ThumbnailBox with the given list of thumbnails.
 * If the Local source type is used, they will be treated as local files.
 *
 * A pre-selected thumbnail can be defined without triggering
 * the itemSelected() signal.
 */
bool
ThumbnailBox::setList(const QStringList &paths, int selected, SourceType type)
{
    //Clear list
    clear();
    if (paths.isEmpty()) return false;

    //Apply source type
    _source_type = type;

    //Disable temporarily
    setEnabled(false);

    //Set pre-defined index or -1
    _index = selected;

    //Check and add provided paths to list of thumbnails
    QStringList &list = _list;
    foreach (QString path, paths)
    {
        if (type == SourceType::Local)
        {
            QFileInfo inf(path);
            if ((!inf.isFile()) &&
                (!inf.isDir() || !directoriesVisible()))
            {
                continue; //not found, ignore invalid entry
            }
            path = inf.absoluteFilePath(); //full local path
        }
        list << path;
    }

    //Re-enable
    setEnabled(true);

    //Draw thumbnails
    scheduleUpdateThumbnails(0);

    //Emit selection signal
    emit selectionChanged();

    return true;
}

/*!
 * This is a convenience function.
 */
bool
ThumbnailBox::setList(const QStringList &paths, SourceType type)
{
    return setList(paths, -1, type);
}

/*!
 * This is a convenience function.
 * The pre-selected thumbnail can be defined using its path.
 */
bool
ThumbnailBox::setList(const QStringList &paths, const QString &selected,
SourceType type)
{
    return setList(paths, paths.indexOf(selected), type);
}

/*!
 * Fills the ThumbnailBox with the given list of remote thumbnails.
 * If the Local source type is used, they will be treated as local files.
 *
 * A pre-selected thumbnail can be defined without triggering
 * the itemSelected() signal.
 */
bool
ThumbnailBox::setList(const QStringList &remote_paths,
                            QImage(*loader)(const QString&))
{
    //Load pictures through remote interface (loader function)
    //A path could be a local file path or pretty much anyhing else.
    //It's not our job to use it directly, we just pass it to the loader.

    //Clear list
    clear();
    if (remote_paths.isEmpty()) return false;

    //Apply source type and loader function
    _source_type = SourceType::LoaderFunction;
    _image_loader_function = loader;

    //Set list
    QStringList &list = _list;
    list = remote_paths;

    //Re-enable
    setEnabled(true);

    //Draw thumbnails
    scheduleUpdateThumbnails(0);

    //Emit selection signal
    emit selectionChanged();

    return true;
}

ThumbnailBoxComponents::Thumb::Thumb(int index, QWidget *parent)
                      : QFrame(parent),
                        index(index)
{
    //Create layout
    QVBoxLayout *vbox = new QVBoxLayout;
    lbl_preview = new QLabel;
    lbl_preview->setScaledContents(true);
    vbox->addWidget(lbl_preview);
    lbl_title = new QLabel;
    lbl_title->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    vbox->addWidget(lbl_title);
    setLayout(vbox);

}

void
ThumbnailBoxComponents::Thumb::setPixmap(const QPixmap &preview)
{
    lbl_preview->setPixmap(preview);
}

void
ThumbnailBoxComponents::Thumb::setTitle(const QString &title)
{
    lbl_title->setText(title);
}

void
ThumbnailBoxComponents::Thumb::mousePressEvent(QMouseEvent *event)
{
    //The 2013 easter egg:
    //It (SIGSEGV) is triggered by those signals,
    //because the widgets have been deleted by the update function.

    if (event->button() == Qt::LeftButton)
    {
        emit clicked(index);
        emit clicked(index, event->globalPos());
    }
    else if (event->button() == Qt::RightButton)
    {
        emit rightClicked(index);
        emit rightClicked(index, event->globalPos());
        emit contextMenuRequested(event->globalPos());
    }
    else if (event->button() == Qt::MidButton)
    {
        emit middleClicked(index);
        emit middleClicked(index, event->globalPos());
    }
    event->accept();
}

