/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_program.h"
#include "ogl/ogl_program_ub.h"
#include "ogl/ogl_programs.h"
#include "ogl/ogl_shader.h"
#include "system/system_assertions.h"
#include "system/system_file_serializer.h"
#include "system/system_hash64.h"
#include "system/system_hash64map.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include "system/system_resizable_vector.h"
#include <sstream>

/** Internal definitions */
const char* file_blob_prefix       = "temp_shader_blob_";
const char* file_sourcecode_prefix = "temp_shader_sourcecode_";

/** Internal type definitions */
typedef struct
{
    system_resizable_vector       active_attributes;
    system_resizable_vector       active_uniforms;
    system_resizable_vector       attached_shaders;
    ogl_context                   context;
    GLuint                        id;
    bool                          link_status;
    system_hashed_ansi_string     name;
    system_hashed_ansi_string     program_info_log;
    ogl_program_syncable_ubs_mode syncable_ubs_mode;

    /* Maps ogl_context instances to ogl_program_ub instances.
     *
     * If syncable_ubs_mode is set to:
     *   a) OGL_PROGRAM_SYNCABLE_UBS_MODE_DISABLE:            this map is NULL.
     *   b) OGL_PROGRAM_SYNCABLE_UBS_MODE_ENABLE_GLOBAL:      this map holds a single entry at key 0.
     *   c) OGL_PROGRAM_SYNCABLE_UBS_MODE_ENABLE_PER_CONTEXT: this map holds as many entries, as there were contexts
     *                                                        that tried to access it. use ogl_context instances as keys.
     */
    system_hash64map context_to_active_ubs_map;

    /* Same as above, but maps an ogl_context instance to a system_hash64map which
     * maps uniform block indices to ogl_program_ub instances */
    system_hash64map context_to_ub_index_to_ub_map;

    /* Same as above, but maps an ogl_context instances to a system_hash64map, which
     * maps uniform block names (represented as system_hash64) to ogl_program_ub instances */
    system_hash64map context_to_ub_name_to_ub_map; /* do NOT release the stored items */

    GLenum                    tf_mode;
    GLchar**                  tf_varyings;
    unsigned int              n_tf_varyings;

    /* GL entry-point cache */
    PFNGLATTACHSHADERPROC              pGLAttachShader;
    PFNGLCREATEPROGRAMPROC             pGLCreateProgram;
    PFNGLDELETEPROGRAMPROC             pGLDeleteProgram;
    PFNGLDETACHSHADERPROC              pGLDetachShader;
    PFNGLGETACTIVEATTRIBPROC           pGLGetActiveAttrib;
    PFNGLGETACTIVEUNIFORMPROC          pGLGetActiveUniform;
    PFNGLGETACTIVEUNIFORMBLOCKNAMEPROC pGLGetActiveUniformBlockName;
    PFNGLGETACTIVEUNIFORMSIVPROC       pGLGetActiveUniformsiv;
    PFNGLGETATTRIBLOCATIONPROC         pGLGetAttribLocation;
    PFNGLGETERRORPROC                  pGLGetError;
    PFNGLGETPROGRAMBINARYPROC          pGLGetProgramBinary;
    PFNGLGETPROGRAMINFOLOGPROC         pGLGetProgramInfoLog;
    PFNGLGETPROGRAMIVPROC              pGLGetProgramiv;
    PFNGLGETUNIFORMLOCATIONPROC        pGLGetUniformLocation;
    PFNGLLINKPROGRAMPROC               pGLLinkProgram;
    PFNGLPROGRAMBINARYPROC             pGLProgramBinary;
    PFNGLPROGRAMPARAMETERIPROC         pGLProgramParameteri;
    PFNGLTRANSFORMFEEDBACKVARYINGSPROC pGLTransformFeedbackVaryings;
    PFNGLUNIFORMBLOCKBINDINGPROC       pGLUniformBlockBinding;

    REFCOUNT_INSERT_VARIABLES
} _ogl_program;

typedef struct
{
    _ogl_program* program_ptr;
    ogl_shader    shader;
} _ogl_attach_shader_callback_argument;

typedef _ogl_attach_shader_callback_argument _ogl_detach_shader_callback_argument;


/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(ogl_program,
                               ogl_program,
                              _ogl_program);


/** Internal variables */

/* Forward declarations */
PRIVATE void _ogl_program_attach_shader_callback   (__in __notnull ogl_context            context,
                                                                   void*                  in_arg);
PRIVATE void _ogl_program_create_callback          (__in __notnull ogl_context            context,
                                                                   void*                  in_arg);
PRIVATE void _ogl_program_detach_shader_callback   (__in __notnull ogl_context            context,
                                                                   void*                  in_arg);
PRIVATE char*_ogl_program_get_binary_blob_file_name(__in __notnull _ogl_program*          program_ptr);
PRIVATE char*_ogl_program_get_source_code_file_name(__in __notnull _ogl_program*          program_ptr);
PRIVATE void _ogl_program_link_callback            (__in __notnull ogl_context            context,
                                                                   void*                  in_arg);
PRIVATE bool _ogl_program_load_binary_blob         (__in __notnull ogl_context,
                                                    __in __notnull _ogl_program*           program_ptr);
PRIVATE void _ogl_program_release                  (__in __notnull void*                   program);
PRIVATE void _ogl_program_release_active_attributes(               system_resizable_vector active_attributes);
PRIVATE void _ogl_program_release_callback         (__in __notnull ogl_context             context,
                                                                   void*                   in_arg);
PRIVATE void _ogl_program_save_binary_blob         (__in __notnull ogl_context,
                                                    __in __notnull _ogl_program*           program_ptr);
PRIVATE void _ogl_program_save_shader_sources      (__in __notnull _ogl_program*           program_ptr);


/** TODO */
PRIVATE void _ogl_program_attach_shader_callback(__in __notnull ogl_context context,
                                                                void*       in_arg)
{
    _ogl_attach_shader_callback_argument* in_data = (_ogl_attach_shader_callback_argument*) in_arg;

    in_data->program_ptr->pGLAttachShader(in_data->program_ptr->id,
                                          ogl_shader_get_id(in_data->shader) );

    /* Retain the shader object. */
    ogl_shader_retain(in_data->shader);

    /* Store the handle */
    system_resizable_vector_push(in_data->program_ptr->attached_shaders,
                                 in_data->shader);
}

/** TODO */
PRIVATE void _ogl_program_create_callback(__in __notnull ogl_context context,
                                                         void*       in_arg)
{
    _ogl_program* program_ptr = (_ogl_program*) in_arg;

    program_ptr->id                    = program_ptr->pGLCreateProgram();
    program_ptr->active_attributes     = system_resizable_vector_create(BASE_PROGRAM_ACTIVE_ATTRIBUTES_NUMBER,
                                                                        sizeof(ogl_program_attribute_descriptor*) );
    program_ptr->active_uniforms       = system_resizable_vector_create(BASE_PROGRAM_ACTIVE_UNIFORMS_NUMBER,
                                                                        sizeof(ogl_program_uniform_descriptor*) );
    program_ptr->attached_shaders      = system_resizable_vector_create(BASE_PROGRAM_ATTACHED_SHADERS_NUMBER,
                                                                        sizeof(ogl_shader) );

    ASSERT_ALWAYS_SYNC(program_ptr->active_attributes != NULL,
                       "Out of memory while allocating active attributes resizable vector");
    ASSERT_ALWAYS_SYNC(program_ptr->active_uniforms != NULL,
                       "Out of memory while allocating active uniforms resizable vector");
    ASSERT_ALWAYS_SYNC(program_ptr->attached_shaders != NULL,
                       "Out of memory while allocating attached shaders resizable vector");

    /* Let the impl know that we will be interested in extracting the blob */
    program_ptr->pGLProgramParameteri(program_ptr->id,
                                      GL_PROGRAM_BINARY_RETRIEVABLE_HINT,
                                      GL_TRUE);
}

