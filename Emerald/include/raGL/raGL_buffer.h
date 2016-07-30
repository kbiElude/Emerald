#ifndef RAGL_BUFFER_H
#define RAGL_BUFFER_H

#include "ogl/ogl_types.h"
#include "raGL/raGL_types.h"
#include "ral/ral_types.h"
#include "system/system_types.h"
#include <memory>
#include <vector>

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
PUBLIC void raGL_buffer_clear_region(raGL_buffer                         buffer_raGL,
                                     uint32_t                            n_clear_ops,
                                     const ral_buffer_clear_region_info* clear_ops,
                                     bool                                sync_other_contexts);

/** TODO */
PUBLIC void raGL_buffer_copy_buffer_to_buffer(raGL_buffer                           dst_buffer_raGL,
                                              raGL_buffer                           src_buffer_raGL,
                                              uint32_t                              n_copy_ops,
                                              const ral_buffer_copy_to_buffer_info* copy_ops,
                                              bool                                  sync_other_contexts);

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
PUBLIC void raGL_buffer_get_property(raGL_buffer          buffer,
                                     raGL_buffer_property property,
                                     void*                out_result_ptr);

/** TODO */
PUBLIC void raGL_buffer_release(raGL_buffer buffer);

/** TODO */
PUBLIC void raGL_buffer_update_regions_with_client_memory(raGL_buffer                                                          buffer,
                                                          std::vector<std::shared_ptr<ral_buffer_client_sourced_update_info> > updates,
                                                          bool                                                                 async,
                                                          bool                                                                 sync_other_contexts);

#endif /* RAGL_BUFFER_H */
