#ifndef RAL_CONTEXT_H
#define RAL_CONTEXT_H

#include "demo/demo_types.h"
#include "gfx/gfx_types.h"
#include "ogl/ogl_types.h" /* TODO: Remove OGL dep */
#include "ral/ral_types.h"


REFCOUNT_INSERT_DECLARATIONS(ral_context,
                             ral_context);

typedef bool (*PFNRALCONTEXTCREATETEXTUREVIEWPROC)(ral_context                         context,
                                                   uint32_t                            n_texture_views,
                                                   const ral_texture_view_create_info* texture_view_create_info_ptr,
                                                   ral_texture_view*                   out_result_texture_views_ptr);

/** Optimus: forces High Performance profile */
#ifdef _WIN32
    #define INCLUDE_OPTIMUS_SUPPORT                                          \
        extern "C"                                                           \
        {                                                                    \
        _declspec(dllexport) extern DWORD NvOptimusEnablement = 0x00000001; \
        }
#else
    #define INCLUDE_OPTIMUS_SUPPORT
#endif

/* ral_context call-backs. These should be used by back-ends to notify about RAL object modifications. */
enum
{
    /* Context is about to be released.
     *
     * arg: ral_context instance.
     **/
    RAL_CONTEXT_CALLBACK_ID_ABOUT_TO_RELEASE,

    /* One or more ral_buffer instances were created.
     *
     * NOTE: This callback ID only supports synchronous call-backs.
     *
     * arg: ral_context_callback_objects_created_callback_arg
     */
    RAL_CONTEXT_CALLBACK_ID_BUFFERS_CREATED,

    /* One or more existing ral_buffer instances were deleted.
     *
     * NOTE: This callback ID only supports synchronous call-backs.
     *
     * arg: ral_context_callback_objects_deleted_callback_arg instance.
     */
    RAL_CONTEXT_CALLBACK_ID_BUFFERS_DELETED,


    /* One or more ral_command_buffer instances were created.
     *
     * NOTE: This callback ID only supports synchronous call-backs.
     *
     * arg: ral_context_callback_objects_created_callback_arg
     */
    RAL_CONTEXT_CALLBACK_ID_COMMAND_BUFFERS_CREATED,

    /* One or more existing ral_command_buffer instances were deleted.
     *
     * NOTE: This callback ID only supports synchronous call-backs.
     *
     * arg: ral_context_callback_objects_deleted_callback_arg instance.
     */
    RAL_CONTEXT_CALLBACK_ID_COMMAND_BUFFERS_DELETED,


    /* One or more ral_gfx_state instances were created.
     *
     * NOTE: This callback ID only supports synchronous call-backs.
     *
     * arg: ral_context_callback_objects_created_callback_arg
     */
    RAL_CONTEXT_CALLBACK_ID_GFX_STATES_CREATED,

    /* One or more existing ral_gfx_state instances were deleted.
     *
     * NOTE: This callback ID only supports synchronous call-backs.
     *
     * arg: ral_context_callback_objects_deleted_callback_arg instance.
     */
    RAL_CONTEXT_CALLBACK_ID_GFX_STATES_DELETED,


    /* One or more ral_program instances were created.
     *
     * NOTE: This callbacK ID only supports synchronous call-backs.
     *
     * arg: ral_context_callback_objects_created_callback_arg
     **/
    RAL_CONTEXT_CALLBACK_ID_PROGRAMS_CREATED,

    /* One or more existing ral_program instances were deleted.
     *
     * NOTE: This callback ID only supports synchronous call-backs.
     *
     * arg: ral_context_callback_objects_deleted_callback_arg instance.
     */
    RAL_CONTEXT_CALLBACK_ID_PROGRAMS_DELETED,


    /* One or more ral_sampler instances were created.
     *
     * NOTE: This callbacK ID only supports synchronous call-backs.
     *
     * arg: ral_context_callback_objects_created_callback_arg
     **/
    RAL_CONTEXT_CALLBACK_ID_SAMPLERS_CREATED,

    /* One or more existing ral_sampler instances were deleted.
     *
     * NOTE: This callback ID only supports synchronous call-backs.
     *
     * arg: ral_context_callback_objects_deleted_callback_arg instance.
     */
    RAL_CONTEXT_CALLBACK_ID_SAMPLERS_DELETED,


