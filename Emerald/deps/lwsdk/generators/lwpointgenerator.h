#ifndef LWSDK_POINT_GENERATOR_H
#define LWSDK_POINT_GENERATOR_H

#include <lwmeshes.h>

struct LWPointGenerator
{
    struct LWPointGeneratorDetail* detail;
    void (*destroy)(struct LWPointGenerator* gen);
    struct LWPointGenerator* (*clone)(struct LWPointGenerator* gen);
    LWPntID (*generate)(struct LWPointGenerator* gen);
};

#endif
