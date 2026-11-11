#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QProcess>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

#include "framegrab.h"
#include "library.h"
#include "playlists.h"
#include "theme.h"
#include "thumbnailer.h"
#include "translator.h"

class Tests : public QObject {
    Q_OBJECT

private slots:
    // Nothing here may touch the real history: default paths go to a test
    // location instead.
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        // Not the live Hyprland: its events would trigger refreshes and hide
        // a broken rerun in the rounding test.
        qunsetenv("HYPRLAND_INSTANCE_SIGNATURE");
    }

    void formatsTime()
    {
        QCOMPARE(Library::format(0), QString("0:00"));
        QCOMPARE(Library::format(65.9), QString("1:05"));
        QCOMPARE(Library::format(3600 + 2 * 60 + 3), QString("1:02:03"));
        QCOMPARE(Library::format(-4), QString("0:00"));
    }

    void creditsCountAsFinished()
    {
        QVERIFY(!Library::isFinished(10, 0));
        QVERIFY(!Library::isFinished(1200, 2400));
        QVERIFY(Library::isFinished(2350, 2400)); // inside the last 5%
        QVERIFY(!Library::isFinished(100, 120));
        QVERIFY(Library::isFinished(112, 120)); // at least the last 10 s
    }

    void resumesOnlyWhatIsWorthResuming()
    {
        QTemporaryDir data, cache;
        Library library(data.path(), cache.path());
        library.update("/films/a.mkv", "A", 600, 2400);
        library.update("/films/b.mkv", "B", 5, 2400);    // barely started
        library.update("/films/c.mkv", "C", 2390, 2400); // finished
        library.update("/films/d.mkv", "D", 40, 90);     // short clip
        QCOMPARE(library.resumePosition("/films/a.mkv"), 600.0);
        QCOMPARE(library.resumePosition("/films/b.mkv"), 0.0);
        QCOMPARE(library.resumePosition("/films/c.mkv"), 0.0);
        QCOMPARE(library.resumePosition("/films/d.mkv"), 0.0);
        QCOMPARE(library.continueWatching().size(), 1);
        QCOMPARE(library.recent().size(), 3);
    }

    void historySurvivesARestart()
    {
        QTemporaryDir data, cache;
        {
            Library library(data.path(), cache.path());
            library.update("/films/a.mkv", "A", 600, 2400);
        }
        Library library(data.path(), cache.path());
        QCOMPARE(library.resumePosition("/films/a.mkv"), 600.0);
        library.forget("/films/a.mkv");
        QCOMPARE(library.resumePosition("/films/a.mkv"), 0.0);
    }

    void twoWindowsKeepEachOthersProgress()
    {
        QTemporaryDir data, cache;
        Library first(data.path(), cache.path());
        Library second(data.path(), cache.path());
        first.update("/films/a.mkv", "A", 600, 2400);
        second.update("/films/b.mkv", "B", 700, 2400);
        first.save();
        second.save();
        Library fresh(data.path(), cache.path());
        QCOMPARE(fresh.resumePosition("/films/a.mkv"), 600.0);
        QCOMPARE(fresh.resumePosition("/films/b.mkv"), 700.0);
    }

    void forgettingSticksAcrossWindows()
    {
        QTemporaryDir data, cache;
        {
            Library seed(data.path(), cache.path());
            seed.update("/films/x.mkv", "X", 600, 2400);
        }
        Library first(data.path(), cache.path());
        Library second(data.path(), cache.path());
        second.forget("/films/x.mkv");
        first.update("/films/y.mkv", "Y", 300, 2400);
        first.save();
        second.save();
        Library fresh(data.path(), cache.path());
        QCOMPARE(fresh.resumePosition("/films/x.mkv"), 0.0);
        QCOMPARE(fresh.resumePosition("/films/y.mkv"), 300.0);
    }

    void forgettingSticksWhicheverWindowSavesFirst()
    {
        QTemporaryDir data, cache;
        {
            Library seed(data.path(), cache.path());
            seed.update("/films/x.mkv", "X", 600, 2400);
        }
        Library first(data.path(), cache.path());
        Library second(data.path(), cache.path());
        second.forget("/films/x.mkv");
        second.save();
        first.update("/films/y.mkv", "Y", 300, 2400);
        first.save(); // still holds x from before the removal
        QCOMPARE(first.resumePosition("/films/x.mkv"), 0.0);
        Library fresh(data.path(), cache.path());
        QCOMPARE(fresh.resumePosition("/films/x.mkv"), 0.0);
        QCOMPARE(fresh.resumePosition("/films/y.mkv"), 300.0);
    }

    void playingAgainUndoesARemoval()
    {
        QTemporaryDir data, cache;
        Library first(data.path(), cache.path());
        first.update("/films/x.mkv", "X", 600, 2400);
        first.forget("/films/x.mkv");
        first.update("/films/x.mkv", "X", 700, 2400);
        first.save();
        Library fresh(data.path(), cache.path());
        QCOMPARE(fresh.resumePosition("/films/x.mkv"), 700.0);
    }

    void newPlaysWinEvenIfTheClockWentBack()
    {
        QTemporaryDir data, cache;
        const qint64 future = QDateTime::currentMSecsSinceEpoch() + 3600 * 1000;
        touch(data.filePath("history.json"),
              QString(R"({"version":1,"items":[{"path":"/films/a.mkv","title":"A","position":100,"duration":2400,"lastPlayed":%1}]})")
                  .arg(future).toUtf8());
        {
            Library library(data.path(), cache.path());
            library.update("/films/a.mkv", "A", 1500, 2400);
            library.save();
            QCOMPARE(library.resumePosition("/films/a.mkv"), 1500.0);
        }
        Library fresh(data.path(), cache.path());
        QCOMPARE(fresh.resumePosition("/films/a.mkv"), 1500.0);
    }

    void capturesInTheSameSecondGetTheirOwnNames()
    {
        QTemporaryDir data, cache;
        Library library(data.path(), cache.path());
        library.setCaptureDirectory(data.filePath("captures"));
        const QString first = library.frameCaptureFile("Film", 61);
        const QString second = library.frameCaptureFile("Film", 61.5);
        QVERIFY(first != second);
    }

    void forgetSticksEvenIfTheClockWentBack()
    {
        QTemporaryDir data, cache;
        const qint64 future = QDateTime::currentMSecsSinceEpoch() + 3600 * 1000;
        touch(data.filePath("history.json"),
              QString(R"({"version":1,"items":[{"path":"/films/a.mkv","title":"A","position":600,"duration":2400,"lastPlayed":%1}]})")
                  .arg(future).toUtf8());
        Library other(data.path(), cache.path()); // loaded before the removal
        {
            Library library(data.path(), cache.path());
            library.forget("/films/a.mkv");
            library.save();
            QCOMPARE(library.resumePosition("/films/a.mkv"), 0.0);
        }
        other.update("/films/b.mkv", "B", 300, 2400);
        other.save();
        Library fresh(data.path(), cache.path());
        QCOMPARE(fresh.resumePosition("/films/a.mkv"), 0.0);
        QCOMPARE(fresh.resumePosition("/films/b.mkv"), 300.0);
    }

    void historyDamagedWhileOpenIsKeptAside()
    {
        QTemporaryDir data, cache;
        Library library(data.path(), cache.path());
        library.update("/films/a.mkv", "A", 600, 2400);
        library.save();
        touch(data.filePath("history.json"), "{ damaged");
        library.update("/films/b.mkv", "B", 300, 2400);
        library.save();
        QCOMPARE(QDir(data.path()).entryList({"history.json.bad-*"}).size(), 1);
    }

    void unreadableHistoryIsKeptAside()
    {
        QTemporaryDir data, cache;
        touch(data.filePath("history.json"), "{ this is not json");
        Library library(data.path(), cache.path());
        library.update("/films/a.mkv", "A", 600, 2400);
        library.save();
        const QStringList aside = QDir(data.path()).entryList({"history.json.bad-*"});
        QCOMPARE(aside.size(), 1);
        QFile kept(data.filePath(aside.first()));
        QVERIFY(kept.open(QIODevice::ReadOnly));
        QCOMPARE(kept.readAll(), QByteArray("{ this is not json"));
    }

    void namesTrackLanguages()
    {
        Library library;
        QCOMPARE(library.languageName("und"), QString());
        QCOMPARE(library.languageName(""), QString());
        QCOMPARE(library.languageName("eng"), QString("English"));
        QCOMPARE(library.languageName("en-US"), QString("English"));
        QCOMPARE(library.languageName("ger"), QString("German"));
        QCOMPARE(library.languageName("pt-BR"), QString("Portuguese"));
        QCOMPARE(library.languageName("qqq"), QString("qqq"));
    }

    void singleFileQueuesItsSiblingsInNaturalOrder()
    {
        QTemporaryDir dir;
        for (const char *name : {"Episode 10.mkv", "Episode 2.mkv", "Episode 1.mkv", "notes.txt", "song.mp3"})
            touch(dir.filePath(name));
        const QVariantMap result = Library().expand({dir.filePath("Episode 2.mkv")});
        const QStringList files = result.value("files").toStringList();
        QCOMPARE(files.size(), 3);
        QVERIFY(files[0].endsWith("Episode 1.mkv"));
        QVERIFY(files[1].endsWith("Episode 2.mkv"));
        QVERIFY(files[2].endsWith("Episode 10.mkv"));
        QCOMPARE(result.value("index").toInt(), 1);
    }

    void folderPlaysEverythingInIt()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("Season 2");
        touch(dir.filePath("Season 2/b.mkv"));
        touch(dir.filePath("a.mp4"));
        touch(dir.filePath("cover.jpg"));
        const QStringList files = Library().expand({QUrl::fromLocalFile(dir.path()).toString()})
                                      .value("files").toStringList();
        QCOMPARE(files.size(), 2);
    }

    void folderAndPlaylistFilesBecomePlaylists()
    {
        QTemporaryDir dir;
        touch(dir.filePath("a.mkv"));
        QVariantMap result = Library().expand({dir.path()});
        QCOMPARE(result.value("collection").toMap().value("kind").toString(), QString("folder"));
        // A single episode only queues its siblings; it's not a playlist.
        QVERIFY(!Library().expand({dir.filePath("a.mkv")}).contains("collection"));

        QFile m3u(dir.filePath("Watch list.m3u8"));
        QVERIFY(m3u.open(QIODevice::WriteOnly));
        m3u.write("\xEF\xBB\xBF#EXTM3U\n#EXTINF:10,First\nShow/one.mkv\n\n/abs/two.mkv\r\n"
                  "file:///abs/three%20x.mkv\nhttps://example.com/live.m3u8\n");
        m3u.close();
        result = Library().expand({m3u.fileName()});
        QCOMPARE(result.value("files").toStringList(),
                 (QStringList{dir.filePath("Show/one.mkv"), "/abs/two.mkv", "/abs/three x.mkv",
                              "https://example.com/live.m3u8"}));
        const QVariantMap collection = result.value("collection").toMap();
        QCOMPARE(collection.value("kind").toString(), QString("file"));
        QCOMPARE(collection.value("name").toString(), QString("Watch list"));

        // "Show/" and "Show" are one folder, and one playlist.
        QCOMPARE(Library().expand({dir.path() + "/"}).value("collection").toMap().value("source").toString(),
                 QDir::cleanPath(dir.path()));

        // A colon doesn't make a link; Windows backslashes and Latin-1 work.
        QFile old(dir.filePath("old.m3u"));
        QVERIFY(old.open(QIODevice::WriteOnly));
        old.write("Ep1:Pilot.mkv\nMusic\\Artist\\a.mp3\nCaf\xE9.mkv\n");
        old.close();
        QCOMPARE(Library::readPlaylistFile(old.fileName()),
                 (QStringList{dir.filePath("Ep1:Pilot.mkv"), dir.filePath("Music/Artist/a.mp3"),
                              dir.filePath(QString::fromUtf8("Caf\xC3\xA9.mkv"))}));

        QFile pls(dir.filePath("radio.pls"));
        QVERIFY(pls.open(QIODevice::WriteOnly));
        pls.write("[playlist]\nFile1=http://radio.example/stream\nTitle1=Radio\nfile2=b.mp3\nNumberOfEntries=2\n");
        pls.close();
        QCOMPARE(Library::readPlaylistFile(pls.fileName()),
                 (QStringList{"http://radio.example/stream", dir.filePath("b.mp3")}));
    }

    void playlistsPickUpWhereTheyWereLeft()
    {
        QTemporaryDir data, cache;
        Library library(data.path(), cache.path());
        const QStringList files{"/s/e1.mkv", "/s/e2.mkv", "/s/e3.mkv"};
        QCOMPARE(Playlists::startIndex(files, &library), 0);
        library.update("/s/e1.mkv", "", 1500, 1500); // finished
        QCOMPARE(Playlists::startIndex(files, &library), 1);
        QTest::qSleep(3); // Plays a few milliseconds apart, as in life.
        library.update("/s/e2.mkv", "", 300, 1500); // halfway
        QCOMPARE(Playlists::startIndex(files, &library), 1);
        QTest::qSleep(3);
        library.update("/s/e3.mkv", "", 1500, 1500); // the end, so from the top
        QCOMPARE(Playlists::startIndex(files, &library), 0);
        QTest::qSleep(3);
        library.update("/s/e1.mkv", "", 900, 1500); // rewatching the first
        QCOMPARE(Playlists::startIndex(files, &library), 0);
    }

    void playlistsAreKeptAcrossWindows()
    {
        QTemporaryDir data, cache, media;
        touch(media.filePath("e1.mkv"));
        touch(media.filePath("e2.mkv"));
        Library library(data.path(), cache.path());
        Playlists first(&library, data.path()), second(&library, data.path());

        const QVariantMap folder = library.expand({media.path()});
        const QString id = first.remember(folder.value("collection").toMap(), folder.value("files").toStringList());
        // Opening the same folder again keeps one playlist.
        QCOMPARE(first.remember(folder.value("collection").toMap(), folder.value("files").toStringList()), id);
        // The other window hasn't read it yet, and saves its own: both stay.
        QTest::qSleep(3);
        const QString saved = second.save("  Friday   night ", {"/x/a.mkv", "/x/b.mkv"});
        QCOMPARE(second.all().size(), 2);
        first.refresh();
        QCOMPARE(first.all().size(), 2);
        const QVariantMap mine = first.all().first().toMap();
        QCOMPARE(mine.value("name").toString(), QString("Friday night"));
        QCOMPARE(mine.value("kind").toString(), QString("saved"));
        QCOMPARE(mine.value("count").toInt(), 2);

        // A folder is scanned again when opened: new episodes show up.
        touch(media.filePath("e3.mkv"));
        library.update(media.filePath("e1.mkv"), "", 1500, 1500);
        QTest::qSleep(3);
        const QVariantMap opened = first.open(id);
        QCOMPARE(opened.value("files").toStringList().size(), 3);
        QCOMPARE(opened.value("index").toInt(), 1);
        const QVariantMap card = first.all().first().toMap(); // opened last, so first
        QCOMPARE(card.value("id").toString(), id);
        QCOMPARE(card.value("watched").toInt(), 1);
        QCOMPARE(card.value("nextTitle").toString(), QString("e2"));

        second.forget(saved);
        first.refresh();
        QCOMPARE(first.all().size(), 1);
        // A folder that's gone opens nothing.
        QVERIFY(QDir(media.path()).removeRecursively());
        QVERIFY(first.open(id).value("files").toStringList().isEmpty());
        QVERIFY(!first.all().first().toMap().value("available").toBool());
    }

    void savedPlaylistsOutlastTheCap()
    {
        QTemporaryDir data, cache;
        Library library(data.path(), cache.path());
        Playlists playlists(&library, data.path());
        const QString saved = playlists.save("Keep", {"/x/a.mkv"});
        for (int i = 0; i < 105; ++i)
            QVERIFY(!playlists.remember({{"kind", "folder"}, {"source", QString("/f/%1").arg(i)}, {"name", "f"}},
                                        {"/f/a.mkv"}).isEmpty());
        const QVariantList all = playlists.all();
        QCOMPARE(all.size(), 100);
        QVERIFY(std::any_of(all.cbegin(), all.cend(), [&](const QVariant &v) { return v.toMap().value("id") == saved; }));
    }

    void playlistEditorReorders()
    {
        QTemporaryDir data, cache, theme;
        Library library(data.path(), cache.path());
        Playlists playlists(&library, data.path());
        const QString id = playlists.save("Mix", {"/m/a.mkv", "/m/b.mkv", "/m/c.mkv", "/m/d.mkv"});
        Theme colors(theme.path());
        QObject picker;
        QQmlEngine engine;
        engine.rootContext()->setContextProperty("theme", &colors);
        engine.rootContext()->setContextProperty("library", &library);
        engine.rootContext()->setContextProperty("playlists", &playlists);
        engine.rootContext()->setContextProperty("picker", &picker);
        QQmlComponent component(&engine);
        component.setData(QString("import QtQuick\n"
                                  "Item {\n"
                                  "    width: 900; height: 600\n"
                                  "    property var played: []\n"
                                  "    property alias editor: editor\n"
                                  "    QtObject { id: app\n"
                                  "        function showError(text) {}\n"
                                  "        function toast(icon, text) {}\n"
                                  "        function openPlaylist(id, path) { played = played.concat([path]) } }\n"
                                  "    PlaylistEditor { id: editor; anchors.fill: parent; app: app; playlistId: \"%1\" }\n"
                                  "}\n").arg(id).toUtf8(),
                          QUrl("qrc:/editor-test.qml"));
        std::unique_ptr<QObject> root(component.create());
        QVERIFY2(root, qPrintable(component.errorString()));
        QQuickWindow window;
        window.resize(900, 600);
        qobject_cast<QQuickItem *>(root.get())->setParentItem(window.contentItem());
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto order = [&] {
            QStringList names;
            for (const auto &entry : playlists.entries(playlists.all().first().toMap().value("id").toString()))
                names.append(QFileInfo(entry.toMap().value("path").toString()).baseName());
            return names.join("");
        };

        // Dragging the first row's grip down two rows.
        // Delegates hang off the visual tree, not the object tree.
        std::function<QQuickItem *(QQuickItem *, const QString &)> find = [&](QQuickItem *item, const QString &name) {
            if (item->objectName() == name)
                return item;
            for (QQuickItem *child : item->childItems())
                if (QQuickItem *found = find(child, name))
                    return found;
            return static_cast<QQuickItem *>(nullptr);
        };
        QQuickItem *grip = nullptr;
        QTRY_VERIFY((grip = find(window.contentItem(), "grip0")));
        const QPoint start = grip->mapToScene(QPointF(grip->width() / 2, grip->height() / 2)).toPoint();
        QTest::mousePress(&window, Qt::LeftButton, {}, start);
        for (int dy = 4; dy <= 54 * 2 + 10; dy += 4) {
            QTest::mouseMove(&window, start + QPoint(0, dy));
            QTest::qWait(5);
        }
        QTest::mouseRelease(&window, Qt::LeftButton, {}, start + QPoint(0, 54 * 2 + 10));
        QTRY_COMPARE(order(), QString("bcad"));

        // Keys: Ctrl+Down moves the selected file, Delete removes it.
        QQuickItem *editor = root->property("editor").value<QQuickItem *>();
        editor->forceActiveFocus();
        editor->setProperty("selection", 0);
        QTest::keyClick(&window, Qt::Key_Down, Qt::ControlModifier);
        QCOMPARE(order(), QString("cbad"));
        QCOMPARE(editor->property("selection").toInt(), 1);
        QTest::keyClick(&window, Qt::Key_Delete);
        QCOMPARE(order(), QString("cad"));
        // Enter plays from the selected file.
        QTest::keyClick(&window, Qt::Key_Return);
        QCOMPARE(root->property("played").toStringList(), QStringList{"/m/a.mkv"});
    }

    // The editor page as HomeView uses it: the id comes from outside and
    // follows moved(). Regressions from review: the page sticking to its
    // first playlist, focus stuck in the name, a grip click converting a
    // folder playlist, a lost rename, and the selection after a removal.
    void playlistEditorFollowsItsPlaylist()
    {
        QTemporaryDir data, cache, theme, media;
        for (const char *name : {"e1.mkv", "e2.mkv", "e3.mkv"})
            touch(media.filePath(name));
        Library library(data.path(), cache.path());
        Playlists playlists(&library, data.path());
        const QVariantMap folder = library.expand({media.path()});
        const QString show = playlists.remember(folder.value("collection").toMap(), folder.value("files").toStringList());
        const QString other = playlists.save("Other", {"/o/x.mkv", "/o/y.mkv", "/o/z.mkv"});
        Theme colors(theme.path());
        QObject picker;
        QQmlEngine engine;
        engine.rootContext()->setContextProperty("theme", &colors);
        engine.rootContext()->setContextProperty("library", &library);
        engine.rootContext()->setContextProperty("playlists", &playlists);
        engine.rootContext()->setContextProperty("picker", &picker);
        QQmlComponent component(&engine);
        component.setData(QString("import QtQuick\n"
                                  "Item {\n"
                                  "    width: 900; height: 600\n"
                                  "    property string current: \"%1\"\n"
                                  "    property alias editor: editor\n"
                                  "    QtObject { id: app\n"
                                  "        function showError(text) {}\n"
                                  "        function toast(icon, text) {}\n"
                                  "        function openPlaylist(id, path) {} }\n"
                                  "    PlaylistEditor { id: editor; anchors.fill: parent; app: app; playlistId: current\n"
                                  "        onMoved: (id) => current = id }\n"
                                  "}\n").arg(show).toUtf8(),
                          QUrl("qrc:/editor-test2.qml"));
        std::unique_ptr<QObject> root(component.create());
        QVERIFY2(root, qPrintable(component.errorString()));
        QQuickWindow window;
        window.resize(900, 600);
        qobject_cast<QQuickItem *>(root.get())->setParentItem(window.contentItem());
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QQuickItem *editor = root->property("editor").value<QQuickItem *>();
        std::function<QQuickItem *(QQuickItem *, const QString &)> find = [&](QQuickItem *item, const QString &name) {
            if (item->objectName() == name)
                return item;
            for (QQuickItem *child : item->childItems())
                if (QQuickItem *found = find(child, name))
                    return found;
            return static_cast<QQuickItem *>(nullptr);
        };

        std::function<QQuickItem *(QQuickItem *)> input = [&](QQuickItem *item) -> QQuickItem * {
            if (QByteArray(item->metaObject()->className()).startsWith("QQuickTextInput"))
                return item;
            for (QQuickItem *child : item->childItems())
                if (QQuickItem *found = input(child))
                    return found;
            return nullptr;
        };
        QCOMPARE(input(editor)->property("text").toString(), folder.value("collection").toMap().value("name").toString());

        // A click on a grip, with no drag, leaves the folder playlist alone.
        QQuickItem *grip = nullptr;
        QTRY_VERIFY((grip = find(window.contentItem(), "grip0")));
        QTest::mouseClick(&window, Qt::LeftButton, {}, grip->mapToScene(QPointF(14, 27)).toPoint());
        QCOMPARE(root->property("current").toString(), show);
        QCOMPARE(playlists.find(show).value("kind").toString(), QString("folder"));

        // Rename, Enter, then the keys work on the list again.
        QMetaObject::invokeMethod(editor, "renameNow");
        for (const char c : QByteArray("My show"))
            QTest::keyClick(&window, c);
        QTest::keyClick(&window, Qt::Key_Return);
        QCOMPARE(playlists.find(show).value("name").toString(), QString("My show"));
        editor->setProperty("selection", 2);
        QTest::keyClick(&window, Qt::Key_Delete);
        // The edit made it a list of its own; the page followed the new id.
        const QString own = root->property("current").toString();
        QVERIFY(own != show && own.startsWith("saved:"));
        QCOMPARE(editor->property("playlistId").toString(), own);
        QCOMPARE(playlists.find(own).value("count").toInt(), 2);

        // Removing a file above the selection keeps the selection on its file.
        editor->setProperty("selection", 1);
        QMetaObject::invokeMethod(editor, "removeRow", Q_ARG(QVariant, 0));
        QCOMPARE(editor->property("selection").toInt(), 0);

        // A name being typed survives a change saved meanwhile, and Back
        // keeps it.
        QMetaObject::invokeMethod(editor, "renameNow");
        for (const char c : QByteArray("Typed"))
            QTest::keyClick(&window, c);
        playlists.refresh();
        QQuickItem *back = find(window.contentItem(), "back");
        QTest::mouseClick(&window, Qt::LeftButton, {}, back->mapToScene(QPointF(back->width() / 2, back->height() / 2)).toPoint());
        QCOMPARE(playlists.find(own).value("name").toString(), QString("Typed"));

        // A typed name is kept when another playlist opens, and the page
        // then shows that one.
        QMetaObject::invokeMethod(editor, "renameNow");
        for (const char c : QByteArray("Kept"))
            QTest::keyClick(&window, c);
        QMetaObject::invokeMethod(editor, "saveName");
        root->setProperty("current", other);
        QCOMPARE(playlists.find(own).value("name").toString(), QString("Kept"));
        QCOMPARE(editor->property("info").toMap().value("name").toString(), QString("Other"));
        QTRY_VERIFY(find(window.contentItem(), "grip2"));
    }

    void editingAFolderPlaylistMakesItsOwnList()
    {
        QTemporaryDir data, cache, media;
        touch(media.filePath("e1.mkv"));
        touch(media.filePath("e2.mkv"));
        Library library(data.path(), cache.path());
        Playlists playlists(&library, data.path());
        const QVariantMap folder = library.expand({media.path()});
        const QString id = playlists.remember(folder.value("collection").toMap(), folder.value("files").toStringList());
        playlists.rename(id, "My show");
        QCOMPARE(playlists.find(id).value("name").toString(), QString("My show"));
        // Reordered: no longer follows the folder, under a new id.
        const QString own = playlists.setFiles(id, {media.filePath("e2.mkv"), media.filePath("e1.mkv")});
        QVERIFY(!own.isEmpty() && own != id);
        QCOMPARE(playlists.find(own).value("kind").toString(), QString("saved"));
        QVERIFY(playlists.find(id).isEmpty());
        touch(media.filePath("e3.mkv"));
        QCOMPARE(playlists.open(own).value("files").toStringList().size(), 2);
        // Opening the folder again makes a fresh folder playlist beside it.
        const QVariantMap again = library.expand({media.path()});
        playlists.remember(again.value("collection").toMap(), again.value("files").toStringList());
        QCOMPARE(playlists.all().size(), 2);
        // Playing from a chosen file, and an empty new playlist.
        QCOMPARE(playlists.open(own, media.filePath("e1.mkv")).value("index").toInt(), 1);
        const QString empty = playlists.save("Later");
        QVERIFY(!empty.isEmpty());
        QVERIFY(playlists.open(empty).value("files").toStringList().isEmpty());
        QCOMPARE(library.collect({media.filePath("e1.mkv")}), QStringList{media.filePath("e1.mkv")});
    }

    void linksPassThrough()
    {
        const QString link = "https://example.com/watch?v=abc";
        QCOMPARE(Library().expand({link}).value("files").toStringList(), QStringList{link});
    }

    void framesComeCoarseToFine()
    {
        const QList<int> order = Thumbnailer::order(8);
        QCOMPARE(order.size(), 8);
        QCOMPARE(order.first(), 4);
        QCOMPARE(QSet<int>(order.begin(), order.end()).size(), 8);
        QCOMPARE(Thumbnailer::frameCount(0), 0);
        QCOMPARE(Thumbnailer::frameCount(20), 10);
        QCOMPARE(Thumbnailer::frameCount(7200), 120);
        QCOMPARE(Thumbnailer::nearest({2, 9}, 7, 10), 9);
        QCOMPARE(Thumbnailer::nearest({}, 7, 10), -1);
    }

    void ffmpegGrabsAFrame()
    {
        const QString ffmpeg = QStandardPaths::findExecutable("ffmpeg");
        if (ffmpeg.isEmpty())
            QSKIP("ffmpeg isn't installed");
        QTemporaryDir dir;
        const QString clip = dir.filePath("clip.mkv"), frame = dir.filePath("frame.jpg");
        QCOMPARE(QProcess::execute(ffmpeg, {"-loglevel", "error", "-f", "lavfi", "-i", "testsrc2=size=320x240:rate=10",
                                            "-t", "3", clip}), 0);
        int result = -1;
        QObject owner;
        grabFrameWithFfmpeg(&owner, clip, 1.5, frame, [&](bool ok) { result = ok; });
        QTRY_COMPARE_WITH_TIMEOUT(result, 1, 10000);
        QVERIFY(QFileInfo(frame).size() > 0);
        QCOMPARE(QDir(dir.path()).entryList({"*.part.jpg"}).size(), 0);

        // A "%d" in the name stays literal, not a frame-number pattern.
        result = -1;
        const QString percent = dir.filePath("Show %d 1.00.jpg");
        grabFrameWithFfmpeg(&owner, clip, 1.5, percent, [&](bool ok) { result = ok; });
        QTRY_COMPARE_WITH_TIMEOUT(result, 1, 10000);
        QVERIFY(QFileInfo(percent).size() > 0);
        QCOMPARE(QDir(dir.path()).entryList({"*.part.jpg"}).size(), 0);

        // A quick grab (library cards) is scaled down to at most 640 wide.
        const QString wide = dir.filePath("wide.mkv"), card = dir.filePath("card.jpg");
        QCOMPARE(QProcess::execute(ffmpeg, {"-loglevel", "error", "-f", "lavfi", "-i",
                                            "testsrc2=size=1280x720:rate=10", "-t", "3", wide}), 0);
        result = -1;
        grabFrameWithFfmpeg(&owner, wide, 1.5, card, [&](bool ok) { result = ok; }, ffmpeg, true);
        QTRY_COMPARE_WITH_TIMEOUT(result, 1, 10000);
        QCOMPARE(QImage(card).width(), 640);

        result = -1;
        grabFrameWithFfmpeg(&owner, dir.filePath("missing.mkv"), 1, frame, [&](bool ok) { result = ok; });
        QCOMPARE(result, 0);
        result = -1;
        grabFrameWithFfmpeg(&owner, dir.filePath("frame.jpg"), 1, dir.filePath("not-a-video.jpg"),
                            [&](bool ok) { result = ok; });
        QTRY_COMPARE_WITH_TIMEOUT(result, 0, 10000);
        QCOMPARE(QDir(dir.path()).entryList({"*.part.jpg"}).size(), 0);
    }

    void previewsCoverTheWholeFile()
    {
        const QString ffmpeg = QStandardPaths::findExecutable("ffmpeg");
        if (ffmpeg.isEmpty())
            QSKIP("ffmpeg isn't installed");
        // B-frames and a single keyframe: the cheap keyframe-only grab comes
        // back empty here, so every preview takes the retry.
        QTemporaryDir dir;
        const QString clip = dir.filePath("clip.mkv");
        QCOMPARE(QProcess::execute(ffmpeg, {"-loglevel", "error", "-f", "lavfi", "-i", "testsrc2=size=320x240:rate=10",
                                            "-t", "12", "-c:v", "libx264", "-bf", "3", clip}), 0);
        Thumbnailer thumbnailer(dir.filePath("frames"), ffmpeg);
        thumbnailer.prepare(clip, 12);
        const int count = Thumbnailer::frameCount(12);
        auto all = [&] {
            for (int i = 0; i < count; ++i)
                if (!thumbnailer.frameAt((i + .5) / count).endsWith(QString("/%1.jpg").arg(i)))
                    return false;
            return true;
        };
        QTRY_VERIFY_WITH_TIMEOUT(all(), 30000);
        QCOMPARE(QDir(dir.filePath("frames")).entryList({"*/*.part.jpg"}).size(), 0);
    }

    void themeFallsBack()
    {
        QTemporaryDir dir;
        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QColor("#FFD60A"));
        QCOMPARE(theme.accentText(), QColor(Qt::black));
        QVERIFY(theme.surface().isValid());
        QVERIFY(!theme.isLight());
    }

    void themeReadsThePalette()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("theme");
        touch(dir.path() + "/theme/colors.toml",
              "mode = \"light\"\naccent = \"#112233\"\nbackground = \"#fafafa\"\nforeground = \"#202020\"\n"
              "lighter_background = \"#ffffff\"\n");
        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QColor("#112233"));
        QCOMPARE(theme.accentText(), QColor(Qt::white));
        QCOMPARE(theme.surface(), QColor("#ffffff"));
        QVERIFY(theme.isLight());
        QVERIFY(theme.deep().isValid()); // derived, since the file leaves it out
    }

    void themeFollowsHyprlandRounding()
    {
        QTemporaryDir dir;
        const QString response = dir.filePath("response.json"), command = dir.filePath("hyprctl");
        // Reads its answer, then lingers, so a refresh asked for meanwhile has
        // to run again to see a newer answer.
        touch(command, "#!/bin/sh\ncat '" + response.toUtf8() + "'\nsleep 0.3\n");
        QVERIFY(QFile::setPermissions(command, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        touch(response, "{\"int\":0}");

        Theme theme(dir.path(), nullptr, command);
        QTRY_COMPARE(theme.radius(), 0);
        // Asked again while a query is still running: it must run once more.
        theme.refreshRounding();
        QTRY_COMPARE(theme.radius(), 0);
        QTest::qWait(150); // The running query has read its answer by now.
        touch(response, "{\"int\":6}");
        theme.refreshRounding();
        QTRY_COMPARE(theme.radius(), 6);
    }

    void translatorFallsBackToEnglish()
    {
        QTemporaryDir dir;
        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.language(), QString("en"));
        QVERIFY(translator.translate("Main", "Open").isNull());
    }

    void translatorPicksPluralForms()
    {
        QTemporaryDir dir;
        touch(dir.path() + "/language", "ru\n");
        touch(dir.path() + "/ru.json", R"({"%n items": {"one": "%n элемент", "few": "%n элемента", "many": "%n элементов", "other": "%n элемента"}})");
        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.translate("Main", "%n items", nullptr, 21), QString("%n элемент"));
        QCOMPARE(translator.translate("Main", "%n items", nullptr, 25), QString("%n элементов"));
    }

private:
    static void touch(const QString &path, const QByteArray &contents = "x")
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(contents);
    }
};

QTEST_MAIN(Tests)
#include "tests.moc"
