#ifndef LWSDK_POLYGON_GENERATOR_H
#define LWSDK_POLYGON_GENERATOR_H

#include <lwmeshes.h>

struct LWPolyGenerator
{
    struct LWPolyGeneratorDetail* detail;
    void (*destroy)(struct LWPolyGenerator* gen);
    struct LWPolyGenerator* (*clone)(struct LWPolyGenerator* gen);
    LWPolID (*generate)(struct LWPolyGenerator* gen);
};

#endif
