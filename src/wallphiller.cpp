#define DEFINE_GLOBALS
#include "wallphiller.hpp"



//---



PreviewLabel::PreviewLabel(QWidget *parent)
            : QLabel(parent)
{
}

void
PreviewLabel::resizeEvent(QResizeEvent *event)
{
    emit resized();
}



//---



QString
Wallphiller::wallpaperSetterInfo()
{
    QString text;
    const QString desktop_session = getenv("DESKTOP_SESSION");

    #if defined(_WIN32)

    text = "[WINDOWS]";

    #else

    if (desktop_session == "gnome")
    {
        text = "[GNOME]";
    }
    else if (desktop_session == "kde-plasma")
    {
        text = "[KDE]";
        text += " ?";
    }
    else if (desktop_session == "fluxbox")
    {
        text = "[FLUXBOX]";
        text += " [fbsetbg]";
    }

    #endif

    if (text.isEmpty()) text = "?";

    return text;
}

void
Wallphiller::setWallpaper(QString file)
{
    if (!QFile::exists(file)) return; //Important (prevents system() abuse)
    const QString desktop_session = getenv("DESKTOP_SESSION");

    QString cmd;

    #if defined(_WIN32)

    //Convert to BMP

    QString bmpfile = QFileInfo(QSettings().fileName()).dir().
        filePath("Wallpaper.bmp");
    if (QImage(file).save(bmpfile))
    {
        SystemParametersInfo(SPI_SETDESKWALLPAPER,
            0,
            const_cast<char*>(bmpfile.toStdString().c_str()),
            SPIF_UPDATEINIFILE | SPIF_SENDCHANGE);
    }

    #else

    if (desktop_session == "gnome")
    {
        cmd = QString("gsettings set "
        "org.gnome.desktop.background "
        "picture-uri \"file://%1\"").arg(file);
        system(cmd.toStdString().c_str());
        cmd = QString("gsettings set "
        "org.gnome.desktop.background "
        "picture-options \"stretched\"");
        system(cmd.toStdString().c_str());
    }
    else if (desktop_session == "kde-plasma")
    {
        //?
    }
    else if (desktop_session == "fluxbox")
    {
        cmd = QString("fbsetbg \"%1\"").arg(file);
        system(cmd.toStdString().c_str());
    }

    #endif

}



//---



Wallphiller* Wallphiller::instanceptr = 0;

