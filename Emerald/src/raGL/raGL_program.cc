/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_context_state_cache.h"
#include "raGL/raGL_program.h"
#include "raGL/raGL_shader.h"
#include "raGL/raGL_utils.h"
#include "ral/ral_context.h"
#include "ral/ral_program.h"
#include "ral/ral_shader.h"
#include "system/system_assertions.h"
#include "system/system_callback_manager.h"
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
typedef struct _raGL_program_block_binding
{
    uint32_t                  block_index;
    system_hashed_ansi_string block_name;
    ral_program_block_type    block_type;
    GLuint                    indexed_bp;

    _raGL_program_block_binding()
    {
        block_index = -1;
        block_name  = NULL;
        block_type  = RAL_PROGRAM_BLOCK_TYPE_UNDEFINED;
        indexed_bp  = 0;
    }
} _raGL_program_block_binding;

typedef struct
{
    system_resizable_vector   active_attributes_raGL; /* holds _raGL_program_attribute instances */
    system_resizable_vector   active_uniforms_raGL;   /* holds _raGL_program_variable instances */
    ral_context               context;                /* DO NOT retain - context owns the instance! */
    GLuint                    id;
    bool                      link_status;
    system_hashed_ansi_string program_info_log;
    ral_program               program_ral;

    system_hash64map             block_name_to_binding_map;
    uint32_t                     n_ssb_bindings;
    uint32_t                     n_ub_bindings;
    _raGL_program_block_binding* ssb_bindings;
    _raGL_program_block_binding* ub_bindings;

    /* GL entry-point cache */
    PFNGLATTACHSHADERPROC               pGLAttachShader;
    PFNGLCREATEPROGRAMPROC              pGLCreateProgram;
    PFNGLDELETEPROGRAMPROC              pGLDeleteProgram;
    PFNGLDETACHSHADERPROC               pGLDetachShader;
    PFNGLGETACTIVEATTRIBPROC            pGLGetActiveAttrib;
    PFNGLGETATTRIBLOCATIONPROC          pGLGetAttribLocation;
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
    PFNGLUNIFORMBLOCKBINDINGPROC        pGLUniformBlockBinding;

    REFCOUNT_INSERT_VARIABLES
} _raGL_program;

typedef struct
{
    _raGL_program* program_ptr;
    raGL_shader    shader;
} _raGL_attach_shader_callback_argument;


/** Internal variables */

/* Forward declarations */
PRIVATE void _raGL_program_attach_shader_callback    (ogl_context             context,
                                                     void*                    in_arg);
/** TODO */
PRIVATE void _raGL_program_clear_bindings_metadata   (_raGL_program*          program_ptr,
                                                      bool                    should_release);
PRIVATE void _raGL_program_create_callback           (ogl_context             context,
                                                     void*                    in_arg);
PRIVATE void _raGL_program_detach_shader_callback    (ogl_context             context,
                                                     void*                    in_arg);
PRIVATE char*_raGL_program_get_binary_blob_file_name (_raGL_program*          program_ptr);
PRIVATE char*_raGL_program_get_source_code_file_name (_raGL_program*          program_ptr);
PRIVATE void _raGL_program_link_callback             (ogl_context             context,
                                                      void*                   in_arg);
PRIVATE bool _raGL_program_load_binary_blob          (ogl_context,
                                                      _raGL_program*          program_ptr);
PRIVATE void _raGL_program_on_shader_compile_callback(const void*             callback_data,
                                                      void*                   user_arg);
PRIVATE void _raGL_program_release                   (void*                   program);
PRIVATE void _raGL_program_release_active_attributes (system_resizable_vector active_attributes);
PRIVATE void _raGL_program_release_active_uniforms   (system_resizable_vector active_uniforms);
PRIVATE void _raGL_program_release_callback          (ogl_context             context,
                                                      void*                   in_arg);
PRIVATE void _raGL_program_save_binary_blob          (ogl_context,
                                                      _raGL_program*          program_ptr);
PRIVATE void _raGL_program_save_shader_sources       (_raGL_program*          program_ptr);


/** TODO */
PRIVATE void _raGL_program_attach_shader_callback(ogl_context context,
                                                  void*       in_arg)
{
    _raGL_attach_shader_callback_argument* callback_arg_ptr = (_raGL_attach_shader_callback_argument*) in_arg;
    GLuint                                 shader_raGL_id   = 0;

    raGL_shader_get_property(callback_arg_ptr->shader,
                             RAGL_SHADER_PROPERTY_ID,
                            &shader_raGL_id);

    callback_arg_ptr->program_ptr->pGLAttachShader(callback_arg_ptr->program_ptr->id,
                                                   shader_raGL_id);
}

/** TODO */
PRIVATE void _raGL_program_clear_bindings_metadata(_raGL_program* program_ptr,
                                                   bool           should_release)
{
    if (program_ptr->block_name_to_binding_map != NULL)
    {
        if (should_release)
        {
            system_hash64map_release(program_ptr->block_name_to_binding_map);

            program_ptr->block_name_to_binding_map = NULL;
        }
        else
        {
            system_hash64map_clear(program_ptr->block_name_to_binding_map);
        }
    }

    if (program_ptr->ssb_bindings != NULL)
    {
        delete [] program_ptr->ssb_bindings;

        program_ptr->ssb_bindings = NULL;
    }

    if (program_ptr->ub_bindings != NULL)
    {
        delete [] program_ptr->ub_bindings;

        program_ptr->ub_bindings = NULL;
    }

    program_ptr->n_ssb_bindings = 0;
    program_ptr->n_ub_bindings  = 0;
}

/** TODO */
PRIVATE void _raGL_program_create_callback(ogl_context context,
                                           void*       in_arg)
{
    _raGL_program* program_ptr = (_raGL_program*) in_arg;

    program_ptr->id                     = program_ptr->pGLCreateProgram();
    program_ptr->active_attributes_raGL = system_resizable_vector_create(4 /* capacity */);
    program_ptr->active_uniforms_raGL   = system_resizable_vector_create(4 /* capacity */);

    /* Let the impl know that we will be interested in extracting the blob */
    program_ptr->pGLProgramParameteri(program_ptr->id,
                                      GL_PROGRAM_BINARY_RETRIEVABLE_HINT,
                                      GL_TRUE);

    /* Attach all shaders */
    uint32_t n_program_shaders = 0;

    ral_program_get_property(program_ptr->program_ral,
                             RAL_PROGRAM_PROPERTY_N_ATTACHED_SHADERS,
                            &n_program_shaders);

    for (uint32_t n_shader = 0;
                  n_shader < n_program_shaders;
                ++n_shader)
    {
        ral_shader  current_shader         = NULL;
        raGL_shader current_shader_raGL    = NULL;
        GLuint      current_shader_raGL_id = 0;

        if (!ral_program_get_attached_shader_at_index(program_ptr->program_ral,
                                                      n_shader,
                                                     &current_shader) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve shader at index [%d], attached to the ral_program instance.",
                              n_shader);

            continue;
        }

        current_shader_raGL = ral_context_get_shader_gl(program_ptr->context,
                                                        current_shader);

        ASSERT_DEBUG_SYNC(current_shader_raGL != NULL,
                          "No raGL_shader instance associated with current ral_shader instance.");

        raGL_shader_get_property(current_shader_raGL,
                                 RAGL_SHADER_PROPERTY_ID,
                                &current_shader_raGL_id);

        program_ptr->pGLAttachShader(program_ptr->id,
                                     current_shader_raGL_id);
    }
}

