#include <iostream>
#include "vec3.h"

int main(){
    Vec3 a(1.0, 2.0, 3.0);
    Vec3 b(4.0, 5.0, 6.0);

    Vec3 c = a+b;

    std::cout << c.x << " " << c.y << " " << c.z <<"\n";
    std::cout << dot(a,b) << "\n";
    std::cout << a.length() << "\n";

    a = -a;

    c = c+a;

    std::cout << c.x << " " << c.y << " " << c.z <<"\n";

}