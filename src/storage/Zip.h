#pragma once
// Minimal ZIP archive reader/writer (stored + deflate) used for GDTF and MVR
// containers and for .lfpackage effect/preset bundles. Uses zlib.
#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QMap>

namespace lf {

class ZipReader {
public:
    bool open(const QString& path);
    bool openData(const QByteArray& data);
    QStringList entries() const { return m_entries.keys(); }
    bool contains(const QString& name) const { return m_entries.contains(name); }
    QByteArray read(const QString& name, bool* ok = nullptr) const;
    QString error() const { return m_error; }
private:
    struct Entry { quint32 localOffset; quint32 compSize; quint32 size; quint16 method; quint32 crc; };
    bool parse();
    QByteArray m_data;
    QMap<QString, Entry> m_entries;
    QString m_error;
};

class ZipWriter {
public:
    void add(const QString& name, const QByteArray& data, bool compress = true);
    QByteArray finish();
    bool save(const QString& path);
private:
    struct Entry { QString name; quint32 offset, compSize, size, crc; quint16 method; };
    QByteArray m_out;
    QVector<Entry> m_entries;
};

} // namespace lf
