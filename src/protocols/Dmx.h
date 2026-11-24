#pragma once
#include <array>
#include <cstdint>
#include <QByteArray>

namespace lf {

constexpr int kDmxSlots = 512;
using DmxFrame = std::array<uint8_t, kDmxSlots>;

inline QByteArray dmxToBytes(const DmxFrame& f, int len = kDmxSlots) {
    return QByteArray(reinterpret_cast<const char*>(f.data()), len);
}
inline void bytesToDmx(const QByteArray& b, DmxFrame& f) {
    f.fill(0);
    const int n = std::min<int>(b.size(), kDmxSlots);
    for (int i = 0; i < n; ++i) f[i] = static_cast<uint8_t>(b[i]);
}

} // namespace lf