/** TODO */
PRIVATE void _ogl_program_detach_shader_callback(__in __notnull ogl_context context,
                                                                void*       in_arg)
{
    _ogl_detach_shader_callback_argument* in_data = (_ogl_detach_shader_callback_argument*) in_arg;

    in_data->program_ptr->pGLDetachShader(in_data->program_ptr->id,
                                          ogl_shader_get_id(in_data->shader) );

    /* Release the shader object. */
    ogl_shader_release(in_data->shader);

    /* Remove the handle */
    system_resizable_vector_delete_element_at(in_data->program_ptr->attached_shaders,
                                              system_resizable_vector_find(in_data->program_ptr->attached_shaders,
                                                                           in_data->shader) );
}

/** TODO */
PRIVATE char*_ogl_program_get_binary_blob_file_name(__in __notnull _ogl_program* program_ptr)
{
    /* Form the hash-based file name */
    std::stringstream file_name_sstream;
    std::string       file_name_string;

    file_name_sstream << file_blob_prefix
                      << system_hashed_ansi_string_get_hash(program_ptr->name);

    file_name_string  = file_name_sstream.str();

    /* Form the result string */
    char* file_name = new (std::nothrow) char[file_name_string.length() + 1];

    memcpy(file_name,
           file_name_string.c_str(),
           file_name_string.length() + 1);

    return file_name;
}

/** TODO */
PRIVATE char*_ogl_program_get_source_code_file_name(__in __notnull _ogl_program* program_ptr)
{
    /* Form the hash-based file name */
    std::stringstream file_name_sstream;
    std::string       file_name_string;

    file_name_sstream << file_sourcecode_prefix
                      << system_hashed_ansi_string_get_hash(program_ptr->name);

    file_name_string  = file_name_sstream.str();

    /* Form the result string */
    char* file_name = new (std::nothrow) char[file_name_string.length() + 1];

    memcpy(file_name,
           file_name_string.c_str(),
           file_name_string.length() + 1);

    return file_name;
}

PRIVATE void _ogl_program_init_uniform_blocks_for_context(__in __notnull _ogl_program* program_ptr,
                                                          __in __notnull ogl_context   context_map_key,
                                                          __in __notnull ogl_context   context)
{
    GLint   n_active_uniform_blocks           = 0;
    GLint   n_active_uniform_block_max_length = 0;
    GLchar* uniform_block_name                = NULL;

    program_ptr->pGLGetProgramiv(program_ptr->id,
                                 GL_ACTIVE_UNIFORM_BLOCKS,
                                &n_active_uniform_blocks);
    program_ptr->pGLGetProgramiv(program_ptr->id,
                                 GL_ACTIVE_UNIFORM_BLOCK_MAX_NAME_LENGTH,
                                &n_active_uniform_block_max_length);

    uniform_block_name = new (std::nothrow) GLchar[n_active_uniform_block_max_length + 1];

    ASSERT_ALWAYS_SYNC(uniform_block_name != NULL,
                       "Could not allocate [%d] bytes for active uniform block name",
                       n_active_uniform_block_max_length + 1);

    for (GLint n_active_uniform_block = 0;
               n_active_uniform_block < n_active_uniform_blocks;
             ++n_active_uniform_block)
    {
        system_hashed_ansi_string uniform_block_name_has = NULL;

        program_ptr->pGLGetActiveUniformBlockName(program_ptr->id,
                                                  n_active_uniform_block,
                                                  n_active_uniform_block_max_length + 1,
                                                  NULL, /* length */
                                                  uniform_block_name);

        uniform_block_name_has = system_hashed_ansi_string_create(uniform_block_name);

        ogl_program_ub new_ub = ogl_program_ub_create(context,
                                                      (ogl_program) program_ptr,
                                                      n_active_uniform_block,
                                                      uniform_block_name_has,
                                                      program_ptr->syncable_ubs_mode != OGL_PROGRAM_SYNCABLE_UBS_MODE_DISABLE);

        ASSERT_ALWAYS_SYNC(new_ub != NULL,
                           "ogl_program_ub_create() returned NULL.");

        /* Go ahead and store the UB info */
        system_resizable_vector context_active_ubs         = NULL;
        system_hash64map        context_ub_index_to_ub_map = NULL;
        system_hash64map        context_ub_name_to_ub_map  = NULL;

        system_hash64map_get(program_ptr->context_to_active_ubs_map,
                             (system_hash64) context_map_key,
                            &context_active_ubs);
        system_hash64map_get(program_ptr->context_to_ub_index_to_ub_map,
                             (system_hash64) context_map_key,
                            &context_ub_index_to_ub_map);
        system_hash64map_get(program_ptr->context_to_ub_name_to_ub_map,
                             (system_hash64) context_map_key,
                            &context_ub_name_to_ub_map);

        if (context_active_ubs         == NULL ||
            context_ub_index_to_ub_map == NULL ||
            context_ub_name_to_ub_map  == NULL)
        {
            context_active_ubs         = system_resizable_vector_create(BASE_PROGRAM_ACTIVE_UNIFORM_BLOCKS_NUMBER,
                                                                        sizeof(ogl_program_ub) );
            context_ub_index_to_ub_map = system_hash64map_create       (sizeof(ogl_program_ub) );
            context_ub_name_to_ub_map  = system_hash64map_create       (sizeof(ogl_program_ub) );

            ASSERT_DEBUG_SYNC(context_active_ubs         != NULL &&
                              context_ub_index_to_ub_map != NULL &&
                              context_ub_name_to_ub_map  != NULL,
                              "Sanity check failed");

            system_hash64map_insert(program_ptr->context_to_active_ubs_map,
                                    (system_hash64) context_map_key,
                                    context_active_ubs,
                                    NULL,  /* on_remove_callback          */
                                    NULL); /* on_remove_callback_user_arg */
            system_hash64map_insert(program_ptr->context_to_ub_index_to_ub_map,
                                    (system_hash64) context_map_key,
                                    context_ub_index_to_ub_map,
                                    NULL,  /* on_remove_callback          */
                                    NULL); /* on_remove_callback_user_arg */
            system_hash64map_insert(program_ptr->context_to_ub_name_to_ub_map,
                                    (system_hash64) context_map_key,
                                    context_ub_name_to_ub_map,
                                    NULL,  /* on_remove_callback          */
                                    NULL); /* on_remove_callback_user_arg */
        }
        else
        {
            ASSERT_DEBUG_SYNC(context_active_ubs         != NULL &&
                              context_ub_index_to_ub_map != NULL &&
                              context_ub_name_to_ub_map  != NULL,
                              "Sanity check failed");
        }

        ASSERT_DEBUG_SYNC(system_resizable_vector_find(context_active_ubs,
                                                       new_ub) == ITEM_NOT_FOUND,
                          "Sanity check failed");
        ASSERT_DEBUG_SYNC(!system_hash64map_contains(context_ub_index_to_ub_map,
                                                     (system_hash64) n_active_uniform_block),
                          "Sanity check failed");
        ASSERT_DEBUG_SYNC(!system_hash64map_contains(context_ub_name_to_ub_map,
                                                     system_hashed_ansi_string_get_hash(uniform_block_name_has) ),
                          "Sanity check failed");

        system_resizable_vector_push(context_active_ubs,
                                     new_ub);
        system_hash64map_insert     (context_ub_index_to_ub_map,
                                     n_active_uniform_block,
                                     new_ub,
                                     NULL,  /* on_remove_callback */
                                     NULL); /* on_remove_callback_user_arg */
        system_hash64map_insert     (context_ub_name_to_ub_map,
                                     system_hashed_ansi_string_get_hash(uniform_block_name_has),
                                     new_ub,
                                     NULL,  /* on_remove_callback */
                                     NULL); /* on_remove_callback_user_arg */

        /* Set up the UB block->binding mapping */
        program_ptr->pGLUniformBlockBinding(program_ptr->id,
                                            n_active_uniform_block,
                                            n_active_uniform_block);
    } /* for (all uniform blocks) */

    if (uniform_block_name != NULL)
    {
        delete [] uniform_block_name;
        uniform_block_name = NULL;
    }
}