Wallphiller::Wallphiller()
           : _is2ndinstance(false),
             _updating_preview_box(false),
             _active(' '),
             _interval(-1),
             _last(0),
             _paused(false)
{
    Wallphiller::instanceptr = this;

    QTextCodec::setCodecForCStrings(QTextCodec::codecForName("utf8"));

    QCoreApplication::setOrganizationName(PROGRAM); //~/.config/Wallphiller
    QCoreApplication::setApplicationName(PROGRAM);
    QCoreApplication::setApplicationVersion(GITVERSION);
    QSettings::setDefaultFormat(QSettings::IniFormat);

    if (QSettings().contains("PID2ndInstance")) //2nd instance?
    {
        if (QMessageBox::critical(this,
            tr("Multiple instances"),
            tr("Something is wrong. Another instance is (was) running as %1."
               " Also, a second instance has been started, "
               "but it failed to close properly."
               " Do you want to override?").
               arg(QSettings().value("PID").toString()),
            QMessageBox::Ok | QMessageBox::Cancel) != QMessageBox::Ok)
        {
            _is2ndinstance = true;
            QTimer::singleShot(0, this, SLOT(close()));
            return;
        }
        QSettings().remove("PID2ndInstance");
    }
    else if (QSettings().contains("PID")) //another instance is running
    {
        std::cout << tr("An instance is already running with PID %1.").
                     arg(QSettings().value("PID").toString()).
                     toStdString() << std::endl;
        QSettings().setValue("PID2ndInstance", true);
        _is2ndinstance = true;
        hide();
        QTimer::singleShot(0, this, SLOT(close()));
        return;
    }
    QSettings().setValue("PID", QApplication::applicationPid());

    //GUI

    QVBoxLayout *vbox;
    QHBoxLayout *hbox;
    QFrame *hline;

    //Preview

    preview_box = new QGroupBox;
    preview_update_timer = new QTimer(this);
    preview_update_timer->stop();
    connect(preview_update_timer,
            SIGNAL(timeout()),
            SLOT(updatePreviewBox()));
    vbox = new QVBoxLayout;
    hbox = new QHBoxLayout;
    QLabel *previewlabel;
    for (int i = 0; i < 5; i++)
    {
        previewlabel = new PreviewLabel;
        connect(previewlabel,
                SIGNAL(resized()),
                SLOT(scheduleUpdatePreviewBox()));
        previewlabel->setFrameStyle(QFrame::Panel | QFrame::Raised);
        previewlabel->setLineWidth(3);
        preview_widgets << previewlabel;
        hbox->addWidget(previewlabel);
    }
    vbox->addLayout(hbox);
    hbox = new QHBoxLayout;
    btn_previous = new QPushButton(tr("Previous"));
    connect(btn_previous, SIGNAL(clicked()), SLOT(previous()));
    hbox->addWidget(btn_previous);
    btn_pause = new QPushButton(tr("Pause"));
    connect(btn_pause, SIGNAL(clicked()), SLOT(pause()));
    hbox->addWidget(btn_pause);
    btn_next = new QPushButton(tr("Next"));
    connect(btn_next, SIGNAL(clicked()), SLOT(next()));
    hbox->addWidget(btn_next);
    vbox->addLayout(hbox);
    preview_box->setLayout(vbox);

    //Playlist

    splitter = new QSplitter(Qt::Vertical);
    splitter->setChildrenCollapsible(false);
    scrollarea = new QScrollArea;
    scrollarea->setWidgetResizable(true);
    scrollarea->setFrameStyle(0);
    scrollarea->setLineWidth(0);
    scrollarea->setWidget(splitter);

    playlist_box = new QGroupBox(tr("Playlists"));
    vbox = new QVBoxLayout;
    cmb_playlist = new QComboBox;
    connect(cmb_playlist,
            SIGNAL(currentIndexChanged(int)),
            SLOT(selectList(int)));
    vbox->addWidget(cmb_playlist);
    hbox = new QHBoxLayout;
    btn_addlist = new QPushButton(tr("Add"));
    connect(btn_addlist, SIGNAL(clicked()), SLOT(addList()));
    hbox->addWidget(btn_addlist);
    btn_renamelist = new QPushButton(tr("Rename"));
    connect(btn_renamelist, SIGNAL(clicked()), SLOT(renameList()));
    hbox->addWidget(btn_renamelist);
    btn_removelist = new QPushButton(tr("Remove"));
    connect(btn_removelist, SIGNAL(clicked()), SLOT(removeList()));
    hbox->addWidget(btn_removelist);
    btn_activatelist = new QPushButton(tr("Activate"));
    connect(btn_activatelist, SIGNAL(clicked()), SLOT(activateList()));
    hbox->addWidget(btn_activatelist);
    vbox->addLayout(hbox);
    hline = new QFrame;
    hline->setFrameShape(QFrame::HLine);
    //hline->setFrameStyle(QFrame::Sunken);
    vbox->addWidget(hline);
    vbox->addWidget(scrollarea);
    playlist_box->setLayout(vbox);

    contents_box = new QGroupBox(tr("Contents"));
    vbox = new QVBoxLayout;
    contents_model = new QStringListModel(this);
    contents_list = new QListView;
    contents_list->setEditTriggers(QAbstractItemView::NoEditTriggers);
    contents_list->setModel(contents_model);
    vbox->addWidget(contents_list);
    hbox = new QHBoxLayout;
    btn_addcnt = new QPushButton(tr("Add content"));
    connect(btn_addcnt, SIGNAL(clicked()), SLOT(addContent()));
    hbox->addWidget(btn_addcnt);
    btn_addfolder = new QPushButton(tr("Add folder"));
    connect(btn_addfolder, SIGNAL(clicked()), SLOT(addFolder()));
    hbox->addWidget(btn_addfolder);
    btn_removecnt = new QPushButton(tr("Remove content"));
    connect(btn_removecnt, SIGNAL(clicked()), SLOT(removeContent()));
    hbox->addWidget(btn_removecnt);
    vbox->addLayout(hbox);
    connect(contents_list,
            SIGNAL(doubleClicked(const QModelIndex&)),
            SLOT(navigateToSelected(const QModelIndex&)));
    contents_box->setLayout(vbox);
    splitter->addWidget(contents_box);

    thumbnails_box = new QGroupBox(tr("Thumbnails"));
    vbox = new QVBoxLayout;
    hbox = new QHBoxLayout;
    txt_pathbox = new QLineEdit;
    hbox->addWidget(txt_pathbox);
    vbox->addLayout(hbox);
    thumbslider = new QSlider(Qt::Horizontal);
    thumbslider->setMinimum(1);
    thumbslider->setMaximum(100);
    vbox->addWidget(thumbslider);
    thumbnailbox = new ThumbnailBox(this);
    thumbnailbox->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::MinimumExpanding);
    thumbnailbox->setNameFilter(nameFilter());

    vbox->addWidget(thumbnailbox);
    hbox = new QHBoxLayout;
    btn_addfile = new QPushButton(tr("Add file"));
    btn_addfile->setEnabled(false);
    hbox->addWidget(btn_addfile);
    vbox->addLayout(hbox);
    connect(txt_pathbox,
            SIGNAL(textEdited(const QString&)),
            thumbnailbox,
            SLOT(navigateTo(const QString&)));
    connect(thumbslider,
            SIGNAL(valueChanged(int)),
            thumbnailbox,
            SLOT(setThumbSize(int)));
    connect(thumbnailbox,
            SIGNAL(selectionChanged()),
            SLOT(updateThumbButtons()));
    connect(btn_addfile,
            SIGNAL(clicked()),
            SLOT(addThumbFile()));
    thumbslider->setValue(QSettings().value("ThumbSize").toInt());
    thumbnails_box->setLayout(vbox);
    splitter->addWidget(thumbnails_box);

    settings_box = new QGroupBox(tr("Settings"));
    vbox = new QVBoxLayout;
    hbox = new QHBoxLayout;
    QLabel *lbl_intval = new QLabel(tr("Interval"));
    txt_intval = new QLineEdit;
    QIntValidator *val_intval = new QIntValidator(this);
    txt_intval->setValidator(val_intval);
    connect(txt_intval, SIGNAL(editingFinished()), SLOT(saveInterval()));
    cmb_intval = new QComboBox;
    cmb_intval->addItem(tr("seconds"), 's');
    cmb_intval->addItem(tr("minutes"), 'm');
    cmb_intval->addItem(tr("hours"), 'h');
    cmb_intval->addItem(tr("days"), 'd');
    connect(cmb_intval, SIGNAL(currentIndexChanged(int)), SLOT(saveInterval(int)));
    hbox->addWidget(lbl_intval);
    hbox->addWidget(txt_intval);
    hbox->addWidget(cmb_intval);
    vbox->addLayout(hbox);
    hline = new QFrame;
    hline->setFrameShape(QFrame::HLine);
    hline->setFrameStyle(QFrame::Sunken);
    vbox->addWidget(hline);
    //vbox->addStretch(1);
    settings_box->setLayout(vbox);
    splitter->addWidget(settings_box);

    //Config

    config_box = new QGroupBox(tr("Config"));
    vbox = new QVBoxLayout;
    rad_auto = new QRadioButton(tr("Auto detection"));
    connect(rad_auto, SIGNAL(toggled(bool)), SLOT(toggleMode(bool)));
    lbl_autoinfo = new QLabel(wallpaperSetterInfo());
    hbox = new QHBoxLayout;
    hbox->addWidget(rad_auto);
    hbox->addWidget(lbl_autoinfo);
    vbox->addLayout(hbox);
    rad_command = new QRadioButton(tr("Execute command"));
    connect(rad_auto, SIGNAL(toggled(bool)), SLOT(toggleMode(bool)));
    txt_command = new QLineEdit;
    txt_command->setText(QSettings().value("Command").toString());
    hbox = new QHBoxLayout;
    hbox->addWidget(rad_command);
    hbox->addWidget(txt_command);
    vbox->addLayout(hbox);
    rad_auto->setChecked(txt_command->text().isEmpty());
    //if (txt_command->text().isEmpty()) else rad_auto->setChecked(true);
    //else rad_command->setChecked(true);
    config_box->setLayout(vbox);

    //Window stuff

    vbox = new QVBoxLayout;
    vbox->addWidget(preview_box);
    vbox->addWidget(playlist_box);
    vbox->addWidget(config_box);
    QWidget *widget = new QWidget;
    widget->setLayout(vbox);
    setCentralWidget(widget);

    setAcceptDrops(true);

    //setAttribute(Qt::WA_DeleteOnClose); //Not necessary

    QShortcut *shortcut;

    shortcut = new QShortcut(QKeySequence(Qt::Key_F5), this);
    connect(shortcut, SIGNAL(activated()), SLOT(refresh()));

    updatePreviewBox();
    updateComboBox();
    selectList();

    restoreGeometry(QSettings().value("Geometry").toByteArray());

    tmr_instance = new QTimer(this);
    tmr_instance->setInterval(1000);
    connect(tmr_instance, SIGNAL(timeout()), SLOT(checkInstance()));
    tmr_instance->start();

    timer = new QTimer(this);
    timer->setInterval(1000);
    timer->stop();
    connect(timer, SIGNAL(timeout()), SLOT(update()));
    timer->start();

    //Enable signal handling

    #if !defined(_WIN32) //!Windows (POSIX)

    struct sigaction sig_handler;

    sigemptyset(&sig_handler.sa_mask);
    sig_handler.sa_handler = sig;

    sigaction(SIGINT, &sig_handler, NULL); //2
    sigaction(SIGTERM, &sig_handler, NULL); //15

    #elif defined(_WIN32) //Windows

    //Note to self: Avoid signal()

    signal(SIGINT, sig); //2
    signal(SIGTERM, sig); //15

    #endif //Windows

}

