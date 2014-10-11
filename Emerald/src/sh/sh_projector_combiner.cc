/**
 *
 * Emerald (kbi/elude @2012)
 *
 */
#include "shared.h"
#include "curve/curve_container.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_rendering_handler.h"
#include "ogl/ogl_texture.h"
#include "sh/sh_projector.h"
#include "sh/sh_projector_combiner.h"
#include "sh/sh_samples.h"
#include "sh/sh_tools.h"
#include "sh/sh_types.h"
#include "system/system_assertions.h"
#include "system/system_critical_section.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_hash64map.h"
#include "system/system_log.h"
#include "system/system_variant.h"

#define DEFAULT_SH_PROJECTION_COLOR_R              (0.000f)
#define DEFAULT_SH_PROJECTION_COLOR_G              (0.000f)
#define DEFAULT_SH_PROJECTION_COLOR_B              (0.0001f)
#define DEFAULT_SH_PROJECTION_CUTOFF               (2.2f)
#define DEFAULT_SH_PROJECTION_LINEAR_EXPOSURE      (1.0f)
#define DEFAULT_SH_PROJECTION_ROTATION_X           (0.0f)
#define DEFAULT_SH_PROJECTION_ROTATION_Y           (0.0f)
#define DEFAULT_SH_PROJECTION_ROTATION_Z           (0.0f)
#define DEFAULT_SH_PROJECTION_SKY_ZENITH_LUMINANCE (1.0f)
#define DEFAULT_SH_PROJECTION_TEXTURE_ID           (1)

#ifdef INCLUDE_OPENCL

/* Internal variables */

/** Internal type definition */
typedef struct
{
    void*                            arg;
    PFNPROJECTORCOMBINERCALLBACKPROC func_ptr;
} _sh_projector_combiner_callback;

typedef struct
{
    /** These fields are initialized, depending on projection type ==> */
    curve_container color;                /* float3 */
    curve_container cutoff;               /* float1 */
    curve_container linear_exposure;      /* float1 */
    curve_container rotation;             /* float3, degrees! */
    curve_container sky_zenith_luminance; /* float1 */
    curve_container texture_id;           /* uint1 */
    /** <== */

    float              last_calc_rotation_angles[3]; /* XYZ */
    sh_projector       projector;
    GLuint             result_bo_id; /* contains rotated SH data as returned by sh_projecdtor for this single projection */
    sh_projection_type type;
} _sh_projector_combiner_projection;

typedef struct
{
    sh_components             components;
    ogl_context               context;
    system_hashed_ansi_string name;
    sh_projection_id          projection_id_counter;
    system_hash64map          projections;               /* maps sh_projector_combiner_projection_id to _sh_projector_combiner_projection instances */
    GLuint                    result_sh_data_bo_id;      /* stores summed-up result SH data */
    uint32_t                  result_sh_data_bo_rrggbb_offset;
    uint32_t                  result_sh_data_bo_rrggbb_size;
    bool                      result_sh_data_tbo_dirty;
    ogl_texture               result_sh_data_tbo;
    uint32_t                  result_sh_data_tbo_offset;
    uint32_t                  result_sh_data_tbo_size;
    sh_samples                samples;
    system_critical_section   serialization_cs;

    system_variant            variant_float;
    system_variant            variant_uint32;

    REFCOUNT_INSERT_VARIABLES
} _sh_projector_combiner;

/* Forward declarations */
PRIVATE void _sh_projector_deinit_sh_projector_combiner_projection                  (__in __notnull                   _sh_projector_combiner_projection* projection_ptr, __in __notnull ogl_context context);
PRIVATE void _sh_projector_deinit_sh_projector_combiner_projection_renderer_callback(__in __notnull                   ogl_context                        context, void* arg);
PRIVATE void _sh_projector_init_sh_projector_combiner_projection                    (__in __notnull                   _sh_projector_combiner_projection* projection_ptr, __in __notnull _sh_projector_combiner* combiner_ptr, __in sh_projection_type projection_type);
PRIVATE void _sh_projector_init_sh_projector_combiner_projection_renderer_callback  (__in __notnull                   ogl_context                        context, void* arg);
PRIVATE void _sh_projector_combiner_init_result_sh_data_buffer_rendering_callback   (__in __notnull                   ogl_context                        context, void* arg);
PRIVATE void _sh_projector_combiner_release                                         (__in __notnull __deallocate(mem) void*                              ptr);
PRIVATE void _sh_projector_combiner_release_rendering_thread_callback               (__in __notnull                   ogl_context                        context, void* arg);

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(sh_projector_combiner, sh_projector_combiner, _sh_projector_combiner);


/* Internal variables */
PRIVATE _sh_projector_combiner_callback callback_on_projection_added   = {0, 0};
PRIVATE _sh_projector_combiner_callback callback_on_projection_deleted = {0, 0};