/** TODO */
PRIVATE void _ogl_program_link_callback(__in __notnull ogl_context context,
                                                       void*       in_arg)
{
    _ogl_program* program_ptr     = (_ogl_program*) in_arg;
    bool          has_used_binary = false;

    system_timeline_time start_time = system_time_now();

    /* If program binaries are supportd, see if we've got a blob file already stashed. If so, no need to link at this point */
    has_used_binary = _ogl_program_load_binary_blob(context,
                                                    program_ptr);

    if (!has_used_binary)
    {
        /* Declare TF varyings, if these were provided earlier */
        if (program_ptr->tf_varyings != NULL)
        {
            program_ptr->pGLTransformFeedbackVaryings(program_ptr->id,
                                                      program_ptr->n_tf_varyings,
                                                      program_ptr->tf_varyings,
                                                      program_ptr->tf_mode);
        }

        /* Okay, let's link */
        program_ptr->pGLLinkProgram(program_ptr->id);
    }

    /* Retrieve link status */
    GLint link_status = 0;

    program_ptr->pGLGetProgramiv(program_ptr->id,
                                 GL_LINK_STATUS,
                                &link_status);

    program_ptr->link_status = (link_status == 1);
    if (program_ptr->link_status)
    {
        if (!has_used_binary)
        {
            /* Stash the binary to local storage! */
            _ogl_program_save_binary_blob(context,
                                          program_ptr);

            /* On debug builds, also stash source code of the shaders that were used
             * to create the program */
#ifdef _DEBUG
            _ogl_program_save_shader_sources(program_ptr);
#endif
        }

        /* Retrieve attibute & uniform data for the program */
        GLint n_active_attributes               = 0;
        GLint n_active_attribute_max_length     = 0;
        GLint n_active_uniforms                 = 0;
        GLint n_active_uniform_max_length       = 0;

        program_ptr->pGLGetProgramiv(program_ptr->id,
                                     GL_ACTIVE_ATTRIBUTES,
                                    &n_active_attributes);
        program_ptr->pGLGetProgramiv(program_ptr->id,
                                     GL_ACTIVE_ATTRIBUTE_MAX_LENGTH,
                                    &n_active_attribute_max_length);
        program_ptr->pGLGetProgramiv(program_ptr->id,
                                     GL_ACTIVE_UNIFORMS,
                                    &n_active_uniforms);
        program_ptr->pGLGetProgramiv(program_ptr->id,
                                     GL_ACTIVE_UNIFORM_MAX_LENGTH,
                                    &n_active_uniform_max_length);

        /* Allocate temporary name buffers */
        GLchar* attribute_name = new (std::nothrow) GLchar[n_active_attribute_max_length + 1];
        GLchar* uniform_name   = new (std::nothrow) GLchar[n_active_uniform_max_length   + 1];

        ASSERT_ALWAYS_SYNC(attribute_name != NULL,
                           "Could not allocate [%d] bytes for active attribute name",
                           n_active_attribute_max_length + 1);
        ASSERT_ALWAYS_SYNC(uniform_name!= NULL,
                           "Could not allocate [%d] bytes for active uniform name",
                           n_active_uniform_max_length + 1);

        if (attribute_name     != NULL &&
            uniform_name       != NULL)
        {
            /* Focus on attributes for a minute */
            for (GLint n_active_attribute = 0;
                       n_active_attribute < n_active_attributes;
                     ++n_active_attribute)
            {
                ogl_program_attribute_descriptor* new_attribute = new ogl_program_attribute_descriptor;

                ASSERT_ALWAYS_SYNC(new_attribute != NULL,
                                   "Out of memory while allocating new attribute descriptor.");
                if (new_attribute != NULL)
                {
                    new_attribute->length = 0;
                    new_attribute->name   = NULL;
                    new_attribute->size   = 0;
                    new_attribute->type   = PROGRAM_ATTRIBUTE_TYPE_UNDEFINED;

                    memset(attribute_name, 0, new_attribute->length + 1);

                    program_ptr->pGLGetActiveAttrib(program_ptr->id,
                                                    n_active_attribute,
                                                    n_active_attribute_max_length+1,
                                                    &new_attribute->length,
                                                    &new_attribute->size,
                                                    (GLenum*) &new_attribute->type,
                                                    attribute_name);

                    new_attribute->name     = system_hashed_ansi_string_create (attribute_name);
                    new_attribute->location = program_ptr->pGLGetAttribLocation(program_ptr->id,
                                                                                attribute_name);
                }

                system_resizable_vector_push(program_ptr->active_attributes,
                                             new_attribute);
            } /* for (GLint n_active_attribute = 0; n_active_attribute < n_active_attributes; ++n_active_attribute)*/

            /* Now for the uniforms */
            for (GLint n_active_uniform = 0;
                       n_active_uniform < n_active_uniforms;
                     ++n_active_uniform)
            {
                ogl_program_uniform_descriptor* new_uniform = new (std::nothrow) ogl_program_uniform_descriptor;

                ASSERT_ALWAYS_SYNC(new_uniform != NULL,
                                   "Out of memory while allocating new uniform descriptor.");
                if (new_uniform != NULL)
                {
                    new_uniform->length   = 0;
                    new_uniform->location = 0;
                    new_uniform->name     = NULL;
                    new_uniform->size     = 0;
                    new_uniform->type     = PROGRAM_UNIFORM_TYPE_UNDEFINED;

                    memset(uniform_name,
                           0,
                           new_uniform->length + 1);

                    program_ptr->pGLGetActiveUniform   (program_ptr->id,
                                                        n_active_uniform,
                                                        n_active_uniform_max_length + 1,
                                                       &new_uniform->length,
                                                       &new_uniform->size,
                                                        (GLenum*) &new_uniform->type,
                                                        uniform_name);
                    program_ptr->pGLGetActiveUniformsiv(program_ptr->id,
                                                        1,
                                                        (const GLuint*) &n_active_uniform,
                                                        GL_UNIFORM_ARRAY_STRIDE,
                                                       &new_uniform->ub_array_stride);
                    program_ptr->pGLGetActiveUniformsiv(program_ptr->id,
                                                        1,
                                                        (const GLuint*) &n_active_uniform,
                                                        GL_UNIFORM_BLOCK_INDEX,
                                                       &new_uniform->ub_id);
                    program_ptr->pGLGetActiveUniformsiv(program_ptr->id,
                                                        1,
                                                        (const GLuint*) &n_active_uniform,
                                                        GL_UNIFORM_IS_ROW_MAJOR,
                                                       &new_uniform->is_row_major_matrix);
                    program_ptr->pGLGetActiveUniformsiv(program_ptr->id,
                                                        1,
                                                        (const GLuint*) &n_active_uniform,
                                                        GL_UNIFORM_MATRIX_STRIDE,
                                                       &new_uniform->ub_matrix_stride);
                    program_ptr->pGLGetActiveUniformsiv(program_ptr->id,
                                                        1,
                                                        (const GLuint*) &n_active_uniform,
                                                        GL_UNIFORM_OFFSET,
                                                       &new_uniform->ub_offset);

                    new_uniform->name     = system_hashed_ansi_string_create  (uniform_name);
                    new_uniform->location = program_ptr->pGLGetUniformLocation(program_ptr->id,
                                                                               uniform_name);
                }

                system_resizable_vector_push(program_ptr->active_uniforms,
                                             new_uniform);
            }

            /* Continue with uniform blocks. */
            ogl_context context_map_key = (program_ptr->syncable_ubs_mode == OGL_PROGRAM_SYNCABLE_UBS_MODE_ENABLE_GLOBAL) ? 0
                                                                                                                          : context;

            _ogl_program_init_uniform_blocks_for_context(program_ptr,
                                                         context_map_key,
                                                         context);
        } /* if (attribute_name != NULL && uniform_name != NULL) */

        /* Free the buffers. */
        if (attribute_name != NULL)
        {
            delete [] attribute_name;
            attribute_name = NULL;
        }

        if (uniform_name != NULL)
        {
            delete [] uniform_name;
            uniform_name = NULL;
        }
    } /* if (program_ptr->link_status) */

    /* Retrieve program info log */
    GLsizei program_info_log_length = 0;

    program_ptr->pGLGetProgramiv(program_ptr->id,
                                 GL_INFO_LOG_LENGTH,
                                &program_info_log_length);

    if (program_info_log_length != 0)
    {
        GLchar* program_info_log = new (std::nothrow) char[program_info_log_length];

        ASSERT_ALWAYS_SYNC(program_info_log != NULL,
                           "Out of memory while allocating buffer for program info log");
        if (program_info_log != NULL)
        {
            program_ptr->pGLGetProgramInfoLog(program_ptr->id,
                                              program_info_log_length,
                                              NULL,
                                              program_info_log);

            program_ptr->program_info_log = system_hashed_ansi_string_create(program_info_log);

            LOG_INFO("Program [%d] info log:\n>>\n%s\n<<",
                     program_ptr->id,
                     program_info_log);

            delete [] program_info_log;
            program_info_log = NULL;
        }
    }

    system_timeline_time end_time            = system_time_now();
    uint32_t             execution_time_msec = 0;

    system_time_get_msec_for_timeline_time(end_time - start_time,
                                          &execution_time_msec);

    LOG_INFO("Linking time: %d ms", execution_time_msec);
}

