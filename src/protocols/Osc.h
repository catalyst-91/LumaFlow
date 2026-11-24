#pragma once
// Open Sound Control 1.0 message and bundle encoding/decoding.
#include <QByteArray>
#include <QString>
#include <QVariant>
#include <QVector>

namespace lf::osc {

struct Message {
    QString address;
    QVariantList args;     // int (i), float/double (f), QString (s), QByteArray (b), bool (T/F)
    QString typeTags() const;
    double numberArg(int i, double fallback = 0.0) const;
    QString toString() const;
};

QByteArray encode(const Message& m);
QByteArray encodeBundle(const QVector<Message>& msgs, quint64 timetag = 1);
// Decodes a packet (message or bundle, recursively) into flat messages.
bool decode(const QByteArray& pkt, QVector<Message>& out);
// OSC address pattern matching (supports * ? [abc] {foo,bar}).
bool matchPattern(const QString& pattern, const QString& address);

} // namespace lf::osc