void
Wallphiller::sig(int signal)
{
    std::cout << "Caught signal " << signal << ", now exiting..." << std::endl;
    Wallphiller *instance = Wallphiller::instanceptr;
    QTimer::singleShot(0, instance, SLOT(close()));
}

QStringList
Wallphiller::playlists()
{
    QStringList lists = QSettings().value("Playlists/Names").toStringList();
    for (int i = 0; i < lists.size(); i++)
    {
        lists[i] = decodeName(lists[i]);
    }
    return lists;
}

void
Wallphiller::setPlaylists(QStringList lists)
{
    for (int i = 0; i < lists.size(); i++)
    {
        lists[i] = encodeName(lists[i]);
    }
    QSettings().setValue("Playlists/Names", lists);
}

QStringList
Wallphiller::nameFilter()
{
    QStringList lst = QSettings().value("NameFilters").toStringList();
    if (lst.isEmpty()) lst << "*.jpg" << "*.bmp" << "*.png";
    return lst;
}

QString
Wallphiller::encodeName(QString name)
{
    return name.toUtf8().toHex().toUpper().constData();
}

QString
Wallphiller::decodeName(QString name)
{
    return QByteArray::fromHex(name.toUtf8());
}

QString
Wallphiller::activeList()
{
    if (_active == QString(' ')) //This is weird (bool _isactive ?)
        _active = decodeName(QSettings().value("Playlists/Active").toString());
    //if (_active.isEmpty())
    //    _active = decodeName(QSettings().value("Playlists/Active").toString());
    return _active;
}