/** TODO */
PRIVATE bool _ogl_program_load_binary_blob(__in __notnull  ogl_context  context,
                                           __in __notnull _ogl_program* program_ptr)
{
    /* Form the name */
    char* file_name = _ogl_program_get_binary_blob_file_name(program_ptr);
    bool  result    = false;

    if (file_name != NULL)
    {
        /* Open the file for reading */
        FILE* blob_file_handle = ::fopen(file_name,
                                         "rb");

        if (blob_file_handle != NULL)
        {
            /* Read how many shaders are attached to the program */
            uint32_t n_shaders_attached_read = 0;
            uint32_t n_shaders_attached      = system_resizable_vector_get_amount_of_elements(program_ptr->attached_shaders);

            ASSERT_ALWAYS_SYNC(::fread(&n_shaders_attached_read,
                                       sizeof(n_shaders_attached),
                                       1,
                                       blob_file_handle) == 1,
                               "Could not read header");

            if (n_shaders_attached == n_shaders_attached_read)
            {
                /* Okay, the number complies. Go on, read the hashes and compare them to hashes for attached shaders' bodies */
                bool hashes_ok = true;

                for (uint32_t n = 0;
                              n < n_shaders_attached;
                            ++n)
                {
                    ogl_shader    shader           = NULL;
                    system_hash64 shader_hash_read = 0;

                    ASSERT_ALWAYS_SYNC(::fread(&shader_hash_read,
                                               sizeof(shader_hash_read),
                                               1,
                                               blob_file_handle) == 1,
                                       "Could not read %dth shader hash.",
                                       n);

                    if (system_resizable_vector_get_element_at(program_ptr->attached_shaders,
                                                               n,
                                                              &shader) )
                    {
                        if (system_hashed_ansi_string_get_hash(ogl_shader_get_body(shader) ) != shader_hash_read)
                        {
                            LOG_INFO("Blob file for program [%s] was found but shader [%s]'s hash does not match the blob's one",
                                     system_hashed_ansi_string_get_buffer(program_ptr->name),
                                     system_hashed_ansi_string_get_buffer(ogl_shader_get_name(shader)) );

                            hashes_ok = false;
                            break;
                        }
                    }
                    else
                    {
                        LOG_ERROR("Could not retrieve %dth element from attached_shaders", n);

                        hashes_ok = false;
                        break;
                    }
                }

                if (hashes_ok)
                {
                    /* Hashes are fine! Go on and load the program binary */
                    GLenum program_binary_format = 0;
                    GLint  program_binary_length = 0;

                    ASSERT_ALWAYS_SYNC(::fread(&program_binary_format,
                                               sizeof(program_binary_format),
                                               1,
                                               blob_file_handle) == 1,
                                       "Could not read program binary format");
                    ASSERT_ALWAYS_SYNC(::fread(&program_binary_length,
                                               sizeof(program_binary_length),
                                               1,
                                               blob_file_handle) == 1,
                                       "Could not read program binary length");

                    /* Allocate space for program binary */
                    char* program_binary_ptr = new (std::nothrow) char[program_binary_length];

                    if (program_binary_ptr != NULL)
                    {
                        /* Read the program binary */
                        ASSERT_ALWAYS_SYNC(::fread(program_binary_ptr,
                                                   program_binary_length,
                                                   1,
                                                   blob_file_handle) == 1,
                                           "Could not read program binary");

                        /* We're in a rendering thread, so go on - use the program binary */
                        program_ptr->pGLProgramBinary(program_ptr->id,
                                                      program_binary_format,
                                                      program_binary_ptr,
                                                      program_binary_length);

                        /* Program binary is no longer needed */
                        delete [] program_binary_ptr;

                        program_binary_ptr = NULL;
                    }
                    else
                    {
                        LOG_ERROR("Could not allocate %d bytes for program binary",
                                  program_binary_length);
                    }
                }
            }
            else
            {
                LOG_INFO("Blob file for program [%s] was found but header does not match the program instance's.",
                         system_hashed_ansi_string_get_buffer(program_ptr->name) );
            }

            /* Done interacting with the file */
            ::fclose(blob_file_handle);
        }
        else
        {
            LOG_INFO("Blob file for program [%s] was not found.",
                     system_hashed_ansi_string_get_buffer(program_ptr->name) );
        }

        delete [] file_name;

        file_name = NULL;
    }

    /* Done */
    return result;
}

