#include "playlist.hpp"

/*! \class Playlist
 *
 * \brief The Playlist class provides an interface to define
 * a dynamic list of pictures.
 *
 * A playlist is used to collect pictures from various sources.
 * The most straight-forward example would be to define a fixed list
 * of picture files.
 * But it's also possible to define a directory, to be scanned recursively.
 * Additional sources are also possible.
 * Regardless of the input method, the list of pictures has to be
 * generated before it can be requested by the calling class.
 * Regenerating the list might be necessary after files have been added
 * to a directory that's being tracked.
 * If pictures are collected from a remote location, regenerating
 * the list will also be necessary to reflect changes in that location.
 *
 * The main feature of this class is to provide a dynamic list of pictures
 * based on one or more defined sources without actually loading
 * all the pictures at the same time.
 * This is very important because if Jon Doe defines a playlist
 * with his collection of 20k high-def pictures, the program
 * should not use as much memory as Firefox.
 * It should never be necessary to load all of those pictures
 * in memory at the same time.
 * After the picture list has been generated, it can be requested.
 * This list contains no actual pictures.
 * It's just a list of identifiers/addresses.
 *
 * Since an address may be a remote address
 * (depending on the playlist), the loader provided by this
 * Playlist object must be used to retrieve the image.
 * Another class should not attempt to parse such an address directly.
 *
 * If the playlist contains pictures from a remote location
 * that requires authentication, the loader will take care of this process.
 * Once all required information has been provided to the playlist,
 * loadImage() will take the address and return a QImage object.
 * Alternatively, loadImageInBackground() can be used to let this
 * process run in another thread.
 *
 */

/*!
 * Constructs a Playlist with the given parent (optional).
 */
Playlist::Playlist(const QStringList &formats, QObject *parent)
        : QObject(parent),
          _formats(formats),
          _loader_thread_maximum_count(0)
{
}

/*!
 * Constructs a Playlist with the given parent (optional).
 * It is restored from serialized.
 */
Playlist::Playlist(const QByteArray &serialized, QObject *parent)
        : QObject(parent),
          _loader_thread_maximum_count(0)
{
    QDataStream stream(serialized);

    //Essential definition
    stream >> _formats;
    stream >> _name;

    //Added sources
    stream >> _added_picture_files;
    stream >> _added_nonrecursive_dirs;
    stream >> _added_recursive_dirs;

    //Generated list
    stream >> _generated_picture_address_list;

}

/*!
 * Constructs a Playlist that is a copy of other.
 */
Playlist::Playlist(const Playlist &other, QObject *parent)
        : Playlist(other.toByteArray(), parent)
{
}

/*!
 * Serializes this playlist into a QByteArray.
 */
QByteArray
Playlist::toByteArray()
const
{
    QByteArray bytes;
    QDataStream stream(&bytes, QIODevice::ReadWrite);
    //QDataStream version not defined

    //Essential definition
    stream << _formats;
    stream << _name;

    //Added sources
    stream << _added_picture_files;
    stream << _added_nonrecursive_dirs;
    stream << _added_recursive_dirs;

    //Generated list
    stream << _generated_picture_address_list;

    return bytes;
}

int
Playlist::loaderThreadLimit()
{
    //Calculate loader thread limit once
    if (!_loader_thread_maximum_count)
    {
        int new_limit = QThread::idealThreadCount();
        if (new_limit < 1) new_limit = 1; //we want to write 2...
        _loader_thread_maximum_count = new_limit;
    }
    return _loader_thread_maximum_count;
}

int
Playlist::runningLoaderThreads()
const
{
    int running_count = 0;

    //Count running threads (excluding queued ones)
    foreach (QThread *thread, _running_image_loaders.values())
    {
        //Count if running (not just queued)
        if (thread->isFinished()) continue; //it's 3 AM
        if (thread->isRunning()) running_count++;
    }

    return running_count;
}

void
Playlist::receiveImage(const QUrl &url, const QImage &image)
{
    //Forward loaded image to whoever is listening
    //This is the FIRST action, this is important!
    emit imageLoaded(url.toString(), image);

    //Remove from "running" queue AFTERWARDS
    //This is done AFTER the result has been forwarded!
    //Otherwise we might see identical requests coming in
    //before the first result has arrived.
    _running_image_loaders.remove(url); //should be 1

    //Queued threads waiting to be started
    QList<QThread*> waiting_threads;
    foreach (QThread *thread, _running_image_loaders.values())
    {
        //Add thread if it's not running yet
        if (thread->isFinished()) continue; //it's 3 AM
        if (!thread->isRunning()) waiting_threads << thread;
    }

    //Start queued threads that have been waiting
    //Prepared threads are queued and not started if too many threads running
    foreach (QThread *thread, waiting_threads)
    {
        if (runningLoaderThreads() < loaderThreadLimit())
        {
            //Thread limit not exceeded yet
            //Start waiting thread
            thread->start();
        }
    }

}

