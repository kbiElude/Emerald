/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_program.h"
#include "ogl/ogl_program_block.h"
#include "ogl/ogl_program_ssb.h"
#include "ogl/ogl_program_ub.h"
#include "ogl/ogl_programs.h"
#include "ogl/ogl_shader.h"
#include "ral/ral_context.h"
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
    ral_context                   context; /* DO NOT retain - context owns the instance! */
    GLuint                        id;
    bool                          link_status;
    system_hashed_ansi_string     name;
    system_hashed_ansi_string     program_info_log;
    ogl_program_syncable_ubs_mode syncable_ubs_mode;

    /* Stores ogl_program_ssb instances.
     *
     * NOTE: All rendering contexts in Emerald use a shared namespace. Since SSB content synchronization
     *       is not currently supported, we need not use any sort of context->ssb mapping.
     */
    system_resizable_vector active_ssbs;

    /* Maps shader storage block index to a corresponding ogl_program_ssb instance. */
    system_hash64map ssb_index_to_ssb_map;

    /* Maps shared storage block name to a corresponding ogl_program_ssb instnace. */
    system_hash64map ssb_name_to_ssb_map;

    /* Maps ogl_context instances to ogl_program_ub instances.
     *
     * If syncable_ubs_mode is set to:
     *   a) OGL_PROGRAM_SYNCABLE_UBS_MODE_DISABLE:            as _GLOBAL.
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
    PFNGLATTACHSHADERPROC               pGLAttachShader;
    PFNGLCREATEPROGRAMPROC              pGLCreateProgram;
    PFNGLDELETEPROGRAMPROC              pGLDeleteProgram;
    PFNGLDETACHSHADERPROC               pGLDetachShader;
    PFNGLGETACTIVEATTRIBPROC            pGLGetActiveAttrib;
    PFNGLGETATTRIBLOCATIONPROC          pGLGetAttribLocation;
    PFNGLGETERRORPROC                   pGLGetError;
    PFNGLGETPROGRAMBINARYPROC           pGLGetProgramBinary;
    PFNGLGETPROGRAMIVPROC               pGLGetProgramiv;
    PFNGLGETPROGRAMINFOLOGPROC          pGLGetProgramInfoLog;
    PFNGLGETPROGRAMINTERFACEIVPROC      pGLGetProgramInterfaceiv;
    PFNGLGETPROGRAMRESOURCEIVPROC       pGLGetProgramResourceiv;
    PFNGLGETPROGRAMRESOURCELOCATIONPROC pGLGetProgramResourceLocation;
    PFNGLGETPROGRAMRESOURCENAMEPROC     pGLGetProgramResourceName;
    PFNGLLINKPROGRAMPROC                pGLLinkProgram;
    PFNGLPROGRAMBINARYPROC              pGLProgramBinary;
    PFNGLPROGRAMPARAMETERIPROC          pGLProgramParameteri;
    PFNGLSHADERSTORAGEBLOCKBINDINGPROC  pGLShaderStorageBlockBinding;
    PFNGLTRANSFORMFEEDBACKVARYINGSPROC  pGLTransformFeedbackVaryings;
    PFNGLUNIFORMBLOCKBINDINGPROC        pGLUniformBlockBinding;

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
PRIVATE void _ogl_program_attach_shader_callback              (ogl_context             context,
                                                               void*                   in_arg);
PRIVATE void _ogl_program_create_callback                     (ogl_context             context,
                                                               void*                   in_arg);
PRIVATE void _ogl_program_detach_shader_callback              (ogl_context             context,
                                                               void*                   in_arg);
PRIVATE char*_ogl_program_get_binary_blob_file_name           (_ogl_program*           program_ptr);
PRIVATE char*_ogl_program_get_source_code_file_name           (_ogl_program*           program_ptr);
PRIVATE void _ogl_program_link_callback                       (ogl_context             context,
                                                               void*                   in_arg);
PRIVATE bool _ogl_program_load_binary_blob                    (ogl_context,
                                                               _ogl_program*           program_ptr);
PRIVATE void _ogl_program_release                             (void*                   program);
PRIVATE void _ogl_program_release_active_attributes           (system_resizable_vector active_attributes);
PRIVATE void _ogl_program_release_active_shader_storage_blocks(_ogl_program*           program_ptr);
PRIVATE void _ogl_program_release_active_uniform_blocks       (_ogl_program*           program_ptr,
                                                               ogl_context             owner_context = NULL);
PRIVATE void _ogl_program_release_active_uniforms             (system_resizable_vector active_uniforms);
PRIVATE void _ogl_program_release_callback                    (ogl_context             context,
                                                               void*                   in_arg);
PRIVATE void _ogl_program_save_binary_blob                    (ogl_context,
                                                               _ogl_program*           program_ptr);
PRIVATE void _ogl_program_save_shader_sources                 (_ogl_program*           program_ptr);


/** TODO */
PRIVATE void _ogl_program_attach_shader_callback(ogl_context context,
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
PRIVATE void _ogl_program_create_callback(ogl_context context,
                                          void*       in_arg)
{
    _ogl_program* program_ptr = (_ogl_program*) in_arg;

    program_ptr->id                    = program_ptr->pGLCreateProgram();
    program_ptr->active_attributes     = system_resizable_vector_create(BASE_PROGRAM_ACTIVE_ATTRIBUTES_NUMBER);
    program_ptr->active_uniforms       = system_resizable_vector_create(BASE_PROGRAM_ACTIVE_UNIFORMS_NUMBER);
    program_ptr->attached_shaders      = system_resizable_vector_create(BASE_PROGRAM_ATTACHED_SHADERS_NUMBER);

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
PRIVATE void _ogl_program_detach_shader_callback(ogl_context context,
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
PRIVATE char*_ogl_program_get_binary_blob_file_name(_ogl_program* program_ptr)
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
PRIVATE char*_ogl_program_get_source_code_file_name(_ogl_program* program_ptr)
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

/** TODO */
PRIVATE void _ogl_program_init_blocks_for_context(ogl_program_block_type block_type,
                                                  _ogl_program*          program_ptr,
                                                  ogl_context            context_map_key,
                                                  ogl_context            context)
{
    const GLenum block_interface_gl        = (block_type == OGL_PROGRAM_BLOCK_TYPE_SHADER_STORAGE_BUFFER) ? GL_SHADER_STORAGE_BLOCK
                                                                                                          : GL_UNIFORM_BLOCK;
    GLchar*      block_name                = NULL;
    GLint        n_active_blocks           = 0;
    GLint        n_active_block_max_length = 0;

    program_ptr->pGLGetProgramInterfaceiv(program_ptr->id,
                                          block_interface_gl,
                                          GL_ACTIVE_RESOURCES, /* pname */
                                          &n_active_blocks);

    /* NOTE: As of driver version 10.18.14.4170, the glGetProgramInterfaceiv() call we're making below does not seem to 
     *       raise some sort of an internal driver flag. This causes the subsequent glGetProgramResourceiv() call
     *       to throw a mysterious bug, telling about the geometry shader not having been compiled. All good & well,
     *       but this happens: a) for POs which do not have any GS attached, b) for POs whose all attached shaders
     *       have been already compiled, since we wouldn't have landed here, had the PO not linked successfully.
     *       Sigh.
     */
    bool is_intel_driver = false;

    ogl_context_get_property(ral_context_get_gl_context(program_ptr->context),
                             OGL_CONTEXT_PROPERTY_IS_INTEL_DRIVER,
                            &is_intel_driver);

    if (!is_intel_driver)
    {
        program_ptr->pGLGetProgramInterfaceiv(program_ptr->id,
                                              block_interface_gl,
                                              GL_MAX_NAME_LENGTH,
                                             &n_active_block_max_length);
    }
    else
    {
        static const GLenum piq_property_name_length = GL_NAME_LENGTH;

        for (unsigned int n_ub = 0;
                          n_ub < n_active_blocks;
                        ++n_ub)
        {
            GLint current_block_name_length = 0;

            program_ptr->pGLGetProgramResourceiv(program_ptr->id,
                                                 GL_UNIFORM_BLOCK,
                                                 n_ub,
                                                 1, /* propCount */
                                                 &piq_property_name_length,
                                                 1,    /* bufSize */
                                                 NULL, /* length */
                                                &current_block_name_length);

            if (current_block_name_length > n_active_block_max_length)
            {
                n_active_block_max_length = current_block_name_length;
            }
        } /* for (all active uniform blocks) */
    }

    block_name = new (std::nothrow) GLchar[n_active_block_max_length + 1];

    ASSERT_ALWAYS_SYNC(block_name != NULL,
                       "Could not allocate [%d] bytes for active block name",
                       n_active_block_max_length + 1);

    for (GLint n_active_block = 0;
               n_active_block < n_active_blocks;
             ++n_active_block)
    {
        system_hashed_ansi_string block_name_has = NULL;

        if (n_active_block_max_length != 0)
        {
            program_ptr->pGLGetProgramResourceName(program_ptr->id,
                                                   block_interface_gl,
                                                   n_active_block,
                                                   n_active_block_max_length + 1,
                                                   NULL, /* length */
                                                   block_name);

            block_name_has = system_hashed_ansi_string_create(block_name);
        }
        else
        {
            block_name_has = system_hashed_ansi_string_get_default_empty_string();
        }

        switch (block_type)
        {
            case OGL_PROGRAM_BLOCK_TYPE_SHADER_STORAGE_BUFFER:
            {
                ogl_program_ssb new_ssb = ogl_program_ssb_create(program_ptr->context,
                                                                 (ogl_program) program_ptr,
                                                                 n_active_block,
                                                                 block_name_has);

                ASSERT_ALWAYS_SYNC(new_ssb != NULL,
                                   "ogl_program_ssb_create() returned NULL.");

                /* Store the SSB info in the internal hash-maps */
                if (program_ptr->active_ssbs          == NULL ||
                    program_ptr->ssb_index_to_ssb_map == NULL ||
                    program_ptr->ssb_name_to_ssb_map  == NULL)
                {
                    program_ptr->active_ssbs          = system_resizable_vector_create(BASE_PROGRAM_ACTIVE_SHADER_STORAGE_BLOCKS_NUMBER);
                    program_ptr->ssb_index_to_ssb_map = system_hash64map_create       (sizeof(ogl_program_ssb) );
                    program_ptr->ssb_name_to_ssb_map  = system_hash64map_create       (sizeof(ogl_program_ssb) );

                    ASSERT_DEBUG_SYNC(program_ptr->active_ssbs          != NULL &&
                                      program_ptr->ssb_index_to_ssb_map != NULL &&
                                      program_ptr->ssb_name_to_ssb_map  != NULL,
                                      "Sanity check failed");
                }

                ASSERT_DEBUG_SYNC(system_resizable_vector_find(program_ptr->active_ssbs,
                                                               new_ssb) == ITEM_NOT_FOUND,
                                  "Sanity check failed");
                ASSERT_DEBUG_SYNC(!system_hash64map_contains(program_ptr->ssb_index_to_ssb_map,
                                                             (system_hash64) n_active_block),
                                  "Sanity check failed");
                ASSERT_DEBUG_SYNC(!system_hash64map_contains(program_ptr->ssb_name_to_ssb_map,
                                                             system_hashed_ansi_string_get_hash(block_name_has) ),
                                  "Sanity check failed");

                system_resizable_vector_push(program_ptr->active_ssbs,
                                             new_ssb);
                system_hash64map_insert     (program_ptr->ssb_index_to_ssb_map,
                                             n_active_block,
                                             new_ssb,
                                             NULL,  /* on_remove_callback */
                                             NULL); /* on_remove_callback_user_arg */
                system_hash64map_insert     (program_ptr->ssb_name_to_ssb_map,
                                             system_hashed_ansi_string_get_hash(block_name_has),
                                             new_ssb,
                                             NULL,  /* on_remove_callback */
                                             NULL); /* on_remove_callback_user_arg */

                break;
            }

            case OGL_PROGRAM_BLOCK_TYPE_UNIFORM_BUFFER:
            {
                ogl_program_ub new_ub = ogl_program_ub_create(program_ptr->context,
                                                              (ogl_program) program_ptr,
                                                              n_active_block,
                                                              block_name_has,
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
                    context_active_ubs         = system_resizable_vector_create(BASE_PROGRAM_ACTIVE_UNIFORM_BLOCKS_NUMBER);
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

                ASSERT_DEBUG_SYNC(system_resizable_vector_find(context_active_ubs,
                                                               new_ub) == ITEM_NOT_FOUND,
                                  "Sanity check failed");
                ASSERT_DEBUG_SYNC(!system_hash64map_contains(context_ub_index_to_ub_map,
                                                             (system_hash64) n_active_block),
                                  "Sanity check failed");
                ASSERT_DEBUG_SYNC(!system_hash64map_contains(context_ub_name_to_ub_map,
                                                             system_hashed_ansi_string_get_hash(block_name_has) ),
                                  "Sanity check failed");

                system_resizable_vector_push(context_active_ubs,
                                             new_ub);
                system_hash64map_insert     (context_ub_index_to_ub_map,
                                             n_active_block,
                                             new_ub,
                                             NULL,  /* on_remove_callback */
                                             NULL); /* on_remove_callback_user_arg */
                system_hash64map_insert     (context_ub_name_to_ub_map,
                                             system_hashed_ansi_string_get_hash(block_name_has),
                                             new_ub,
                                             NULL,  /* on_remove_callback */
                                             NULL); /* on_remove_callback_user_arg */

                break;
            }

            default:
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Unrecognized block type");
            }
        } /* switch (block_type) */

        /* Set up the block binding */
        switch (block_type)
        {
            case OGL_PROGRAM_BLOCK_TYPE_SHADER_STORAGE_BUFFER:
            {
                program_ptr->pGLShaderStorageBlockBinding(program_ptr->id,
                                                          n_active_block,
                                                          n_active_block);

                break;
            }

            case OGL_PROGRAM_BLOCK_TYPE_UNIFORM_BUFFER:
            {
                program_ptr->pGLUniformBlockBinding(program_ptr->id,
                                                    n_active_block,
                                                    n_active_block);

                break;
            }

            default:
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Unsupported block type.");
            }
        } /* switch (block_type) */
    } /* for (all blocks of the requested type) */

    if (block_name != NULL)
    {
        delete [] block_name;
        block_name = NULL;
    }
}

/** TODO */
PRIVATE void _ogl_program_link_callback(ogl_context context,
                                        void*       in_arg)
{
    _ogl_program* program_ptr     = (_ogl_program*) in_arg;
    bool          has_used_binary = false;

    system_time start_time = system_time_now();

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
        const unsigned int uniform_name_length = n_active_uniform_max_length + 1;

        GLchar* attribute_name = new (std::nothrow) GLchar[n_active_attribute_max_length + 1];
        GLchar* uniform_name   = new (std::nothrow) GLchar[uniform_name_length];

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

                    memset(attribute_name,
                           0,
                           new_attribute->length + 1);

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
                ogl_program_variable* new_uniform = new (std::nothrow) ogl_program_variable;

                ASSERT_ALWAYS_SYNC(new_uniform != NULL,
                                   "Out of memory while allocating new uniform descriptor.");
                if (new_uniform != NULL)
                {
                    ogl_program_fill_ogl_program_variable( (ogl_program) program_ptr,
                                                          uniform_name_length,
                                                          uniform_name,
                                                          new_uniform,
                                                          GL_UNIFORM,
                                                          n_active_uniform);
                }

                system_resizable_vector_push(program_ptr->active_uniforms,
                                             new_uniform);
            }

            /* Continue with uniform blocks. */
            ogl_context context_map_key = (program_ptr->syncable_ubs_mode == OGL_PROGRAM_SYNCABLE_UBS_MODE_ENABLE_GLOBAL) ? 0
                                                                                                                          : context;

            _ogl_program_init_blocks_for_context(OGL_PROGRAM_BLOCK_TYPE_UNIFORM_BUFFER,
                                                 program_ptr,
                                                 context_map_key,
                                                 context);

            /* Finish with shader storage blocks. */
            _ogl_program_init_blocks_for_context(OGL_PROGRAM_BLOCK_TYPE_SHADER_STORAGE_BUFFER,
                                                 program_ptr,
                                                 0, /* context_map_key */
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

            LOG_INFO("Program [%u] info log:\n>>\n%s\n<<",
                     program_ptr->id,
                     program_info_log);

            delete [] program_info_log;
            program_info_log = NULL;
        }
    }

    system_time end_time            = system_time_now();
    uint32_t    execution_time_msec = 0;

    system_time_get_msec_for_time(end_time - start_time,
                                 &execution_time_msec);

    LOG_INFO("Linking time: %u ms",
             execution_time_msec);
}