    /* One or more ral_shader instances were created.
     *
     * NOTE: This callback ID only supports synchronous call-backs.
     *
     * arg: ral_context_callback_objects_created_callback_arg
     **/
    RAL_CONTEXT_CALLBACK_ID_SHADERS_CREATED,

    /* One or more existing ral_shader instances were deleted.
     *
     * NOTE: This callback ID only supports synchronous call-backs.
     *
     * arg: ral_context_callback_objects_deleted_callback_arg instance.
     */
    RAL_CONTEXT_CALLBACK_ID_SHADERS_DELETED,


    /* One or more ral_texture_view instances were created.
     *
     * NOTE: This callback ID only supports synchronous call-backs.
     *
     * arg: ral_context_callback_objects_created_callback_arg
     **/
    RAL_CONTEXT_CALLBACK_ID_TEXTURE_VIEWS_CREATED,

    /* One or more existing ral_texture_view instances were deleted.
     *
     * NOTE: This callback ID only supports synchronous call-backs.
     *
     * arg: ral_context_callback_objects_deleted_callback_arg instance.
     */
    RAL_CONTEXT_CALLBACK_ID_TEXTURE_VIEWS_DELETED,


    /* One or more ral_texture instances were created.
     *
     * NOTE: This callback ID only supports synchronous call-backs.
     *
     * arg: ral_context_callback_objects_created_callback_arg
     **/
    RAL_CONTEXT_CALLBACK_ID_TEXTURES_CREATED,

    /* One or more existing ral_texture instances were deleted.
     *
     * NOTE: This callback ID only supports synchronous call-backs.
     *
     * arg: ral_context_callback_objects_deleted_callback_arg instance.
     */
    RAL_CONTEXT_CALLBACK_ID_TEXTURES_DELETED,


    /* Always last */
    RAL_CONTEXT_CALLBACK_ID_COUNT
};

typedef enum
{
    /* NOTE: If you modify enum values below, make sure to also update ral_utils_get_ral_context_object_type_has() */

    RAL_CONTEXT_OBJECT_TYPE_FIRST,
    RAL_CONTEXT_OBJECT_TYPE_BUFFER = RAL_CONTEXT_OBJECT_TYPE_FIRST,
    RAL_CONTEXT_OBJECT_TYPE_COMMAND_BUFFER,
    RAL_CONTEXT_OBJECT_TYPE_GFX_STATE,
    RAL_CONTEXT_OBJECT_TYPE_PROGRAM,
    RAL_CONTEXT_OBJECT_TYPE_SAMPLER,
    RAL_CONTEXT_OBJECT_TYPE_SHADER,
    RAL_CONTEXT_OBJECT_TYPE_TEXTURE,
    RAL_CONTEXT_OBJECT_TYPE_TEXTURE_VIEW,

    RAL_CONTEXT_OBJECT_TYPE_COUNT,
    RAL_CONTEXT_OBJECT_TYPE_UNKNOWN = RAL_CONTEXT_OBJECT_TYPE_COUNT,

    /* Only used internally by ral_context: */
    RAL_CONTEXT_OBJECT_TYPE_TEXTURE_FROM_FILE_NAME,
    RAL_CONTEXT_OBJECT_TYPE_TEXTURE_FROM_GFX_IMAGE,

} ral_context_object_type;

typedef enum
{
    /* not settable; PFNRALCONTEXTCREATETEXTUREVIEWPROC */
    RAL_CONTEXT_PRIVATE_PROPERTY_CREATE_TEXTURE_VIEW_FUNC_PTR,

    /* not settable; ral_texture_pool */
    RAL_CONTEXT_PRIVATE_PROPERTY_TEXTURE_POOL,

} ral_context_private_property;

typedef struct
{
    void**                  created_objects;
    ral_context_object_type object_type;
    uint32_t                n_objects;

} ral_context_callback_objects_created_callback_arg;

typedef struct
{
    void* const*            deleted_objects;
    ral_context_object_type object_type;
    uint32_t                n_objects;

} ral_context_callback_objects_deleted_callback_arg;


typedef void (*PFNRALCONTEXTNOTIFYBACKENDABOUTNEWOBJECTPROC)(ral_context             context,
                                                             void*                   result_object,
                                                             ral_context_object_type object_type);


/** TODO */
PUBLIC ral_context ral_context_create(system_hashed_ansi_string name,
                                      demo_window               window);

