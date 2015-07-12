/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#ifndef OGL_TEXT_H
#define OGL_TEXT_H

#include "gfx/gfx_types.h"
#include "ogl/ogl_types.h"

REFCOUNT_INSERT_DECLARATIONS(ogl_text,
                             ogl_text)

typedef uint32_t ogl_text_string_id;
const            ogl_text_string_id TEXT_STRING_ID_DEFAULT = (ogl_text_string_id) -1;

typedef enum
{
    OGL_TEXT_STRING_PROPERTY_COLOR,          /* settable,     float[3]. Also takes TEXT_STRING_ID_DEFAULT as string id */
    OGL_TEXT_STRING_PROPERTY_POSITION_PX,    /* settable,     int[2] */
    OGL_TEXT_STRING_PROPERTY_POSITION_SS,    /* settable,     float[2] */
    OGL_TEXT_STRING_PROPERTY_SCALE,          /* settable,     float.    Also takes TEXT_STRING_ID_DEFAULT as string id */
    OGL_TEXT_STRING_PROPERTY_SCISSOR_BOX,    /* settable,     GLint[4]. (-1, -1, -1, -1) disables the scissor test for the string.
                                              *               Scissor test is disabled by default */
    OGL_TEXT_STRING_PROPERTY_TEXT_HEIGHT_PX, /* not settable, int */
    OGL_TEXT_STRING_PROPERTY_TEXT_HEIGHT_SS, /* not settable, float */
    OGL_TEXT_STRING_PROPERTY_TEXT_WIDTH_PX,  /* not settable, int */
    OGL_TEXT_STRING_PROPERTY_VISIBILITY      /* settable,     bool */
} ogl_text_string_property;

/** TODO */
PUBLIC EMERALD_API ogl_text_string_id ogl_text_add_string(ogl_text text);

/** TODO */
PUBLIC EMERALD_API ogl_text ogl_text_create(system_hashed_ansi_string name,
                                            ogl_context               context,
                                            gfx_bfg_font_table        font_table,
                                            uint32_t                  screen_width,
                                            uint32_t                  screen_height);

/** TODO */
PUBLIC EMERALD_API void ogl_text_delete_string(ogl_text           text,
                                               ogl_text_string_id text_id);

/** TODO */
PUBLIC EMERALD_API void ogl_text_draw(ogl_context context,
                                      ogl_text    text);

/** TODO */
PUBLIC EMERALD_API const unsigned char* ogl_text_get(ogl_text           text,
                                                     ogl_text_string_id text_string_id);

/** TODO */
PUBLIC EMERALD_API uint32_t ogl_text_get_added_strings_counter(ogl_text instance);

/** TODO */
PUBLIC EMERALD_API ogl_context ogl_text_get_context(ogl_text text);

/** TODO */
PUBLIC EMERALD_API void ogl_text_get_text_string_property(ogl_text                 text,
                                                          ogl_text_string_property property,
                                                          ogl_text_string_id       text_string_id,
                                                          void*                    out_result);

/** TODO */
PUBLIC EMERALD_API void ogl_text_set(ogl_text           text,
                                     ogl_text_string_id text_string_id,
                                     const char*        raw_text_ptr);

/** TODO */
PUBLIC EMERALD_API void ogl_text_set_screen_properties(ogl_text instance,
                                                       uint32_t screen_width,
                                                       uint32_t screen_height);

/** TODO */
PUBLIC EMERALD_API void ogl_text_set_text_string_property(ogl_text                 text,
                                                          ogl_text_string_id       text_string_id,
                                                          ogl_text_string_property property,
                                                          const void*              data);

#endif /* OGL_TEXT_H */