/** TODO */
PRIVATE void _ogl_program_release(__in __notnull void* program)
{
    _ogl_program* program_ptr = (_ogl_program*) program;

    /* Unregister the PO from the registry */
    ogl_programs programs = NULL;

    ogl_context_get_property       (program_ptr->context,
                                    OGL_CONTEXT_PROPERTY_PROGRAMS,
                                   &programs);
    ogl_programs_unregister_program(programs,
                                    (ogl_program) program);

    /* Do releasing stuff in GL context first */
    ogl_context_request_callback_from_context_thread(program_ptr->context,
                                                     _ogl_program_release_callback,
                                                     program_ptr);

    /* Release all attached shaders */
    if (program_ptr->attached_shaders != NULL)
    {
        while (system_resizable_vector_get_amount_of_elements(program_ptr->attached_shaders) > 0)
        {
            ogl_shader shader     = NULL;
            bool       result_get = system_resizable_vector_pop(program_ptr->attached_shaders,
                                                               &shader);

            ASSERT_DEBUG_SYNC(result_get, "Could not retrieve shader instance.");

            ogl_shader_release(shader);
        }

        system_resizable_vector_release(program_ptr->attached_shaders);
        program_ptr->attached_shaders = NULL;
    }

    /* Release resizable vectors */
    ogl_program_attribute_descriptor* attribute_ptr = NULL;
    ogl_program_uniform_descriptor*   uniform_ptr   = NULL;
    ogl_program_ub                    uniform_block = NULL;

    if (program_ptr->active_attributes != NULL)
    {
        while (system_resizable_vector_pop(program_ptr->active_attributes,
                                          &attribute_ptr) )
        {
            delete attribute_ptr;

            attribute_ptr = NULL;
        }

        system_resizable_vector_release(program_ptr->active_attributes);
        program_ptr->active_attributes = NULL;
    }

    if (program_ptr->active_uniforms != NULL)
    {
        while (system_resizable_vector_pop(program_ptr->active_uniforms,
                                          &uniform_ptr) )
        {
            delete uniform_ptr;

            uniform_ptr = NULL;
        }

        system_resizable_vector_release(program_ptr->active_uniforms);
        program_ptr->active_uniforms = NULL;
    }

    if (program_ptr->context_to_active_ubs_map != NULL)
    {
        const unsigned int n_contexts = system_hash64map_get_amount_of_elements(program_ptr->context_to_active_ubs_map);

        for (unsigned int n_context = 0;
                          n_context < n_contexts;
                        ++n_context)
        {
            system_resizable_vector active_ubs = NULL;

            system_hash64map_get_element_at(program_ptr->context_to_active_ubs_map,
                                            n_context,
                                           &active_ubs,
                                            NULL); /* pOutHash */

            ASSERT_DEBUG_SYNC(active_ubs != NULL,
                              "Sanity check failed");

            while (system_resizable_vector_pop(active_ubs,
                                              &uniform_block) )
            {
                ogl_program_ub_release(uniform_block);

                uniform_block = NULL;
            }

            system_resizable_vector_release(active_ubs);
            active_ubs = NULL;
        } /* for (all recognized contexts) */

        system_hash64map_release(program_ptr->context_to_active_ubs_map);
        program_ptr->context_to_active_ubs_map = NULL;
    } /* if (program_ptr->context_to_active_ubs_map != NULL) */

    /* Release TF varying name array, if one was allocated */
    if (program_ptr->tf_varyings != NULL)
    {
        for (unsigned int n_varying = 0;
                          n_varying < program_ptr->n_tf_varyings;
                        ++n_varying)
        {
            delete program_ptr->tf_varyings[n_varying];

            program_ptr->tf_varyings[n_varying] = NULL;
        }

        delete [] program_ptr->tf_varyings;

        program_ptr->n_tf_varyings = 0;
        program_ptr->tf_varyings   = NULL;
    }

    /* Release the context */
    ogl_context_release(program_ptr->context);
}

/** TODO */
PRIVATE void _ogl_program_release_active_attributes(system_resizable_vector active_attributes)
{
    while (system_resizable_vector_get_amount_of_elements(active_attributes) > 0)
    {
        ogl_program_attribute_descriptor* program_attribute_ptr = NULL;
        bool                              result_get            = system_resizable_vector_pop(active_attributes,
                                                                                             &program_attribute_ptr);

        ASSERT_DEBUG_SYNC(result_get,
                          "Could not retrieve program attribute pointer.");

        delete program_attribute_ptr;
    }
}

/** TODO */
PRIVATE void _ogl_program_release_callback(__in __notnull ogl_context context,
                                                          void*       in_arg)
{
    _ogl_program* program_ptr = (_ogl_program*) in_arg;

    program_ptr->pGLDeleteProgram(program_ptr->id);

    program_ptr->id = 0;
}

/** TODO */
PRIVATE void _ogl_program_save_binary_blob(__in __notnull ogl_context   context_ptr,
                                           __in __notnull _ogl_program* program_ptr)
{
    /* Program file names tend to be length and often exceed Windows ANSI API limits.
     * Therefore, we use HAS' hash as actual blob file name, and store the actual
     * program file name at the beginning of the source code file. (implemented elsewhere)
     */
    char* file_name_src = _ogl_program_get_binary_blob_file_name(program_ptr);

    if (file_name_src != NULL)
    {
        /* Open the file for writing */
        system_file_serializer blob_file_serializer = system_file_serializer_create_for_writing(system_hashed_ansi_string_create(file_name_src) );

        ASSERT_DEBUG_SYNC(blob_file_serializer != NULL,
                          "Could not open file [%s] for writing",
                          file_name_src);

        if (blob_file_serializer != NULL)
        {
            /* Retrieve the blob */
            GLint blob_length = 0;

            program_ptr->pGLGetProgramiv(program_ptr->id,
                                         GL_PROGRAM_BINARY_LENGTH,
                                        &blob_length);

            ASSERT_DEBUG_SYNC(blob_length != 0,
                              "Program blob length is 0");

            if (blob_length != 0)
            {
                char*  blob        = new (std::nothrow) char[blob_length];
                GLenum blob_format = 0;

                ASSERT_DEBUG_SYNC(blob != NULL,
                                  "Could not allocate %d bytes for program blob",
                                  blob_length);

                if (blob != NULL)
                {
                    program_ptr->pGLGetProgramBinary(program_ptr->id,
                                                     blob_length,
                                                     NULL,
                                                    &blob_format,
                                                    blob);

                    /* We have the blob now. Proceed and write to the file */
                    unsigned int n_attached_shaders = system_resizable_vector_get_amount_of_elements(program_ptr->attached_shaders);

                    system_file_serializer_write(blob_file_serializer,
                                                 sizeof(n_attached_shaders),
                                                &n_attached_shaders);

                    for (unsigned int n_shader = 0;
                                      n_shader < n_attached_shaders;
                                    ++n_shader)
                    {
                        ogl_shader    shader      = NULL;
                        system_hash64 shader_hash = 0;

                        system_resizable_vector_get_element_at(program_ptr->attached_shaders,
                                                               n_shader,
                                                              &shader);

                        shader_hash = system_hashed_ansi_string_get_hash(ogl_shader_get_body(shader) );

                        system_file_serializer_write(blob_file_serializer,
                                                     sizeof(shader_hash),
                                                    &shader_hash);
                    }

                    system_file_serializer_write(blob_file_serializer,
                                                 sizeof(blob_format),
                                                &blob_format);
                    system_file_serializer_write(blob_file_serializer,
                                                 sizeof(blob_length),
                                                &blob_length);
                    system_file_serializer_write(blob_file_serializer,
                                                 blob_length,
                                                 blob);

                    /* Cool to release the blob now */
                    delete [] blob;
                    blob = NULL;
                }
            }

            /* Release the seiralizer */
            system_file_serializer_release(blob_file_serializer);

            blob_file_serializer = NULL;
        } /* if (blob_file_serializer != NULL) */

        /* Safe to release file name buffer now */
        delete [] file_name_src;
        file_name_src = NULL;
    } /* if (file_name_src != NULL) */
}