bool
Wallphiller::isListActive(QString name)
{
    if (name.isEmpty()) name = currentList();
    if (name.isEmpty()) return false;
    QString e = encodeName(name);
    return QSettings().value("Playlists/Active").toString() == e;
}

void
Wallphiller::setListActive(QString name)
{
    _active = ' ';
    _interval = -1;
    QString e;
    if (!name.isEmpty()) e = encodeName(name);
    QSettings().setValue("Playlists/Active", e);
}

QString
Wallphiller::currentList()
{
    int index = cmb_playlist->currentIndex();
    return cmb_playlist->itemData(index).toString();
}

qint64
Wallphiller::lastChange()
{
    if (_last <= 0) _last = QSettings().value("Playlists/LastChange").toUInt();
    if (_last < 0) _last = 0; //For the future
    return _last;
}

void
Wallphiller::setLastChange(qint64 time)
{
    _last = -1;
    QSettings().setValue("Playlists/LastChange", (uint)time);
}

bool
Wallphiller::paused()
{
    return _paused;
}

void
Wallphiller::setPaused(bool paused)
{
    _paused = paused;
}

QVariant
Wallphiller::playlistSetting(QString name, QString setting)
{
    QString e = encodeName(name);
    return QSettings().value(QString("%1/%2").arg(e).arg(setting));
}

void
Wallphiller::setPlaylistSetting(QString name, QString setting, QVariant value)
{
    QString e = encodeName(name);
    if (e.isEmpty()) return;
    QSettings().setValue(QString("%1/%2").arg(e).arg(setting), value);
}

