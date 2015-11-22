#define DEFINE_GLOBALS
#include "wallphiller.hpp"

Wallphiller* Wallphiller::instanceptr = 0;

Wallphiller::Wallphiller()
           : shared_memory("WALLPHILLER_INSTANCE"),
             tmr_check_shared_memory(0),
             _configured_interval_value(0),
             _configured_thumbnail_cache_limit(0),
             _current_playlist(0),
             _position(-1),
             _de(DE::None)
{
    //Store self reference for singleton call (cache callback)
    instanceptr = this;

    //UTF-8
    QTextCodec::setCodecForCStrings(QTextCodec::codecForName("utf8"));

    //QSettings defaults
    QCoreApplication::setOrganizationName(PROGRAM); //~/.config/Wallphiller
    QCoreApplication::setApplicationName(PROGRAM);
    QCoreApplication::setApplicationVersion(GITVERSION);
    QSettings::setDefaultFormat(QSettings::IniFormat);

    //The shared memory segment contains a "request" flag (boolean),
    //which is used by a second instance to tell the first instance
    //to show up. The first instance will check this flag periodically.
    //If a shared memory segment is found, another instance is probably
    //already running, so the flag is set and this instance terminates.
    //However, a "last update" timestamp is also stored, which is used
    //to determine if the memory segment is actually a dead leftover
    //from a previously killed instance. In this case, it will be discarded.
    //TODO IDEA access request flag directly without QDataStream
    //We serialize and deserialize the whole memory segment using QDataStream
    //every couple of seconds. This is overkill.
    //We should get rid of QDataStream and the flag should be checked
    //directly, for example by defining it do be at position 99.
    //Really, reading, parsing, writing the whole segment every 5 seconds
    //is useless overhead. But it should work for now and will be fixed.

    //Shared memory for single instance constraint
    QString shmem_title = "Wallphiller";
    qint64 current_time = QDateTime::currentMSecsSinceEpoch() / 1000; //sec
    qint64 own_pid = QCoreApplication::applicationPid();
    QBuffer shmem_buffer_out;
    shmem_buffer_out.open(QBuffer::ReadWrite);
    {
        QDataStream stream(&shmem_buffer_out);
        stream << shmem_title;
        stream << current_time; //last update
        stream << own_pid; //pid of first instance
        stream << (bool)false; //show request flag
    }
    if (shared_memory.attach())
    {
        //Found and attached to existing memory segment

        //Check time, how old is it?
        QBuffer shmem_buffer_in;
        shared_memory.lock();
        shmem_buffer_in.setData((char*)shared_memory.constData(),
            shared_memory.size());
        shared_memory.unlock();
        //Not detaching yet, see below
        shmem_buffer_in.open(QBuffer::ReadOnly);
        QString other_shmem_title;
        qint64 other_update;
        qint64 other_pid;
        bool other_show_requested;
        {
            QDataStream stream(&shmem_buffer_in);
            stream
                >> other_shmem_title
                >> other_update
                >> other_pid
                >> other_show_requested;
        }
        int timeout = 15;
        int age = current_time - other_update;
        if (age > timeout)
        {
            //Too old, it's a leftover
            qDebug() << "Found old memory segment, discarding";

            //Detach, ignore dead leftover
            if (!shared_memory.detach())
            {
                qWarning() << "Detaching failed";
            }

        }
        else
        {
            //It's an active memory segment
            //Other instance is running
            qWarning() << "Another instance is already running";

            //Request first instance (set show flag)
            //Only this flag is changed from false to true!
            //The size of the whole segment does not change!
            //So no need to worry about overflowing.
            QBuffer shmem_buffer_flag_out;
            shmem_buffer_flag_out.open(QBuffer::ReadWrite);
            {
                QDataStream stream(&shmem_buffer_flag_out);
                stream << shmem_title; //unchanged
                stream << current_time; //unchanged
                stream << own_pid; //unchanged
                stream << (bool)true; //show request flag ON
            }
            shared_memory.lock();
            char *to = (char*)shared_memory.data();
            const char *from = shmem_buffer_flag_out.data().data();
            memcpy(to, from, shared_memory.size());
            shared_memory.unlock();

            //Explicitly detach (just to make it obvious that we're done)
            shared_memory.detach();

            //Terminate
            QTimer::singleShot(0, this, SLOT(close()));
            return;
        }
    }
    //First instance
    if (!shared_memory.create(shmem_buffer_out.size()))
    {
        //Creating shared memory segment failed
        qWarning() << "Creating shared memory segment failed";
        qWarning() << shared_memory.errorString();
        //Now what?
    }
    else
    {
        //Write initial stuff
        shared_memory.lock();
        char *to = (char*)shared_memory.data();
        const char *from = shmem_buffer_out.data().data();
        memcpy(to, from, shared_memory.size());
        shared_memory.unlock();

        //Set timer to check request flag periodically
        tmr_check_shared_memory = new QTimer(this);
        connect(tmr_check_shared_memory,
                SIGNAL(timeout()),
                SLOT(checkAndUpdateMemory()));
        tmr_check_shared_memory->start(5000);

    }

    //Detect desktop environment
    _de = detectDesktopEnvironment();

    //Default settings
    foreach (QByteArray f, QImageReader::supportedImageFormats())
    {
        QString type = f; //"jpg"
        _read_formats << type;
    }

    //GUI
    //The main window consists of 4 horizontal sections;
    //playlist/directory, thumbnail box (most space), thumbnail slider,
    //general settings and buttons.
    //Specific settings are in a separate window, the user has to click
    //a button to get there.
    //The main window should be simple and not have hundreds of buttons,
    //menus and "did you know" nonsense (like certain KDE applications),
    //but specific settings should still be available
    //(unlike many Gnome 3 applications that lack most options).

    //Layout items
    QVBoxLayout *vbox = 0;
    QHBoxLayout *hbox = 0;
    QFrame *hline = 0;
    QVBoxLayout *vbox_main = new QVBoxLayout;

    //Section 1: Playlist
    //-------------------

    //Section layout
    hbox = new QHBoxLayout;

    //Title (selected playlist)
    txt_playlist_title = new QLineEdit;
    txt_playlist_title->setReadOnly(true);
    hbox->addWidget(txt_playlist_title);

    //Playlist selection button (dropdown menu)
    //An option to simply select a directory and be done is provided.
    //This menu could contain a list of bookmarked playlists.
    btn_playlist = new QToolButton;
    btn_playlist->setText(tr("&Playlist"));
    hbox->addWidget(btn_playlist);
    btn_playlist->setPopupMode(QToolButton::InstantPopup);
    setPlaylistMenu();

    //Add to layout
    vbox_main->addLayout(hbox);
    hbox = 0;

    //Section 2: Thumbnail box
    //------------------------

    //Section layout
    hbox = new QHBoxLayout;

    //Thumbnail box
    thumbnailbox = new ThumbnailBox(this);
    hbox->addWidget(thumbnailbox);
    thumbnailbox->setFrame();
    thumbnailbox->setDarkBackground();
    connect(thumbnailbox,
            SIGNAL(itemSelected(int)),
            SLOT(selectWallpaper(int)));

    //Add to layout
    vbox_main->addLayout(hbox, 1);
    hbox = 0;

    //Thumbnail size changer thingy
    sld_thumb_size = new QSlider(Qt::Horizontal);
    vbox_main->addWidget(sld_thumb_size);
    sld_thumb_size->setMinimum(10);
    sld_thumb_size->setMaximum(90);
    connect(sld_thumb_size,
            SIGNAL(valueChanged(int)),
            thumbnailbox,
            SLOT(setThumbSize(int)));
    int thumb_size_percentage = 33;
    sld_thumb_size->setValue(thumb_size_percentage);

    //Section 3: Thumbnail slider
    //---------------------------

    //Section layout
    hbox = new QHBoxLayout;

    //TODO to be implemented
    //Thumbnail slider should be scrollable (horizontally) (mouse wheel).
    //Current wallpaper should be in the middle.
    QLabel *lbl_thumbnailslider = new QLabel; //TODO
    hbox->addWidget(lbl_thumbnailslider);

    //Horizontal line
    hline = new QFrame;
    hline->setFrameShape(QFrame::HLine);
    vbox_main->addWidget(hline);
    hline = 0;

    //Add to layout
    vbox_main->addLayout(hbox);
    hbox = 0;

    //Horizontal line
    hline = new QFrame;
    hline->setFrameShape(QFrame::HLine);
    vbox_main->addWidget(hline);
    hline = 0;

    //Section 4: General settings and buttons
    //---------------------------------------

    //General section -> bottom_widget -> vbox
    hbox = new QHBoxLayout;
    QWidget *bottom_area = new QWidget;
    hbox->addWidget(bottom_area);
    vbox = new QVBoxLayout;
    bottom_area->setLayout(vbox);
    vbox_main->addLayout(hbox);
    hbox = 0;

    //Buttons
    btn_settings = new QPushButton(tr("&Settings"));
    connect(btn_settings,
            SIGNAL(clicked()),
            SLOT(openSettingsWindow()));
    btn_tray = new QPushButton(tr("Minimize to &Tray"));
    connect(btn_tray,
            SIGNAL(clicked()),
            SLOT(minimizeToTray()));
    btn_hide = new QPushButton(tr("&Hide"));
    btn_hide->setToolTip(tr(
        "This will hide the program, "
        "which will continue running in the background. "
        "Restart the program to make it reappear."
    ));
    connect(btn_hide,
            SIGNAL(clicked()),
            SLOT(hideInstance()));
    btn_quit = new QPushButton(tr("&Quit"));
    btn_quit->setToolTip(tr(
        "This will terminate the program."
    ));
    connect(btn_quit,
            SIGNAL(clicked()),
            SLOT(close()));

    //Button row
    hbox = new QHBoxLayout;
    hbox->addWidget(btn_settings);
    hbox->addStretch();
    hbox->addWidget(btn_tray);
    hbox->addWidget(btn_hide);
    hbox->addWidget(btn_quit);
    vbox->addLayout(hbox);
    hbox = 0;
    vbox = 0;

    //Window/misc
    //-----------

    //Wallpaper timer
    tmr_next_wallpaper = new QTimer(this);
    connect(tmr_next_wallpaper,
            SIGNAL(timeout()),
            SLOT(next()));

    //Window layout
    QWidget *widget = new QWidget;
    widget->setLayout(vbox_main);
    setCentralWidget(widget);

    //Quit on close (don't keep running invisibly by default)
    setAttribute(Qt::WA_DeleteOnClose); //quit on close

    //Application icon
    QPixmap icon_pixmap(":/Apps-preferences-desktop-wallpaper-icon.png");
    QIcon icon = QIcon(icon_pixmap);
    setWindowIcon(icon);

    //Tray icon
    tray_icon = new QSystemTrayIcon(this);
    tray_icon->setIcon(icon);
    tray_icon->setToolTip(PROGRAM);
    connect(tray_icon,
            SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
            SLOT(handleTrayClicked(QSystemTrayIcon::ActivationReason)));

    //Tray icon menu
    QMenu *tray_menu = new QMenu(this);
    tray_menu->addAction("Wallphiller")->setEnabled(false);
    tray_menu->addSeparator();
    tray_menu->addAction(tr("&Show window"), this, SLOT(showInstance()));
    tray_menu->addAction(tr("&Quit"), this, SLOT(close()));
    tray_icon->setContextMenu(tray_menu);

    //Restore settings
    QSettings settings;
    Playlist *saved_playlist = 0;
    int start_position = 0;
    if (settings.contains("Playlist"))
    {
        //Read last playlist
        QByteArray serialized = settings.value("Playlist").toByteArray();
        saved_playlist = new Playlist(serialized, this);

        //Last wallpaper
        QStringList list = saved_playlist->pictureAddressList();
        QString last_wallpaper = settings.value("LastWallpaper").toString();
        if (list.contains(last_wallpaper))
        {
            start_position = list.indexOf(last_wallpaper);
        }

    }
    restoreGeometry(settings.value("Geometry").toByteArray());
    _change_routine = settings.value("ChangeRoutine").toString();
    _change_routine_command =
        settings.value("ChangeRoutineCommand").toString();
    if (settings.contains("CacheLimit"))
    {
        int limit = settings.value("CacheLimit").toInt();
        if (limit < 1) limit = 10; //suggest but don't enforce 10 MB
        _configured_thumbnail_cache_limit = limit;
        if (limit) thumbnailbox->setCacheLimit(limit);
    }
    else
    {
        _configured_thumbnail_cache_limit = 10; //default 10 MB
    }
    if (settings.contains("IntervalValue"))
    {
        int value = settings.value("IntervalValue").toInt();
        if (value < 0) value = 0;
        _configured_interval_value = value;
        QString unit =settings.value("IntervalUnit").toString(); 
        _configured_interval_unit = unit;
    }

    //Start minimized to tray if requested
    //Also starting minimized if last instance was stopped minimized
    QStringList args = QCoreApplication::arguments();
    args.removeFirst(); //skip argv[0]
    bool was_minimized = false;
    if (settings.contains("Minimized"))
        was_minimized = settings.value("Minimized").toBool();
    if (args.contains("-minimized") || was_minimized)
    {
        QTimer::singleShot(0, this, SLOT(minimizeToTray()));
    }

    //Restore playlist
    //This may start the timer
    //Playlist continues where it was stopped last time
    setPlaylist(saved_playlist, start_position);

    //Keyboard shortcuts
    QShortcut *shortcut;

    //F4 clear cache
    shortcut = new QShortcut(QKeySequence(Qt::Key_F4), this);
    connect(shortcut, SIGNAL(activated()), thumbnailbox, SLOT(clearCache()));

    //Ctrl + PageUp previous
    shortcut = new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_PageUp), this);
    connect(shortcut, SIGNAL(activated()), SLOT(previous()));

    //Ctrl + PageDown next
    shortcut = new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_PageDown), this);
    connect(shortcut, SIGNAL(activated()), SLOT(next()));

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

