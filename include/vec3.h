#pragma once

struct Vec3{
    double x, y, z;
    
    Vec3 (double x, double y, double z);

    double length() const;
    Vec3 normalize() const;

    Vec3 operator+ (const Vec3& other) const;

    Vec3 operator- (const Vec3& other) const;

    Vec3 operator* (const double other) const;

    Vec3 operator/ (const double other) const;

    Vec3 operator- () const;
};

double dot(Vec3 a, Vec3 b);

Vec3 cross(Vec3 a, Vec3 b);

Vec3 operator* (double t, Vec3 a);