/**
 *
 * Emerald (kbi/elude @2012)
 * 
 * TODO
 *
 */
#ifndef SH_TYPES_H
#define SH_TYPES_H

#include "sh/sh_config.h"

#ifdef INCLUDE_KRIVANEK_FAST_SH_Y_ROTATION
    DECLARE_HANDLE(sh_derivative_rotation_matrices);
#endif

DECLARE_HANDLE(sh_projector);
DECLARE_HANDLE(sh_rot);
DECLARE_HANDLE(sh_samples);
DECLARE_HANDLE(sh_shaders_object);
DECLARE_HANDLE(sh_projector_combiner);

typedef enum
{
    SH_COMPONENTS_UNDEFINED = 0,
    SH_COMPONENTS_RED       = 1,
    SH_COMPONENTS_RG        = 2,
    SH_COMPONENTS_RGB       = 3,

    /* Always last */
    SH_COMPONENTS_LAST
} sh_components;

typedef enum
{
    SH_INTEGRATION_LOGIC_NONE,
    SH_INTEGRATION_LOGIC_VISIBILITY_FROM_UCHAR8                                     = 1, // input_data = visibility (0 - no hit, 1 - hit)                                                    x n_samples
    SH_INTEGRATION_LOGIC_DIFFUSE_UNSHADOWED_TRANSFER_CONSTANT_ALBEDO                = 2, // input_data = albedo, {per vertex normal vec3}                                                    x n_vertices
    SH_INTEGRATION_LOGIC_DIFFUSE_UNSHADOWED_TRANSFER_PER_VERTEX_ALBEDO              = 3, // input_data = {per vertex albedo+normal vec3}                                                     x n_vertices
    SH_INTEGRATION_LOGIC_DIFFUSE_SHADOWED_TRANSFER_CONSTANT_ALBEDO                  = 4, // input_data = albedo, {per vertex normal vec3}                                                    x n_vertices, 
                                                                                         // {per vertex visibility (0 - no hit, 1 - hit)}                                                    x n_vertices x n_samples
    SH_INTEGRATION_LOGIC_DIFFUSE_SHADOWED_TRANSFER_PER_VERTEX_ALBEDO                = 5, // input_data = {per vertex albedo+normal vec3}                                                     x n_vertices, 
                                                                                         // {per vertex visibility (0 - no hit, 1 - hit)}                                                    x n_vertices x n_samples
    SH_INTEGRATION_LOGIC_DIFFUSE_SHADOWED_TRANSFER_INTERREFLECTED_PER_VERTEX_ALBEDO = 6, // input_data = {per vertex albedo+normal vec3}                                                     x n_vertices, 
                                                                                         //              {visibility (float2 with barycentric coordinates of closest hit or -1)              x n_vertices x n_samples,
                                                                                         //              {3xpoint ids making up a triangle that was hit IF there was a hit for given sample) x n_vertices x n_samples.
    /* Always last */
    SH_INTEGRATION_LOGIC_LAST
} sh_integration_logic;

typedef enum
{
    SH_PROJECTION_TYPE_FIRST,

    SH_PROJECTION_TYPE_CIE_OVERCAST = SH_PROJECTION_TYPE_FIRST,
    SH_PROJECTION_TYPE_STATIC_COLOR,
    SH_PROJECTION_TYPE_SYNTHETIC_LIGHT,
    SH_PROJECTION_TYPE_SPHERICAL_TEXTURE,

    SH_PROJECTION_TYPE_UNKNOWN,
    SH_PROJECTION_TYPE_LAST = SH_PROJECTION_TYPE_UNKNOWN
} sh_projection_type;

typedef enum
{
    SH_PROJECTOR_PROPERTY_FIRST,

    SH_PROJECTOR_PROPERTY_SKY_ZENITH_LUMINANCE = SH_PROJECTOR_PROPERTY_FIRST, /* curve_container, float1 data */
    SH_PROJECTOR_PROPERTY_COLOR,                                              /* curve_container, float3 data */
    SH_PROJECTOR_PROPERTY_CUTOFF,                                             /* curve_container, float1 data */
    SH_PROJECTOR_PROPERTY_TEXTURE_ID,                                         /* curve_container, uint1 data */
    SH_PROJECTOR_PROPERTY_LINEAR_EXPOSURE,                                    /* curve_container, float1 data */
    SH_PROJECTOR_PROPERTY_ROTATION,                                           /* curve_container, float3 data, radians */
    SH_PROJECTOR_PROPERTY_TYPE,                                               /* sh_projection_type */

    /* always last */
    SH_PROJECTOR_PROPERTY_LAST
} sh_projector_property;

typedef enum
{
    SH_PROJECTOR_PROPERTY_DATATYPE_CURVE_CONTAINER,
    SH_PROJECTOR_PROPERTY_DATATYPE_SH_PROJECTION_TYPE,

    SH_PROJECTOR_PROPERTY_DATATYPE_UNKNOWN
} sh_projector_property_datatype;

#endif /* SH_TYPES_H */