/*!
 * Returns the name of the playlist.
 */
QString
Playlist::name()
const
{
    return _name; //might be empty
}

/*!
 * Checks if the file located at given path is a valid picture.
 */
bool
Playlist::isValidPicture(const QString &path)
const
{
    return !QImage(path).isNull();
}

/*!
 * Returns list of directories in playlist that are scanned recursively.
 */
QStringList
Playlist::recursiveDirectories()
const
{
    return _added_recursive_dirs;
}

/*!
 * Returns list of directories in playlist that are not scanned recursively.
 */
QStringList
Playlist::nonrecursiveDirectories()
const
{
    return _added_nonrecursive_dirs;
}

/*!
 * Returns a list of all directories in this playlist.
 */
QStringList
Playlist::directories()
const
{
    QStringList dirs;

    //A directory can not be in both lists at the same time
    foreach (QString dir, recursiveDirectories())
    {
        dirs << dir;
    }
    foreach (QString dir, nonrecursiveDirectories())
    {
        dirs << dir;
    }

    return dirs;
}

/*!
 * Returns a list of all files which have been added to this playlist.
 *
 * This does not include files in added directories.
 */
QStringList
Playlist::localPictureFiles()
const
{
    return _added_picture_files;
}

/*!
 * Returns the generated list of picture address objects.
 *
 * This list will be empty if generate() has never been called.
 * Likewise, it might be obsolete if generate() has not been called
 * after a source change.
 *
 * No other module should try to load such a url directly
 * because a url isn't always a local file path.
 * This playlist has a getter that returns a QImage object.
 */
QList<QUrl>
Playlist::pictureAddresses()
const
{
    QList<QUrl> urls = _generated_picture_address_list;
    return urls;
}

/*!
 * This is a convenience function.
 *
 * Returns picture addresses as strings.
 *
 * No other module should try to load such an address directly
 * because an address isn't always a local file path.
 * This playlist has a getter that returns a QImage object.
 */
QStringList
Playlist::pictureAddressList()
const
{
    QStringList addresses;
    foreach (QUrl address, pictureAddresses())
        addresses << address.toString();
    return addresses;
}

/*!
 * Loads the image at the given address and returns a QImage object.
 *
 * The address should be one of the addresses in the generated list.
 *
 * A fresh version of the image is retrieved. No cache involved.
 *
 * The request will block this thread.
 * Consider using loadImageInBackground() instead.
 */
QImage
Playlist::loadImage(const QUrl &address)
const
{
    //Include components
    using namespace PlaylistComponents;

    //Load image (blocking)
    QVariantMap data;
    Loader loader(data);
    loader.addUrl(address);
    QImage image = loader.loadImages().value(address);

    return image;
}

/*!
 * Loads the image at the given address in a background process.
 * The image will be returned by a signal: imageLoaded()
 *
 * The address should be one of the addresses in the generated list.
 */