/** TODO */
PUBLIC void raGL_program_get_program_variable_details(raGL_program            program,
                                                      unsigned int            temp_variable_name_storage_size,
                                                      char*                   temp_variable_name_storage,
                                                      ral_program_variable*   variable_ral_ptr,
                                                      _raGL_program_variable* variable_raGL_ptr,
                                                      GLenum                  variable_interface_type,
                                                      unsigned int            n_variable)
{
    bool                is_temp_variable_defined            = (temp_variable_name_storage != NULL) ? true
                                                                                                   : false;
    GLint               name_length                         = 0;
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
    _raGL_program*      program_ptr                         = (_raGL_program*) program;

    if (variable_raGL_ptr != NULL)
    {
        variable_raGL_ptr->block_index = -1;
        variable_raGL_ptr->location    = -1;
    }

    if (variable_ral_ptr != NULL)
    {
        variable_ral_ptr->array_stride           = 0;
        variable_ral_ptr->block_offset           = -1;
        variable_ral_ptr->is_row_major_matrix    = false;
        variable_ral_ptr->length                 = 0;
        variable_ral_ptr->matrix_stride          = 0;
        variable_ral_ptr->name                   = NULL;
        variable_ral_ptr->size                   = 0;
        variable_ral_ptr->top_level_array_size   = 0;
        variable_ral_ptr->top_level_array_stride = 0;
        variable_ral_ptr->type                   = RAL_PROGRAM_VARIABLE_TYPE_UNDEFINED;
    }

    ASSERT_DEBUG_SYNC(variable_interface_type == GL_BUFFER_VARIABLE ||
                      variable_interface_type == GL_UNIFORM,
                      "Sanity check failed");

    program_ptr->pGLGetProgramResourceiv(program_ptr->id,
                                         variable_interface_type,
                                         n_variable,
                                         1, /* propCount */
                                        &piq_property_name_length,
                                         1,    /* bufSize */
                                         NULL, /* length  */
                                        &name_length);

    if (variable_interface_type == GL_BUFFER_VARIABLE ||
        variable_interface_type == GL_UNIFORM)
    {
        GLint type_gl = 0;

        if (variable_ral_ptr != NULL)
        {
            variable_ral_ptr->length = name_length;

            program_ptr->pGLGetProgramResourceiv(program_ptr->id,
                                                 variable_interface_type,
                                                 n_variable,
                                                 1, /* propCount */
                                                &piq_property_array_size,
                                                 1, /* bufSize */
                                                 NULL, /* length */
                                                 &variable_ral_ptr->size);
            program_ptr->pGLGetProgramResourceiv(program_ptr->id,
                                                 variable_interface_type,
                                                 n_variable,
                                                 1, /* propCount */
                                                &piq_property_array_stride,
                                                 1,    /* bufSize */
                                                 NULL, /* length  */
                                                 (GLint*) &variable_ral_ptr->array_stride);
            program_ptr->pGLGetProgramResourceiv(program_ptr->id,
                                                 variable_interface_type,
                                                 n_variable,
                                                 1, /* propCount */
                                                &piq_property_is_row_major,
                                                 1,    /* bufSize */
                                                 NULL, /* length  */
                                                (GLint*) &variable_ral_ptr->is_row_major_matrix);
            program_ptr->pGLGetProgramResourceiv(program_ptr->id,
                                                 variable_interface_type,
                                                 n_variable,
                                                 1, /* propCount */
                                                &piq_property_matrix_stride,
                                                 1,    /* bufSize */
                                                 NULL, /* length  */
                                                 (GLint*) &variable_ral_ptr->matrix_stride);
            program_ptr->pGLGetProgramResourceiv(program_ptr->id,
                                                 variable_interface_type,
                                                 n_variable,
                                                 1, /* propCount */
                                                &piq_property_offset,
                                                 1,    /* bufSize */
                                                 NULL, /* length  */
                                                 (GLint*) &variable_ral_ptr->block_offset);
            program_ptr->pGLGetProgramResourceiv(program_ptr->id,
                                                 variable_interface_type,
                                                 n_variable,
                                                 1, /* propCount */
                                                &piq_property_type,
                                                 1,    /* bufSize */
                                                 NULL, /* length  */
                                       (GLint*) &type_gl);

            variable_ral_ptr->type = raGL_utils_get_ral_program_variable_type_for_ogl_enum(type_gl);
        } /* if (variable_ral_ptr != NULL) */

        if (variable_raGL_ptr != NULL)
        {
            program_ptr->pGLGetProgramResourceiv(program_ptr->id,
                                                 variable_interface_type,
                                                 n_variable,
                                                 1, /* propCount */
                                                &piq_property_block_index,
                                                 1,    /* bufSize */
                                                 NULL, /* length  */
                                                 (GLint*) &variable_raGL_ptr->block_index);
        }
    }
    else
    if (variable_interface_type == GL_BUFFER_VARIABLE)
    {
        if (variable_ral_ptr != NULL)
        {
            program_ptr->pGLGetProgramResourceiv(program_ptr->id,
                                                 variable_interface_type,
                                                 n_variable,
                                                 1, /* propCount */
                                                &piq_property_top_level_array_size,
                                                 1,    /* bufSize */
                                                 NULL, /* length  */
                                                 (GLint*) &variable_ral_ptr->top_level_array_size);
            program_ptr->pGLGetProgramResourceiv(program_ptr->id,
                                                 variable_interface_type,
                                                 n_variable,
                                                 1, /* propCount */
                                                &piq_property_top_level_array_stride,
                                                 1,    /* bufSize */
                                                 NULL, /* length  */
                                                (GLint*) &variable_ral_ptr->top_level_array_stride);
        } /* if (variable_ral_ptr != NULL) */
    }

    if (!is_temp_variable_defined)
    {
        temp_variable_name_storage = new (std::nothrow) char[name_length + 1];
    }

    memset(temp_variable_name_storage,
           0,
           name_length + 1);

    program_ptr->pGLGetProgramResourceName(program_ptr->id,
                                           variable_interface_type,
                                           n_variable,
                                           name_length + 1,
                                           NULL,
                                           temp_variable_name_storage);

    /* If the member name is prefixed with block name, skip that prefix. */
    char*                     final_variable_name     = temp_variable_name_storage;
    system_hashed_ansi_string final_variable_name_has = NULL;

    while (true)
    {
        char* tmp = strchr(final_variable_name,
                           '.');

        if (tmp == NULL)
        {
            break;
        }
        else
        {
            final_variable_name = tmp + 1;
        }
    }

    if (variable_raGL_ptr != NULL)
    {
        if (variable_interface_type == GL_UNIFORM)
        {
            variable_raGL_ptr->location = program_ptr->pGLGetProgramResourceLocation(program_ptr->id,
                                                                                     variable_interface_type,
                                                                                     temp_variable_name_storage);
        }

        variable_raGL_ptr->name = system_hashed_ansi_string_create(final_variable_name);
    }

    if (variable_ral_ptr != NULL)
    {
        variable_ral_ptr->name = system_hashed_ansi_string_create(final_variable_name);
    }

    if (!is_temp_variable_defined)
    {
        delete [] temp_variable_name_storage;

        temp_variable_name_storage = NULL;
    }
}