/** TODO */
PRIVATE bool _ogl_program_load_binary_blob( ogl_context  context,
                                           _ogl_program* program_ptr)
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
            uint32_t n_shaders_attached      = 0;

            system_resizable_vector_get_property(program_ptr->attached_shaders,
                                                 SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                &n_shaders_attached);

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
                        LOG_ERROR("Could not retrieve %uth element from attached_shaders", n);

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

                        /* Is the program considered linked at this point? */
                        GLint link_status = GL_FALSE;

                        program_ptr->pGLGetProgramiv(program_ptr->id,
                                                     GL_LINK_STATUS,
                                                    &link_status);

                        result = (link_status != GL_FALSE);

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
PRIVATE void _ogl_program_release(void* program)
{
    _ogl_program* program_ptr = (_ogl_program*) program;

    /* Unregister the PO from the registry */
    ogl_programs programs = NULL;

    ogl_context_get_property(ral_context_get_gl_context(program_ptr->context),
                             OGL_CONTEXT_PROPERTY_PROGRAMS,
                            &programs);

    if (programs == NULL)
    {
        /* We may end up here if the user explicitly killed the window (eg. with ALT+F4).
         *
         * In this case, the whole context will have been wiped out at this point, so there's
         * no sense in trying to release ogl_shader and ogl_program instances.
         */
        LOG_FATAL("Program manager is NULL. An ogl_program instance will not be unregistered.")
    }
    else
    {
        ogl_programs_unregister_program(programs,
                                        (ogl_program)program);
    }

    /* Do releasing stuff in GL context first */
    ogl_context_request_callback_from_context_thread(ral_context_get_gl_context(program_ptr->context),
                                                     _ogl_program_release_callback,
                                                     program_ptr);

    /* Release all attached shaders */
    if (program_ptr->attached_shaders != NULL)
    {
        while (true)
        {
            ogl_shader shader     = NULL;
            bool       result_get = system_resizable_vector_pop(program_ptr->attached_shaders,
                                                               &shader);

            if (!result_get)
            {
                break;
            }

            ogl_shader_release(shader);
        }

        system_resizable_vector_release(program_ptr->attached_shaders);
        program_ptr->attached_shaders = NULL;
    }

    /* Release resizable vectors */
    ogl_program_attribute_descriptor* attribute_ptr = NULL;
    ogl_program_variable*             uniform_ptr   = NULL;
    ogl_program_ub                    uniform_block = NULL;

    _ogl_program_release_active_attributes           (program_ptr->active_attributes);
    _ogl_program_release_active_shader_storage_blocks(program_ptr);
    _ogl_program_release_active_uniforms             (program_ptr->active_uniforms);
    _ogl_program_release_active_uniform_blocks       (program_ptr);

    if (program_ptr->active_attributes != NULL)
    {
        system_resizable_vector_release(program_ptr->active_attributes);
        program_ptr->active_attributes = NULL;
    }

    if (program_ptr->active_uniforms != NULL)
    {
        system_resizable_vector_release(program_ptr->active_uniforms);
        program_ptr->active_uniforms = NULL;
    }

    if (program_ptr->context_to_active_ubs_map != NULL)
    {
        system_hash64map_release(program_ptr->context_to_active_ubs_map);
        program_ptr->context_to_active_ubs_map = NULL;
    }

    if (program_ptr->context_to_ub_index_to_ub_map != NULL)
    {
        system_hash64map_release(program_ptr->context_to_ub_index_to_ub_map);
        program_ptr->context_to_ub_index_to_ub_map = NULL;
    }

    if (program_ptr->context_to_ub_name_to_ub_map != NULL)
    {
        system_hash64map_release(program_ptr->context_to_ub_name_to_ub_map);
        program_ptr->context_to_ub_name_to_ub_map = NULL;
    }

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
}