/*
 * Note:
 * This function takes the name of a playlist as argument
 * and scans the content entries of that list,
 * which may even take up to a few minutes (for large directories).
 * It'll also cache the results, so every following call won't take as long.
 * Everytime, the user fiddles with the contents list,
 * the cache should be cleared.
 * Although it's possible to scan every playlist,
 * it probably only makes sense for the currently active list.
 */
QStringList
Wallphiller::fileList(QString name)
{
    if (name.isEmpty()) name = activeList(); //Default
    QString e = encodeName(name);
    if (_files_cache.contains(e)) return _files_cache[e];

    QStringList lst_contents;
    lst_contents = playlistSetting(name, "Contents").toStringList();
    QStringList lst_files;
    foreach (QString entry, lst_contents)
    {
        QStringList sublst;
        if (QFileInfo(entry).isDir())
        {
            QDir::Filters flags = QDir::Files | QDir::Hidden;
            QFileInfoList lst = QDir(entry).entryInfoList(nameFilter(), flags);
            foreach (QFileInfo entry, lst)
            {
                sublst << entry.absoluteFilePath();
            }
            //TODO:
            //This is NOT recursive!
            //Maybe this would be a place to use the File module...
            //Setting recursive on/off ?
        }
        else if (QFileInfo(entry).isFile())
        {
            sublst << entry;
        }
        else
        {
            //Special stuff (flickr?)
        }
        lst_files << sublst;
    }
    lst_files.removeDuplicates();
    if (!lst_files.isEmpty()) _files_cache[e] = lst_files;

    return lst_files;
}

void
Wallphiller::resetFileListCache(QString name)
{
    QString e = encodeName(name);
    _files_cache.remove(e);
}

void
Wallphiller::resetFileListCache()
{
    _files_cache.clear();
}

int
Wallphiller::position(QString name)
{
    if (name.isEmpty()) name = activeList();
    return playlistSetting(name, "Position").toInt();
}

int
Wallphiller::position(QString name, int position)
{
    if (name.isEmpty()) name = activeList();
    //-1 -> last, 10 -> 1 (if count 10)
    int count = fileList(name).size(); //This may take long if not cached!
    if (!count) return -1;
    while (position >= count) position -= count;
    while (position < 0) position += count;
    //assert(position >= 0 && position < count);
    return position;
}

int
Wallphiller::position(int position)
{
    return this->position("", position);
}

void
Wallphiller::setPosition(QString name, int position)
{
    if (name.isEmpty()) name = activeList();
    setPlaylistSetting(name, "Position", position);
}

void
Wallphiller::setPosition(int position)
{
    setPosition("", position); //Active list
}

QString
Wallphiller::fileAt(int position)
{
    position = this->position(position);
    if (position == -1) return QString();
    else return fileList()[position];
}

int
Wallphiller::interval(QString name)
{
    int interval;
    if (name == activeList())
    {
        if (_interval < 0)
            _interval = playlistSetting(name, "Interval").toInt();
        if (_interval < 0) _interval = 0;
        interval = _interval;
    }
    else
    {
        interval = playlistSetting(name, "Interval").toInt();
    }
    return interval;
}

int
Wallphiller::intervalNumber(QString name)
{
    int interval = this->interval(name);
    int number = interval;
    int minute = 60;
    int hour = minute * 60;
    int day = hour * 24;

    while (number >= day) number /= day;
    while (number >= hour) number /= hour;
    while (number >= minute) number /= minute;

    return number;
}

char
Wallphiller::intervalUnit(QString name)
{
    int interval = this->interval(name);
    char unit = 's';
    int minute = 60;
    int hour = minute * 60;
    int day = hour * 24;

    if (interval >= day) unit = 'd';
    else if (interval >= hour) unit = 'h';
    else if (interval >= minute) unit = 'm';

    return unit;
}

void
Wallphiller::setInterval(QString name, int interval)
{
    if (name == activeList()) _interval = -1; //Clear cache
    if (interval < 0) interval = 0;
    setPlaylistSetting(name, "Interval", interval);
}

void
Wallphiller::changeEvent(QEvent *event)
{
    if (!event) return;
    if (event->type() == QEvent::WindowStateChange)
    {
        if (windowState() & Qt::WindowMinimized)
        {
            hideInstance();
        }
    }
}