/** TODO */
PUBLIC EMERALD_API bool ral_context_create_buffers(ral_context                   context,
                                                   uint32_t                      n_buffers,
                                                   const ral_buffer_create_info* buffer_create_info_ptr,
                                                   ral_buffer*                   out_result_buffers_ptr);

/** TODO */
PUBLIC EMERALD_API bool ral_context_create_command_buffers(ral_context                           context,
                                                           uint32_t                              n_command_buffers,
                                                           const ral_command_buffer_create_info* command_buffer_create_info_ptr,
                                                           ral_command_buffer*                   out_result_command_buffers_ptr);

/** TODO */
PUBLIC EMERALD_API bool ral_context_create_gfx_states(ral_context                      context,
                                                      uint32_t                         n_create_info_items,
                                                      const ral_gfx_state_create_info* create_info_ptrs,
                                                      ral_gfx_state*                   out_result_gfx_states_ptr);

/** TODO */
PUBLIC EMERALD_API bool ral_context_create_programs(ral_context                    context,
                                                    uint32_t                       n_create_info_items,
                                                    const ral_program_create_info* create_info_ptrs,
                                                    ral_program*                   out_result_program_ptrs);

/** TODO **/
PUBLIC EMERALD_API bool ral_context_create_samplers(ral_context                    context,
                                                    uint32_t                       n_create_info_items,
                                                    const ral_sampler_create_info* create_info_ptrs,
                                                    ral_sampler*                   out_result_sampler_ptrs);

/** TODO */
PUBLIC EMERALD_API bool ral_context_create_shaders(ral_context                   context,
                                                   uint32_t                      n_create_info_items,
                                                   const ral_shader_create_info* create_info_ptrs,
                                                   ral_shader*                   out_result_shader_ptrs);

/** TODO */
PUBLIC EMERALD_API bool ral_context_create_textures(ral_context                    context,
                                                    uint32_t                       n_textures,
                                                    const ral_texture_create_info* texture_create_info_ptr,
                                                    ral_texture*                   out_result_textures_ptr);

/** TODO */
PUBLIC EMERALD_API bool ral_context_create_textures_from_file_names(ral_context                      context,
                                                                    uint32_t                         n_file_names,
                                                                    const system_hashed_ansi_string* file_names_ptr,
                                                                    ral_texture*                     out_result_textures_ptr);

/** TODO */
PUBLIC EMERALD_API bool ral_context_create_textures_from_gfx_images(ral_context      context,
                                                                    uint32_t         n_images,
                                                                    const gfx_image* images,
                                                                    ral_texture*     out_result_textures_ptr);
/** TODO */
PUBLIC EMERALD_API bool ral_context_delete_objects(ral_context             context,
                                                   ral_context_object_type object_type,
                                                   uint32_t                n_objects,
                                                   void* const*            objects);

/** TODO */
PUBLIC EMERALD_API ral_program ral_context_get_program_by_name(ral_context               context,
                                                               system_hashed_ansi_string name);

/** TODO */
PUBLIC void ral_context_get_private_property(ral_context                  context,
                                             ral_context_private_property property,
                                             void*                        out_result_ptr);

/** TODO */
PUBLIC EMERALD_API void ral_context_get_property(ral_context          context,
                                                 ral_context_property property,
                                                 void*                out_result_ptr);

/** TODO */
PUBLIC EMERALD_API ral_shader ral_context_get_shader_by_name(ral_context               context,
                                                             system_hashed_ansi_string name);

/** TODO */
PUBLIC EMERALD_API ral_texture ral_context_get_texture_by_file_name(ral_context               context,
                                                                    system_hashed_ansi_string file_name);

/** TODO */
PUBLIC EMERALD_API ral_texture ral_context_get_texture_by_name(ral_context               context,
                                                               system_hashed_ansi_string name);

/** TODO */
PUBLIC EMERALD_API bool ral_context_retain_object(ral_context             context,
                                                  ral_context_object_type object_type,
                                                  void*                   object);

/** TODO */
PUBLIC EMERALD_API bool ral_context_retain_objects(ral_context             context,
                                                   ral_context_object_type object_type,
                                                   uint32_t                n_objects,
                                                   void* const*            objects);
/** TODO */
PUBLIC void ral_context_set_property(ral_context          context,
                                     ral_context_property property,
                                     void* const*         data);

#endif /* RAL_CONTEXT_H */