/** TODO */
PRIVATE void _ogl_program_release_active_attributes(system_resizable_vector active_attributes)
{
    while (true)
    {
        ogl_program_attribute_descriptor* program_attribute_ptr = NULL;
        bool                              result_get            = system_resizable_vector_pop(active_attributes,
                                                                                             &program_attribute_ptr);

        if (!result_get)
        {
            break;
        }

        delete program_attribute_ptr;
    }
}

/** TODO */
PRIVATE void _ogl_program_release_active_shader_storage_blocks(_ogl_program* program_ptr)
{
    if (program_ptr->active_ssbs != NULL)
    {
        ogl_program_ssb current_ssb = NULL;

        while (system_resizable_vector_pop(program_ptr->active_ssbs,
                                          &current_ssb))
        {
            ogl_program_ssb_release(current_ssb);

            current_ssb = NULL;
        }

        system_resizable_vector_release(program_ptr->active_ssbs);
        program_ptr->active_ssbs = NULL;
    }

    if (program_ptr->ssb_index_to_ssb_map != NULL)
    {
        system_hash64map_release(program_ptr->ssb_index_to_ssb_map);

        program_ptr->ssb_index_to_ssb_map = NULL;
    }

    if (program_ptr->ssb_name_to_ssb_map != NULL)
    {
        system_hash64map_release(program_ptr->ssb_name_to_ssb_map);

        program_ptr->ssb_name_to_ssb_map = NULL;
    }
}