/** TODO */
PRIVATE char* _raGL_program_get_binary_blob_file_name(_raGL_program* program_ptr)
{
    /* Form the hash-based file name */
    std::stringstream         file_name_sstream;
    std::string               file_name_string;
    system_hashed_ansi_string program_name      = NULL;

    ral_program_get_property(program_ptr->program_ral,
                             RAL_PROGRAM_PROPERTY_NAME,
                            &program_name);

    file_name_sstream << file_blob_prefix
                      << system_hashed_ansi_string_get_hash(program_name);

    file_name_string  = file_name_sstream.str();

    /* Form the result string */
    char* file_name = new (std::nothrow) char[file_name_string.length() + 1];

    memcpy(file_name,
           file_name_string.c_str(),
           file_name_string.length() + 1);

    return file_name;
}

/** TODO */
PRIVATE char* _raGL_program_get_source_code_file_name(_raGL_program* program_ptr)
{
    /* Form the hash-based file name */
    std::stringstream         file_name_sstream;
    std::string               file_name_string;
    system_hashed_ansi_string program_name      = NULL;

    ral_program_get_property(program_ptr->program_ral,
                             RAL_PROGRAM_PROPERTY_NAME,
                            &program_name);

    file_name_sstream << file_sourcecode_prefix
                      << system_hashed_ansi_string_get_hash(program_name);

    file_name_string  = file_name_sstream.str();

    /* Form the result string */
    char* file_name = new (std::nothrow) char[file_name_string.length() + 1];

    memcpy(file_name,
           file_name_string.c_str(),
           file_name_string.length() + 1);

    return file_name;
}