Wallphiller::~Wallphiller()
{
    //Explicitly detach from the shared memory segment
    shared_memory.detach();

}

void
Wallphiller::sig(int signal)
{
    std::cerr
        << "Caught signal " << signal
        << ", now exiting..." << std::endl;

    Wallphiller *instance = Wallphiller::instanceptr;
    QTimer::singleShot(0, instance, SLOT(close()));
}

DE
Wallphiller::detectDesktopEnvironment()
{
    DE de = DE::None;

    //How do we detect the current desktop environment?
    //Short answer: We don't. Because we don't actually "detect", we guess.
    //(Exception: Windows, Windows is its own DE, so that's always fixed.)
    //Many popular distributions install and use GDM by default.
    //If GDM is used, certain environment variables are usually
    //very good indicators of the running desktop session,
    //specifically XDG_CURRENT_DESKTOP and GDMSESSION.
    //If other display managers are used, these variables may not be set.
    //For example, wdm doesn't set them. 

    //Alternatively, we can walk up the process tree because this process
    //is probably a (grand-)child of a well known root process
    //like gnome-session (/usr/bin/gnome-session).
    //The topmost process that's still owned by this user (then: root)
    //should usually be ck-launch-session aka ConsoleKit on init systems.
    //On systemd systems, this gnome-session process would usually be the
    //topmost process, which should be owned by the dm.
    //Unfortunately, there are many variations. For example,
    //in XFCE, the xfce4-session process is not the topmost user process.

    //Platform: Windows
    #if defined(_WIN32)
    de = DE::Windows;
    return de;
    #endif
    //Platform: UNIX

    //Process id of this instance
    //Even though most pids probably fit in an int,
    //we use a long long int, to be safe. After all, that's what Qt does.
    qint64 own_pid = QCoreApplication::applicationPid();

    //Walk up the process tree and collect process information
    QList<QVariantMap> parent_process_list;
    QStringList parent_process_name_list;
    for (qint64 current_pid = own_pid; current_pid;)
    {
        //Determine parent process id
        qint64 old_pid = current_pid;
        {
            //Regex used to extract pid from output
            QRegExp rx("^\\s*(\\d+)\\s*$");

            //Run ps
            QProcess process;
            QStringList args;
            args << "o" << "ppid=" << "p" << QString::number(current_pid);
            process.start("ps", args);
            if (!process.waitForStarted())
            {
                //ps couldn't be executed
                break;
            }
            process.waitForFinished();
            if (process.exitCode())
            {
                //ps call failed
                break;
            }

            //ps output may contain leading whitespace(s): " 123"
            //And a trailing linebreak
            QString ps_output = process.readAllStandardOutput();

            //Extract numeric id from output using regular expression
            //Abort on error, no match if stdout empty
            QString extracted_pid;
            if (rx.indexIn(ps_output) != -1 && rx.capturedTexts().size() == 2)
                extracted_pid = rx.capturedTexts().at(1);
            if (extracted_pid.isEmpty())
            {
                //ps output did not contain id (or unrecognized pattern)
                break;
            }
            current_pid = extracted_pid.toLongLong();
            if (!current_pid)
            {
                //Got zero somehow (error converting maybe)
                break;
            }

            //Prevent infinite loop
            if (current_pid == old_pid)
            {
                //What just happened
                break;
            }

        }

        //Determine process name
        QString name;
        {
            //Run ps
            QProcess process;
            QStringList args;
            args << "o" << "comm=" << "p" << QString::number(current_pid);
            process.start("ps", args);
            if (!process.waitForStarted())
            {
                //ps couldn't be executed
                break;
            }
            process.waitForFinished();
            if (process.exitCode())
            {
                //ps call failed
                break;
            }

            //ps output contains process name
            QString ps_output = process.readAllStandardOutput();

            //Remove trailing linebreak
            ps_output.remove(QRegExp("\\n$"));

            //Get name from output
            name = ps_output;

        }

        //Determine executable path
        QString exe;
        {
            //Resolve exe symlink in proc filesystem
            //Works for own processes only, not for root
            QFileInfo fi("/proc/" + QString::number(current_pid) + "/exe");
            exe = fi.symLinkTarget();
        }

        //Collect process information
        QVariantMap info;
        info["id"] = current_pid;
        info["name"] = name;
        info["exe"] = exe;
        parent_process_list.prepend(info);

    }

    //List names of parent processes
    foreach (QVariantMap info, parent_process_list)
    {
        parent_process_name_list << info["name"].toString();
    }

    //Detect environment by its session manager process (by name)
    if (parent_process_name_list.contains("gnome-session"))
        de = DE::Gnome;
    else if (parent_process_name_list.contains("mate-session"))
        de = DE::Mate;
    else if (parent_process_name_list.contains("cinnamon-session"))
        de = DE::Cinnamon;
    else if (parent_process_name_list.contains("xfce4-session"))
        de = DE::XFCE;
    else if (parent_process_name_list.contains("lxsession")) //TODO
        de = DE::LXDE;
    else if (parent_process_name_list.contains("ksmserver")) //TODO
        de = DE::KDE;

    //Return result if detection by session manager worked
    //Should be the most reliable method
    //See below for a fallback solution
    if (de != DE::None) return de;

    //Environment variables used for detection (guessing)
    //If the user manually changes one of these variables
    //then this user doesn't deserve automatic de recognition.
    //It may however be a problem if no approproate variable is set,
    //maybe because someone's uber-simple display manager didn't set any.
    //GDM should generally set XDG_CURRENT_DESKTOP properly.
    //Also interesting: DESKTOP_SESSION, GDMSESSION, XDG_DATA_DIRS
    const QString env_xdg_current_desktop =
        QString(std::getenv("XDG_CURRENT_DESKTOP"));
    const QString env_desktop_session =
        QString(std::getenv("DESKTOP_SESSION"));
    const QString env_gdmsession =
        QString(std::getenv("GDMSESSION"));
    const QString env_xdg_data_dirs =
        QString(std::getenv("XDG_DATA_DIRS"));

    //Detect desktop environment using environment variables
    //TODO not all of these values have been verified, double-check!
    if (!env_xdg_current_desktop.isEmpty())
    {
        //XDG_CURRENT_DESKTOP
        if (env_xdg_current_desktop == "GNOME")
            de = DE::Gnome;
        else if (env_xdg_current_desktop == "Unity")
            de = DE::Gnome;
        else if (env_xdg_current_desktop == "MATE")
            de = DE::Mate;
        else if (env_xdg_current_desktop == "X-Cinnamon")
            de = DE::Cinnamon;
        else if (env_xdg_current_desktop == "XFCE")
            de = DE::XFCE;
        else if (env_xdg_current_desktop == "LXDE")
            de = DE::LXDE;
        else if (env_xdg_current_desktop == "KDE")
            de = DE::KDE;
    }
    else if (!env_gdmsession.isEmpty())
    {
        //GDMSESSION
        if (env_gdmsession.startsWith("gnome"))
            de = DE::Gnome;
        else if (env_gdmsession.startsWith("mate"))
            de = DE::Mate;
        else if (env_gdmsession == "cinnamon")
            de = DE::Cinnamon;
        else if (env_gdmsession == "xfce")
            de = DE::XFCE;
        else if (env_gdmsession == "lxde")
            de = DE::LXDE;
        else if (env_gdmsession == "kde-plasma")
            de = DE::KDE;
    }

    return de;
}