/** TODO */
PRIVATE void _ogl_program_release_active_uniform_blocks(_ogl_program* program_ptr,
                                                        ogl_context   owner_context)
{
    if (program_ptr->context_to_active_ubs_map != NULL)
    {
        uint32_t n_contexts = 0;

        system_hash64map_get_property(program_ptr->context_to_active_ubs_map,
                                      SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                     &n_contexts);

        for (unsigned int n_context = 0;
                          n_context < n_contexts;
                        ++n_context)
        {
            system_resizable_vector active_ubs                 = NULL;
            system_hash64           current_owner_context_hash = 0;
            ogl_context             current_owner_context      = NULL;
            ogl_program_ub          uniform_block              = NULL;

            system_hash64map_get_element_at(program_ptr->context_to_active_ubs_map,
                                            n_context,
                                           &active_ubs,
                                           &current_owner_context_hash);

            current_owner_context = (ogl_context) current_owner_context_hash;

#if 0
            TODO: pending fix for hash map container

            ASSERT_DEBUG_SYNC(active_ubs != NULL,
                              "Sanity check failed");
#endif

            if (active_ubs != NULL)
            {
                if (owner_context == NULL ||
                    owner_context != NULL && owner_context == current_owner_context)
                {
                    while (system_resizable_vector_pop(active_ubs,
                                                      &uniform_block) )
                    {
                        ogl_program_ub_release(uniform_block);

                        uniform_block = NULL;
                    }

                    system_resizable_vector_release(active_ubs);
                    active_ubs = NULL;
                }
            }
        } /* for (all recognized contexts) */
    } /* if (program_ptr->context_to_active_ubs_map != NULL) */

    if (program_ptr->context_to_ub_index_to_ub_map != NULL)
    {
        unsigned int n_contexts = 0;

        system_hash64map_get_property(program_ptr->context_to_ub_index_to_ub_map,
                                      SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                     &n_contexts);

        for (unsigned int n_context = 0;
                          n_context < n_contexts;
                        ++n_context)
        {
            system_hash64    current_owner_context_hash = 0;
            ogl_context      current_owner_context      = NULL;
            system_hash64map current_ub_index_to_ub_map = NULL;

            if (!system_hash64map_get_element_at(program_ptr->context_to_ub_index_to_ub_map,
                                                 n_context,
                                                &current_ub_index_to_ub_map,
                                                &current_owner_context_hash) )
            {
#if 0
            TODO: pending fix for hash map container

                ASSERT_DEBUG_SYNC(false,
                                  "Sanity check failed");
#endif

                continue;
            }

            current_owner_context = (ogl_context) current_owner_context_hash;

            if (owner_context == NULL ||
                owner_context != NULL && owner_context == current_owner_context)
            {
                system_hash64map_clear(current_ub_index_to_ub_map);
                current_ub_index_to_ub_map = NULL;

                system_hash64map_remove(program_ptr->context_to_ub_index_to_ub_map,
                                        current_owner_context_hash);
            }
        } /* for (all recognized contexts) */
    } /* if (program_ptr->context_to_ub_index_to_ub_map != NULL) */

    if (program_ptr->context_to_ub_name_to_ub_map != NULL)
    {
        system_hash64 current_owner_context_hash = 0;
        ogl_context   current_owner_context      = NULL;
        unsigned int  n_contexts                 = 0;

        system_hash64map_get_property(program_ptr->context_to_ub_name_to_ub_map,
                                      SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                     &n_contexts);

        for (unsigned int n_context = 0;
                          n_context < n_contexts;
                        ++n_context)
        {
            system_hash64map current_ub_name_to_ub_map = NULL;

            if (!system_hash64map_get_element_at(program_ptr->context_to_ub_name_to_ub_map,
                                                 n_context,
                                                &current_ub_name_to_ub_map,
                                                &current_owner_context_hash) )
            {
#if 0
            TODO: pending fix for hash map container

                ASSERT_DEBUG_SYNC(false,
                                  "Sanity check failed");
#endif

                continue;
            }

            current_owner_context = (ogl_context) current_owner_context_hash;

            if (owner_context == NULL ||
                owner_context != NULL && owner_context == current_owner_context)
            {
                system_hash64map_clear(current_ub_name_to_ub_map);
                current_ub_name_to_ub_map = NULL;

                system_hash64map_remove(program_ptr->context_to_ub_name_to_ub_map,
                                        current_owner_context_hash);
            }
        } /* for (all recognized contexts) */
    } /* if (program_ptr->context_to_ub_name_to_ub_map != NULL) */

    /* Do NOT release the maps in this function, as it can also be called right before
     * linking time.
     */
}

