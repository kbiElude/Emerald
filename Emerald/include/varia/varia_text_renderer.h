/**
 *
 * Emerald (kbi/elude @2012-2016)
 *
 */
#ifndef VARIA_TEXT_H
#define VARIA_TEXT_H

#include "gfx/gfx_types.h"
#include "ogl/ogl_types.h"
#include "ral/ral_types.h"
#include "varia/varia_types.h"


REFCOUNT_INSERT_DECLARATIONS(varia_text_renderer,
                             varia_text_renderer)

typedef uint32_t varia_text_renderer_text_string_id;
const            varia_text_renderer_text_string_id VARIA_TEXT_RENDERER_TEXT_STRING_ID_DEFAULT = (varia_text_renderer_text_string_id) -1;

typedef enum
{
    /* not settable; ral_context */
    VARIA_TEXT_RENDERER_PROPERTY_CONTEXT,

    /* not settable; uint32_t */
    VARIA_TEXT_RENDERER_PROPERTY_N_ADDED_TEXT_STRINGS,

} varia_text_renderer_property;

typedef enum
{
    /* settable, float[3]. Also takes TEXT_STRING_ID_DEFAULT as string id */
    VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_COLOR,

    /* settable, int[2] */
    VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_POSITION_PX,

    /* settable, float[2] */
    VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_POSITION_SS,

    /* settable, float.
     *
     * Also takes TEXT_STRING_ID_DEFAULT as string id */
    VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_SCALE,

    /* settable, int32_t[4]. (-1, -1, -1, -1) disables the scissor test for the string.
     *
     * [0]: x
     * [1]: y
     * [2]: width
     * [3]: height
     *
     * Scissor test is disabled by default
     */
    VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_SCISSOR_BOX,

    /* not settable, int */
    VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_TEXT_HEIGHT_PX,

    /* not settable, float */
    VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_TEXT_HEIGHT_SS,

    /* not settable, int */
    VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_TEXT_WIDTH_PX,

    /* settable, bool */
    VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_VISIBILITY
} varia_text_renderer_text_string_property;

/** TODO */
PUBLIC EMERALD_API varia_text_renderer_text_string_id varia_text_renderer_add_string(varia_text_renderer text);

/** TODO */
PUBLIC varia_text_renderer varia_text_renderer_create(system_hashed_ansi_string name,
                                                      ral_context               context,
                                                      gfx_bfg_font_table        font_table,
                                                      uint32_t                  screen_width,
                                                      uint32_t                  screen_height);

/** TODO */
PUBLIC EMERALD_API void varia_text_renderer_delete_string(varia_text_renderer                text,
                                                          varia_text_renderer_text_string_id text_id);

/** TODO */
PUBLIC EMERALD_API const unsigned char* varia_text_renderer_get(varia_text_renderer                text,
                                                                varia_text_renderer_text_string_id text_string_id);

/** TODO */
PUBLIC ral_present_task varia_text_renderer_get_present_task(varia_text_renderer text,
                                                             ral_texture_view    target_texture_view);

/** TODO */
PUBLIC EMERALD_API void varia_text_renderer_get_property(varia_text_renderer          text,
                                                         varia_text_renderer_property property,
                                                         void*                        out_result_ptr);

/** TODO */
PUBLIC EMERALD_API void varia_text_renderer_get_text_string_property(varia_text_renderer                      text,
                                                                     varia_text_renderer_text_string_property property,
                                                                     varia_text_renderer_text_string_id       text_string_id,
                                                                     void*                                    out_result_ptr);

/** TODO */
PUBLIC EMERALD_API void varia_text_renderer_set(varia_text_renderer                text,
                                                varia_text_renderer_text_string_id text_string_id,
                                                const char*                        raw_text_ptr);

/** TODO */
PUBLIC EMERALD_API void varia_text_renderer_set_text_string_property(varia_text_renderer                      text,
                                                                     varia_text_renderer_text_string_id       text_string_id,
                                                                     varia_text_renderer_text_string_property property,
                                                                     const void*                              data);

#endif /* VARIA_TEXT_RENDERER_H */