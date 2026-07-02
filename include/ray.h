#pragma once
#include "vec3.h"

struct Ray {
    Vec3 origin, direction; //variaveis

    Ray (Vec3 origin, Vec3 direction); //o que o ray precisa
    //origin = ponto de qual o raio sai
    // direction vetor que o raio percorre

    //onde o raio está após percorrer t?
    //P(t) = origin + t*direction

    Vec3 at(const double t) const; //métodos que vamos precisar

};