void
Wallphiller::closeEvent(QCloseEvent *event)
{
    if (!_is2ndinstance)
    {
        QSettings().remove("PID");
        QSettings().setValue("ThumbSize", thumbslider->value());
        QSettings().setValue("Geometry", saveGeometry());
    }
}

void
Wallphiller::dragEnterEvent(QDragEnterEvent *event)
{
    if (currentList().size()) event->accept();
}

void
Wallphiller::dropEvent(QDropEvent *event)
{
    const QMimeData* data = event->mimeData();
 
    if (data->hasUrls())
    {
        QStringList paths;
        QList<QUrl> urls = data->urls();
 
        for (int i = 0; i < urls.size(); i++)
        {
            paths.append(urls.at(i).toLocalFile());
        }
 
        if (paths.size() > 10)
        {
            if (QMessageBox::question(this,
                tr("Add items"),
                tr("Do you want to add all %1 items?").arg(paths.size()),
                QMessageBox::Yes | QMessageBox::Cancel) != QMessageBox::Yes)
                return;
        }
        foreach (QString path, paths) addContent(path);
    }
}

void
Wallphiller::checkInstance()
{
    if (!QSettings().contains("PID2ndInstance")) return;

    QSettings().remove("PID2ndInstance");
    showInstance();
}

void
Wallphiller::refresh()
{
    resetFileListCache();
    updateComboBox();
    updatePreviewBox();
}

void
Wallphiller::updatePreviewBox()
{
    if (_updating_preview_box) return;
    else _updating_preview_box = true;

    QString name = activeList();
    bool isactive = !name.isEmpty();

    preview_box->setEnabled(isactive);
    if (isactive) preview_box->setTitle(tr("Preview - %1").arg(name));
    else preview_box->setTitle(tr("[Preview]"));
    //if (!isactive) return; //return - updating_preview_box !

    int count = preview_widgets.size(); //Number of preview thumbnails
    int middle = (count - 1) / 2; //count is multiple of 2
    //The current picture will show up in the middle
    for (int i = 0; i < count; i++)
    {
        PreviewLabel *label = qobject_cast<PreviewLabel*>(preview_widgets[i]);
        if (!label) continue;
        label->clear();
        int index = position() + i - middle;
        if (i == middle) label->setStyleSheet("background-color:red;");
        else label->setStyleSheet(" "); //White space!
        QString file = fileAt(index);
        QString filename = QFileInfo(file).fileName();
        label->setToolTip(filename);
        QImage img(file);
        if (img.isNull())
        {
            //statusBar()->showMessage(
            //tr("Couldn't load picture: %1").arg(file), 5000);
            continue;
        }
        QSize size = img.size();
        int w = label->width();
        size.scale(w, w, Qt::KeepAspectRatio);
        QPixmap pix1(QPixmap::fromImage(img));
        if (pix1.isNull())
        {
            //statusBar()->showMessage(
            //tr("Loaded empty picture: %1").arg(file), 5000);
            continue;
        }
        QPixmap pix2(size);
        QPainter ptr(&pix2);
        ptr.eraseRect(0, 0, size.width(), size.height());
        int dist = middle - i;
        if (dist < 0) dist *= (-1);
        int opacity = 100 - ((100 / (middle + 1)) * (dist));
        ptr.setOpacity(((double)opacity) / 100);
        ptr.drawPixmap(0, 0, size.width(), size.height(), pix1);
        //label->setScaledContents(true); //Stretch
        label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        label->setPixmap(pix2);
    }

    _updating_preview_box = false;
}

void
Wallphiller::scheduleUpdatePreviewBox()
{
    preview_update_timer->stop();
    preview_update_timer->setSingleShot(true);
    preview_update_timer->setInterval(100);
    preview_update_timer->start();
}

void
Wallphiller::updateComboBox(QString selected)
{
    QStringList lists;
    lists = playlists();
    if (selected.isEmpty()) selected = currentList();
    cmb_playlist->clear();
    foreach (QString item, lists)
    {
        QString text = item;
        if (isListActive(item)) text += " [ACTIVE]";
        cmb_playlist->addItem(text, item); //Changes selection
    }
    cmb_playlist->setCurrentIndex(cmb_playlist->findData(selected));
    statusBar()->showMessage(tr("Found %1 playlist(s)").
    arg(lists.size()), 1000);
}