void
Playlist::loadImageInBackground(const QUrl &address)
{
    //Include components
    using namespace PlaylistComponents;

    //A few notes on multi-threaded loader jobs.
    //Since a request does not block the main thread,
    //20 more identical requests could arrive before
    //the first one has finished.
    //This is prevented by simply ignoring all identical requests
    //while the first one is running.
    //However, in our very first attempt, we saw that new identical
    //loader requests were coming in after the first one had finished,
    //before it was transferred to all the listeners.
    //This is an awkward race condition,
    //it causes all images to be loaded twice.
    //This was caused by the signal being emitted after
    //the request had been removed from the queue.

    //Check if image already being loaded
    if (_running_image_loaders.contains(address))
    {
        //Already being loaded, prevent multiple identical requests
        return;
    }

    //Put it in "running" queue
    //Will be removed by receiver (slot)
    _running_image_loaders[address] = 0;

    //Create loader object and set url to be loaded
    QVariantMap data; //runtime/session data (like username and password)
    Loader *loader = new Loader(data);
    loader->addUrl(address);

    //Move loader to new thread
    QThread *thread = new QThread;
    _running_image_loaders[address] = thread;
    loader->moveToThread(thread);

    //Start worker when thread starts
    connect(thread,
            SIGNAL(started()),
            loader,
            SLOT(process()));

    //Retrieve image when done
    connect(loader,
            SIGNAL(loaded(const QUrl&, const QImage&)),
            this,
            SLOT(receiveImage(const QUrl&, const QImage&)));

    //Stop thread when worker done (stops event loop)
    connect(loader,
            SIGNAL(finished()),
            thread,
            SLOT(quit()));

    //Delete worker when done
    connect(loader,
            SIGNAL(finished()),
            loader,
            SLOT(deleteLater()));

    //Delete thread when thread done (event loop stopped)
    connect(thread,
            SIGNAL(finished()),
            thread,
            SLOT(deleteLater()));

    //A better thread management might be necessary. TODO
    //We can't spawn an infinite number of threads.
    //Too many threads aren't just inefficient, they use a lot of memory.
    //Memory usage can easily skyrocket above 1.5 GB in seconds
    //if there are many requests coming in.
    //This is why we use a simple form of thread management,
    //we simply don't start more threads than the defined limit allows us.
    //All other threads are prepared and queued but not started immediately.
    //Whenever a thread has finished, the receiver checks if there are
    //queued thread waiting to be started and starts some of them,
    //always making sure the thread limit is not exceeded.
    //This works, but it could be improved.
    //Many requests means many threads that are constantly being
    //created and destroyed, which is wasteful.
    //As soon as the programmer is sufficiently annoyed by this,
    //a more complex thread management will probably be implemented,
    //using a pool or sub-threads (ImageLoader). Or not.

    //Start thread or queue it if too many threads running
    //Queued threads will be started by the receiveImage() slot
    //after another thread has finished,
    //when the number of running threads is below the threshold.
    if (runningLoaderThreads() < loaderThreadLimit())
    {
        thread->start();
    }

}

/*!
 * This is a convenience function.
 * The address can be provided as string.
 */
void
Playlist::loadImageInBackground(const QString &address)
{
    loadImageInBackground(QUrl(address));
}

/*!
 * Sets name of playlist.
 */
void
Playlist::setName(const QString &name)
{
    _name = name;
    emit nameChanged(name);
}

/*!
 * Removes all picture sources from this playlist.
 */
void
Playlist::clear()
{
    _added_picture_files.clear();
    _added_nonrecursive_dirs.clear();
    _added_recursive_dirs.clear();

    emit definitionChanged();
}

/*!
 * Adds a directory as picture source.
 * The directory will be scanned recursively if that boolean flag is true.
 * Returns true on success and false on error.
 */
bool
Playlist::addDirectory(const QString &path, bool recursive)
{
    //Validate path
    if (!QFileInfo(path).isDir()) return false;

    //A directory can either be set in recursive mode or not - not both
    if (directories().contains(path)) return false; //prevent duplicate

    //Add
    if (recursive)
    {
        _added_recursive_dirs << path;
    }
    else
    {
        _added_nonrecursive_dirs << path;
    }

    emit definitionChanged();
    return true;
}

/*!
 * Removes the given directory from the playlist.
 */
void
Playlist::removeDirectory(const QString &path)
{
    //Remove directory (does not have to be valid anymore)

    //Recursive list
    if (_added_recursive_dirs.contains(path))
    {
        _added_recursive_dirs.removeAll(path);
        emit definitionChanged();
    }

    //Non-recursive list
    if (_added_nonrecursive_dirs.contains(path))
    {
        _added_nonrecursive_dirs.removeAll(path);
        emit definitionChanged();
    }

}

/*!
 * Adds a single local picture file to the playlist.
 * Returns true on success and false on error.
 */
bool
Playlist::addFile(const QString &path)
{
    //Validate path
    if (!QFileInfo(path).isFile()) return false;

    //Validate picture
    if (!isValidPicture(path)) return false;

    //Prevent duplicate
    if (_added_picture_files.contains(path)) return false;

    //Add
    _added_picture_files << path;

    emit definitionChanged();
    return true;
}

/*!
 * This is a convenience function.
 *
 * Adds a list of local picture files and returns the number of
 * successfully added files.
 */
int
Playlist::addFiles(const QStringList &paths)
{
    int success_count = 0;
    foreach (QString path, paths)
    {
        if (addFile(path)) success_count++;
    }
    return success_count;
}

/*!
 * Removes the file from the playlist.
 */
