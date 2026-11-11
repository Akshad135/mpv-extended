#pragma once

#include <QObject>
#include <QStringList>
#include <QVariantMap>

// Open dialogs through xdg-desktop-portal, so the picker is the one the
// desktop chose (Omarchy's file manager) rather than Qt's own.
class FilePicker : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)

public:
    explicit FilePicker(QObject *parent = nullptr);

    bool busy() const { return !m_requestPath.isEmpty(); }
    // The purpose comes back with picked(): "media" plays the files, while
    // "playlist" adds them to the playlist being edited.
    Q_INVOKABLE void openFiles(const QString &purpose = QStringLiteral("media"));
    Q_INVOKABLE void openFolder(const QString &purpose = QStringLiteral("media"));
    Q_INVOKABLE void openSubtitle();

signals:
    void picked(const QStringList &urls, const QString &purpose);
    void failed(const QString &message);
    void busyChanged();

private slots:
    void onResponse(uint response, const QVariantMap &results);

private:
    void unsubscribe();
    void request(const QString &title, const QString &purpose, bool directory, bool multiple, int filters);

    QString m_requestPath;
    QString m_purpose;
};