void
Wallphiller::selectList()
{
    int index = cmb_playlist->currentIndex();
    QString list = cmb_playlist->itemData(index).toString();
    bool selected = index != -1;
    contents_box->setEnabled(selected);
    thumbnails_box->setEnabled(selected);
    settings_box->setEnabled(selected);
    if (!selected)
    {
        setWindowTitle(PROGRAM);
    }
    else
    {
        setWindowTitle(QString("%1 - %2").arg(PROGRAM).arg(list));
        if (isListActive())
        {
            btn_activatelist->setText(tr("Active list"));
            QFont font = btn_activatelist->font();
            font.setBold(true);
            btn_activatelist->setFont(font);
        }
        else
        {
            btn_activatelist->setText(tr("Set active"));
            QFont font = btn_activatelist->font();
            font.setBold(false);
            btn_activatelist->setFont(font);
        }
    }
    resetContentsBox();
    btn_renamelist->setEnabled(false); //TODO
    btn_removelist->setEnabled(selected);
    btn_activatelist->setEnabled(selected);

    int number = intervalNumber(currentList());
    char unit = intervalUnit(currentList());
    txt_intval->setText(QString::number(number));
    cmb_intval->setCurrentIndex(cmb_intval->findData(unit));

}

void
Wallphiller::selectList(int index)
{
    selectList();
}

void
Wallphiller::selectList(const QString &list)
{
    int index = cmb_playlist->findData(list);
    selectList(index);
}

void
Wallphiller::addList()
{
    QStringList list = playlists();
    QString name;
    QString oldname;

    do
    {
        oldname = QInputDialog::getText(this,
            tr("Add playlist"),
            tr("Enter a name for the new playlist."),
            QLineEdit::Normal,
            oldname);
        if (oldname.isEmpty()) return;
        else name = oldname;
        if (list.contains(name))
        {
            QMessageBox::critical(this,
                tr("Invalid name"),
                tr("This name is taken."));
        }
        else
        {
            oldname.clear();
        }
    }
    while (oldname.size());

    list << name;
    setPlaylists(list);
    setPlaylistSetting(name, "Contents", QVariant());

    updateComboBox(name);
}

void
Wallphiller::renameList()
{
    //Copy all values, remove key group
}

