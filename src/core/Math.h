#pragma once
#include <cmath>
#include <algorithm>
#include <QJsonArray>

namespace lf {

struct Vec3 {
    double x = 0, y = 0, z = 0;
    Vec3() = default;
    Vec3(double X, double Y, double Z) : x(X), y(Y), z(Z) {}
    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator*(double s) const { return {x * s, y * s, z * s}; }
    double dot(const Vec3& o) const { return x * o.x + y * o.y + z * o.z; }
    Vec3 cross(const Vec3& o) const { return {y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x}; }
    double length() const { return std::sqrt(dot(*this)); }
    Vec3 normalized() const { double l = length(); return l > 1e-12 ? (*this) * (1.0 / l) : Vec3(); }
    bool operator==(const Vec3& o) const { return x == o.x && y == o.y && z == o.z; }
    QJsonArray toJson() const { return QJsonArray{x, y, z}; }
    static Vec3 fromJson(const QJsonArray& a) { return a.size() >= 3 ? Vec3(a[0].toDouble(), a[1].toDouble(), a[2].toDouble()) : Vec3(); }
};

// Row-major 4x4 matrix used for MVR/GDTF transforms and 3D view projection.
struct Mat4 {
    double m[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    static Mat4 identity() { return {}; }
    static Mat4 translation(const Vec3& t) { Mat4 r; r.m[3] = t.x; r.m[7] = t.y; r.m[11] = t.z; return r; }
    static Mat4 rotationXYZDeg(const Vec3& deg);
    static Mat4 scale(double s) { Mat4 r; r.m[0] = r.m[5] = r.m[10] = s; return r; }
    Mat4 operator*(const Mat4& o) const {
        Mat4 r;
        for (int i = 0; i < 4; ++i) for (int j = 0; j < 4; ++j) {
            double s = 0; for (int k = 0; k < 4; ++k) s += m[i * 4 + k] * o.m[k * 4 + j];
            r.m[i * 4 + j] = s;
        }
        return r;
    }
    Vec3 transformPoint(const Vec3& p) const {
        return {m[0]*p.x + m[1]*p.y + m[2]*p.z + m[3], m[4]*p.x + m[5]*p.y + m[6]*p.z + m[7], m[8]*p.x + m[9]*p.y + m[10]*p.z + m[11]};
    }
    Vec3 translationPart() const { return {m[3], m[7], m[11]}; }
    Vec3 eulerXYZDeg() const;   // inverse of rotationXYZDeg for the rotation block
};

inline Mat4 Mat4::rotationXYZDeg(const Vec3& d) {
    const double k = M_PI / 180.0;
    const double cx = std::cos(d.x * k), sx = std::sin(d.x * k);
    const double cy = std::cos(d.y * k), sy = std::sin(d.y * k);
    const double cz = std::cos(d.z * k), sz = std::sin(d.z * k);
    Mat4 rx, ry, rz;
    rx.m[5] = cx; rx.m[6] = -sx; rx.m[9] = sx; rx.m[10] = cx;
    ry.m[0] = cy; ry.m[2] = sy; ry.m[8] = -sy; ry.m[10] = cy;
    rz.m[0] = cz; rz.m[1] = -sz; rz.m[4] = sz; rz.m[5] = cz;
    return rz * ry * rx;   // apply X, then Y, then Z
}

inline Vec3 Mat4::eulerXYZDeg() const {
    const double k = 180.0 / M_PI;
    // R = Rz*Ry*Rx ; m[8] = -sin(y)
    double sy = -m[8];
    sy = std::clamp(sy, -1.0, 1.0);
    double y = std::asin(sy), x, z;
    if (std::abs(sy) < 0.999999) { x = std::atan2(m[9], m[10]); z = std::atan2(m[4], m[0]); }
    else { x = std::atan2(-m[6], m[5]); z = 0; }
    return {x * k, y * k, z * k};
}

inline double clamp01(double v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }
inline double lerp(double a, double b, double t) { return a + (b - a) * t; }

} // namespace lf
