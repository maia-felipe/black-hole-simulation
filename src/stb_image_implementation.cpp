// The single translation unit that defines the stb_image implementation, kept apart
// so no other file can pull the definitions in a second time and break the ODR.
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