void
Wallphiller::setPlaylistMenu()
{
    //Recreate menu
    if (btn_playlist->menu()) btn_playlist->menu()->deleteLater();
    QMenu *menu = new QMenu(btn_playlist);
    btn_playlist->setMenu(menu);

    //Load full directory (quick, no detour)
    QAction *act_load_full = new QAction(menu);
    act_load_full->setText(tr("Load full &directory"));
    menu->addAction(act_load_full);
    connect(act_load_full,
            SIGNAL(triggered()),
            SLOT(createPlaylistWithFullDirectory()));

    //Load directory non-recursive (quick, no detour)
    QAction *act_load_shallow = new QAction(menu);
    act_load_shallow->setText(tr("Load directory (without &subdirectories)"));
    menu->addAction(act_load_shallow);
    connect(act_load_shallow,
            SIGNAL(triggered()),
            SLOT(createPlaylistWithShallowDirectory()));

    //Load files (quick, no detour)
    QAction *act_load_files = new QAction(menu);
    act_load_files->setText(tr("Load &files"));
    menu->addAction(act_load_files);
    connect(act_load_files,
            SIGNAL(triggered()),
            SLOT(createPlaylistWithFiles()));

    //Playlist settings window TODO

    //Separator
    menu->addSeparator();

    //Unload
    QAction *act_unload = new QAction(menu);
    act_unload->setText(tr("&Unload"));
    menu->addAction(act_unload);
    connect(act_unload,
            SIGNAL(triggered()),
            SLOT(unloadPlaylist()));

}