/** TODO */
PRIVATE void _sh_projector_deinit_sh_projector_combiner_projection(__in __notnull _sh_projector_combiner_projection* projection_ptr, __in __notnull ogl_context context)
{
    if (projection_ptr->color != NULL)
    {
        curve_container_release(projection_ptr->color);
    }

    if (projection_ptr->cutoff != NULL)
    {
        curve_container_release(projection_ptr->cutoff);
    }

    if (projection_ptr->linear_exposure != NULL)
    {
        curve_container_release(projection_ptr->linear_exposure);
    }

    if (projection_ptr->rotation != NULL)
    {
        curve_container_release(projection_ptr->rotation);
    }

    if (projection_ptr->sky_zenith_luminance != NULL)
    {
        curve_container_release(projection_ptr->sky_zenith_luminance);
    }

    if (projection_ptr->texture_id != NULL)
    {
        curve_container_release(projection_ptr->texture_id);
    }

    if (projection_ptr->projector != NULL)
    {
        sh_projector_release(projection_ptr->projector);
    }

    /* We need a renderer thread call-back to finish the deinitialization */
    ogl_context_request_callback_from_context_thread(context,
                                                     _sh_projector_deinit_sh_projector_combiner_projection_renderer_callback,
                                                     projection_ptr);

    if (context != NULL)
    {
        ogl_context_release(context);
    }
}

/** TODO */
PRIVATE void _sh_projector_deinit_sh_projector_combiner_projection_renderer_callback(__in __notnull ogl_context context, void* arg)
{
    const ogl_context_gl_entrypoints*  entry_points   = NULL;
    _sh_projector_combiner_projection* projection_ptr = (_sh_projector_combiner_projection*) arg;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS,
                            &entry_points);

    entry_points->pGLDeleteBuffers(1, &projection_ptr->result_bo_id);
}

/** TODO */
PRIVATE void _sh_projector_combiner_init_result_sh_data_buffer_rendering_callback(__in __notnull ogl_context context, void* arg)
{
    _sh_projector_combiner*                                   combiner_ptr     = (_sh_projector_combiner*) arg;
    const ogl_context_gl_entrypoints*                         entrypoints      = NULL;
    const ogl_context_gl_entrypoints_ext_direct_state_access* dsa_entrypoints  = NULL;
    uint32_t                                                  sh_data_set_size = 0;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_EXT_DIRECT_STATE_ACCESS,
                            &dsa_entrypoints);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS,
                            &entrypoints);

    /* Result buffer object can only be created if we have at least one projector at hand. */
    ASSERT_DEBUG_SYNC(combiner_ptr->result_sh_data_bo_id == 0, "Result SH data buffer already initialized");
    if (combiner_ptr->result_sh_data_bo_id == 0)
    {
        /* Retrieve a descriptor of any of the projections available */
        system_hash64                      projection_id  = -1;
        _sh_projector_combiner_projection* projection_ptr = NULL;

        if (system_hash64map_get_element_at(combiner_ptr->projections, 0, &projection_ptr, &projection_id) )
        {
            sh_projector_get_projection_result_details(projection_ptr->projector,
                                                       SH_PROJECTION_RESULT_RRGGBB,
                                                       NULL,
                                                       &combiner_ptr->result_sh_data_bo_rrggbb_size);

            entrypoints->pGLGenBuffers            (1, &combiner_ptr->result_sh_data_bo_id);
            dsa_entrypoints->pGLNamedBufferDataEXT(combiner_ptr->result_sh_data_bo_id, combiner_ptr->result_sh_data_bo_rrggbb_size, NULL, GL_DYNAMIC_COPY);
        }
        else
        {
            ASSERT_DEBUG_SYNC(false, "Could not retrieve first projection descriptor instance");
        }
    } /* if (combiner_ptr->result_sh_data_bo_id == 0) */
}

/** TODO */
PRIVATE void _sh_projector_init_sh_projector_combiner_projection(__in __notnull _sh_projector_combiner_projection* projection_ptr, __in __notnull _sh_projector_combiner* combiner_ptr, __in sh_projection_type projection_type)
{
    memset(projection_ptr, 0, sizeof(_sh_projector_combiner_projection) );

    projection_ptr->type      = projection_type;
    projection_ptr->projector = sh_projector_create(combiner_ptr->context, 
                                                    combiner_ptr->samples, 
                                                    combiner_ptr->components, 
                                                    system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(combiner_ptr->name), 
                                                                                                                                                 " projector"),
                                                    1,
                                                    &projection_type);

    /* We need to generate a BO to hold the data for this projection */
    ogl_context_request_callback_from_context_thread(combiner_ptr->context,
                                                     _sh_projector_init_sh_projector_combiner_projection_renderer_callback,
                                                     projection_ptr);
}