void
Wallphiller::removeList()
{
    QString name = currentList();
    QString e = encodeName(name);
    if (name.isEmpty()) return;
    if (QMessageBox::question(this,
        tr("Remove playlist"),
        tr("Do you want to remove this playlist?\n%1").
        arg(name),
        QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) return;
    QStringList list = playlists();
    list.removeAll(name);
    setPlaylists(list);
    QSettings().remove(e);

    updateComboBox();
}

void
Wallphiller::activateList()
{
    QString name = currentList();
    if (name.isEmpty()) return;
    if (!isListActive())
    {
        if (QMessageBox::question(this,
            tr("Activate list"),
            tr("Do you want to activate this list?"),
            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) return;
        setListActive(name);
    }
    else
    {
        if (QMessageBox::question(this,
            tr("Deactivate list"),
            tr("This is the active list. Do you want to deactivate it?"),
            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) return;
        setListActive();
    }
    refresh();
}

void
Wallphiller::resetContentsBox()
{
    QString name = currentList();
    QStringList lst;
    if (name.size()) lst = playlistSetting(name, "Contents").toStringList();
    contents_model->setStringList(lst);
}

void
Wallphiller::addContent(QString content)
{
    if (content.isEmpty()) return;
    if (!QFileInfo(content).isDir() &&
        !QFileInfo(content).isFile())
    {
        QMessageBox::critical(this,
            tr("Invalid source"),
            tr("This is not a valid source:\n%1").
            arg(content));
        return;
    }
    QString name = currentList();
    QStringList lst;
    lst = playlistSetting(name, "Contents").toStringList();
    if (lst.contains(content)) return;
    lst << content;
    setPlaylistSetting(name, "Contents", lst);

    resetFileListCache();
    resetContentsBox();
}

void
Wallphiller::addContent()
{
    addContent(QInputDialog::getText(this,
        tr("Add content to playlist"),
        tr("Enter a valid path.")));
}

void
Wallphiller::addFolder()
{
    addContent(QFileDialog::getExistingDirectory(this,
        tr("Add folder to playlist")));
}

void
Wallphiller::removeContent()
{
    QString name = currentList();
    if (name.isEmpty()) return;
    QModelIndex index = contents_list->currentIndex();
    if (!index.isValid()) return;
    QString content = contents_model->data(index, Qt::DisplayRole).toString();
    if (content.isEmpty()) return;
    if (QMessageBox::question(this,
        tr("Remove content from playlist"),
        tr("Do you want to remove this item from the playlist?\n%1").
        arg(content),
        QMessageBox::Yes | QMessageBox::No)
        != QMessageBox::Yes) return;
    QStringList lst;
    lst = playlistSetting(name, "Contents").toStringList();
    lst.removeAll(content);
    setPlaylistSetting(name, "Contents", lst);

    resetFileListCache();
    resetContentsBox();
}

void
Wallphiller::navigateToSelected(const QModelIndex &index)
{
    if (!index.isValid()) return;
    QString path;
    path = contents_list->model()->data(index).toString();
    if (!QDir(path).exists()) return;
    thumbnailbox->navigateTo(path);
}

void
Wallphiller::updateThumbButtons()
{
    btn_addfile->setEnabled(thumbnailbox->isSelected());
}

void
Wallphiller::addThumbFile()
{
    QString file = thumbnailbox->itemPath();
    if (file.size()) addContent(file);
}

void
Wallphiller::saveInterval()
{
    int interval;

    interval = txt_intval->text().toInt();
    if (interval < 0) interval = 0;

    switch (cmb_intval->itemData(cmb_intval->currentIndex()).toInt())
    {
        case 'd': interval *= 24;
        case 'h': interval *= 60;
        case 'm': interval *= 60;
    }

    setInterval(currentList(), interval);
}

void
Wallphiller::saveInterval(int index)
{
    saveInterval();
}

void
Wallphiller::toggleMode()
{
    bool is_auto = rad_auto->isChecked();
    QFont font;
    font = rad_auto->font();
    font.setBold(is_auto);
    rad_auto->setFont(font);
    lbl_autoinfo->setEnabled(is_auto);
    font = rad_command->font();
    font.setBold(!is_auto);
    rad_command->setFont(font);
    txt_command->setEnabled(!is_auto);
    if (!is_auto) txt_command->setFocus();

    QSettings().setValue("Command", txt_command->text());
}

void
Wallphiller::toggleMode(bool checked)
{
    toggleMode();
}

void
Wallphiller::hideInstance()
{
    //hide(); //Too quick (will reappear)
    QTimer::singleShot(500, this, SLOT(hide()));
}

void
Wallphiller::showInstance()
{
    raise();
    show();
    activateWindow();
}

void
Wallphiller::setCurrentWallpaper()
{
    QString file = fileAt(position());
    QString command = txt_command->text();
    if (command.size())
    {
        command.replace("$FILE", file);
        system(command.toStdString().c_str()); //Dangerous / could be abused
    }
    else
    {
        setWallpaper(file);
    }
}

void
Wallphiller::select(int index)
{
    qint64 current_time;
    setPosition(index);
    setCurrentWallpaper();
    updatePreviewBox();
    current_time = QDateTime::currentMSecsSinceEpoch() / 1000;
    setLastChange(current_time);
}

void
Wallphiller::previous()
{
    select(position() - 1);
}

void
Wallphiller::pause()
{
    setPaused(!paused());
    if (paused()) btn_pause->setText(tr("Continue"));
    else btn_pause->setText(tr("Pause"));
}

void
Wallphiller::next()
{
    select(position() + 1);
}

void
Wallphiller::update()
{
    //Things that should be cached (timer):
    //lastChange()
    //activeList()
    //interval()
    qint64 current_time = QDateTime::currentMSecsSinceEpoch() / 1000;
    qint64 last_change = lastChange();
    qint64 interval = this->interval(activeList());
    qint64 tdiff = current_time - last_change;
    qint64 tremaining = interval - tdiff;
    if (tremaining < 0) tremaining = 0;
    bool running = activeList().size() && !paused() && interval;

    if (!running)
    {
        btn_next->setText(tr("Next"));
        return;
    }

    if (tremaining)
    {
        QString str_rem = QString::number(tremaining);
        btn_next->setText(tr("Next - %1").arg(str_rem));
    }
    else
    {
        btn_next->setText(tr("Next - ..."));
        next();
    }

}

