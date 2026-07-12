#include "image.h"
#include "stb_image_write.h"
#include <cmath>

img::img(int w, int h){
    this->w = w;
    this->h = h;
    color.resize(3*w*h);
}


void img::set_pixel(int x, int y, unsigned char r, unsigned char g, unsigned char b){
    if(x>=w || y>=h ) return;
    else{
        color[y*w*3 + x*3] = r;
        color[y*w*3 + x*3+1] = g;
        color[y*w*3 + x*3+2] = b;       
    }
}

void img::save(const std::string& filename) const{


    stbi_write_png(filename.c_str(), w, h, 3, color.data(), 3*w);
}