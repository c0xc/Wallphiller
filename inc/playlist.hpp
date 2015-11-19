#ifndef PLAYLIST_HPP
#define PLAYLIST_HPP

#include <cassert>
#include <cstdlib>
#include <ctime>

#include <QObject>
#include <QPixmap>
#include <QUrl>
#include <QThread>
#include <QMultiMap>
#include <QVariantMap>

#include "scan.hpp"

namespace PlaylistComponents { class Loader; } //I ♥ C++

class Playlist : public QObject
{
    Q_OBJECT

signals:

    void
    nameChanged(const QString &name);

    void
    definitionChanged();

    void
    generated();

    void
    imageLoaded(const QString &address, const QImage &image);

public:

    enum class Order
    {
        None         = 0,
        Descending   = 1 << 0,
        Alphabetical = 1 << 1,
        Random       = 1 << 2,
    };

    Playlist(const QStringList &formats, QObject *parent = 0);

    Playlist(const QByteArray &serialized, QObject *parent = 0);

    Playlist(const Playlist &other, QObject *parent = 0);

    QByteArray
    toByteArray() const;

private:

    Playlist
    operator=(const Playlist &other);

    QStringList
    _formats;

    QString
    _name;

    QStringList
    _added_picture_files;

    QStringList
    _added_nonrecursive_dirs;

    QStringList
    _added_recursive_dirs;

    QList<QUrl>
    _generated_picture_address_list;

    int
    _loader_thread_maximum_count;

    QMap<QUrl, QThread*>
    _running_image_loaders;

    int
    loaderThreadLimit();

    int
    runningLoaderThreads() const;

private slots:

    void
    receiveImage(const QUrl &url, const QImage &image);

public:

    QString
    name() const;

    bool
    isValidPicture(const QString &path) const;

    QStringList
    recursiveDirectories() const;

    QStringList
    nonrecursiveDirectories() const;

    QStringList
    directories() const;

    QStringList
    localPictureFiles() const;

    QList<QUrl>
    pictureAddresses() const;

    QStringList
    pictureAddressList() const;

    QImage
    loadImage(const QUrl &address) const;

public slots:

    void
    loadImageInBackground(const QUrl &address);

    void
    loadImageInBackground(const QString &address);

    void
    setName(const QString &name);

    void
    clear();

    bool
    addDirectory(const QString &path, bool recursive = false);

    void
    removeDirectory(const QString &path);

    bool
    addFile(const QString &path);

    int
    addFiles(const QStringList &paths);

    void
    removeFile(const QString &path);

    bool
    add(const QUrl &item);

    void
    generate(Order order = Order::None);

    void
    sort(Order order);

};

class PlaylistComponents::Loader : public QObject
{
    Q_OBJECT

signals:

    void
    finished();

    void
    loaded(const QUrl &url, const QImage &image);

public:

    Loader(const QVariantMap &runtime_data);

    QMap<QUrl, QImage>
    loadImages();

public slots:

    bool
    addUrl(const QUrl &url);

    void
    process();

private slots:

    QImage
    loadImage(const QUrl &url);

private:

    QVariantMap
    _runtime_data;

    QList<QUrl>
    _urls_to_be_loaded;

};

#endif
