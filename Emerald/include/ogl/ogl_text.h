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
PUBLIC EMERALD_API ogl_text_string_id ogl_text_add_string(__in __notnull ogl_text text);

/** TODO */
PUBLIC EMERALD_API ogl_text ogl_text_create(__in __notnull system_hashed_ansi_string name,
                                            __in __notnull ogl_context               context,
                                            __in __notnull gfx_bfg_font_table        font_table,
                                            __in           uint32_t                  screen_width,
                                            __in           uint32_t                  screen_height);

/** TODO */
PUBLIC EMERALD_API void ogl_text_delete_string(__in __notnull ogl_text           text,
                                               __in           ogl_text_string_id text_id);

/** TODO */
PUBLIC EMERALD_API void ogl_text_draw(__in __notnull ogl_context context,
                                      __in __notnull ogl_text    text);

/** TODO */
PUBLIC EMERALD_API const unsigned char* ogl_text_get(__in __notnull ogl_text           text,
                                                     __in           ogl_text_string_id text_string_id);

/** TODO */
PUBLIC EMERALD_API uint32_t ogl_text_get_added_strings_counter(__in __notnull ogl_text instance);

/** TODO */
PUBLIC EMERALD_API ogl_context ogl_text_get_context(__in __notnull ogl_text text);

/** TODO */
PUBLIC EMERALD_API void ogl_text_get_text_string_property(__in  __notnull ogl_text                 text,
                                                          __in            ogl_text_string_property property,
                                                          __in            ogl_text_string_id       text_string_id,
                                                          __out __notnull void*                    out_result);

/** TODO */
PUBLIC EMERALD_API void ogl_text_set(__in __notnull ogl_text           text,
                                     __in           ogl_text_string_id text_string_id,
                                     __in __notnull const char*        raw_text_ptr);

/** TODO */
PUBLIC EMERALD_API void ogl_text_set_screen_properties(__in __notnull ogl_text instance,
                                                       __in           uint32_t screen_width,
                                                       __in           uint32_t screen_height);

/** TODO */
PUBLIC EMERALD_API void ogl_text_set_text_string_property(__in __notnull ogl_text                 text,
                                                          __in           ogl_text_string_id       text_string_id,
                                                          __in           ogl_text_string_property property,
                                                          __in __notnull const void*              data);

#endif /* OGL_TEXT_H */