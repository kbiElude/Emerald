#ifndef LWSDK_EDGE_GENERATOR_H
#define LWSDK_EDGE_GENERATOR_H

#include <lwmeshes.h>

struct LWEdgeGenerator
{
    struct LWEdgeGeneratorDetail* detail;
    void (*destroy)(struct LWEdgeGenerator* gen);
    struct LWEdgeGenerator* (*clone)(struct LWEdgeGenerator* gen);
    LWEdgeID (*generate)(struct LWEdgeGenerator* gen);
};

#endif
