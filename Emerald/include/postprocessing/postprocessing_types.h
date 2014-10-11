/**
 *
 * Emerald (kbi/elude @2012)
 * 
 * TODO
 *
 */
#ifndef POSTPROCESSING_TYPES_H
#define POSTPROCESSING_TYPES_H

typedef enum
{
    POSTPROCESSING_BLUR_POISSON_BLUR_BLURRINESS_SOURCE_UNIFORM,
    POSTPROCESSING_BLUR_POISSON_BLUR_BLURRINESS_SOURCE_INPUT_ALPHA,
    POSTPROCESSING_BLUR_POISSON_BLUR_BLURRINESS_SOURCE_CUSTOM
} postprocessing_blur_poisson_blur_bluriness_source;

DECLARE_HANDLE(postprocessing_bloom);
DECLARE_HANDLE(postprocessing_blur_poisson);
DECLARE_HANDLE(postprocessing_reinhard_tonemap);


#endif /* POSTPROCESSING_TYPES_H */