void
Wallphiller::closeEvent(QCloseEvent *event)
{
    //Save settings ONLY if playlist defined
    //It should generally be possible for the user to remove the config file
    //as well as temporary files, including the config directory itself.
    //Simply starting this program without doing anything
    //should not leave any traces on the system.
    bool is_playlist = playlist();
    QStringList current_list = sortedAddresses();
    bool save_settings = is_playlist;
    if (save_settings)
    {
        //Save settings
        QSettings settings;

        //General settings
        settings.setValue("Geometry", saveGeometry());
        settings.setValue("Minimized", isMinimized());

        //Playlist
        if (playlist())
            settings.setValue("Playlist", playlist()->toByteArray());
        else
            settings.remove("Playlist");

        //Remember last wallpaper for next startup
        int index = position();
        QString wallpaper_address = current_list.value(index); //or empty
        settings.setValue("LastWallpaper", wallpaper_address);

    }
    else
    {
        //TODO delete config if no playlist selected? -> SettingsDialog...

        //Settings
        QSettings settings;

        //Clear settings
        settings.clear(); //TODO not sure if this is good

        //TODO also delete config directory...?

    }

    //Call base implementation
    QMainWindow::closeEvent(event);
}

void
Wallphiller::dragEnterEvent(QDragEnterEvent *event)
{
    //Do we accept drag&drop? Sure!
    event->accept();
}

