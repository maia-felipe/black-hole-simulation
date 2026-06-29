#include "vec3.h"
#include <cmath>

Vec3::Vec3(double x, double y, double z) {
    this->x = x;
    this->y = y;
    this->z = z;
}

double Vec3::length() const {
     return std::sqrt(x*x + y*y + z*z);
}

Vec3 Vec3::normalize() const {
    return Vec3(x/length(), y/length(), z/length());
}

Vec3 Vec3::operator+ (const Vec3& other) const{
    return Vec3 (x + other.x, y + other.y, z+other.z);
}

Vec3 Vec3::operator- (const Vec3& other) const{
    return Vec3 (x - other.x, y - other.y, z-other.z);
}

Vec3 Vec3::operator* (double const other) const {
    return Vec3 (x*other, y*other, z*other);
}

Vec3 Vec3::operator/ (double const other) const {
    return Vec3(x/other, y/other, z/other);
}

Vec3 Vec3::operator- () const{
    return Vec3(-x, -y, -z);
}

double dot(Vec3 const a, Vec3 const b){
    return a.x*b.x + a.y*b.y+a.z*b.z;
}

Vec3 cross(Vec3 const a, Vec3 const b){

    return Vec3(a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x);
}

Vec3 operator*(double const t, Vec3 const a){
    return Vec3(a.x*t, a.y*t, a.z*t);
}