/** TODO */
PRIVATE void _sh_projector_init_sh_projector_combiner_projection_renderer_callback(__in __notnull ogl_context context, void* arg)
{
    const ogl_context_gl_entrypoints_ext_direct_state_access* dsa_entry_points = NULL;
    const ogl_context_gl_entrypoints*                         entry_points     = NULL;
    _sh_projector_combiner_projection*                        projection_ptr   = (_sh_projector_combiner_projection*) arg;
    uint32_t                                                  dataset_size     = sh_projector_get_projection_result_total_size(projection_ptr->projector);

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_EXT_DIRECT_STATE_ACCESS,
                            &dsa_entry_points);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS,
                            &entrypoints);

    entry_points->pGLGenBuffers            (1, &projection_ptr->result_bo_id);
    dsa_entry_points->pGLNamedBufferDataEXT(projection_ptr->result_bo_id, dataset_size, NULL, GL_STATIC_COPY);
}

/** TODO **/
PRIVATE void _sh_projector_combiner_release(__in __notnull __deallocate(mem) void* ptr)
{
    _sh_projector_combiner* data_ptr = (_sh_projector_combiner*) ptr;

    /* First, request a rendering thread call-back to release helper GL objects */
    ogl_context_request_callback_from_context_thread(data_ptr->context,
                                                     _sh_projector_combiner_release_rendering_thread_callback,
                                                     data_ptr);

    /* Carry on with release procedure */
    if (data_ptr->projections != NULL)
    {
        while (system_hash64map_get_amount_of_elements(data_ptr->projections) != 0)
        {
            system_hash64                      projection_id;
            _sh_projector_combiner_projection* projection_ptr = NULL;

            while (system_hash64map_get_element_at(data_ptr->projections, 0, &projection_ptr, &projection_id))
            {
                _sh_projector_deinit_sh_projector_combiner_projection(projection_ptr, data_ptr->context);

                delete projection_ptr;

                system_hash64map_remove(data_ptr->projections, projection_id);
            }
        }

        system_hash64map_release(data_ptr->projections);
        data_ptr->projections = NULL;
    }

    if (data_ptr->variant_float != NULL)
    {
        system_variant_release(data_ptr->variant_float);

        data_ptr->variant_float = NULL;
    }

    if (data_ptr->variant_uint32 != NULL)
    {
        system_variant_release(data_ptr->variant_uint32);

        data_ptr->variant_uint32 = NULL;
    }

    if (data_ptr->serialization_cs != NULL)
    {
        system_critical_section_release(data_ptr->serialization_cs);

        data_ptr->serialization_cs = NULL;
    }
}

/** TODO */
PRIVATE void _sh_projector_combiner_release_rendering_thread_callback(__in __notnull ogl_context context, void* arg)
{
    _sh_projector_combiner*           combiner_ptr = (_sh_projector_combiner*) arg;
    const ogl_context_gl_entrypoints* entrypoints  = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS,
                            &entrypoints);

    /* Delete result SH data buffer object */
    entrypoints->pGLDeleteBuffers (1, &combiner_ptr->result_sh_data_bo_id);

    ogl_texture_release(combiner_ptr->result_sh_data_tbo);

    combiner_ptr->result_sh_data_bo_id = 0;
}