/** TODO */
PRIVATE void _ogl_program_save_shader_sources(__in __notnull _ogl_program* program_ptr)
{
    char*                  file_name  = _ogl_program_get_source_code_file_name        (program_ptr);
    const uint32_t         n_shaders  = system_resizable_vector_get_amount_of_elements(program_ptr->attached_shaders);
    system_file_serializer serializer = system_file_serializer_create_for_writing     (system_hashed_ansi_string_create(file_name) );

    /* Store full program name */
    static const char* full_program_name = "Full program name: ";
    static const char* newline           = "\n";

    system_file_serializer_write(serializer,
                                 strlen(full_program_name),
                                 full_program_name);
    system_file_serializer_write(serializer,
                                 system_hashed_ansi_string_get_length(program_ptr->name),
                                 system_hashed_ansi_string_get_buffer(program_ptr->name) );
    system_file_serializer_write(serializer,
                                 1, /* size */
                                 newline);
    system_file_serializer_write(serializer,
                                 1, /* size */
                                 newline);

    /* Store shader info */
    for (uint32_t n_shader = 0;
                  n_shader < n_shaders;
                ++n_shader)
    {
        ogl_shader                current_shader = NULL;
        system_hashed_ansi_string shader_body    = NULL;
        ogl_shader_type           shader_type    = SHADER_TYPE_UNKNOWN;

        if (!system_resizable_vector_get_element_at(program_ptr->attached_shaders,
                                                    n_shader,
                                                   &current_shader) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve shader at index [%d]",
                              n_shader);

            continue;
        }

        shader_body = ogl_shader_get_body(current_shader);
        shader_type = ogl_shader_get_type(current_shader);

        /* Store information about the shader type */
        static const char* marker_pre        = "--- ";
        static const char* marker_post       = " ---\n";
        static const char* shader_type_string = NULL;

        switch (shader_type)
        {
            case SHADER_TYPE_COMPUTE:
            {
                shader_type_string = "Compute Shader";

                break;
            }

            case SHADER_TYPE_FRAGMENT:
            {
                shader_type_string = "Fragment Shader";

                break;
            }

            case SHADER_TYPE_GEOMETRY:
            {
                shader_type_string = "Geometry Shader";

                break;
            }

            case SHADER_TYPE_VERTEX:
            {
                shader_type_string = "Vertex Shader";

                break;
            }

            default:
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Unrecognized shader type");
            }
        } /* switch (shader_type) */

        /* Store the body */
        system_file_serializer_write(serializer,
                                     strlen(marker_pre),
                                     marker_pre);
        system_file_serializer_write(serializer,
                                     strlen(shader_type_string),
                                     shader_type_string);
        system_file_serializer_write(serializer,
                                     strlen(marker_post),
                                     marker_post);

        system_file_serializer_write(serializer,
                                     1, /* size */
                                     newline);
        system_file_serializer_write(serializer,
                                     1, /* size */
                                     newline);

        system_file_serializer_write(serializer,
                                     system_hashed_ansi_string_get_length(shader_body),
                                     system_hashed_ansi_string_get_buffer(shader_body) );

        system_file_serializer_write(serializer,
                                     1, /* size */
                                     newline);
    } /* for (all attached shaders) */

    if (file_name != NULL)
    {
        delete [] file_name;

        file_name = NULL;
    }

    system_file_serializer_release(serializer);
    serializer = NULL;
}


