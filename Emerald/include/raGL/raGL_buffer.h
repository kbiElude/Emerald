#ifndef RAGL_BUFFER_H
#define RAGL_BUFFER_H

#include "ogl/ogl_types.h"
#include "ral/ral_types.h"
#include "system/system_types.h"


DECLARE_HANDLE(raGL_buffer);


typedef enum
{
    /* not settable, GLint */
    RAGL_BUFFER_PROPERTY_ID,

    /* not settable, system_memory_manager */
    RAGL_BUFFER_PROPERTY_MEMORY_MANAGER,

    /* not settable, uint32_t */
    RAGL_BUFFER_PROPERTY_START_OFFSET

} raGL_buffer_property;

/** TODO */
PUBLIC raGL_buffer raGL_buffer_create(ogl_context           context,
                                      GLint                 id,
                                      system_memory_manager memory_manager,
                                      uint32_t              start_offset,
                                      uint32_t              size);

/** TODO
 *
 *  @param size TODO. Pass 0 to use all available space.
 **/
PUBLIC raGL_buffer raGL_buffer_create_raGL_buffer_subregion(raGL_buffer parent_buffer,
                                                            uint32_t    start_offset,
                                                            uint32_t    size);

/** TODO */
PUBLIC EMERALD_API void raGL_buffer_get_property(raGL_buffer          buffer,
                                                 raGL_buffer_property property,
                                                 void*                out_result_ptr);

/** TODO */
PUBLIC void raGL_buffer_release(raGL_buffer buffer);

#endif /* RAGL_BUFFER_H */