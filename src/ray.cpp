#include "ray.h"
#include "vec3.h"

Ray::Ray(Vec3 origin, Vec3 direction) : origin(origin), direction(direction) {
    this->origin = origin;
    this->direction = direction;
}

Vec3 Ray::at(const double t) const{
    return origin + t*direction;
}