/** TODO */
PRIVATE void _raGL_program_init_blocks_for_context(ral_program_block_type block_type,
                                                   _raGL_program*         program_ptr)
{
    _raGL_program_block_binding* block_bindings_ptr              = NULL;
    static const GLenum          block_property_buffer_data_size = GL_BUFFER_DATA_SIZE;
    const GLenum                 block_interface_gl              = (block_type == RAL_PROGRAM_BLOCK_TYPE_STORAGE_BUFFER) ? GL_SHADER_STORAGE_BLOCK
                                                                                                                         : GL_UNIFORM_BLOCK;
    GLchar*                      block_name                      = NULL;
    GLint                        n_active_blocks                 = 0;
    GLint                        n_active_block_max_length       = 0;

    program_ptr->pGLGetProgramInterfaceiv(program_ptr->id,
                                          block_interface_gl,
                                          GL_ACTIVE_RESOURCES, /* pname */
                                          &n_active_blocks);

    if (n_active_blocks != 0)
    {
        block_bindings_ptr = new (std::nothrow) _raGL_program_block_binding[n_active_blocks];

        ASSERT_ALWAYS_SYNC(block_bindings_ptr != NULL,
                           "Out of memory");
    }

    switch (block_type)
    {
        case RAL_PROGRAM_BLOCK_TYPE_STORAGE_BUFFER:
        {
            ASSERT_DEBUG_SYNC(program_ptr->ssb_bindings == NULL,
                              "");

            program_ptr->n_ssb_bindings = n_active_blocks;
            program_ptr->ssb_bindings   = block_bindings_ptr;
            break;
        }

        case RAL_PROGRAM_BLOCK_TYPE_UNIFORM_BUFFER:
        {
            ASSERT_DEBUG_SYNC(program_ptr->ub_bindings  == NULL,
                              "");

            program_ptr->n_ub_bindings = n_active_blocks;
            program_ptr->ub_bindings   = block_bindings_ptr;
            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized RAL program block type");
        }
    }

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
                                                 block_interface_gl,
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
        /* Register the new block */
        system_hashed_ansi_string block_name_has = NULL;
        GLint                     block_size     = 0;

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

        program_ptr->pGLGetProgramResourceiv(program_ptr->id,
                                             block_interface_gl,
                                             n_active_block,
                                             1, /* propCount */
                                            &block_property_buffer_data_size,
                                             1,    /* bufSize */
                                             NULL, /* length */
                                            &block_size);

        ral_program_add_block(program_ptr->program_ral,
                              block_size,
                              block_type,
                              block_name_has);
    }

    for (GLint n_active_block = 0;
               n_active_block < n_active_blocks;
             ++n_active_block)
    {
        system_hashed_ansi_string block_name_has = NULL;

        ral_program_get_block_property_by_index(program_ptr->program_ral,
                                                block_type,
                                                n_active_block,
                                                RAL_PROGRAM_BLOCK_PROPERTY_NAME,
                                               &block_name_has);

        /* For UBs, also enumerate members */
        if (block_type == RAL_PROGRAM_BLOCK_TYPE_UNIFORM_BUFFER)
        {
                         GLint* active_variable_indices             = NULL;
            static const GLenum block_property_active_variables     = GL_ACTIVE_VARIABLES;
            static const GLenum block_property_buffer_data_size     = GL_BUFFER_DATA_SIZE;
            static const GLenum block_property_num_active_variables = GL_NUM_ACTIVE_VARIABLES;
                         GLint  n_active_variables                  = 0;
                         GLint  n_active_uniform_blocks             = 0;
                         GLint  n_active_uniform_block_max_length   = 0;

            program_ptr->pGLGetProgramResourceiv(program_ptr->id,
                                                 GL_UNIFORM_BLOCK,
                                                 n_active_block,
                                                 1, /* propCount */
                                                &block_property_num_active_variables,
                                                 1,    /* bufSize */
                                                 NULL, /* length */
                                                &n_active_variables);

            if (n_active_variables > 0)
            {
                active_variable_indices = new (std::nothrow) GLint[n_active_variables];

                ASSERT_ALWAYS_SYNC(active_variable_indices != NULL,
                                   "Out of memory");

                program_ptr->pGLGetProgramResourceiv(program_ptr->id,
                                                     GL_UNIFORM_BLOCK,
                                                     n_active_block,
                                                     1, /* propCount */
                                                    &block_property_active_variables,
                                                     n_active_variables,
                                                     NULL, /* length */
                                                     active_variable_indices);

                for (int n_active_variable = 0;
                         n_active_variable < n_active_variables;
                       ++n_active_variable)
                {
                    ral_program_variable* variable_ral_ptr = NULL;

                    variable_ral_ptr = new (std::nothrow) ral_program_variable();

                    raGL_program_get_program_variable_details( (raGL_program) program_ptr,
                                                              0,    /* temp_variable_name_storage */
                                                              NULL, /* temp_variable_name */
                                                              variable_ral_ptr,
                                                              NULL,
                                                              GL_UNIFORM,
                                                              active_variable_indices[n_active_variable]);

                    ral_program_attach_variable_to_block(program_ptr->program_ral,
                                                         block_name_has,
                                                         variable_ral_ptr);
                } /* for (all active variables) */

                delete [] active_variable_indices;
                active_variable_indices = NULL;
            } /* if (n_active_variable > 0) */
        } /* if (block_type == RAL_PROGRAM_BLOCK_TYPE_UNIFORM_BUFFER) */

        /* Set up the block binding */
        block_bindings_ptr[n_active_block].block_index = n_active_block;
        block_bindings_ptr[n_active_block].block_name  = block_name_has;
        block_bindings_ptr[n_active_block].block_type  = block_type;
        block_bindings_ptr[n_active_block].indexed_bp  = n_active_block;

        ASSERT_DEBUG_SYNC(!system_hash64map_contains(program_ptr->block_name_to_binding_map,
                                                     system_hashed_ansi_string_get_hash(block_name_has) ),
                          "Block [%s] is already registered",
                          system_hashed_ansi_string_get_buffer(block_name_has) );

        system_hash64map_insert(program_ptr->block_name_to_binding_map,
                                system_hashed_ansi_string_get_hash(block_name_has),
                               &block_bindings_ptr[n_active_block],
                                NULL,  /* on_removal_callback          */
                                NULL); /* on_removal_callback_user_arg */

        switch (block_type)
        {
            case RAL_PROGRAM_BLOCK_TYPE_STORAGE_BUFFER:
            {
                program_ptr->pGLShaderStorageBlockBinding(program_ptr->id,
                                                          n_active_block,
                                                          n_active_block);

                break;
            }

            case RAL_PROGRAM_BLOCK_TYPE_UNIFORM_BUFFER:
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
PRIVATE void _raGL_program_link_callback(ogl_context context,
                                         void*       in_arg)
{
    _raGL_program* program_ptr     = (_raGL_program*) in_arg;
    bool           has_used_binary = false;

    system_time start_time = system_time_now();

    /* Clear metadata before proceeding .. */
    ral_program_clear_metadata           (program_ptr->program_ral);
    _raGL_program_clear_bindings_metadata(program_ptr,
                                          false /* should_release */);

    /* If program binaries are supportd, see if we've got a blob file already stashed. If so, no need to link at this point */
    has_used_binary = _raGL_program_load_binary_blob(context,
                                                     program_ptr);

    if (!has_used_binary)
    {
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
            _raGL_program_save_binary_blob(context,
                                           program_ptr);

            /* On debug builds, also stash source code of the shaders that were used
             * to create the program */
            #ifdef _DEBUG
            {
                _raGL_program_save_shader_sources(program_ptr);
            }
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
                _raGL_program_attribute* new_attribute_raGL_ptr = new _raGL_program_attribute;
                ral_program_attribute*   new_attribute_ral_ptr  = new ral_program_attribute;

                ASSERT_ALWAYS_SYNC(new_attribute_raGL_ptr != NULL &&
                                   new_attribute_ral_ptr  != NULL,
                                   "Out of memory while allocating space for vertex attribute descriptors.");

                new_attribute_ral_ptr->length = 0;
                new_attribute_ral_ptr->name   = NULL;
                new_attribute_ral_ptr->size   = 0;
                new_attribute_ral_ptr->type   = RAL_PROGRAM_ATTRIBUTE_TYPE_UNDEFINED;

                memset(attribute_name,
                       0,
                       new_attribute_ral_ptr->length + 1);

                program_ptr->pGLGetActiveAttrib(program_ptr->id,
                                                n_active_attribute,
                                                n_active_attribute_max_length+1,
                                                &new_attribute_ral_ptr->length,
                                                &new_attribute_ral_ptr->size,
                                                (GLenum*) &new_attribute_ral_ptr->type,
                                                attribute_name);

                new_attribute_raGL_ptr->name     = system_hashed_ansi_string_create (attribute_name);
                new_attribute_raGL_ptr->location = program_ptr->pGLGetAttribLocation(program_ptr->id,
                                                                                     attribute_name);
                new_attribute_ral_ptr->name      = new_attribute_raGL_ptr->name;

                ral_program_attach_vertex_attribute(program_ptr->program_ral,
                                                    new_attribute_ral_ptr);

                system_resizable_vector_push(program_ptr->active_attributes_raGL,
                                             new_attribute_raGL_ptr);
            } /* for (all active attributes) */

            /* Continue with uniform blocks. */
            _raGL_program_init_blocks_for_context(RAL_PROGRAM_BLOCK_TYPE_UNIFORM_BUFFER,
                                                  program_ptr);

            /* Finish with shader storage blocks. */
            _raGL_program_init_blocks_for_context(RAL_PROGRAM_BLOCK_TYPE_STORAGE_BUFFER,
                                                  program_ptr);

            /* Now for the uniforms coming from the default uniform block */
            ral_program_add_block(program_ptr->program_ral,
                                  0, /* block_size */
                                  RAL_PROGRAM_BLOCK_TYPE_UNIFORM_BUFFER,
                                  system_hashed_ansi_string_create("") );

            for (GLint n_active_uniform = 0;
                       n_active_uniform < n_active_uniforms;
                     ++n_active_uniform)
            {
                _raGL_program_variable temp_raGL;
                ral_program_variable   temp_ral;

                raGL_program_get_program_variable_details((raGL_program) program_ptr,
                                                          uniform_name_length,
                                                          uniform_name,
                                                         &temp_ral,
                                                         &temp_raGL,
                                                          GL_UNIFORM,
                                                          n_active_uniform);

                if (temp_ral.block_offset == -1)
                {
                    _raGL_program_variable* new_uniform_raGL_ptr = new (std::nothrow) _raGL_program_variable(temp_raGL);
                    ral_program_variable*   new_uniform_ral_ptr  = new (std::nothrow) ral_program_variable  (temp_ral);

                    ASSERT_ALWAYS_SYNC(new_uniform_raGL_ptr != NULL &&
                                       new_uniform_ral_ptr  != NULL,
                                       "Out of memory while allocating space for uniform descriptors.");

                    system_resizable_vector_push(program_ptr->active_uniforms_raGL,
                                                 new_uniform_raGL_ptr);

                    ral_program_attach_variable_to_block(program_ptr->program_ral,
                                                         system_hashed_ansi_string_create(""),
                                                         new_uniform_ral_ptr);
                }
            }
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
PRIVATE bool _raGL_program_load_binary_blob(ogl_context    context,
                                            _raGL_program* program_ptr)
{
    /* Form the name */
    char* file_name = _raGL_program_get_binary_blob_file_name(program_ptr);
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

            ral_program_get_property(program_ptr->program_ral,
                                     RAL_PROGRAM_PROPERTY_N_ATTACHED_SHADERS,
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
                    ral_shader    shader           = NULL;
                    system_hash64 shader_hash_read = 0;

                    ASSERT_ALWAYS_SYNC(::fread(&shader_hash_read,
                                               sizeof(shader_hash_read),
                                               1,
                                               blob_file_handle) == 1,
                                       "Could not read %dth shader hash.",
                                       n);

                    if (ral_program_get_attached_shader_at_index(program_ptr->program_ral,
                                                                 n,
                                                                &shader) )
                    {
                        system_hashed_ansi_string shader_body = NULL;

                        ral_shader_get_property(shader,
                                                RAL_SHADER_PROPERTY_GLSL_BODY,
                                               &shader_body);

                        if (system_hashed_ansi_string_get_hash(shader_body) != shader_hash_read)
                        {
                            system_hashed_ansi_string program_name = NULL;
                            system_hashed_ansi_string shader_name  = NULL;

                            ral_program_get_property(program_ptr->program_ral,
                                                     RAL_PROGRAM_PROPERTY_NAME,
                                                    &program_name);
                            ral_shader_get_property (shader,
                                                     RAL_SHADER_PROPERTY_NAME,
                                                    &shader_name);

                            LOG_INFO("Blob file for program [%s] was found but shader [%s]'s hash does not match the blob's one",
                                     system_hashed_ansi_string_get_buffer(program_name),
                                     system_hashed_ansi_string_get_buffer(shader_name));

                            hashes_ok = false;
                            break;
                        }
                    }
                    else
                    {
                        LOG_ERROR("Could not retrieve %uth shader attached to the RAL program",
                                  n);

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
                system_hashed_ansi_string program_name;

                ral_program_get_property(program_ptr->program_ral,
                                         RAL_PROGRAM_PROPERTY_NAME,
                                        &program_name);

                LOG_INFO("Blob file for program [%s] was found but header does not match the program instance's.",
                         system_hashed_ansi_string_get_buffer(program_name) );
            }

            /* Done interacting with the file */
            ::fclose(blob_file_handle);
        }
        else
        {
            system_hashed_ansi_string program_name;

            ral_program_get_property(program_ptr->program_ral,
                                     RAL_PROGRAM_PROPERTY_NAME,
                                    &program_name);

            LOG_INFO("Blob file for program [%s] was not found.",
                     system_hashed_ansi_string_get_buffer(program_name) );
        }

        delete [] file_name;

        file_name = NULL;
    }

    /* Done */
    return result;
}

/** TODO */
PRIVATE void _raGL_program_on_shader_compile_callback(const void* callback_data,
                                                      void*       user_arg)
{
    _raGL_program*            program_ptr  = (_raGL_program*) user_arg;
    system_hashed_ansi_string program_name = NULL;
    raGL_shader               shader       = (raGL_shader)    callback_data;
    ral_shader                shader_ral   = NULL;
    system_hashed_ansi_string shader_name  = NULL;

    ral_program_get_property(program_ptr->program_ral,
                             RAL_PROGRAM_PROPERTY_NAME,
                            &program_name);
    raGL_shader_get_property(shader,
                             RAGL_SHADER_PROPERTY_SHADER_RAL,
                            &shader_ral);

    ral_shader_get_property(shader_ral,
                            RAL_SHADER_PROPERTY_NAME,
                           &shader_name);

    LOG_INFO("Shader [%s] recompiled - linking program [%s] ..",
             system_hashed_ansi_string_get_buffer(shader_name),
             system_hashed_ansi_string_get_buffer(program_name) );

    raGL_program_link( (raGL_program) program_ptr);
}

/** TODO */
PRIVATE void _raGL_program_release(void* program)
{
    _raGL_program* program_ptr = (_raGL_program*) program;

    ogl_context_request_callback_from_context_thread(ral_context_get_gl_context(program_ptr->context),
                                                     _raGL_program_release_callback,
                                                     program_ptr);

    /* Release resizable vectors */
    ral_program_attribute* attribute_ptr = NULL;
    ral_program_variable*  uniform_ptr   = NULL;

    _raGL_program_release_active_attributes(program_ptr->active_attributes_raGL);
    _raGL_program_release_active_uniforms  (program_ptr->active_uniforms_raGL);

    if (program_ptr->active_attributes_raGL != NULL)
    {
        system_resizable_vector_release(program_ptr->active_attributes_raGL);
        program_ptr->active_attributes_raGL = NULL;
    }

    if (program_ptr->active_uniforms_raGL != NULL)
    {
        system_resizable_vector_release(program_ptr->active_uniforms_raGL);
        program_ptr->active_uniforms_raGL = NULL;
    }

    _raGL_program_clear_bindings_metadata(program_ptr,
                                          true /* should_release */);
}

/** TODO */
PRIVATE void _raGL_program_release_active_attributes(system_resizable_vector active_attributes)
{
    while (true)
    {
        ral_program_attribute* program_attribute_ptr = NULL;
        bool                   result_get            = system_resizable_vector_pop(active_attributes,
                                                                                  &program_attribute_ptr);

        if (!result_get)
        {
            break;
        }

        delete program_attribute_ptr;
    }
}

/** TODO */
PRIVATE void _raGL_program_release_active_uniforms(system_resizable_vector active_uniforms)
{
    while (true)
    {
        ral_program_variable* program_uniform_ptr = NULL;
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
PRIVATE void _raGL_program_release_callback(ogl_context context,
                                            void*       in_arg)
{
    _raGL_program* program_ptr = (_raGL_program*) in_arg;

    program_ptr->pGLDeleteProgram(program_ptr->id);

    program_ptr->id = 0;
}

/** TODO */
PRIVATE void _raGL_program_save_binary_blob(ogl_context   context_ptr,
                                            _raGL_program* program_ptr)
{
    /* Program file names tend to be length and often exceed Windows ANSI API limits.
     * Therefore, we use HAS' hash as actual blob file name, and store the actual
     * program file name at the beginning of the source code file. (implemented elsewhere)
     */
    char* file_name_src = _raGL_program_get_binary_blob_file_name(program_ptr);

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

                    ral_program_get_property(program_ptr->program_ral,
                                             RAL_PROGRAM_PROPERTY_N_ATTACHED_SHADERS,
                                            &n_attached_shaders);

                    system_file_serializer_write(blob_file_serializer,
                                                 sizeof(n_attached_shaders),
                                                &n_attached_shaders);

                    for (unsigned int n_shader = 0;
                                      n_shader < n_attached_shaders;
                                    ++n_shader)
                    {
                        ral_shader                shader      = NULL;
                        system_hashed_ansi_string shader_body = NULL;
                        system_hash64             shader_hash = 0;

                        ral_program_get_attached_shader_at_index(program_ptr->program_ral,
                                                                 n_shader,
                                                                &shader);
                        ral_shader_get_property                 (shader,
                                                                 RAL_SHADER_PROPERTY_GLSL_BODY,
                                                                &shader_body);

                        shader_hash = system_hashed_ansi_string_get_hash(shader_body);

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
PRIVATE void _raGL_program_subscribe_for_shader_notifications(_raGL_program* program_ptr,
                                                              raGL_shader    shader,
                                                              bool           should_subscribe)
{
    system_callback_manager shader_callback_manager = NULL;

    raGL_shader_get_property(shader,
                             RAGL_SHADER_PROPERTY_CALLBACK_MANAGER,
                            &shader_callback_manager);

    if (should_subscribe)
    {
        system_callback_manager_subscribe_for_callbacks(shader_callback_manager,
                                                        RAGL_SHADER_CALLBACK_ID_SHADER_COMPILED,
                                                        CALLBACK_SYNCHRONICITY_SYNCHRONOUS,
                                                        _raGL_program_on_shader_compile_callback,
                                                        program_ptr);
    }
    else
    {
        system_callback_manager_unsubscribe_from_callbacks(shader_callback_manager,
                                                           RAGL_SHADER_CALLBACK_ID_SHADER_COMPILED,
                                                           _raGL_program_on_shader_compile_callback,
                                                           program_ptr);
    }
}

/** TODO */
PRIVATE void _raGL_program_save_shader_sources(_raGL_program* program_ptr)
{
    char*                     file_name    = _raGL_program_get_source_code_file_name(program_ptr);
    uint32_t                  n_shaders    = 0;
    system_hashed_ansi_string program_name = NULL;
    system_file_serializer    serializer   = system_file_serializer_create_for_writing(system_hashed_ansi_string_create(file_name) );

    ral_program_get_property(program_ptr->program_ral,
                             RAL_PROGRAM_PROPERTY_NAME,
                            &program_name);
    ral_program_get_property(program_ptr->program_ral,
                             RAL_PROGRAM_PROPERTY_N_ATTACHED_SHADERS,
                            &n_shaders);

    /* Store full program name */
    static const char* full_program_name = "Full program name: ";
    static const char* newline           = "\n";

    system_file_serializer_write(serializer,
                                 strlen(full_program_name),
                                 full_program_name);
    system_file_serializer_write(serializer,
                                 system_hashed_ansi_string_get_length(program_name),
                                 system_hashed_ansi_string_get_buffer(program_name) );
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
        ral_shader                current_shader = NULL;
        system_hashed_ansi_string shader_body    = NULL;
        ral_shader_type           shader_type    = RAL_SHADER_TYPE_UNKNOWN;

        if (!ral_program_get_attached_shader_at_index(program_ptr->program_ral,
                                                      n_shader,
                                                     &current_shader) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve shader at index [%d]",
                              n_shader);

            continue;
        }

        ral_shader_get_property(current_shader,
                                RAL_SHADER_PROPERTY_GLSL_BODY,
                               &shader_body);
        ral_shader_get_property(current_shader,
                                RAL_SHADER_PROPERTY_TYPE,
                               &shader_type);

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
PUBLIC bool raGL_program_attach_shader(raGL_program program,
                                       raGL_shader  shader)
{
    _raGL_program* program_ptr = (_raGL_program*) program;
    bool           result      = false;

    ASSERT_DEBUG_SYNC(program != NULL,
                      "Program is NULL - crash ahead.");
    ASSERT_DEBUG_SYNC(shader != NULL,
                      "Shader is NULL.");

    if (program_ptr->id != 0)
    {
        /* Request a rendering thread call-back, so that we can pass the request to the driver */
        _raGL_attach_shader_callback_argument callback_argument;

        callback_argument.program_ptr = program_ptr;
        callback_argument.shader      = shader;

        ogl_context_request_callback_from_context_thread(ral_context_get_gl_context(program_ptr->context),
                                                         _raGL_program_attach_shader_callback,
                                                        &callback_argument);

        /* Sign up for 'on shader compiled' notifications, so that we know when to re-link the program */
        _raGL_program_subscribe_for_shader_notifications(program_ptr,
                                                         shader,
                                                         true /* should_subscribe */);

        result = true;
    }
    else
    {
        ASSERT_DEBUG_SYNC(false,
                          "Program has a zero id! Cannot attach the specified shader.");
    }

    return result;
}

/** Please see header for specification */
PUBLIC raGL_program raGL_program_create(ral_context context,
                                        ral_program program_ral)
{
    _raGL_program* new_program_ptr = new (std::nothrow) _raGL_program;

    if (new_program_ptr != NULL)
    {
        new_program_ptr->active_attributes_raGL    = NULL;
        new_program_ptr->active_uniforms_raGL      = NULL;
        new_program_ptr->block_name_to_binding_map = system_hash64map_create(sizeof(_raGL_program_block_binding*) );
        new_program_ptr->context                   = context;
        new_program_ptr->id                        = 0;
        new_program_ptr->link_status               = false;
        new_program_ptr->n_ssb_bindings            = 0;
        new_program_ptr->n_ub_bindings             = 0;
        new_program_ptr->program_ral               = program_ral;
        new_program_ptr->ssb_bindings              = NULL;
        new_program_ptr->ub_bindings               = NULL;

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

            new_program_ptr->pGLAttachShader               = entry_points->pGLAttachShader;
            new_program_ptr->pGLCreateProgram              = entry_points->pGLCreateProgram;
            new_program_ptr->pGLDeleteProgram              = entry_points->pGLDeleteProgram;
            new_program_ptr->pGLDetachShader               = entry_points->pGLDetachShader;
            new_program_ptr->pGLGetActiveAttrib            = entry_points->pGLGetActiveAttrib;
            new_program_ptr->pGLGetProgramResourceiv       = entry_points->pGLGetProgramResourceiv;
            new_program_ptr->pGLGetAttribLocation          = entry_points->pGLGetAttribLocation;
            new_program_ptr->pGLGetProgramBinary           = entry_points->pGLGetProgramBinary;
            new_program_ptr->pGLGetProgramInfoLog          = entry_points->pGLGetProgramInfoLog;
            new_program_ptr->pGLGetProgramInterfaceiv      = entry_points->pGLGetProgramInterfaceiv;
            new_program_ptr->pGLGetProgramiv               = entry_points->pGLGetProgramiv;
            new_program_ptr->pGLGetProgramResourceiv       = entry_points->pGLGetProgramResourceiv;
            new_program_ptr->pGLGetProgramResourceLocation = entry_points->pGLGetProgramResourceLocation;
            new_program_ptr->pGLGetProgramResourceName     = entry_points->pGLGetProgramResourceName;
            new_program_ptr->pGLLinkProgram                = entry_points->pGLLinkProgram;
            new_program_ptr->pGLProgramBinary              = entry_points->pGLProgramBinary;
            new_program_ptr->pGLProgramParameteri          = entry_points->pGLProgramParameteri;
            new_program_ptr->pGLShaderStorageBlockBinding  = entry_points->pGLShaderStorageBlockBinding;
            new_program_ptr->pGLUniformBlockBinding        = entry_points->pGLUniformBlockBinding;
        }
        else
        {
            ASSERT_DEBUG_SYNC(backend_type == RAL_BACKEND_TYPE_GL,
                              "Unrecognized context type");

            const ogl_context_gl_entrypoints* entry_points = NULL;

            ogl_context_get_property(ral_context_get_gl_context(context),
                                     OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                                    &entry_points);

            new_program_ptr->pGLAttachShader               = entry_points->pGLAttachShader;
            new_program_ptr->pGLCreateProgram              = entry_points->pGLCreateProgram;
            new_program_ptr->pGLDeleteProgram              = entry_points->pGLDeleteProgram;
            new_program_ptr->pGLDetachShader               = entry_points->pGLDetachShader;
            new_program_ptr->pGLGetActiveAttrib            = entry_points->pGLGetActiveAttrib;
            new_program_ptr->pGLGetProgramResourceiv       = entry_points->pGLGetProgramResourceiv;
            new_program_ptr->pGLGetAttribLocation          = entry_points->pGLGetAttribLocation;
            new_program_ptr->pGLGetProgramBinary           = entry_points->pGLGetProgramBinary;
            new_program_ptr->pGLGetProgramInfoLog          = entry_points->pGLGetProgramInfoLog;
            new_program_ptr->pGLGetProgramInterfaceiv      = entry_points->pGLGetProgramInterfaceiv;
            new_program_ptr->pGLGetProgramiv               = entry_points->pGLGetProgramiv;
            new_program_ptr->pGLGetProgramResourceiv       = entry_points->pGLGetProgramResourceiv;
            new_program_ptr->pGLGetProgramResourceLocation = entry_points->pGLGetProgramResourceLocation;
            new_program_ptr->pGLGetProgramResourceName     = entry_points->pGLGetProgramResourceName;
            new_program_ptr->pGLLinkProgram                = entry_points->pGLLinkProgram;
            new_program_ptr->pGLProgramBinary              = entry_points->pGLProgramBinary;
            new_program_ptr->pGLProgramParameteri          = entry_points->pGLProgramParameteri;
            new_program_ptr->pGLShaderStorageBlockBinding  = entry_points->pGLShaderStorageBlockBinding;
            new_program_ptr->pGLUniformBlockBinding        = entry_points->pGLUniformBlockBinding;
        }

        /* Carry on */
        ogl_context_request_callback_from_context_thread(ral_context_get_gl_context(context),
                                                         _raGL_program_create_callback,
                                                         new_program_ptr);
    }

    return (raGL_program) new_program_ptr;
}

/** Please see header for specification */
PUBLIC void raGL_program_get_block_property(raGL_program                program,
                                            ral_program_block_type      block_type,
                                            uint32_t                    index,
                                            raGL_program_block_property property,
                                            void*                       out_result_ptr)
{
    _raGL_program* program_ptr = (_raGL_program*) program;

    if (program == NULL)
    {
        ASSERT_DEBUG_SYNC(program != NULL,
                          "Input raGL_program instance is NULL");

        goto end;
    }

    if (property != RAGL_PROGRAM_BLOCK_PROPERTY_INDEXED_BP)
    {
        ASSERT_DEBUG_SYNC(property == RAGL_PROGRAM_BLOCK_PROPERTY_INDEXED_BP,
                          "Invalid raGL_program_block_property arg value specified.");

        goto end;
    }

    switch (block_type)
    {
        case RAL_PROGRAM_BLOCK_TYPE_STORAGE_BUFFER:
        {
            if (index >= program_ptr->n_ssb_bindings)
            {
                ASSERT_DEBUG_SYNC(!(index >= program_ptr->n_ssb_bindings),
                                  "Invalid block index specified.");

                goto end;
            }

            *(uint32_t*) out_result_ptr = program_ptr->ssb_bindings[index].indexed_bp;

            break;
        }

        case RAL_PROGRAM_BLOCK_TYPE_UNIFORM_BUFFER:
        {
            if (index >= program_ptr->n_ub_bindings)
            {
                ASSERT_DEBUG_SYNC(!(index >= program_ptr->n_ub_bindings),
                                  "Invalid block index specified.");

                goto end;
            }

            *(uint32_t*) out_result_ptr = program_ptr->ub_bindings[index].indexed_bp;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unsupported ral_program_block_type argument value specified.");
        }
    } /* switch (block_type) */
end:
    ;
}

/** Please see header for specification */
PUBLIC EMERALD_API void raGL_program_get_property(raGL_program          program,
                                                  raGL_program_property property,
                                                  void*                 out_result_ptr)
{
    _raGL_program* program_ptr = (_raGL_program*) program;

    if (program_ptr == NULL)
    {
        ASSERT_DEBUG_SYNC(program_ptr != NULL,
                          "Input raGL_program instance is NULL");

        goto end;
    }

    if (out_result_ptr == NULL)
    {
        ASSERT_DEBUG_SYNC(out_result_ptr != NULL,
                          "Output variable is NULL");

        goto end;
    }

    switch (property)
    {
        case RAGL_PROGRAM_PROPERTY_ID:
        {
            *(GLuint*) out_result_ptr = program_ptr->id;

            break;
        }

        case RAGL_PROGRAM_PROPERTY_INFO_LOG:
        {
            ASSERT_DEBUG_SYNC(program_ptr->link_status,
                              "You cannot retrieve program info log without linking the program beforehand.");

             *(system_hashed_ansi_string*) out_result_ptr = program_ptr->program_info_log;

             break;
        }

        case RAGL_PROGRAM_PROPERTY_PARENT_RAL_PROGRAM:
        {
            *(ral_program*) out_result_ptr = program_ptr->program_ral;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized raGL_program_property value.");
        }
    } /* switch (property) */
end:
    ;
}

/** Please see header for specification */
PUBLIC EMERALD_API void raGL_program_get_block_property_by_name(raGL_program                program,
                                                                system_hashed_ansi_string   block_name,
                                                                raGL_program_block_property property,
                                                                void*                       out_result_ptr)
{
    _raGL_program_block_binding* binding_ptr = NULL;
    _raGL_program*               program_ptr = (_raGL_program*) program;

    if (program == NULL)
    {
        ASSERT_DEBUG_SYNC(program != NULL,
                          "Input raGL_program instance is NULL");

        goto end;
    }

    if (block_name                                       == NULL ||
        system_hashed_ansi_string_get_length(block_name) == 0)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input block name is NULL or of 0 length.");

        goto end;
    }

    if (property != RAGL_PROGRAM_BLOCK_PROPERTY_INDEXED_BP)
    {
        ASSERT_DEBUG_SYNC(property == RAGL_PROGRAM_BLOCK_PROPERTY_INDEXED_BP,
                          "Invalid raGL_program_block_property value passed.");

        goto end;
    }

    if (!system_hash64map_get(program_ptr->block_name_to_binding_map,
                              system_hashed_ansi_string_get_hash(block_name),
                             &binding_ptr) )
    {
        system_hashed_ansi_string program_name = NULL;

        ral_program_get_property(program_ptr->program_ral,
                                 RAL_PROGRAM_PROPERTY_NAME,
                                &program_name);

        ASSERT_DEBUG_SYNC(false,
                          "Block [%s] is not recognized for program [%s]",
                          system_hashed_ansi_string_get_buffer(block_name),
                          system_hashed_ansi_string_get_buffer(program_name) );

        goto end;
    }

    *(uint32_t*) out_result_ptr = binding_ptr->indexed_bp;

end:
    ;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool raGL_program_get_uniform_by_name(raGL_program                   program,
                                                         system_hashed_ansi_string      name,
                                                         const _raGL_program_variable** out_uniform_ptr)
{
    _raGL_program* program_ptr = (_raGL_program*) program;
    bool           result      = false;

    ASSERT_DEBUG_SYNC(program_ptr->link_status,
                      "You cannot retrieve an uniform descriptor without linking the program beforehand.");

    if (program_ptr->link_status)
    {
        unsigned int n_uniforms = 0;

        system_resizable_vector_get_property(program_ptr->active_uniforms_raGL,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                            &n_uniforms);

        for (unsigned int n_uniform = 0;
                          n_uniform < n_uniforms;
                        ++n_uniform)
        {
            _raGL_program_variable* uniform_ptr = NULL;

            result = system_resizable_vector_get_element_at(program_ptr->active_uniforms_raGL,
                                                            n_uniform,
                                                           &uniform_ptr);

            if (result                                                              &&
                system_hashed_ansi_string_is_equal_to_hash_string(uniform_ptr->name,
                                                                  name) )
            {
                *out_uniform_ptr = uniform_ptr;
                result           = true;

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
PUBLIC EMERALD_API bool raGL_program_get_vertex_attribute_by_name(raGL_program                    program,
                                                                  system_hashed_ansi_string       name,
                                                                  const _raGL_program_attribute** out_attribute_ptr)
{
    _raGL_program* program_ptr = (_raGL_program*) program;
    bool           result      = false;

    ASSERT_DEBUG_SYNC(program_ptr->link_status,
                      "You cannot retrieve an attribute descriptor without linking the program beforehand.");

    if (program_ptr->link_status)
    {
        unsigned int n_attributes = 0;

        system_resizable_vector_get_property(program_ptr->active_attributes_raGL,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                            &n_attributes);

        for (unsigned int n_attribute = 0;
                          n_attribute < n_attributes;
                        ++n_attribute)
        {
            _raGL_program_attribute* attribute_ptr = NULL;

            result = system_resizable_vector_get_element_at(program_ptr->active_attributes_raGL,
                                                            n_attribute,
                                                           &attribute_ptr);

            if (result                                                                &&
                system_hashed_ansi_string_is_equal_to_hash_string(attribute_ptr->name,
                                                                  name) )
            {
                *out_attribute_ptr = attribute_ptr;
                result             = true;

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
PUBLIC bool raGL_program_link(raGL_program program)
{
    _raGL_program* program_ptr        = (_raGL_program*) program;
    unsigned int   n_attached_shaders = 0;
    bool           result             = false;

    ral_program_get_property(program_ptr->program_ral,
                             RAL_PROGRAM_PROPERTY_N_ATTACHED_SHADERS,
                            &n_attached_shaders);

    ASSERT_DEBUG_SYNC(n_attached_shaders > 0,
                      "Linking will fail - no shaders attached.");

    if (n_attached_shaders > 0)
    {
        /* Clean up */
        _raGL_program_release_active_attributes(program_ptr->active_attributes_raGL);
        _raGL_program_release_active_uniforms  (program_ptr->active_uniforms_raGL);

        /* Run through all the attached shaders and make sure these are compiled.
        *
         * If not, compile them before proceeding with handling the actual linking request.
         */
        bool all_shaders_compiled = true;

        for (unsigned int n_shader = 0;
                          n_shader < n_attached_shaders;
                        ++n_shader)
        {
            ral_shader  current_shader           = NULL;
            raGL_shader current_shader_raGL      = NULL;
            GLuint      current_shader_raGL_id   = 0;
            bool        is_compiled_successfully = false;

            if (!ral_program_get_attached_shader_at_index(program_ptr->program_ral,
                                                          n_shader,
                                                         &current_shader) )
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Could not retrieve attached shader object at index [%d]",
                                  n_shader);

                continue;
            }

            current_shader_raGL = ral_context_get_shader_gl(program_ptr->context,
                                                            current_shader);

            raGL_shader_get_property(current_shader_raGL,
                                     RAGL_SHADER_PROPERTY_ID,
                                    &current_shader_raGL_id);
            raGL_shader_get_property(current_shader_raGL,
                                     RAGL_SHADER_PROPERTY_COMPILE_STATUS,
                                    &is_compiled_successfully);

            if (!is_compiled_successfully)
            {
                LOG_ERROR("Shader object [%u] has not been compiled successfully prior to linking. Attempting to compile.",
                          current_shader_raGL_id);

                raGL_shader_compile(current_shader_raGL);

                raGL_shader_get_property(current_shader_raGL,
                                         RAGL_SHADER_PROPERTY_COMPILE_STATUS,
                                        &is_compiled_successfully);
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
                                                             _raGL_program_link_callback,
                                                             program);

            result = program_ptr->link_status;
        }
    } /* if (n_attached_shaders > 0) */

    return result;
}

/** Please see header for specification */
PUBLIC void raGL_program_release(raGL_program program)
{
    ogl_context    context_gl  = NULL;
    _raGL_program* program_ptr = (_raGL_program*) program;

    ral_context_get_property(program_ptr->context,
                             RAL_CONTEXT_PROPERTY_BACKEND_CONTEXT,
                            &context_gl);

    ogl_context_request_callback_from_context_thread(context_gl,
                                                     _raGL_program_release_callback,
                                                     program_ptr);

    /* Iterate over all attached shaders and sign out of notifications */
    uint32_t n_shaders_attached = 0;

    ral_program_get_property(program_ptr->program_ral,
                             RAL_PROGRAM_PROPERTY_N_ATTACHED_SHADERS,
                            &n_shaders_attached);

    for (uint32_t n_attached_shader = 0;
                  n_attached_shader < n_shaders_attached;
                ++n_attached_shader)
    {
        ral_shader  shader      = NULL;
        raGL_shader shader_raGL = NULL;

        ral_program_get_attached_shader_at_index(program_ptr->program_ral,
                                                 n_attached_shader,
                                                &shader);

        shader_raGL = ral_context_get_shader_gl(program_ptr->context,
                                                shader);

        _raGL_program_subscribe_for_shader_notifications(program_ptr,
                                                         shader_raGL,
                                                         false /* should_subscribe */);
    }

    /* Proceed with actual object destruction */
    delete (_raGL_program*) program;
}

/** Please see header for specification */
PUBLIC void raGL_program_set_block_property(raGL_program                program,
                                            ral_program_block_type      block_type,
                                            uint32_t                    index,
                                            raGL_program_block_property property,
                                            const void*                 value_ptr)
{
    _raGL_program* program_ptr = (_raGL_program*) program;

    if (program == NULL)
    {
        ASSERT_DEBUG_SYNC(program != NULL,
                          "Input raGL_program instance is NULL");

        goto end;
    }

    if (property != RAGL_PROGRAM_BLOCK_PROPERTY_INDEXED_BP)
    {
        ASSERT_DEBUG_SYNC(property == RAGL_PROGRAM_BLOCK_PROPERTY_INDEXED_BP,
                          "Invalid raGL_program_block_property arg value specified.");

        goto end;
    }

    switch (block_type)
    {
        case RAL_PROGRAM_BLOCK_TYPE_STORAGE_BUFFER:
        {
            if (index >= program_ptr->n_ssb_bindings)
            {
                ASSERT_DEBUG_SYNC(!(index >= program_ptr->n_ssb_bindings),
                                  "Invalid block index specified.");

                goto end;
            }

            raGL_program_set_block_property_by_name(program,
                                                    program_ptr->ssb_bindings[index].block_name,
                                                    property,
                                                    value_ptr);

            break;
        }

        case RAL_PROGRAM_BLOCK_TYPE_UNIFORM_BUFFER:
        {
            if (index >= program_ptr->n_ub_bindings)
            {
                ASSERT_DEBUG_SYNC(!(index >= program_ptr->n_ub_bindings),
                                  "Invalid block index specified.");

                goto end;
            }

            raGL_program_set_block_property_by_name(program,
                                                    program_ptr->ub_bindings[index].block_name,
                                                    property,
                                                    value_ptr);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unsupported ral_program_block_type argument value specified.");
        }
    } /* switch (block_type) */
end:
    ;
}

/** Please see header for specification */
PUBLIC RENDERING_CONTEXT_CALL void raGL_program_set_block_property_by_name(raGL_program                program,
                                                                           system_hashed_ansi_string   block_name,
                                                                           raGL_program_block_property property,
                                                                           const void*                 value_ptr)
{
    _raGL_program_block_binding* binding_ptr = NULL;
    _raGL_program*               program_ptr = (_raGL_program*) program;

    if (program == NULL)
    {
        ASSERT_DEBUG_SYNC(program != NULL,
                          "Input raGL_program instance is NULL");

        goto end;
    }

    if (property != RAGL_PROGRAM_BLOCK_PROPERTY_INDEXED_BP)
    {
        ASSERT_DEBUG_SYNC(property == RAGL_PROGRAM_BLOCK_PROPERTY_INDEXED_BP,
                          "Invalid raGL_program_block_property arg value specified.");

        goto end;
    }

    if (!system_hash64map_get(program_ptr->block_name_to_binding_map,
                              system_hashed_ansi_string_get_hash(block_name),
                             &binding_ptr) )
    {
        system_hashed_ansi_string program_name = NULL;

        ral_program_get_property(program_ptr->program_ral,
                                 RAL_PROGRAM_PROPERTY_NAME,
                                &program_name);

        ASSERT_DEBUG_SYNC(false,
                          "Program [%s] does not have a block [%s] defined.",
                          system_hashed_ansi_string_get_buffer(program_name),
                          system_hashed_ansi_string_get_buffer(block_name) );

        goto end;
    }

    if (binding_ptr->indexed_bp != *(uint32_t*) value_ptr)
    {
        ogl_context                 current_context = ogl_context_get_current_context();
        ogl_context_gl_entrypoints* entrypoints_ptr = NULL;

        binding_ptr->indexed_bp = *(uint32_t*) value_ptr;

        ogl_context_get_property(current_context,
                                 OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                                &entrypoints_ptr);

        if (binding_ptr->block_type == RAL_PROGRAM_BLOCK_TYPE_STORAGE_BUFFER)
        {
            entrypoints_ptr->pGLShaderStorageBlockBinding(program_ptr->id,
                                                          binding_ptr->block_index,
                                                          binding_ptr->indexed_bp);
        }
        else
        {
            ASSERT_DEBUG_SYNC(binding_ptr->block_type == RAL_PROGRAM_BLOCK_TYPE_UNIFORM_BUFFER,
                              "Unrecognized block type encountered");

            entrypoints_ptr->pGLUniformBlockBinding(program_ptr->id,
                                                    binding_ptr->block_index,
                                                    binding_ptr->indexed_bp);
        }
    }
end:
    ;
}