/** TODO */
PUBLIC EMERALD_API bool sh_projector_combiner_add_sh_projection(__in  __notnull   sh_projector_combiner combiner,
                                                                __in              sh_projection_type    new_projection_type,
                                                                __out __maybenull sh_projection_id*     out_result_projection_id)
{
    _sh_projector_combiner*            combiner_ptr         = (_sh_projector_combiner*) combiner;
    _sh_projector_combiner_projection* new_projection_ptr   = new (std::nothrow) _sh_projector_combiner_projection;
    system_hashed_ansi_string          projection_type_name = sh_tools_get_sh_projection_type_string(new_projection_type);
    bool                               result               = false;

    ASSERT_ALWAYS_SYNC(new_projection_ptr != NULL, "Out of memory");
    if (new_projection_ptr != NULL)
    {
        result = true;

        _sh_projector_init_sh_projector_combiner_projection(new_projection_ptr, combiner_ptr, new_projection_type);

        switch (new_projection_type)
        {
            case SH_PROJECTION_TYPE_CIE_OVERCAST:
            {
                const char* string_fragments[] =
                {
                    system_hashed_ansi_string_get_buffer(combiner_ptr->name),
                    " ",
                    system_hashed_ansi_string_get_buffer(projection_type_name),
                    " sky zenith luminance"
                };

                new_projection_ptr->sky_zenith_luminance = curve_container_create(system_hashed_ansi_string_create_by_merging_strings(sizeof(string_fragments) / sizeof(string_fragments[0]),
                                                                                                                                      string_fragments), 
                                                                                  SYSTEM_VARIANT_FLOAT,
                                                                                  1);

                system_variant_set_float         (combiner_ptr->variant_float,              DEFAULT_SH_PROJECTION_SKY_ZENITH_LUMINANCE);
                curve_container_set_default_value(new_projection_ptr->sky_zenith_luminance, 0, combiner_ptr->variant_float);

                break;
            }

            case SH_PROJECTION_TYPE_SPHERICAL_TEXTURE:
            {
                const char* string_fragments1[] = 
                {
                    system_hashed_ansi_string_get_buffer(combiner_ptr->name),
                    " ",
                    system_hashed_ansi_string_get_buffer(projection_type_name),
                    " texture id"
                };
                const char* string_fragments2[] = 
                {
                    system_hashed_ansi_string_get_buffer(combiner_ptr->name),
                    " ",
                    system_hashed_ansi_string_get_buffer(projection_type_name),
                    " linear exposure"
                };

                new_projection_ptr->texture_id = curve_container_create(system_hashed_ansi_string_create_by_merging_strings(sizeof(string_fragments1) / sizeof(string_fragments1[0]),
                                                                                                                            string_fragments1),
                                                                        SYSTEM_VARIANT_INTEGER,
                                                                        1);
                
                new_projection_ptr->linear_exposure = curve_container_create(system_hashed_ansi_string_create_by_merging_strings(sizeof(string_fragments2) / sizeof(string_fragments2[0]),
                                                                                                                                 string_fragments2),
                                                                        SYSTEM_VARIANT_FLOAT,
                                                                        1);

                system_variant_set_float         (combiner_ptr->variant_float,    DEFAULT_SH_PROJECTION_LINEAR_EXPOSURE);
                system_variant_set_integer       (combiner_ptr->variant_uint32,   DEFAULT_SH_PROJECTION_TEXTURE_ID, false);

                curve_container_set_default_value(new_projection_ptr->linear_exposure, 0, combiner_ptr->variant_float);
                curve_container_set_default_value(new_projection_ptr->texture_id,      0, combiner_ptr->variant_uint32);

                break;
            }

            case SH_PROJECTION_TYPE_STATIC_COLOR:
            {
                const char* string_fragments[] =
                {
                    system_hashed_ansi_string_get_buffer(combiner_ptr->name),
                    " ",
                    system_hashed_ansi_string_get_buffer(projection_type_name),
                    " color"
                };

                new_projection_ptr->color = curve_container_create(system_hashed_ansi_string_create_by_merging_strings(sizeof(string_fragments) / sizeof(string_fragments[0]),
                                                                                                                       string_fragments),
                                                                   SYSTEM_VARIANT_FLOAT,
                                                                   3);

                system_variant_set_float         (combiner_ptr->variant_float, DEFAULT_SH_PROJECTION_COLOR_R);
                curve_container_set_default_value(new_projection_ptr->color,   0, combiner_ptr->variant_float);
                system_variant_set_float         (combiner_ptr->variant_float, DEFAULT_SH_PROJECTION_COLOR_G);
                curve_container_set_default_value(new_projection_ptr->color,   1, combiner_ptr->variant_float);
                system_variant_set_float         (combiner_ptr->variant_float, DEFAULT_SH_PROJECTION_COLOR_B);
                curve_container_set_default_value(new_projection_ptr->color,   2, combiner_ptr->variant_float);

                break;
            }

            case SH_PROJECTION_TYPE_SYNTHETIC_LIGHT:
            {
                const char* string_fragments[] =
                {
                    system_hashed_ansi_string_get_buffer(combiner_ptr->name),
                    " ",
                    system_hashed_ansi_string_get_buffer(projection_type_name),
                    " cutoff"
                };

                new_projection_ptr->cutoff = curve_container_create(system_hashed_ansi_string_create_by_merging_strings(sizeof(string_fragments) / sizeof(string_fragments[0]),
                                                                                                                        string_fragments),
                                                                    SYSTEM_VARIANT_FLOAT,
                                                                    1);

                system_variant_set_float         (combiner_ptr->variant_float, DEFAULT_SH_PROJECTION_CUTOFF);
                curve_container_set_default_value(new_projection_ptr->cutoff,  0, combiner_ptr->variant_float);

                break;
            }

            default:
            {
                ASSERT_DEBUG_SYNC(false, "SH projection type not recognized");

                result = false;
            }
        } /* switch (new_projection_type) */

        if (result)
        {
            /* Configure new projection type */
            new_projection_ptr->type = new_projection_type;

            /* All projections support rotation */
            const char* string_fragments[] =
            {
                system_hashed_ansi_string_get_buffer(combiner_ptr->name),
                " ",
                system_hashed_ansi_string_get_buffer(projection_type_name),
                " rotation"
            };

            new_projection_ptr->rotation = curve_container_create(system_hashed_ansi_string_create_by_merging_strings(sizeof(string_fragments) / sizeof(string_fragments[0]),
                                                                                                                      string_fragments),
                                                                  SYSTEM_VARIANT_FLOAT,
                                                                  3);

            system_variant_set_float         (combiner_ptr->variant_float,  DEFAULT_SH_PROJECTION_ROTATION_X);
            curve_container_set_default_value(new_projection_ptr->rotation, 0, combiner_ptr->variant_float);
            system_variant_set_float         (combiner_ptr->variant_float,  DEFAULT_SH_PROJECTION_ROTATION_Y);
            curve_container_set_default_value(new_projection_ptr->rotation, 1, combiner_ptr->variant_float);
            system_variant_set_float         (combiner_ptr->variant_float,  DEFAULT_SH_PROJECTION_ROTATION_Z);
            curve_container_set_default_value(new_projection_ptr->rotation, 2, combiner_ptr->variant_float);
        }
        
        /* Add the new projection */
        combiner_ptr->projection_id_counter++;

        system_hash64map_insert(combiner_ptr->projections,
                                (system_hash64) combiner_ptr->projection_id_counter,
                                new_projection_ptr,
                                NULL,
                                NULL);

        if (out_result_projection_id != NULL)
        {
            *out_result_projection_id = combiner_ptr->projection_id_counter;
        }

        /* Mark the TBO as dirty */
        combiner_ptr->result_sh_data_tbo_dirty = true;

        /* If this is the first projection added, we can initialize result SH data buffer. */
        if (combiner_ptr->result_sh_data_bo_id == 0)
        {
            ogl_context_request_callback_from_context_thread(combiner_ptr->context,
                                                             _sh_projector_combiner_init_result_sh_data_buffer_rendering_callback,
                                                             combiner_ptr);
        } /* if (combiner_ptr->result_sh_data_bo_id == 0) */

        /* Call-back if requested */
        if (callback_on_projection_added.func_ptr != NULL)
        {
            callback_on_projection_added.func_ptr(callback_on_projection_added.arg);
        }
    } /* if (new_projection_ptr != NULL) */

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API sh_projector_combiner sh_projector_combiner_create(__in __notnull ogl_context               context, 
                                                                      __in           sh_components             components,
                                                                      __in __notnull sh_samples                samples, 
                                                                      __in __notnull system_hashed_ansi_string name)
{
    /* Create new instance */
    _sh_projector_combiner* new_instance = new (std::nothrow) _sh_projector_combiner;

    ASSERT_DEBUG_SYNC(new_instance != NULL, "Could not create _sh_projector_combiner instance.");
    if (new_instance != NULL)
    {
        memset(new_instance, 0, sizeof(_sh_projector_combiner) );

        new_instance->components                      = components;
        new_instance->context                         = context;
        new_instance->name                            = name;
        new_instance->samples                         = samples;
        new_instance->projection_id_counter           = 0;
        new_instance->projections                     = system_hash64map_create(sizeof(void*) );
        new_instance->result_sh_data_bo_id            = 0;
        new_instance->result_sh_data_bo_rrggbb_offset = 0;
        new_instance->result_sh_data_bo_rrggbb_size   = 0;
        new_instance->result_sh_data_tbo_dirty        = false;
        new_instance->result_sh_data_tbo              = 0;
        new_instance->result_sh_data_tbo_offset       = 0;
        new_instance->result_sh_data_tbo_size         = 0;
        new_instance->serialization_cs                = system_critical_section_create();
        new_instance->variant_float                   = system_variant_create(SYSTEM_VARIANT_FLOAT);
        new_instance->variant_uint32                  = system_variant_create(SYSTEM_VARIANT_INTEGER);

        ogl_context_retain(new_instance->context);

        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(new_instance, 
                                                       _sh_projector_combiner_release,
                                                       OBJECT_TYPE_SH_PROJECTOR_COMBINER,
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\SH Projector Combiner\\", system_hashed_ansi_string_get_buffer(name)) );
    }

    return (sh_projector_combiner) new_instance;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool sh_projector_combiner_delete_sh_projection(__in __notnull sh_projector_combiner combiner,
                                                                   __in __notnull sh_projection_id      projection_id)
{
    _sh_projector_combiner*            combiner_ptr   = (_sh_projector_combiner*) combiner;
    _sh_projector_combiner_projection* projection_ptr = NULL;
    bool                               result         = false;

    if (system_hash64map_get(combiner_ptr->projections, (system_hash64) projection_id, &projection_ptr) )
    {
        if (system_hash64map_remove(combiner_ptr->projections, (system_hash64) projection_id) )
        {
            /* Mark the TBO as dirty */
            combiner_ptr->result_sh_data_tbo_dirty = true;

            /* Call-back if requested */
            if (callback_on_projection_deleted.func_ptr != NULL)
            {
                callback_on_projection_deleted.func_ptr(callback_on_projection_deleted.arg);
            }

            /* Release all resources assigned to the projection descriptor */
            _sh_projector_deinit_sh_projector_combiner_projection(projection_ptr, combiner_ptr->context);

            /* Go on and free the descriptor */
            delete projection_ptr;

            result = true;
        }
        else
        {
            ASSERT_DEBUG_SYNC(false, "Could not remove projection from the projector combiner internal map");
        }
    }
    else
    {
        ASSERT_DEBUG_SYNC(false, "Could not delete SH projection");
    }

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool sh_projector_combiner_execute(__in __notnull sh_projector_combiner combiner,
                                                      __in           system_timeline_time  time)
{
    _sh_projector_combiner*                                    combiner_ptr     = (_sh_projector_combiner*) combiner;
    const ogl_context_gl_entrypoints_ext_direct_state_access*  dsa_entry_points = NULL;
    const ogl_context_gl_entrypoints*                          entry_points     = NULL;

    ogl_context_get_property(combiner_ptr->context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_EXT_DIRECT_STATE_ACCESS,
                            &dsa_entry_points);
    ogl_context_get_property(combiner_ptr->context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS,
                            &entry_points);

    /* We MUST NOT enter CS here as this would lock up in case projector combiner needs to update the projector in another thread. The update
     * would require access to a rendering thread that we would be blocking.
     *
     * SO. Try to enter the CS - if this would have failed, just report a problem to the caller. */
    if (system_critical_section_try_enter(combiner_ptr->serialization_cs) )
    {
        /* Update projection handler on property values */
        uint32_t n_projections = system_hash64map_get_amount_of_elements(combiner_ptr->projections);

        for (uint32_t n_projection = 0; n_projection < n_projections; ++n_projection)
        {
            system_hash64                      projection_id    = 0;
            _sh_projector_combiner_projection* projection_ptr   = NULL;
            bool                               update_succeeded = false;

            if (system_hash64map_get_element_at(combiner_ptr->projections, n_projection, &projection_ptr, &projection_id) )
            {
                switch (projection_ptr->type)
                {
                    case SH_PROJECTION_TYPE_CIE_OVERCAST:
                    {
                        float zenith_luminance;

                        update_succeeded = curve_container_get_value(projection_ptr->sky_zenith_luminance, 0, time, false, combiner_ptr->variant_float);
                        system_variant_get_float(combiner_ptr->variant_float, &zenith_luminance);

                        if (update_succeeded)
                        {
                            update_succeeded = sh_projector_set_projection_property(projection_ptr->projector, 0, SH_PROJECTOR_PROPERTY_SKY_ZENITH_LUMINANCE, &zenith_luminance);
                        }

                        break;
                    }

                    case SH_PROJECTION_TYPE_SPHERICAL_TEXTURE:
                    {
                        float linear_exposure;
                        int   texture_id;

                        update_succeeded =  curve_container_get_value(projection_ptr->linear_exposure, 0, time, false, combiner_ptr->variant_float);
                        update_succeeded &= curve_container_get_value(projection_ptr->texture_id,      0, time, false, combiner_ptr->variant_uint32);

                        system_variant_get_float  (combiner_ptr->variant_float,  &linear_exposure);
                        system_variant_get_integer(combiner_ptr->variant_uint32, &texture_id);

                        if (update_succeeded)
                        {
                            update_succeeded  = sh_projector_set_projection_property(projection_ptr->projector, 0, SH_PROJECTOR_PROPERTY_LINEAR_EXPOSURE, &linear_exposure);
                            update_succeeded &= sh_projector_set_projection_property(projection_ptr->projector, 0, SH_PROJECTOR_PROPERTY_TEXTURE_ID,      &texture_id);
                        }

                        break;
                    }

                    case SH_PROJECTION_TYPE_STATIC_COLOR:
                    {
                        float color[3];
                    
                        update_succeeded = curve_container_get_value(projection_ptr->color, 0, time, false, combiner_ptr->variant_float);
                        system_variant_get_float(combiner_ptr->variant_float, color);

                        update_succeeded &= curve_container_get_value(projection_ptr->color, 1, time, false, combiner_ptr->variant_float);
                        system_variant_get_float(combiner_ptr->variant_float, color + 1);

                        update_succeeded &= curve_container_get_value(projection_ptr->color, 2, time, false, combiner_ptr->variant_float);
                        system_variant_get_float(combiner_ptr->variant_float, color + 2);

                        if (update_succeeded)
                        {
                            update_succeeded = sh_projector_set_projection_property(projection_ptr->projector, 0, SH_PROJECTOR_PROPERTY_COLOR, color);
                        }

                        break;
                    }

                    case SH_PROJECTION_TYPE_SYNTHETIC_LIGHT:
                    {
                        float cutoff;

                        update_succeeded = curve_container_get_value(projection_ptr->cutoff, 0, time, false, combiner_ptr->variant_float);
                        system_variant_get_float(combiner_ptr->variant_float, &cutoff);

                        if (update_succeeded)
                        {
                            update_succeeded = sh_projector_set_projection_property(projection_ptr->projector, 0, SH_PROJECTOR_PROPERTY_CUTOFF, &cutoff);
                        }

                        break;
                    }
                
                    default:
                    {
                        ASSERT_DEBUG_SYNC(false, "Unrecognized projection type");
                    }
                } /* switch (projection_ptr->type) */

                /* Each projection can be rotated. Does any of the angle differ from the one we used in previous calculation? */
                float rotation_angles[3] = {0};

                for (uint32_t n_axis = 0; n_axis < 3; ++n_axis)
                {
                    curve_container_get_value(projection_ptr->rotation, n_axis, time, false, combiner_ptr->variant_float);
                    system_variant_get_float (combiner_ptr->variant_float, rotation_angles + n_axis);

                    rotation_angles[n_axis] = DEG_TO_RAD(rotation_angles[n_axis]);
                }

                if (memcmp(rotation_angles, projection_ptr->last_calc_rotation_angles, sizeof(rotation_angles) ) != 0)
                {
                    /* Yes, we do need a recalc */
                    memcpy(projection_ptr->last_calc_rotation_angles, rotation_angles, sizeof(rotation_angles) );

                    update_succeeded = true;
                }

                /* If necessary, it's time to recalc */
                if (update_succeeded)
                {
                    sh_projector_execute(projection_ptr->projector, 
                                         projection_ptr->last_calc_rotation_angles,
                                         projection_ptr->result_bo_id,
                                         0,
                                         NULL);
                }
            }
            else
            {
                ASSERT_DEBUG_SYNC(false, "Could not retrieve %dth projection descriptor", n_projection);
            }

            /* If there's only a single projection, a buffer copy operation will do */
            if (update_succeeded)
            {
                if (n_projections == 1)
                {
                    uint32_t read_sh_data_bo_offset =-1;

                    sh_projector_get_projection_result_details(projection_ptr->projector,
                                                               SH_PROJECTION_RESULT_RRGGBB,
                                                               &read_sh_data_bo_offset,
                                                               NULL);

                    dsa_entry_points->pGLNamedCopyBufferSubDataEXT(projection_ptr->result_bo_id,
                                                                   combiner_ptr->result_sh_data_bo_id,
                                                                   read_sh_data_bo_offset,
                                                                   combiner_ptr->result_sh_data_bo_rrggbb_offset,
                                                                   combiner_ptr->result_sh_data_bo_rrggbb_size);
                }
                else
                {
                    /* Sigh */
                    ASSERT_DEBUG_SYNC(false, "Unsupported atm");
                }

                /* Update TBO object as well if needed */
                if (combiner_ptr->result_sh_data_tbo == 0 || combiner_ptr->result_sh_data_tbo_dirty)
                {
                    uint32_t rgbx_sh_data_bo_offset = 0;
                    uint32_t rgbx_sh_data_bo_size   = 0;

                    sh_projector_get_projection_result_details(projection_ptr->projector,
                                                               SH_PROJECTION_RESULT_RGBRGB,
                                                               &rgbx_sh_data_bo_offset,
                                                               &rgbx_sh_data_bo_size);

                    if (combiner_ptr->result_sh_data_tbo == 0)
                    {
                        combiner_ptr->result_sh_data_tbo = ogl_texture_create(combiner_ptr->context, system_hashed_ansi_string_create_by_merging_two_strings("SH Projector combiner ",
                                                                                                                                                             system_hashed_ansi_string_get_buffer(combiner_ptr->name)));
                    }

                    dsa_entry_points->pGLTextureBufferRangeEXT(combiner_ptr->result_sh_data_tbo,
                                                               GL_TEXTURE_BUFFER,
                                                               GL_R32F,
                                                               projection_ptr->result_bo_id,
                                                               rgbx_sh_data_bo_offset,
                                                               rgbx_sh_data_bo_size);

                    combiner_ptr->result_sh_data_tbo_offset = rgbx_sh_data_bo_offset;
                    combiner_ptr->result_sh_data_tbo_size   = rgbx_sh_data_bo_size;
                    combiner_ptr->result_sh_data_tbo_dirty  = false;

                    ASSERT_DEBUG_SYNC(entry_points->pGLGetError() == GL_NO_ERROR, "Could not update texture buffer");
                }
            }
        }

        system_critical_section_leave(combiner_ptr->serialization_cs);
    } /* if (system_critical_section_try_enter(combiner_ptr->serialization_cs) ) */

    return true;
}

/** Please see header for specification */
PUBLIC EMERALD_API uint32_t sh_projector_combiner_get_amount_of_sh_projections(__in __notnull sh_projector_combiner combiner)
{
    return system_hash64map_get_amount_of_elements( ((_sh_projector_combiner*)combiner)->projections);
}

/** Please see header for specification */
PUBLIC EMERALD_API bool sh_projector_combiner_get_sh_projection_property_details(__in __notnull sh_projector_combiner  combiner,
                                                                                 __in           sh_projection_id       projection_id,
                                                                                 __in           sh_projector_property  projection_property,
                                                                                 __out          void*                  out_result)
{
    _sh_projector_combiner*            combiner_ptr   = (_sh_projector_combiner*) combiner;
    _sh_projector_combiner_projection* projection_ptr = NULL;
    bool                               result         = false;

    if (system_hash64map_get(combiner_ptr->projections, (system_hash64) projection_id, &projection_ptr) )
    {
        result = true;

        switch (projection_property)
        {
            case SH_PROJECTOR_PROPERTY_SKY_ZENITH_LUMINANCE: *((curve_container*)   out_result) = projection_ptr->sky_zenith_luminance; result &= projection_ptr->sky_zenith_luminance != NULL; break;
            case SH_PROJECTOR_PROPERTY_COLOR:                *((curve_container*)   out_result) = projection_ptr->color;                result &= projection_ptr->color                != NULL; break;
            case SH_PROJECTOR_PROPERTY_CUTOFF:               *((curve_container*)   out_result) = projection_ptr->cutoff;               result &= projection_ptr->cutoff               != NULL; break;
            case SH_PROJECTOR_PROPERTY_TEXTURE_ID:           *((curve_container*)   out_result) = projection_ptr->texture_id;           result &= projection_ptr->texture_id           != NULL; break;
            case SH_PROJECTOR_PROPERTY_LINEAR_EXPOSURE:      *((curve_container*)   out_result) = projection_ptr->linear_exposure;      result &= projection_ptr->linear_exposure      != NULL; break;
            case SH_PROJECTOR_PROPERTY_ROTATION:             *((curve_container*)   out_result) = projection_ptr->rotation;             result &= projection_ptr->rotation             != NULL; break;
            case SH_PROJECTOR_PROPERTY_TYPE:                 *((sh_projection_type*)out_result) = projection_ptr->type;                                                                         break;

            default:
            {
                result = false;

                ASSERT_DEBUG_SYNC(false, "Unrecognized SH projection property");
            }
        } /* switch (projection_property) */
    }

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool sh_projector_combiner_get_sh_projection_property_id(__in  __notnull sh_projector_combiner combiner,
                                                                            __in            uint32_t              index,
                                                                            __out __notnull sh_projection_id*     result)
{
    bool                               b_result       = false;
    _sh_projector_combiner*            combiner_ptr   = (_sh_projector_combiner*) combiner;
    _sh_projector_combiner_projection* projection_ptr = NULL;
    system_hash64                      projection_id  = 0;

    if (system_hash64map_get_element_at(combiner_ptr->projections, index, &projection_ptr, &projection_id) )
    {
        *result  = (sh_projection_id) projection_id;
        b_result = true;
    }

    return b_result;
}

/** Please see header for specification */
PUBLIC EMERALD_API void sh_projector_combiner_get_sh_projection_result_bo_data_details(__in  __notnull   sh_projector_combiner instance,
                                                                                       __out __maybenull GLuint*               result_bo_id,
                                                                                       __out __maybenull GLuint*               result_bo_rrggbb_offset,
                                                                                       __out __maybenull GLuint*               result_bo_rrggbb_data_size,
                                                                                       __out __maybenull ogl_texture*          result_tbo)
{
    _sh_projector_combiner* combiner_ptr = (_sh_projector_combiner*) instance;

    if (result_bo_id != NULL)
    {
        *result_bo_id = combiner_ptr->result_sh_data_bo_id;
    }

    if (result_bo_rrggbb_offset != NULL)
    {
        *result_bo_rrggbb_offset = combiner_ptr->result_sh_data_bo_rrggbb_offset;
    }

    if (result_bo_rrggbb_data_size != NULL)
    {
        *result_bo_rrggbb_data_size = combiner_ptr->result_sh_data_bo_rrggbb_size;
    }

    if (result_tbo != NULL)
    {
        *result_tbo = combiner_ptr->result_sh_data_tbo;
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API void sh_projector_combiner_set_callback(__in           _sh_projector_combiner_callback_type callback_type,
                                                           __in __notnull PFNPROJECTORCOMBINERCALLBACKPROC     func_ptr,
                                                           __in __notnull void*                                user_arg)
{
    switch (callback_type)
    {
        case SH_PROJECTOR_COMBINER_CALLBACK_TYPE_ON_PROJECTION_ADDED:
        {
            callback_on_projection_added.arg      = user_arg;
            callback_on_projection_added.func_ptr = func_ptr;

            break;
        }
        
        case SH_PROJECTOR_COMBINER_CALLBACK_TYPE_ON_PROJECTION_DELETED:
        {
            callback_on_projection_deleted.arg      = user_arg;
            callback_on_projection_deleted.func_ptr = func_ptr;

            break;
        }
        
        default:
        {
            ASSERT_DEBUG_SYNC(false, "Unrecognized callback type");
        }
    } /* switch (callback_type) */
}

#endif