/** Please see header for specification */
PUBLIC EMERALD_API bool ogl_program_attach_shader(__in __notnull ogl_program program,
                                                  __in __notnull ogl_shader  shader)
{
    _ogl_program* program_ptr = (_ogl_program*) program;
    bool          result      = false;

    ASSERT_DEBUG_SYNC(program != NULL, "Program is NULL - crash ahead.");
    ASSERT_DEBUG_SYNC(shader  != NULL, "Shader is NULL.");

    if (program_ptr->id != 0)
    {
        _ogl_attach_shader_callback_argument callback_argument;

        callback_argument.program_ptr = program_ptr;
        callback_argument.shader      = shader;

        ogl_context_request_callback_from_context_thread(program_ptr->context,
                                                         _ogl_program_attach_shader_callback,
                                                        &callback_argument);

        result = (system_resizable_vector_find(program_ptr->attached_shaders,
                                               shader) != ITEM_NOT_FOUND);
    }
    else
    {
        LOG_ERROR("Program has zero id! Cannot attach shader.");
    }

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API ogl_program ogl_program_create(__in __notnull ogl_context                   context,
                                                  __in __notnull system_hashed_ansi_string     name,
                                                  __in           ogl_program_syncable_ubs_mode syncable_ubs_mode)
{
    _ogl_program* result = new (std::nothrow) _ogl_program;

    if (result != NULL)
    {
        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(result,
                                                       _ogl_program_release,
                                                       OBJECT_TYPE_OGL_PROGRAM,
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\OpenGL Programs\\",
                                                                                                               system_hashed_ansi_string_get_buffer(name)) );

        result->active_attributes             = NULL;
        result->active_uniforms               = NULL;
        result->attached_shaders              = NULL;
        result->context                       = context;
        result->context_to_active_ubs_map     = system_hash64map_create(sizeof(system_hash64map),
                                                                        true); /* should_be_thread_safe */
        result->context_to_ub_index_to_ub_map = system_hash64map_create(sizeof(system_hash64map),
                                                                        true); /* should_be_thread_safe */
        result->context_to_ub_name_to_ub_map  = system_hash64map_create(sizeof(system_hash64map),
                                                                        true); /* should_be_thread_safe */
        result->id                            = 0;
        result->link_status                   = false;
        result->name                          = name;
        result->n_tf_varyings                 = 0;
        result->syncable_ubs_mode             = syncable_ubs_mode;
        result->tf_mode                       = GL_NONE;
        result->tf_varyings                   = NULL;

        /* Init GL entry-point cache */
        ogl_context_type context_type = OGL_CONTEXT_TYPE_UNDEFINED;

        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_TYPE,
                                &context_type);

        if (context_type == OGL_CONTEXT_TYPE_ES)
        {
            const ogl_context_es_entrypoints* entry_points = NULL;

            ogl_context_get_property(context,
                                     OGL_CONTEXT_PROPERTY_ENTRYPOINTS_ES,
                                    &entry_points);

            result->pGLAttachShader              = entry_points->pGLAttachShader;
            result->pGLCreateProgram             = entry_points->pGLCreateProgram;
            result->pGLDeleteProgram             = entry_points->pGLDeleteProgram;
            result->pGLDetachShader              = entry_points->pGLDetachShader;
            result->pGLGetActiveAttrib           = entry_points->pGLGetActiveAttrib;
            result->pGLGetActiveUniform          = entry_points->pGLGetActiveUniform;
            result->pGLGetActiveUniformBlockName = entry_points->pGLGetActiveUniformBlockName;
            result->pGLGetActiveUniformsiv       = entry_points->pGLGetActiveUniformsiv;
            result->pGLGetAttribLocation         = entry_points->pGLGetAttribLocation;
            result->pGLGetError                  = entry_points->pGLGetError;
            result->pGLGetProgramBinary          = entry_points->pGLGetProgramBinary;
            result->pGLGetProgramInfoLog         = entry_points->pGLGetProgramInfoLog;
            result->pGLGetProgramiv              = entry_points->pGLGetProgramiv;
            result->pGLGetUniformLocation        = entry_points->pGLGetUniformLocation;
            result->pGLLinkProgram               = entry_points->pGLLinkProgram;
            result->pGLProgramBinary             = entry_points->pGLProgramBinary;
            result->pGLProgramParameteri         = entry_points->pGLProgramParameteri;
            result->pGLTransformFeedbackVaryings = entry_points->pGLTransformFeedbackVaryings;
            result->pGLUniformBlockBinding       = entry_points->pGLUniformBlockBinding;
        }
        else
        {
            ASSERT_DEBUG_SYNC(context_type == OGL_CONTEXT_TYPE_GL,
                              "Unrecognized context type");

            const ogl_context_gl_entrypoints* entry_points = NULL;

            ogl_context_get_property(context,
                                     OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                                    &entry_points);

            result->pGLAttachShader              = entry_points->pGLAttachShader;
            result->pGLCreateProgram             = entry_points->pGLCreateProgram;
            result->pGLDeleteProgram             = entry_points->pGLDeleteProgram;
            result->pGLDetachShader              = entry_points->pGLDetachShader;
            result->pGLGetActiveAttrib           = entry_points->pGLGetActiveAttrib;
            result->pGLGetActiveUniform          = entry_points->pGLGetActiveUniform;
            result->pGLGetActiveUniformBlockName = entry_points->pGLGetActiveUniformBlockName;
            result->pGLGetActiveUniformsiv       = entry_points->pGLGetActiveUniformsiv;
            result->pGLGetAttribLocation         = entry_points->pGLGetAttribLocation;
            result->pGLGetError                  = entry_points->pGLGetError;
            result->pGLGetProgramBinary          = entry_points->pGLGetProgramBinary;
            result->pGLGetProgramInfoLog         = entry_points->pGLGetProgramInfoLog;
            result->pGLGetProgramiv              = entry_points->pGLGetProgramiv;
            result->pGLGetUniformLocation        = entry_points->pGLGetUniformLocation;
            result->pGLLinkProgram               = entry_points->pGLLinkProgram;
            result->pGLProgramBinary             = entry_points->pGLProgramBinary;
            result->pGLProgramParameteri         = entry_points->pGLProgramParameteri;
            result->pGLTransformFeedbackVaryings = entry_points->pGLTransformFeedbackVaryings;
            result->pGLUniformBlockBinding       = entry_points->pGLUniformBlockBinding;
        }

        /* Carry on */
        ogl_context_retain                              (context);
        ogl_context_request_callback_from_context_thread(context,
                                                         _ogl_program_create_callback,
                                                         result);

        /* Register the program object. This will give us a nifty assertion failure
         * if the caller attempts to register a PO under an already taken name.
         */
        ogl_programs context_programs = NULL;

        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_PROGRAMS,
                                &context_programs);

        ogl_programs_register_program(context_programs,
                                      (ogl_program) result);
    }

    return (ogl_program) result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool ogl_program_detach_shader(__in __notnull ogl_program program,
                                                  __in __notnull ogl_shader  shader)
{
    _ogl_program* program_ptr = (_ogl_program*) program;
    bool          result      = false;

    ASSERT_DEBUG_SYNC(program != NULL, "Program is NULL - crash ahead.");
    ASSERT_DEBUG_SYNC(shader  != NULL, "Shader is NULL.");

    if (program_ptr->id != 0)
    {
        /* Make sure the shader is attached to the program */
        if (system_resizable_vector_find(program_ptr->attached_shaders,
                                         shader) != ITEM_NOT_FOUND)
        {
            _ogl_detach_shader_callback_argument callback_argument;

            callback_argument.program_ptr = program_ptr;
            callback_argument.shader      = shader;

            ogl_context_request_callback_from_context_thread(program_ptr->context,
                                                             _ogl_program_detach_shader_callback,
                                                            &callback_argument);

            result = (system_resizable_vector_find(program_ptr->attached_shaders, shader) == ITEM_NOT_FOUND);
        }
        else
        {
            LOG_ERROR("Could not detach shader [%d] from program [%d] - not attached.",
                      ogl_shader_get_id(shader),
                      program_ptr->id);
        }
    }
    else
    {
        LOG_ERROR("Program has zero id! Cannot detach shader.");
    }

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool ogl_program_get_attached_shader(__in  __notnull ogl_program program,
                                                        __in            uint32_t    n_attached_shader,
                                                        __out __notnull ogl_shader* out_shader)
{
    _ogl_program* program_ptr = (_ogl_program*) program;

    return system_resizable_vector_get_element_at(program_ptr->attached_shaders,
                                                  n_attached_shader,
                                                 *out_shader);
}

/** Please see header for specification */
PUBLIC EMERALD_API bool ogl_program_get_attribute_by_index(__in  __notnull ogl_program                              program,
                                                           __in            size_t                                   n_attribute,
                                                           __out __notnull const ogl_program_attribute_descriptor** out_attribute)
{
    _ogl_program* program_ptr = (_ogl_program*) program;
    bool          result      = false;

    ASSERT_DEBUG_SYNC(program_ptr->link_status,
                      "You cannot retrieve an attribute descriptor without linking the program beforehand.");

    if (program_ptr->link_status)
    {
        result = system_resizable_vector_get_element_at(program_ptr->active_attributes,
                                                        n_attribute,
                                                        (void*) *out_attribute);
    }

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool ogl_program_get_attribute_by_name(__in  __notnull ogl_program                              program,
                                                          __in  __notnull system_hashed_ansi_string                name,
                                                          __out __notnull const ogl_program_attribute_descriptor** out_attribute)
{
    _ogl_program* program_ptr = (_ogl_program*) program;
    bool          result      = false;

    ASSERT_DEBUG_SYNC(program_ptr->link_status,
                      "You cannot retrieve an attribute descriptor without linking the program beforehand.");

    if (program_ptr->link_status)
    {
        unsigned int n_attributes = system_resizable_vector_get_amount_of_elements(program_ptr->active_attributes);

        for (unsigned int n_attribute = 0;
                          n_attribute < n_attributes;
                        ++n_attribute)
        {
            ogl_program_attribute_descriptor* descriptor = NULL;

            result = system_resizable_vector_get_element_at(program_ptr->active_attributes,
                                                            n_attribute,
                                                           &descriptor);

            if (result                                                             &&
                system_hashed_ansi_string_is_equal_to_hash_string(descriptor->name,
                                                                  name) )
            {
                *out_attribute = descriptor;
                result         = true;

                break;
            }
            else
            {
                result = false;
            }
        }
    }

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API GLuint ogl_program_get_id(__in __notnull ogl_program program)
{
    return ((_ogl_program*)program)->id;
}

/** Please see header for specification */
PUBLIC EMERALD_API system_hashed_ansi_string ogl_program_get_name(__in __notnull ogl_program program)
{
    return ((_ogl_program*) program)->name;
}

/** Please see header for specification */
PUBLIC EMERALD_API system_hashed_ansi_string ogl_program_get_program_info_log(__in __notnull ogl_program program)
{
    _ogl_program*             program_ptr = (_ogl_program*) program;
    system_hashed_ansi_string result      = system_hashed_ansi_string_get_default_empty_string();

    ASSERT_DEBUG_SYNC(program_ptr->link_status,
                      "You cannot retrieve program info log without linking the program beforehand.");

    if (program_ptr->link_status)
    {
        result = program_ptr->program_info_log;
    }

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool ogl_program_get_uniform_by_index(__in  __notnull ogl_program                            program,
                                                         __in            size_t                                 n_uniform,
                                                         __out __notnull const ogl_program_uniform_descriptor** out_uniform)
{
    _ogl_program* program_ptr = (_ogl_program*) program;
    bool          result      = false;

    ASSERT_DEBUG_SYNC(program_ptr->link_status,
                      "You cannot retrieve an uniform descriptor without linking the program beforehand.");

    if (program_ptr->link_status)
    {
        ogl_program_uniform_descriptor* descriptor = NULL;

        result = system_resizable_vector_get_element_at(program_ptr->active_uniforms,
                                                        n_uniform,
                                                        &descriptor);

        if (result)
        {
            *out_uniform = descriptor;
             result      = true;
        }
    } /* if (program_ptr->link_status) */

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool ogl_program_get_uniform_by_name(__in  __notnull ogl_program                            program,
                                                        __in  __notnull system_hashed_ansi_string              name,
                                                        __out __notnull const ogl_program_uniform_descriptor** out_uniform)
{
    _ogl_program* program_ptr = (_ogl_program*) program;
    bool          result      = false;

    ASSERT_DEBUG_SYNC(program_ptr->link_status,
                      "You cannot retrieve an uniform descriptor without linking the program beforehand.");

    if (program_ptr->link_status)
    {
        unsigned int n_uniforms = system_resizable_vector_get_amount_of_elements(program_ptr->active_uniforms);

        for (unsigned int n_uniform = 0;
                          n_uniform < n_uniforms;
                        ++n_uniform)
        {
            ogl_program_uniform_descriptor* descriptor = NULL;

            result = system_resizable_vector_get_element_at(program_ptr->active_uniforms,
                                                            n_uniform,
                                                            &descriptor);

            if (result                                                              &&
                system_hashed_ansi_string_is_equal_to_hash_string(descriptor->name,
                                                                  name) )
            {
                *out_uniform = descriptor;
                result       = true;

                break;
            }
            else
            {
                result = false;
            }
        }
    }

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool ogl_program_get_uniform_block_by_index(__in  __notnull ogl_program     program,
                                                               __in            unsigned int    index,
                                                               __out __notnull ogl_program_ub* out_ub_ptr)
{
    ogl_context      current_context    = NULL;
    _ogl_program*    program_ptr        = (_ogl_program*) program;
    bool             result             = false;
    system_hash64map ub_index_to_ub_map = NULL;

    if (program_ptr->syncable_ubs_mode == OGL_PROGRAM_SYNCABLE_UBS_MODE_ENABLE_PER_CONTEXT)
    {
        current_context = ogl_context_get_current_context();
    }

    if (!system_hash64map_contains(program_ptr->context_to_ub_name_to_ub_map,
                                   (system_hash64) current_context) )
    {
        _ogl_program_init_uniform_blocks_for_context(program_ptr,
                                                     current_context,
                                                     ogl_context_get_current_context() );
    }

    if (system_hash64map_get(program_ptr->context_to_ub_index_to_ub_map,
                             (system_hash64) current_context,
                            &ub_index_to_ub_map) )
    {
        result = system_hash64map_get(ub_index_to_ub_map,
                                      index,
                                      out_ub_ptr);
    }

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool ogl_program_get_uniform_block_by_name(__in  __notnull ogl_program               program,
                                                              __in  __notnull system_hashed_ansi_string name,
                                                              __out __notnull ogl_program_ub*           out_ub_ptr)
{
    ogl_context      current_context   = NULL;
    _ogl_program*    program_ptr       = (_ogl_program*) program;
    bool             result            = false;
    system_hash64map ub_name_to_ub_map = NULL;

    if (program_ptr->syncable_ubs_mode == OGL_PROGRAM_SYNCABLE_UBS_MODE_ENABLE_PER_CONTEXT)
    {
        current_context = ogl_context_get_current_context();
    }

    if (!system_hash64map_contains(program_ptr->context_to_ub_name_to_ub_map,
                                   (system_hash64) current_context) )
    {
        _ogl_program_init_uniform_blocks_for_context(program_ptr,
                                                     current_context,
                                                     ogl_context_get_current_context() );
    }

    if (system_hash64map_get(program_ptr->context_to_ub_name_to_ub_map,
                             (system_hash64) current_context,
                            &ub_name_to_ub_map) )
    {
        result = system_hash64map_get(ub_name_to_ub_map,
                                      system_hashed_ansi_string_get_hash(name),
                                      out_ub_ptr);
    }

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool ogl_program_link(__in __notnull ogl_program program)
{
    _ogl_program* program_ptr = (_ogl_program*) program;
    bool          result      = false;

    ASSERT_DEBUG_SYNC(system_resizable_vector_get_amount_of_elements(program_ptr->attached_shaders) > 0,
                      "Linking will fail - no shaders attached.");

    if (system_resizable_vector_get_amount_of_elements(program_ptr->attached_shaders) > 0)
    {
        /* Clean up */
        _ogl_program_release_active_attributes(program_ptr->active_attributes);

        /* Let's go. */
        ogl_context_request_callback_from_context_thread(program_ptr->context,
                                                         _ogl_program_link_callback,
                                                         program);

        result = program_ptr->link_status;
    }

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API void ogl_program_set_tf_varyings(__in __notnull ogl_program    program,
                                                    __in           unsigned int   n_varyings,
                                                    __in __notnull const GLchar** varying_names,
                                                    __in           GLenum         tf_mode)
{
    _ogl_program* program_ptr = (_ogl_program*) program;

    ASSERT_DEBUG_SYNC(n_varyings != 0,
                      "Invalid n_varyings value");
    ASSERT_DEBUG_SYNC(tf_mode == GL_INTERLEAVED_ATTRIBS ||
                      tf_mode == GL_SEPARATE_ATTRIBS,
                      "Invalid tf_mode value");

    if (program_ptr->tf_varyings != NULL)
    {
        for (unsigned int n_varying = 0;
                          n_varying < program_ptr->n_tf_varyings;
                        ++n_varying)
        {
            delete [] program_ptr->tf_varyings[n_varying];

            program_ptr->tf_varyings[n_varying] = NULL;
        }

        delete [] program_ptr->tf_varyings;

        program_ptr->n_tf_varyings = 0;
        program_ptr->tf_varyings   = NULL;
    }

    program_ptr->tf_mode     = tf_mode;
    program_ptr->tf_varyings = new (std::nothrow) GLchar*[n_varyings];

    ASSERT_ALWAYS_SYNC(program_ptr->tf_varyings != NULL,
                       "Out of memory");

    if (program_ptr->tf_varyings != NULL)
    {
        for (unsigned int n_varying = 0;
                          n_varying < n_varyings;
                        ++n_varying)
        {
            const GLchar*      varying_name   = varying_names[n_varying];
            const unsigned int varying_length = strlen( (const char*) varying_name) + 1;

            program_ptr->tf_varyings[n_varying] = new (std::nothrow) GLchar[varying_length];

            ASSERT_ALWAYS_SYNC(program_ptr->tf_varyings[n_varying] != NULL,
                               "Out of memory");

            memcpy(program_ptr->tf_varyings[n_varying],
                   varying_name,
                   varying_length);
        } /* for (all varyings) */

        program_ptr->n_tf_varyings = n_varyings;
    }
}