void
Wallphiller::dropEvent(QDropEvent *event)
{
    const QMimeData* data = event->mimeData();
 
    if (data->hasUrls())
    {
        QList<QUrl> urls = data->urls();
        createPlaylist(urls);
    }
}

void
Wallphiller::checkAndUpdateMemory()
{

    //Read memory
    QBuffer shmem_buffer_in;
    shared_memory.lock();
    shmem_buffer_in.setData((char*)shared_memory.constData(),
        shared_memory.size());
    shared_memory.unlock();
    shmem_buffer_in.open(QBuffer::ReadOnly);
    QString old_shmem_title;
    qint64 old_update = 0;
    qint64 old_pid = 0;
    bool show_requested = false;
    {
        QDataStream stream(&shmem_buffer_in);
        stream
            >> old_shmem_title
            >> old_update
            >> old_pid
            >> show_requested;
    }

    //Check show request
    if (show_requested)
    {
        qDebug() << "Instance requested";

        showInstance();
    }

    //Update memory
    //Timestamp must be updated (heartbeat)
    qint64 current_time = QDateTime::currentMSecsSinceEpoch() / 1000; //sec
    QBuffer shmem_buffer_out;
    shmem_buffer_out.open(QBuffer::ReadWrite);
    {
        QDataStream stream(&shmem_buffer_out);
        stream << old_shmem_title; //unchanged
        stream << current_time; //update time
        stream << old_pid; //unchanged
        stream << (bool)false; //show request flag off
    }
    shared_memory.lock();
    char *to = (char*)shared_memory.data();
    const char *from = shmem_buffer_out.data().data();
    memcpy(to, from, shared_memory.size());
    shared_memory.unlock();

}