/** TODO */
PRIVATE void _ogl_program_release_active_uniforms(system_resizable_vector active_uniforms)
{
    while (true)
    {
        ogl_program_variable* program_uniform_ptr = NULL;
        bool                  result_get          = system_resizable_vector_pop(active_uniforms,
                                                                               &program_uniform_ptr);

        if (!result_get)
        {
            break;
        }

        delete program_uniform_ptr;
        program_uniform_ptr = NULL;
    }
}

/** TODO */
PRIVATE void _ogl_program_release_callback(ogl_context context,
                                           void*       in_arg)
{
    _ogl_program* program_ptr = (_ogl_program*) in_arg;

    program_ptr->pGLDeleteProgram(program_ptr->id);

    program_ptr->id = 0;
}

/** TODO */
PRIVATE void _ogl_program_save_binary_blob(ogl_context   context_ptr,
                                           _ogl_program* program_ptr)
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
                    unsigned int n_attached_shaders = 0;

                    system_resizable_vector_get_property(program_ptr->attached_shaders,
                                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                        &n_attached_shaders);

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
PRIVATE void _ogl_program_save_shader_sources(_ogl_program* program_ptr)
{
    char*                  file_name  = _ogl_program_get_source_code_file_name   (program_ptr);
    uint32_t               n_shaders  = 0;
    system_file_serializer serializer = system_file_serializer_create_for_writing(system_hashed_ansi_string_create(file_name) );

    system_resizable_vector_get_property(program_ptr->attached_shaders,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_shaders);

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
        ral_shader_type           shader_type    = RAL_SHADER_TYPE_UNKNOWN;

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
            case RAL_SHADER_TYPE_COMPUTE:
            {
                shader_type_string = "Compute Shader";

                break;
            }

            case RAL_SHADER_TYPE_FRAGMENT:
            {
                shader_type_string = "Fragment Shader";

                break;
            }

            case RAL_SHADER_TYPE_GEOMETRY:
            {
                shader_type_string = "Geometry Shader";

                break;
            }

            case RAL_SHADER_TYPE_VERTEX:
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
PUBLIC EMERALD_API bool ogl_program_attach_shader(ogl_program program,
                                                  ogl_shader  shader)
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

        ogl_context_request_callback_from_context_thread(ral_context_get_gl_context(program_ptr->context),
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
PUBLIC EMERALD_API ogl_program ogl_program_create(ral_context                   context,
                                                  system_hashed_ansi_string     name,
                                                  ogl_program_syncable_ubs_mode syncable_ubs_mode)
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
        result->active_ssbs                   = NULL;
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
        result->ssb_index_to_ssb_map          = NULL;
        result->ssb_name_to_ssb_map           = NULL;
        result->syncable_ubs_mode             = syncable_ubs_mode;
        result->tf_mode                       = GL_NONE;
        result->tf_varyings                   = NULL;

        /* Init GL entry-point cache */
        ral_backend_type backend_type = RAL_BACKEND_TYPE_UNKNOWN;

        ral_context_get_property(context,
                                 RAL_CONTEXT_PROPERTY_BACKEND_TYPE,
                                &backend_type);

        if (backend_type == RAL_BACKEND_TYPE_ES)
        {
            const ogl_context_es_entrypoints* entry_points = NULL;

            ogl_context_get_property(ral_context_get_gl_context(context),
                                     OGL_CONTEXT_PROPERTY_ENTRYPOINTS_ES,
                                    &entry_points);

            result->pGLAttachShader               = entry_points->pGLAttachShader;
            result->pGLCreateProgram              = entry_points->pGLCreateProgram;
            result->pGLDeleteProgram              = entry_points->pGLDeleteProgram;
            result->pGLDetachShader               = entry_points->pGLDetachShader;
            result->pGLGetActiveAttrib            = entry_points->pGLGetActiveAttrib;
            result->pGLGetProgramResourceiv       = entry_points->pGLGetProgramResourceiv;
            result->pGLGetAttribLocation          = entry_points->pGLGetAttribLocation;
            result->pGLGetError                   = entry_points->pGLGetError;
            result->pGLGetProgramBinary           = entry_points->pGLGetProgramBinary;
            result->pGLGetProgramInfoLog          = entry_points->pGLGetProgramInfoLog;
            result->pGLGetProgramInterfaceiv      = entry_points->pGLGetProgramInterfaceiv;
            result->pGLGetProgramiv               = entry_points->pGLGetProgramiv;
            result->pGLGetProgramResourceiv       = entry_points->pGLGetProgramResourceiv;
            result->pGLGetProgramResourceLocation = entry_points->pGLGetProgramResourceLocation;
            result->pGLGetProgramResourceName     = entry_points->pGLGetProgramResourceName;
            result->pGLLinkProgram                = entry_points->pGLLinkProgram;
            result->pGLProgramBinary              = entry_points->pGLProgramBinary;
            result->pGLProgramParameteri          = entry_points->pGLProgramParameteri;
            result->pGLShaderStorageBlockBinding  = entry_points->pGLShaderStorageBlockBinding;
            result->pGLTransformFeedbackVaryings  = entry_points->pGLTransformFeedbackVaryings;
            result->pGLUniformBlockBinding        = entry_points->pGLUniformBlockBinding;
        }
        else
        {
            ASSERT_DEBUG_SYNC(backend_type == RAL_BACKEND_TYPE_GL,
                              "Unrecognized context type");

            const ogl_context_gl_entrypoints* entry_points = NULL;

            ogl_context_get_property(ral_context_get_gl_context(context),
                                     OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                                    &entry_points);

            result->pGLAttachShader               = entry_points->pGLAttachShader;
            result->pGLCreateProgram              = entry_points->pGLCreateProgram;
            result->pGLDeleteProgram              = entry_points->pGLDeleteProgram;
            result->pGLDetachShader               = entry_points->pGLDetachShader;
            result->pGLGetActiveAttrib            = entry_points->pGLGetActiveAttrib;
            result->pGLGetProgramResourceiv       = entry_points->pGLGetProgramResourceiv;
            result->pGLGetAttribLocation          = entry_points->pGLGetAttribLocation;
            result->pGLGetError                   = entry_points->pGLGetError;
            result->pGLGetProgramBinary           = entry_points->pGLGetProgramBinary;
            result->pGLGetProgramInfoLog          = entry_points->pGLGetProgramInfoLog;
            result->pGLGetProgramInterfaceiv      = entry_points->pGLGetProgramInterfaceiv;
            result->pGLGetProgramiv               = entry_points->pGLGetProgramiv;
            result->pGLGetProgramResourceiv       = entry_points->pGLGetProgramResourceiv;
            result->pGLGetProgramResourceLocation = entry_points->pGLGetProgramResourceLocation;
            result->pGLGetProgramResourceName     = entry_points->pGLGetProgramResourceName;
            result->pGLLinkProgram                = entry_points->pGLLinkProgram;
            result->pGLProgramBinary              = entry_points->pGLProgramBinary;
            result->pGLProgramParameteri          = entry_points->pGLProgramParameteri;
            result->pGLShaderStorageBlockBinding  = entry_points->pGLShaderStorageBlockBinding;
            result->pGLTransformFeedbackVaryings  = entry_points->pGLTransformFeedbackVaryings;
            result->pGLUniformBlockBinding        = entry_points->pGLUniformBlockBinding;
        }

        /* Carry on */
        ogl_context_request_callback_from_context_thread(ral_context_get_gl_context(context),
                                                         _ogl_program_create_callback,
                                                         result);

        /* Register the program object. This will give us a nifty assertion failure
         * if the caller attempts to register a PO under an already taken name.
         */
        ogl_programs context_programs = NULL;

        ogl_context_get_property(ral_context_get_gl_context(context),
                                 OGL_CONTEXT_PROPERTY_PROGRAMS,
                                &context_programs);

        ogl_programs_register_program(context_programs,
                                      (ogl_program) result);
    }

    return (ogl_program) result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool ogl_program_detach_shader(ogl_program program,
                                                  ogl_shader  shader)
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

            ogl_context_request_callback_from_context_thread(ral_context_get_gl_context(program_ptr->context),
                                                             _ogl_program_detach_shader_callback,
                                                            &callback_argument);

            result = (system_resizable_vector_find(program_ptr->attached_shaders, shader) == ITEM_NOT_FOUND);
        }
        else
        {
            LOG_ERROR("Could not detach shader [%u] from program [%u] - not attached.",
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

/** TODO */
PUBLIC void ogl_program_fill_ogl_program_variable(ogl_program           program,
                                                  unsigned int          temp_variable_name_storage_size,
                                                  char*                 temp_variable_name_storage,
                                                  ogl_program_variable* variable_ptr,
                                                  GLenum                variable_interface_type,
                                                  unsigned int          n_variable)
{
    bool                is_temp_variable_defined            = (temp_variable_name_storage != NULL) ? true
                                                                                                   : false;
    static const GLenum piq_property_array_size             = GL_ARRAY_SIZE;
    static const GLenum piq_property_array_stride           = GL_ARRAY_STRIDE;
    static const GLenum piq_property_block_index            = GL_BLOCK_INDEX;
    static const GLenum piq_property_is_row_major           = GL_IS_ROW_MAJOR;
    static const GLenum piq_property_matrix_stride          = GL_MATRIX_STRIDE;
    static const GLenum piq_property_name_length            = GL_NAME_LENGTH;
    static const GLenum piq_property_offset                 = GL_OFFSET;
    static const GLenum piq_property_top_level_array_size   = GL_TOP_LEVEL_ARRAY_SIZE;
    static const GLenum piq_property_top_level_array_stride = GL_TOP_LEVEL_ARRAY_STRIDE;
    static const GLenum piq_property_type                   = GL_TYPE;
    _ogl_program*       program_ptr                         = (_ogl_program*) program;

    variable_ptr->array_stride           = 0;
    variable_ptr->block_index            = -1;
    variable_ptr->block_offset           = -1;
    variable_ptr->is_row_major_matrix    = false;
    variable_ptr->length                 = 0;
    variable_ptr->location               = -1;
    variable_ptr->matrix_stride          = 0;
    variable_ptr->name                   = NULL;
    variable_ptr->size                   = 0;
    variable_ptr->top_level_array_size   = 0;
    variable_ptr->top_level_array_stride = 0;
    variable_ptr->type                   = PROGRAM_UNIFORM_TYPE_UNDEFINED;

    ASSERT_DEBUG_SYNC(variable_interface_type == GL_BUFFER_VARIABLE ||
                      variable_interface_type == GL_UNIFORM,
                      "Sanity check failed");

    if (variable_interface_type == GL_BUFFER_VARIABLE ||
        variable_interface_type == GL_UNIFORM)
    {
        program_ptr->pGLGetProgramResourceiv(program_ptr->id,
                                             variable_interface_type,
                                             n_variable,
                                             1, /* propCount */
                                            &piq_property_array_size,
                                             1, /* bufSize */
                                             NULL, /* length */
                                             &variable_ptr->size);
        program_ptr->pGLGetProgramResourceiv(program_ptr->id,
                                             variable_interface_type,
                                             n_variable,
                                             1, /* propCount */
                                            &piq_property_array_stride,
                                             1, /* bufSize */
                                             NULL, /* length */
                                            &variable_ptr->array_stride);
        program_ptr->pGLGetProgramResourceiv(program_ptr->id,
                                             variable_interface_type,
                                             n_variable,
                                             1, /* propCount */
                                            &piq_property_block_index,
                                             1, /* bufSize */
                                             NULL, /* length */
                                            &variable_ptr->block_index);
        program_ptr->pGLGetProgramResourceiv(program_ptr->id,
                                             variable_interface_type,
                                             n_variable,
                                             1, /* propCount */
                                            &piq_property_is_row_major,
                                             1, /* bufSize */
                                             NULL, /* length */
                                            &variable_ptr->is_row_major_matrix);
        program_ptr->pGLGetProgramResourceiv(program_ptr->id,
                                             variable_interface_type,
                                             n_variable,
                                             1, /* propCount */
                                            &piq_property_matrix_stride,
                                             1, /* bufSize */
                                             NULL, /* length */
                                            &variable_ptr->matrix_stride);
        program_ptr->pGLGetProgramResourceiv(program_ptr->id,
                                             variable_interface_type,
                                             n_variable,
                                             1, /* propCount */
                                            &piq_property_offset,
                                             1, /* bufSize */
                                             NULL, /* length */
                                            &variable_ptr->block_offset);
        program_ptr->pGLGetProgramResourceiv(program_ptr->id,
                                             variable_interface_type,
                                             n_variable,
                                             1, /* propCount */
                                            &piq_property_name_length,
                                             1, /* bufSize */
                                             NULL, /* length */
                                             &variable_ptr->length);
        program_ptr->pGLGetProgramResourceiv(program_ptr->id,
                                             variable_interface_type,
                                             n_variable,
                                             1, /* propCount */
                                            &piq_property_type,
                                             1, /* bufSize */
                                             NULL, /* length */
                                   (GLint*) &variable_ptr->type);
    }

    if (variable_interface_type == GL_BUFFER_VARIABLE)
    {
        program_ptr->pGLGetProgramResourceiv(program_ptr->id,
                                             variable_interface_type,
                                             n_variable,
                                             1, /* propCount */
                                            &piq_property_top_level_array_size,
                                             1, /* bufSize */
                                             NULL, /* length */
                                            &variable_ptr->top_level_array_size);
        program_ptr->pGLGetProgramResourceiv(program_ptr->id,
                                             variable_interface_type,
                                             n_variable,
                                             1, /* propCount */
                                            &piq_property_top_level_array_stride,
                                             1, /* bufSize */
                                             NULL, /* length */
                                            &variable_ptr->top_level_array_stride);
    }

    if (!is_temp_variable_defined)
    {
        temp_variable_name_storage = new (std::nothrow) char[variable_ptr->length + 1];
    }

    memset(temp_variable_name_storage,
           0,
           variable_ptr->length + 1);

    program_ptr->pGLGetProgramResourceName(program_ptr->id,
                                           variable_interface_type,
                                           n_variable,
                                           variable_ptr->length + 1,
                                           NULL,
                                           temp_variable_name_storage);

    if (variable_interface_type == GL_UNIFORM)
    {
        variable_ptr->location = program_ptr->pGLGetProgramResourceLocation(program_ptr->id,
                                                                            variable_interface_type,
                                                                            temp_variable_name_storage);
    }

    variable_ptr->name = system_hashed_ansi_string_create(temp_variable_name_storage);

    if (!is_temp_variable_defined)
    {
        delete [] temp_variable_name_storage;

        temp_variable_name_storage = NULL;
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API bool ogl_program_get_attached_shader(ogl_program program,
                                                        uint32_t    n_attached_shader,
                                                        ogl_shader* out_shader)
{
    _ogl_program* program_ptr = (_ogl_program*) program;

    return system_resizable_vector_get_element_at(program_ptr->attached_shaders,
                                                  n_attached_shader,
                                                 *out_shader);
}

/** Please see header for specification */
PUBLIC EMERALD_API bool ogl_program_get_attribute_by_index(ogl_program                              program,
                                                           size_t                                   n_attribute,
                                                           const ogl_program_attribute_descriptor** out_attribute)
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
PUBLIC EMERALD_API bool ogl_program_get_attribute_by_name(ogl_program                              program,
                                                          system_hashed_ansi_string                name,
                                                          const ogl_program_attribute_descriptor** out_attribute)
{
    _ogl_program* program_ptr = (_ogl_program*) program;
    bool          result      = false;

    ASSERT_DEBUG_SYNC(program_ptr->link_status,
                      "You cannot retrieve an attribute descriptor without linking the program beforehand.");

    if (program_ptr->link_status)
    {
        unsigned int n_attributes = 0;

        system_resizable_vector_get_property(program_ptr->active_attributes,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                            &n_attributes);

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
PUBLIC EMERALD_API GLuint ogl_program_get_id(ogl_program program)
{
    return ((_ogl_program*)program)->id;
}

/** Please see header for specification */
PUBLIC EMERALD_API system_hashed_ansi_string ogl_program_get_name(ogl_program program)
{
    return ((_ogl_program*) program)->name;
}

/** Please see header for specification */
PUBLIC EMERALD_API system_hashed_ansi_string ogl_program_get_program_info_log(ogl_program program)
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
PUBLIC EMERALD_API bool ogl_program_get_shader_storage_block_by_sb_index(ogl_program      program,
                                                                         unsigned int     index,
                                                                         ogl_program_ssb* out_ssb_ptr)
{
    _ogl_program*   program_ptr = (_ogl_program*) program;
    bool            result      = false;
    ogl_program_ssb result_ssb  = NULL;

    result = system_hash64map_get_element_at(program_ptr->ssb_index_to_ssb_map,
                                  index,
                                 &result_ssb,
                                  NULL); /* pOutHash */

    if (result)
    {
        *out_ssb_ptr = result_ssb;
    }

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool ogl_program_get_shader_storage_block_by_name(ogl_program               program,
                                                                     system_hashed_ansi_string name,
                                                                     ogl_program_ssb*          out_ssb_ptr)
{
    _ogl_program* program_ptr = (_ogl_program*) program;
    bool          result      = false;

    result = system_hash64map_get(program_ptr->ssb_name_to_ssb_map,
                                  system_hashed_ansi_string_get_hash(name),
                                  out_ssb_ptr);

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool ogl_program_get_uniform_by_index(ogl_program                  program,
                                                         size_t                       n_uniform,
                                                         const ogl_program_variable** out_uniform)
{
    _ogl_program* program_ptr = (_ogl_program*) program;
    bool          result      = false;

    ASSERT_DEBUG_SYNC(program_ptr->link_status,
                      "You cannot retrieve an uniform descriptor without linking the program beforehand.");

    if (program_ptr->link_status)
    {
        ogl_program_variable* descriptor = NULL;

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
PUBLIC EMERALD_API bool ogl_program_get_uniform_by_name(ogl_program                  program,
                                                        system_hashed_ansi_string    name,
                                                        const ogl_program_variable** out_uniform)
{
    _ogl_program* program_ptr = (_ogl_program*) program;
    bool          result      = false;

    ASSERT_DEBUG_SYNC(program_ptr->link_status,
                      "You cannot retrieve an uniform descriptor without linking the program beforehand.");

    if (program_ptr->link_status)
    {
        unsigned int n_uniforms = 0;

        system_resizable_vector_get_property(program_ptr->active_uniforms,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                            &n_uniforms);

        for (unsigned int n_uniform = 0;
                          n_uniform < n_uniforms;
                        ++n_uniform)
        {
            ogl_program_variable* descriptor = NULL;

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
PUBLIC EMERALD_API bool ogl_program_get_uniform_block_by_ub_index(ogl_program     program,
                                                                  unsigned int    index,
                                                                  ogl_program_ub* out_ub_ptr)
{
    ogl_context      current_context    = NULL;
    _ogl_program*    program_ptr        = (_ogl_program*) program;
    bool             result             = false;
    ogl_program_ub   result_ub          = NULL;
    system_hash64map ub_index_to_ub_map = NULL;

    if (program_ptr->syncable_ubs_mode == OGL_PROGRAM_SYNCABLE_UBS_MODE_ENABLE_PER_CONTEXT)
    {
        current_context = ogl_context_get_current_context();
    } /* if (program_ptr->syncable_ubs_mode == OGL_PROGRAM_SYNCABLE_UBS_MODE_ENABLE_PER_CONTEXT) */

    if (!system_hash64map_contains(program_ptr->context_to_ub_name_to_ub_map,
                                   (system_hash64) current_context) )
    {
        _ogl_program_init_blocks_for_context(OGL_PROGRAM_BLOCK_TYPE_UNIFORM_BUFFER,
                                             program_ptr,
                                             current_context,
                                             ogl_context_get_current_context() );
    }

    if (system_hash64map_contains(program_ptr->context_to_ub_name_to_ub_map,
                                  (system_hash64) current_context) )
    {
        if (system_hash64map_get(program_ptr->context_to_ub_index_to_ub_map,
                                 (system_hash64) current_context,
                                &ub_index_to_ub_map) )
        {
            result = system_hash64map_get(ub_index_to_ub_map,
                                          index,
                                         &result_ub);

            if (result)
            {
                *out_ub_ptr = result_ub;
            }
        }
    }

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool ogl_program_get_uniform_block_by_name(ogl_program               program,
                                                              system_hashed_ansi_string name,
                                                              ogl_program_ub*           out_ub_ptr)
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
        _ogl_program_init_blocks_for_context(OGL_PROGRAM_BLOCK_TYPE_UNIFORM_BUFFER,
                                             program_ptr,
                                             current_context,
                                             ogl_context_get_current_context() );
    }

    if (system_hash64map_contains(program_ptr->context_to_ub_name_to_ub_map,
                                  (system_hash64) current_context) )
    {
        if (system_hash64map_get(program_ptr->context_to_ub_name_to_ub_map,
                                 (system_hash64) current_context,
                                &ub_name_to_ub_map) )
        {
            result = system_hash64map_get(ub_name_to_ub_map,
                                          system_hashed_ansi_string_get_hash(name),
                                          out_ub_ptr);
        }
    }

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool ogl_program_link(ogl_program program)
{
    _ogl_program* program_ptr        = (_ogl_program*) program;
    unsigned int  n_attached_shaders = 0;
    bool          result             = false;

    system_resizable_vector_get_property(program_ptr->attached_shaders,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_attached_shaders);

    ASSERT_DEBUG_SYNC(n_attached_shaders > 0,
                      "Linking will fail - no shaders attached.");

    if (n_attached_shaders > 0)
    {
        /* Clean up */
        _ogl_program_release_active_attributes           (program_ptr->active_attributes);
        _ogl_program_release_active_shader_storage_blocks(program_ptr);
        _ogl_program_release_active_uniform_blocks       (program_ptr);
        _ogl_program_release_active_uniforms             (program_ptr->active_uniforms);

        /* Run through all the attached shaders and make sure these are compiled.
        *
         * If not, compile them before proceeding with handling the actual linking request.
         */
        bool all_shaders_compiled = true;

        for (unsigned int n_shader = 0;
                          n_shader < n_attached_shaders;
                        ++n_shader)
        {
            ogl_shader current_shader           = NULL;
            GLuint     current_shader_gl_id     = 0;
            bool       is_compiled_successfully = false;

            if (!system_resizable_vector_get_element_at(program_ptr->attached_shaders,
                                                        n_shader,
                                                       &current_shader) )
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Could not retrieve attached shader object at index [%d]",
                                  n_shader);

                continue;
            }

            current_shader_gl_id     = ogl_shader_get_id            (current_shader);
            is_compiled_successfully = ogl_shader_get_compile_status(current_shader);

            if (!is_compiled_successfully)
            {
                LOG_ERROR("Shader object [%u] has not been compiled successfully prior to linking. Attempting to compile.",
                          current_shader_gl_id
                );

                ogl_shader_compile(current_shader);

                is_compiled_successfully = ogl_shader_get_compile_status(current_shader);
            }

            if (!is_compiled_successfully)
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Cannot proceeding with linking - at least one shader object failed to compile");

                all_shaders_compiled = false;
                break;
            }
        } /* for (all attached shaders) */

        if (all_shaders_compiled)
        {
            /* Let's go. */
            ogl_context_request_callback_from_context_thread(ral_context_get_gl_context(program_ptr->context),
                                                             _ogl_program_link_callback,
                                                             program);

            result = program_ptr->link_status;
        }
    } /* if (n_attached_shaders > 0) */

    return result;
}

/** Please see header for specification */
PUBLIC RENDERING_CONTEXT_CALL void ogl_program_release_context_objects(ogl_program program,
                                                                       ogl_context context)
{
    _ogl_program* program_ptr = (_ogl_program*) program;

    ASSERT_DEBUG_SYNC(context != NULL,
                      "Sanity check failed");

    if (context != NULL)
    {
        _ogl_program_release_active_uniform_blocks(program_ptr,
                                                   context);
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API void ogl_program_set_tf_varyings(ogl_program    program,
                                                    unsigned int   n_varyings,
                                                    const GLchar** varying_names,
                                                    GLenum         tf_mode)
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
