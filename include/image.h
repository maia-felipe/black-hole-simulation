#pragma once
#include <vector>
#include <string>

struct img{
    int w, h;

    std::vector<unsigned char> color;

    img(int w, int h);

    void set_pixel(int x, int y, unsigned char r, unsigned char g, unsigned char b);

    void save (const std::string& filename) const;

};