void
Wallphiller::playlistNameChanged(const QString &name)
{
    QString title(tr("Playlist active"));
    if (!name.isEmpty()) title += ": " + name;
    txt_playlist_title->setText(title);
}

QStringList
Wallphiller::formatFilters()
const
{
    QStringList formats = _read_formats; //"jpg", "png"
    QStringList filters; //"*.jpg", "*.png"
    foreach (QString format, formats)
        filters << QString("*.%1").arg(format);
    return filters;
}

Playlist*
Wallphiller::playlist()
const
{
    return _current_playlist;
}

int
Wallphiller::cacheLimit()
const
{
    return _configured_thumbnail_cache_limit;
}

int
Wallphiller::intervalValue()
const
{
    int value = _configured_interval_value;
    return value;
}

QString
Wallphiller::intervalUnit()
const
{
    QString unit = _configured_interval_unit;
    if (unit.isEmpty() || !intervalValue()) unit = "ONCE";
    return unit;
}

int
Wallphiller::interval()
const
{
    int seconds = 0;

    int value = intervalValue();
    QString unit = intervalUnit();
    if (unit == "SECONDS")
        seconds = value;
    else if (unit == "MINUTES")
        seconds = value * 60;
    else if (unit == "HOURS")
        seconds = value * 60 * 60;
    else if (unit == "NYMINUTES")
        seconds = value * 12;
    else if (unit == "NANOCENTURIES")
        seconds = value * ((52 * 60) + 36);

    if (seconds < 0) seconds = 0; //still not enough overflow protection

    return seconds;
}

int
Wallphiller::position()
const
{
    return _position;
}

QStringList
Wallphiller::sortedAddresses()
const
{
    return _sorted_picture_addresses;
}

DE
Wallphiller::desktopEnvironment()
const
{
    return _de;
}

QString
Wallphiller::changeRoutine()
const
{
    return _change_routine;
}

QString
Wallphiller::changeRoutineCommand()
const
{
    return _change_routine_command;
}

void
Wallphiller::hideInstance()
{
    //hide(); //too early (will reappear)
    QTimer::singleShot(500, this, SLOT(hide()));
}