void
Playlist::removeFile(const QString &path)
{
    //Remove file (does not have to be valid anymore)
    if (_added_picture_files.contains(path))
    {
        _added_picture_files.removeAll(path);
        emit definitionChanged();
    }

}

/*!
 * Adds path to playlist, if possible.
 * Returns true on success and false on error.
 * Depending on the referenced target, either a file or a directory is added.
 */
bool
Playlist::add(const QUrl &item)
{
    //Add path (probably local path)
    //Note that remote sources are usually not defined by a
    //simple QUrl address, but rather as an API configuration or similar.

    if (item.isLocalFile())
    {
        QString path = item.toLocalFile();
        QFileInfo fi(path);
        if (fi.isFile()) return addFile(path);
        else if (fi.isDir()) return addDirectory(path);
    }

    return false;
}

/*!
 * Generates the list of pictures based on the available sources.
 *
 * This function must be called after all required sources have been added
 * or whenever a source may have changed.
 */
void
Playlist::generate(Order order)
{
    QList<QUrl> &generated_list = _generated_picture_address_list; //reference
    generated_list.clear();

    //Formats
    QStringList formats = _formats;

    //Manually selected local files
    foreach (QString file, localPictureFiles())
    {
        generated_list << QUrl::fromLocalFile(file);
    }

    //Non-recursive directories
    foreach (QString dir, nonrecursiveDirectories())
    {
        QStringList files =
            Scan::scan(dir, formats, Scan::RecursionMode::NonRecursive);
        foreach (QString file, files)
        {
            generated_list << QUrl::fromLocalFile(file);
        }
    }

    //Recursive directories
    foreach (QString dir, recursiveDirectories())
    {
        QStringList files =
            Scan::scan(dir, formats, Scan::RecursionMode::Recursive);
        foreach (QString file, files)
        {
            generated_list << QUrl::fromLocalFile(file);
        }
    }

    //Sort
    if (order != Order::None)
        sort(order);

    emit generated();
}

/*!
 * Sorts the generated list.
 */
void
Playlist::sort(Order order)
{
    QList<QUrl> &urls = _generated_picture_address_list; //reference

    QList<QUrl> sorted_urls = urls;

    //Alphabetical
    if ((int)order & (int)Order::Alphabetical) //why do enums suck
    {
        QMultiMap<QUrl, QUrl> map;
        foreach (QUrl url, sorted_urls)
            map.insert(url, url);
        sorted_urls = map.values();
    }

    //Random
    if ((int)order & (int)Order::Random)
    {
        QMultiMap<int, QUrl> map;
        srand(time(0));
        int count = sorted_urls.size();
        foreach (QUrl url, sorted_urls)
        {
            //Generated random key might be duplicate, hence QMultiMap
            int random_key = rand() % count;
            map.insert(random_key, url);
        }
        sorted_urls = map.values();
    }

    //Invert
    if ((int)order & (int)Order::Descending)
    {
        for (int i = 0, ii = sorted_urls.size(); i < (ii / 2); i++)
            sorted_urls.swap(i, ii - (1 + i));
    }

    urls = sorted_urls;
}

PlaylistComponents::Loader::Loader(const QVariantMap &runtime_data)
                  : _runtime_data(runtime_data)
{
}

QMap<QUrl, QImage>
PlaylistComponents::Loader::loadImages()
{
    //List of urls to be loaded
    QList<QUrl> urls = _urls_to_be_loaded;

    //Load images (return them all at once)
    QMap<QUrl, QImage> url_image_map;
    foreach (QUrl url, urls)
    {
        QImage image = loadImage(url);
        url_image_map[url] = image;
    }

    return url_image_map;
}

bool
PlaylistComponents::Loader::addUrl(const QUrl &url)
{
    //Check url
    if (url.isEmpty()) return false;

    //Add url
    _urls_to_be_loaded << url;

    return true;
}

void
PlaylistComponents::Loader::process()
{
    //List of urls to be loaded
    QList<QUrl> urls = _urls_to_be_loaded;

    //Load images one by one (return one by one, 21st century style)
    foreach (QUrl url, urls)
    {
        QImage image = loadImage(url);
        emit loaded(url, image);
    }

    //Done
    emit finished();
}

QImage
PlaylistComponents::Loader::loadImage(const QUrl &url)
{
    //Fetch url and return image
    //Any kind of session data that might be required to access this url
    //would be stored in this object. So this could not be a free function.

    //Load image with given address
    QImage image;
    if (url.isLocalFile())
    {
        //Local file
        image = QImage(url.toLocalFile());
    }
    //TODO support other sources

    return image;
}