void
Wallphiller::showInstance()
{
    show(); //make this visible (again)
    setWindowState( //unminimize
        (windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
    raise(); //for Mac
    activateWindow(); //for Windows
}

void
Wallphiller::handleTrayClicked(QSystemTrayIcon::ActivationReason reason)
{
    //Just show context menu if requested
    if (reason == QSystemTrayIcon::Context)
    {
        return;
    }

    //Show window
    showInstance();
    tray_icon->hide();

}

void
Wallphiller::minimizeToTray()
{
    tray_icon->show();
    hide();
}

void
Wallphiller::openSettingsWindow()
{
    if (settings_dialog)
    {
        settings_dialog->show();
        return;
    }

    settings_dialog = new SettingsDialog(this);
    settings_dialog->show();

}

void
Wallphiller::applyChangeRoutine(const QString &routine,
    const QString &command)
{
    QString new_routine(routine);
    QString new_command(command);

    if (new_routine == "command")
    {
    }
    else
    {
        new_routine = "auto";
    }

    _change_routine = new_routine;
    _change_routine_command = new_command;

    QSettings settings;
    settings.setValue("ChangeRoutine", new_routine);
    settings.setValue("ChangeRoutineCommand", new_command);

}

void
Wallphiller::applyInterval(int value, QString unit)
{
    //Apply interval
    _configured_interval_value = value;
    _configured_interval_unit = unit;
    int seconds = interval(); //new timeout
    tmr_next_wallpaper->setInterval(seconds * 1000);

    //Start or stop timer if settings have changed
    if (seconds && !tmr_next_wallpaper->isActive())
    {
        //Timeout configured but timer not running yet
        tmr_next_wallpaper->start();
    }
    else if (!seconds && tmr_next_wallpaper->isActive())
    {
        //Timeout not configured but timer still running
        tmr_next_wallpaper->stop();
    }

    //Save interval
    QSettings settings;
    settings.setValue("IntervalValue", value);
    settings.setValue("IntervalUnit", unit);

}

void
Wallphiller::applyCacheLimit(int max_mb)
{
    //Apply limit
    if (max_mb < 0) max_mb = 0;
    _configured_thumbnail_cache_limit = max_mb;
    thumbnailbox->setCacheLimit(max_mb);

    //Save limit
    QSettings settings;
    settings.setValue("CacheLimit", max_mb);

}

void
Wallphiller::generateList()
{
    //Get generated list
    Playlist *playlist = this->playlist();
    playlist->generate(Playlist::Order::Random);
    QStringList new_list = playlist->pictureAddressList();

    //Apply list
    _sorted_picture_addresses = new_list;

    //Set thumbnails
    //Type is External to load images in the background
    //The Playlist actually does the loading (only it knows how)
    thumbnailbox->setList(_sorted_picture_addresses,
        ThumbnailBox::SourceType::External);

    //TODO notify if playlist empty but don't show annoying message box

}

void
Wallphiller::setPlaylist(Playlist *playlist, int start_index)
{
    //Stop timer
    tmr_next_wallpaper->stop();

    //Clear thumbnail box
    thumbnailbox->clear();

    //Delete old playlist
    if (_current_playlist) _current_playlist->deleteLater();
    _current_playlist = 0;
    _position = -1;
    _sorted_picture_addresses.clear();

    //Reset title
    txt_playlist_title->setText(tr("(No playlist defined)"));

    //Done if no new playlist provided
    if (!playlist) return;

    //Set playlist
    _current_playlist = playlist;

    //Update title (include playlist name, if defined)
    connect(playlist,
            SIGNAL(nameChanged(const QString&)),
            SLOT(playlistNameChanged(const QString&)));
    playlistNameChanged(playlist->name());

    //Connect ThumbnailBox to Playlist
    //Send image requests from thumbnailbox to new playlist
    //Forward image responses from playlist to thumbnailbox
    connect(thumbnailbox,
            SIGNAL(imageRequested(const QString&)),
            playlist,
            SLOT(loadImageInBackground(const QString&)));
    connect(playlist,
            SIGNAL(imageLoaded(const QString&, const QImage&)),
            thumbnailbox,
            SLOT(cacheImage(const QString&, const QImage&)));

    //Generate list and fill ThumbnailBox
    generateList();

    //Start with first (or specified) wallpaper
    //If no change interval is configured,
    //this will be the only automatic wallpaper change.
    selectWallpaper(start_index);

    //Start timer (unless disabled)
    //It might make sense to keep the timer disabled if you only
    //want to change your wallpaper on startup.
    int interval = this->interval(); //seconds
    if (interval)
    {
        tmr_next_wallpaper->setInterval(interval * 1000);
        tmr_next_wallpaper->start();
    }

}

void
Wallphiller::createPlaylist(const QList<QUrl> &addresses, bool empty)
{
    //Formats
    QStringList formats = this->formatFilters();

    //Create playlist
    Playlist *old_playlist = playlist();
    Playlist *playlist;
    if (empty || !old_playlist)
        playlist = new Playlist(formats, this);
    else
        playlist = new Playlist(*old_playlist, this);

    //Fill playlist
    foreach (QUrl address, addresses)
    {
        playlist->add(address);
    }

    //Set new playlist
    setPlaylist(playlist);

}

void
Wallphiller::createPlaylistWithFullDirectory(bool empty)
{
    //Ask for directory
    QStringList formats = this->formatFilters();
    QString pwd;
    QString path;
    path = QFileDialog::getExistingDirectory(this,
        tr("Select wallpaper directory"),
        pwd,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (path.isEmpty()) return; //abort

    //Create playlist
    Playlist *old_playlist = playlist();
    Playlist *playlist;
    if (empty || !old_playlist)
        playlist = new Playlist(formats, this);
    else
        playlist = new Playlist(*old_playlist, this);

    //Add directory to playlist
    playlist->addDirectory(path, true); //recursive

    //Set new playlist
    setPlaylist(playlist);
    playlist->setName(QDir(path).dirName());

}

void
Wallphiller::createPlaylistWithShallowDirectory(bool empty)
{
    //Ask for directory
    QStringList formats = this->formatFilters();
    QString pwd;
    QString path;
    path = QFileDialog::getExistingDirectory(this,
        tr("Select wallpaper directory"),
        pwd,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (path.isEmpty()) return; //abort

    //Create playlist
    Playlist *old_playlist = playlist();
    Playlist *playlist;
    if (empty || !old_playlist)
        playlist = new Playlist(formats, this);
    else
        playlist = new Playlist(*old_playlist, this);

    //Add directory to playlist
    playlist->addDirectory(path, false); //non-recursive

    //Set new playlist
    setPlaylist(playlist);
    playlist->setName(QDir(path).dirName());

}

void
Wallphiller::createPlaylistWithFiles(bool empty)
{
    //Ask for picture files
    QStringList formats = this->formatFilters();
    QString pwd;
    QStringList files;
    QString filter = QString("%1 (%2)").
        arg(tr("Images")).
        arg(formats.join(" "));
    files = QFileDialog::getOpenFileNames(this,
        tr("Select wallpaper files"),
        pwd,
        filter,
        0,
        QFileDialog::DontResolveSymlinks);
    if (files.isEmpty()) return; //abort

    //Create playlist
    Playlist *old_playlist = playlist();
    Playlist *playlist;
    if (empty || !old_playlist)
        playlist = new Playlist(formats, this);
    else
        playlist = new Playlist(*old_playlist, this);

    //Add single files to playlist
    playlist->addFiles(files);

    //Set new playlist
    setPlaylist(playlist);

}

void
Wallphiller::unloadPlaylist()
{
    setPlaylist(0);
}

void
Wallphiller::setWallpaper(const QString &file_path)
{
    qDebug() << "Setting wallpaper...";

    //Check file path
    if (file_path.isEmpty() || !QFile::exists(file_path))
    {
        qWarning() << "Invalid file!";
        qWarning() << file_path;
        return;
    }

    //File uri (file://...)
    QString file_uri = QUrl::fromLocalFile(file_path).toString();

    //Quote because this will be used in a system() call
    QString file_path_escaped = QString(file_path).replace("'", "\'");
    QString file_uri_escaped = QString(file_uri).replace("'", "\'");
    QString file_path_quoted = QString("'%1'").arg(file_path_escaped);
    QString file_uri_quoted = QString("'%1'").arg(file_uri_escaped);

    //Configured wallpaper change routine
    DE de = DE::None;
    QString cmd;
    if (changeRoutine() == "command")
    {
        cmd = changeRoutineCommand();
    }
    else
    {
        de = desktopEnvironment();
    }

    //Input and system():
    //This program will NOT blindly strcat the input string to some possibly
    //user-defined custom command and pass it to system().
    //This could be pretty dangerous.
    //Although it's impossible to protect a user from his own stupid input,
    //we can at least try to do some very basic sanity checking.
    //For example, only one command should be executed, so the user-defined
    //command string should not contain a semicolon.
    //Also, we might replace the uri variable (like %u)
    //with the path wrapped in single quotes to prevent
    //whitespace-related crap.
    //Also, even though we normally use a temporary file with a hardcoded
    //name like "wallpaper.png", it should not be possible to cause harm
    //by using a filename like "file ;badcommand".
    //However, as long as the filename is hardcoded,
    //all we need to check is the user-defined command.

    //Command expansion:
    //%u = single uri
    //%f = single file path

    //Call configured change routine
    bool done = false;
    bool no_action = false;
    switch (de)
    {
        case DE::None:
        no_action = cmd.isEmpty();
        break;

        case DE::Gnome:
        cmd = "gsettings set org.gnome.desktop.background picture-uri \%u";
        break;

        case DE::Mate:
        cmd = "gsettings set org.mate.desktop.background picture-uri \%u";
        break;

        case DE::Cinnamon:
        cmd = "gsettings set org.cinnamon.desktop.background picture-uri \%u";
        break;

        //TODO XFCE
        //TODO LXDE
        //TODO KDE

        case DE::Windows:
        #if defined(_WIN32)
        SystemParametersInfo(
            SPI_SETDESKWALLPAPER,
            0,
            file_path.toLatin1().data(),
            SPIF_SENDWININICHANGE | SPIF_UPDATEINIFILE);
        done = true;
        #endif
        break;

        default:
        no_action = true;
        break;
    }

    //Run command
    if (!done && !cmd.isEmpty())
    {
        //Complete command
        cmd.replace("\%u", file_uri_quoted);
        cmd.replace("\%f", file_path_quoted);

        //Run
        //We've alredy made sure that the file path argument is quoted.
        //The change command itself could be a user-defined custom command,
        //which could be pretty much anything.
        int code = system(cmd.toUtf8().constData());
        if (code)
        {
            qWarning() << "Error, wallpaper change command returned" << code;
        }
        done = true;
    }

    //Anything to do
    if (no_action)
    {
        //TODO graphical alert
        qWarning() << "No configured command, no action!";
    }

}

void
Wallphiller::selectWallpaper(int index)
{
    //Poor people's mutex
    //static bool is_running = false;

    //Check index
    QStringList list(sortedAddresses()); //sorted when list (re)started
    if (index < 0 || index >= list.count()) return; //index out of bounds

    //Abort if not loaded
    if (!playlist()) return;

    //Set index
    _position = index;

    //Update thumbnail selection but prevent infinite loop!
    //Selecting a thumbnail will trigger this function/slot!
    thumbnailbox->select(index, false); //no signal, prevent infinite loop!
    thumbnailbox->ensureItemVisible(index);

    //Load picture
    QString address = list.at(index); //local path or remote address
    QImage image = playlist()->loadImage(address);
    if (image.isNull())
    {
        //Picture empty, abort
        qWarning() <<
            "Picture empty (probably not found or error retrieving)";
        return;
    }

    //Save picture as temporary file
    QSettings settings;
    QString config_dir = QFileInfo(settings.fileName()).absolutePath();
    QList<QByteArray> write_formats = QImageWriter::supportedImageFormats();
    QString suffix;
    #if defined(_WIN32)
    suffix = ".bmp";
    #else
    if (write_formats.contains("jpg"))
        suffix = ".jpg";
    else if (write_formats.contains("png"))
        suffix = ".png";
    else if (write_formats.contains("bmp"))
        suffix = ".bmp";
    #endif
    QString temporary_file;
    if (!suffix.isEmpty())
        temporary_file = config_dir + "/wallpaper" + suffix;
    if (temporary_file.isEmpty() || !image.save(temporary_file))
    {
        qWarning() << "Saving temporary image file failed!";
        qDebug() << temporary_file;
        return;
    }

    //Set wallpaper
    setWallpaper(temporary_file);

    //Restart timer (if running)
    //This is done on purpose so that selecting a wallpaper manually
    //will reset the clock to keep that picture on display the same time
    //as every other picture.
    //Otherwise, it would feel unnatural if the wallpaper is changed again
    //five seconds after one has been selected manually.
    if (tmr_next_wallpaper->isActive())
    {
        tmr_next_wallpaper->start();
    }

}

void
Wallphiller::previous()
{
    int new_position = position() - 1;
    if (new_position == -1)
    {
        //End of list
        //Regenerate list
        generateList();

        //Continue at end
        new_position = _sorted_picture_addresses.count() - 1;
    }

    selectWallpaper(new_position);
}

void
Wallphiller::next()
{
    int new_position = position() + 1;
    if (new_position == sortedAddresses().count())
    {
        //End of list
        //Regenerate list
        generateList();

        //Continue at beginning
        new_position = 0;
    }

    selectWallpaper(new_position);
}

