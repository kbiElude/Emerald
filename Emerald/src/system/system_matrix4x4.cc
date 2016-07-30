/**
 *
 * Emerald (kbi/elude @2012-2016)
 *
 */
#include "shared.h"
#include "system/system_assertions.h"
#include "system/system_constants.h"
#include "system/system_math_vector.h"
#include "system/system_matrix4x4.h"
#include "system/system_resource_pool.h"
#include <math.h>
#include <string.h>

#define WORD_INDEX(col, row)        ((row<<2)+col)
#define WORD_INDEX_FROM_1(col, row) WORD_INDEX(col-1, row-1)

#define COL_FROM_ELEMENT_INDEX(element_index) (element_index & 0x3)
#define ROW_FROM_ELEMENT_INDEX(element_index) ((element_index & 0xC) >> 2)

const float pi = 3.141592653589793238462f;


/** Internal type defintions **/
/** Structure describing a signle 4x4 matrix instance. Data is the main point of reference,
 *  column_major_data is filled with proper values only when necesasry.
 */
typedef struct
{
    /* Row-major ordered data */
    float data[16];

    /* Column-major ordered data */
    float column_major_data[16];

    /* True if column major data must be generated */
    bool is_data_dirty;

} _system_matrix4x4;

/*** Internal variables **/
/** Resource pool storing 4x4 matrix instances */
system_resource_pool matrix_pool = nullptr;

/** Internal functions **/
/** Function that generates column-major ordered data from row-major data for a given 4x4 matrix descriptor.
 *
 *  @param descriptor Descriptor to use for the process.
 */
PRIVATE void _system_matrix4x4_generate_column_major_data(_system_matrix4x4* matrix_ptr)
{
    ASSERT_DEBUG_SYNC(matrix_ptr->is_data_dirty,
                      "Invalid call detected");

    matrix_ptr->column_major_data[0]  = matrix_ptr->data[0];
    matrix_ptr->column_major_data[4]  = matrix_ptr->data[1];
    matrix_ptr->column_major_data[8]  = matrix_ptr->data[2];
    matrix_ptr->column_major_data[12] = matrix_ptr->data[3];
    matrix_ptr->column_major_data[1]  = matrix_ptr->data[4];
    matrix_ptr->column_major_data[5]  = matrix_ptr->data[5];
    matrix_ptr->column_major_data[9]  = matrix_ptr->data[6];
    matrix_ptr->column_major_data[13] = matrix_ptr->data[7];
    matrix_ptr->column_major_data[2]  = matrix_ptr->data[8];
    matrix_ptr->column_major_data[6]  = matrix_ptr->data[9];
    matrix_ptr->column_major_data[10] = matrix_ptr->data[10];
    matrix_ptr->column_major_data[14] = matrix_ptr->data[11];
    matrix_ptr->column_major_data[3]  = matrix_ptr->data[12];
    matrix_ptr->column_major_data[7]  = matrix_ptr->data[13];
    matrix_ptr->column_major_data[11] = matrix_ptr->data[14];
    matrix_ptr->column_major_data[15] = matrix_ptr->data[15];
}

/** Please see header for specification */
PUBLIC EMERALD_API system_matrix4x4 system_matrix4x4_create()
{
    system_matrix4x4 result = (system_matrix4x4) system_resource_pool_get_from_pool(matrix_pool);

    memset(reinterpret_cast<_system_matrix4x4*>(result)->column_major_data,
           0,
           sizeof(float) * 16);
    memset(reinterpret_cast<_system_matrix4x4*>(result)->data,
           0,
           sizeof(float) * 16);

    reinterpret_cast<_system_matrix4x4*>(result)->is_data_dirty = false;

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API system_matrix4x4 system_matrix4x4_create_copy(system_matrix4x4 src_matrix)
{
    _system_matrix4x4* new_matrix_ptr = reinterpret_cast<_system_matrix4x4*>(system_resource_pool_get_from_pool(matrix_pool) );
    _system_matrix4x4* src_matrix_ptr = reinterpret_cast<_system_matrix4x4*>(src_matrix);

    memcpy(new_matrix_ptr,
           src_matrix_ptr,
           sizeof(*new_matrix_ptr) );

    return (system_matrix4x4) new_matrix_ptr;
}

/** Please see header for specification */
PUBLIC EMERALD_API system_matrix4x4 system_matrix4x4_create_lookat_matrix(const float* camera_location_ptr,
                                                                          const float* look_at_point_ptr,
                                                                          const float* base_up_vector_ptr)
{
    float              direction_vector[3];
    system_matrix4x4   new_matrix          = system_matrix4x4_create();
    _system_matrix4x4* new_matrix_ptr      = reinterpret_cast<_system_matrix4x4*>(new_matrix);
    float              right_vector    [3];
    float              up_vector       [3];

    /* Calculate direction vector */
    system_math_vector_minus3    (look_at_point_ptr,
                                  camera_location_ptr,
                                  direction_vector);
    system_math_vector_normalize3(direction_vector,
                                  direction_vector);

    /* Calculate right vector (f x up)*/
    system_math_vector_cross3    (base_up_vector_ptr,
                                  direction_vector,
                                  right_vector);
    system_math_vector_normalize3(right_vector,
                                  right_vector);

    /* Calculate up vector (dir x right) */
    system_math_vector_cross3(direction_vector,
                              right_vector,
                              up_vector);

    /* Construct the result matrix (D3DXMatrixLookAtRH () ) */
    new_matrix_ptr->data[0]       = right_vector[0];
    new_matrix_ptr->data[1]       = right_vector[1];
    new_matrix_ptr->data[2]       = right_vector[2];
    new_matrix_ptr->data[3]       = system_math_vector_dot3(camera_location_ptr,
                                                            right_vector); /* translation factor */
    new_matrix_ptr->data[4]       = up_vector[0];
    new_matrix_ptr->data[5]       = up_vector[1];
    new_matrix_ptr->data[6]       = up_vector[2];
    new_matrix_ptr->data[7]       = system_math_vector_dot3(camera_location_ptr,
                                                            up_vector); /* translation factor */
    new_matrix_ptr->data[8]       = direction_vector[0];
    new_matrix_ptr->data[9]       = direction_vector[1];
    new_matrix_ptr->data[10]      = direction_vector[2];
    new_matrix_ptr->data[11]      = system_math_vector_dot3(camera_location_ptr,
                                                            direction_vector); /* translation factor */
    new_matrix_ptr->data[12]      = 0.0f;
    new_matrix_ptr->data[13]      = 0.0f;
    new_matrix_ptr->data[14]      = 0.0f;
    new_matrix_ptr->data[15]      = 1.0f;
    new_matrix_ptr->is_data_dirty = true;

    return new_matrix;
}

/** Please see header for specification */
PUBLIC EMERALD_API system_matrix4x4 system_matrix4x4_create_ortho_projection_matrix(float left,
                                                                                    float right,
                                                                                    float bottom,
                                                                                    float top,
                                                                                    float z_near,
                                                                                    float z_far)
{
    system_matrix4x4   new_matrix     = system_matrix4x4_create();
    _system_matrix4x4* new_matrix_ptr = reinterpret_cast<_system_matrix4x4*>(new_matrix);

    ASSERT_DEBUG_SYNC(left < right,
                      "Sanity check failed");
    ASSERT_DEBUG_SYNC(bottom < top,
                      "Sanity check failed");
    ASSERT_DEBUG_SYNC(z_near < z_far, 
                      "Sanity check failed");

    float a = 2.0f / (right  - left);
    float b = 2.0f / (top    - bottom);

    /* D3DXMatrixOrthoOffCenterRH () */
    new_matrix_ptr->data[0 ]      = a;
    new_matrix_ptr->data[1 ]      = 0;
    new_matrix_ptr->data[2 ]      = 0;
    new_matrix_ptr->data[3 ]      = (left + right) / (left - right);

    new_matrix_ptr->data[4 ]      = 0;
    new_matrix_ptr->data[5 ]      = b;
    new_matrix_ptr->data[6 ]      = 0;
    new_matrix_ptr->data[7 ]      = (top + bottom) / (bottom - top);

    new_matrix_ptr->data[8 ]      = 0;
    new_matrix_ptr->data[9 ]      = 0;
    new_matrix_ptr->data[10]      = 1.0f   / (z_near - z_far);
    new_matrix_ptr->data[11]      = z_near / (z_near - z_far);

    new_matrix_ptr->data[12]      = 0;
    new_matrix_ptr->data[13]      = 0;
    new_matrix_ptr->data[14]      = 0.0f;
    new_matrix_ptr->data[15]      = 1;
    new_matrix_ptr->is_data_dirty = true;

    return new_matrix;
}

/** Please see header for specification */
PUBLIC EMERALD_API system_matrix4x4 system_matrix4x4_create_perspective_projection_matrix(float fov_y,
                                                                                          float ar,
                                                                                          float z_near,
                                                                                          float z_far)
{
    system_matrix4x4   new_matrix     = system_matrix4x4_create();
    _system_matrix4x4* new_matrix_ptr = reinterpret_cast<_system_matrix4x4*>(new_matrix);

    /* D3DXMatrixPerspectiveFovRH() */
#if 0
    ASSERT_DEBUG_SYNC(z_near > 0, "Near Z must be positive");

    float f = tan(pi / 2.0f - fov_y / 2.0f);

    new_matrix_ptr->data[0]  = f / ar;
    new_matrix_ptr->data[1]  = 0;
    new_matrix_ptr->data[2]  = 0;
    new_matrix_ptr->data[3]  = 0;

    new_matrix_ptr->data[4]  = 0;
    new_matrix_ptr->data[5]  = f;
    new_matrix_ptr->data[6]  = 0;
    new_matrix_ptr->data[7]  = 0;

    new_matrix_ptr->data[8]  = 0;
    new_matrix_ptr->data[9]  = 0;
    new_matrix_ptr->data[10] = (z_far + z_near)   / (z_near - z_far);
    new_matrix_ptr->data[11] = 2 * z_far * z_near / (z_near - z_far);

    new_matrix_ptr->data[12] = 0;
    new_matrix_ptr->data[13] = 0;
    new_matrix_ptr->data[14] = -1;
    new_matrix_ptr->data[15] = 0;
#else
    float h = 1.0f / tan(fov_y / 2.0f);
    float w = h / ar;

    new_matrix_ptr->data[0 ] = w;
    new_matrix_ptr->data[1 ] = 0;
    new_matrix_ptr->data[2 ] = 0;
    new_matrix_ptr->data[3 ] = 0;
    new_matrix_ptr->data[4 ] = 0;
    new_matrix_ptr->data[5 ] = h;
    new_matrix_ptr->data[6 ] = 0;
    new_matrix_ptr->data[7 ] = 0;
    new_matrix_ptr->data[8 ] = 0;
    new_matrix_ptr->data[9 ] = 0;
    new_matrix_ptr->data[10] =          z_far / (z_near - z_far);
    new_matrix_ptr->data[11] = z_near * z_far / (z_near - z_far);
    new_matrix_ptr->data[12] = 0;
    new_matrix_ptr->data[13] = 0;
    new_matrix_ptr->data[14] = -1.0f;
    new_matrix_ptr->data[15] = 0;
#endif

    new_matrix_ptr->is_data_dirty = true;

    return new_matrix;
}

/** Please see header for specification */
PUBLIC EMERALD_API system_matrix4x4 system_matrix4x4_create_perspective_projection_matrix_custom(float left,
                                                                                                 float right,
                                                                                                 float bottom,
                                                                                                 float top,
                                                                                                 float z_near,
                                                                                                 float z_far)
{
    system_matrix4x4   new_matrix     = system_matrix4x4_create();
    _system_matrix4x4* new_matrix_ptr = reinterpret_cast<_system_matrix4x4*>(new_matrix);

    ASSERT_DEBUG_SYNC(left < right,
                      "Sanity check failed");
    ASSERT_DEBUG_SYNC(bottom < top,
                      "Sanity check failed");
    ASSERT_DEBUG_SYNC(z_near < z_far,
                      "Sanity check failed");

    /* D3DXMatrixPerspectiveOffCenterRH() */
    new_matrix_ptr->data[0]       =  2.0f * z_near / (right - left);
    new_matrix_ptr->data[1]       =  0.0f;
    new_matrix_ptr->data[2]       =  (left + right) / (right - left);
    new_matrix_ptr->data[3]       =  0.0f;
    new_matrix_ptr->data[4]       =  0.0f;
    new_matrix_ptr->data[5]       =  2.0f * z_near  / (top - bottom);
    new_matrix_ptr->data[6]       =  (top + bottom) / (top - bottom);
    new_matrix_ptr->data[7]       =  0.0f;
    new_matrix_ptr->data[8]       =  0.0f;
    new_matrix_ptr->data[9]       =  0.0f;
    new_matrix_ptr->data[10]      =           z_far / (z_near - z_far);
    new_matrix_ptr->data[11]      =  z_near * z_far / (z_near - z_far);
    new_matrix_ptr->data[12]      =  0.0f;
    new_matrix_ptr->data[13]      =  0.0f;
    new_matrix_ptr->data[14]      = -1.0f;
    new_matrix_ptr->data[15]      =  0.0f;
    new_matrix_ptr->is_data_dirty = true;

    return new_matrix;
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_matrix4x4_release(system_matrix4x4 matrix)
{
    system_resource_pool_return_to_pool(matrix_pool,
                                        (system_resource_pool_block) matrix);
}

/** Please see header for specification */
PUBLIC EMERALD_API const float* system_matrix4x4_get_column_major_data(system_matrix4x4 matrix)
{
    _system_matrix4x4* matrix_ptr = reinterpret_cast<_system_matrix4x4*>(matrix);

    if (matrix_ptr->is_data_dirty)
    {
        _system_matrix4x4_generate_column_major_data(matrix_ptr);

        matrix_ptr->is_data_dirty = false;
    }

    return matrix_ptr->column_major_data;
}

/** Please see header for specification */
PUBLIC EMERALD_API const float* system_matrix4x4_get_row_major_data(system_matrix4x4 matrix)
{
    return reinterpret_cast<_system_matrix4x4*>(matrix)->data;
}

/** Please see header for specification */
PUBLIC EMERALD_API system_matrix4x4 system_matrix4x4_create_by_mul(system_matrix4x4 mat_a,
                                                                   system_matrix4x4 mat_b)
{
    system_matrix4x4   result     = system_matrix4x4_create();
    _system_matrix4x4* mat_a_ptr  = reinterpret_cast<_system_matrix4x4*>(mat_a);
    _system_matrix4x4* mat_b_ptr  = reinterpret_cast<_system_matrix4x4*>(mat_b);
    _system_matrix4x4* result_ptr = reinterpret_cast<_system_matrix4x4*>(result);

    for (unsigned char column = 0;
                       column < 4;
                     ++column)
    {
        for (unsigned char row = 0;
                           row < 4;
                         ++row)
        {
            result_ptr->data[WORD_INDEX(column, row)] = 
             mat_a_ptr->data[WORD_INDEX(0,      row)] * mat_b_ptr->data[WORD_INDEX(column, 0)] +
             mat_a_ptr->data[WORD_INDEX(1,      row)] * mat_b_ptr->data[WORD_INDEX(column, 1)] +
             mat_a_ptr->data[WORD_INDEX(2,      row)] * mat_b_ptr->data[WORD_INDEX(column, 2)] +
             mat_a_ptr->data[WORD_INDEX(3,      row)] * mat_b_ptr->data[WORD_INDEX(column, 3)];
        }
    }

    result_ptr->is_data_dirty = true;

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool system_matrix4x4_invert(system_matrix4x4 matrix)
{
    // Naive implementatino from http://www.euclideanspace.com/maths/algebra/matrix/functions/inverse/fourD/index.htm 
    float              m[16];
    _system_matrix4x4* matrix_ptr = reinterpret_cast<_system_matrix4x4*>(matrix);
    float              det        = matrix_ptr->data[WORD_INDEX(0,3)] * matrix_ptr->data[WORD_INDEX(1,2)] * matrix_ptr->data[WORD_INDEX(2,1)] * matrix_ptr->data[WORD_INDEX(3,0)] -
                                    matrix_ptr->data[WORD_INDEX(0,2)] * matrix_ptr->data[WORD_INDEX(1,3)] * matrix_ptr->data[WORD_INDEX(2,1)] * matrix_ptr->data[WORD_INDEX(3,0)] -
                                    matrix_ptr->data[WORD_INDEX(0,3)] * matrix_ptr->data[WORD_INDEX(1,1)] * matrix_ptr->data[WORD_INDEX(2,2)] * matrix_ptr->data[WORD_INDEX(3,0)] +
                                    matrix_ptr->data[WORD_INDEX(0,1)] * matrix_ptr->data[WORD_INDEX(1,3)] * matrix_ptr->data[WORD_INDEX(2,2)] * matrix_ptr->data[WORD_INDEX(3,0)] +
                                    matrix_ptr->data[WORD_INDEX(0,2)] * matrix_ptr->data[WORD_INDEX(1,1)] * matrix_ptr->data[WORD_INDEX(2,3)] * matrix_ptr->data[WORD_INDEX(3,0)] -
                                    matrix_ptr->data[WORD_INDEX(0,1)] * matrix_ptr->data[WORD_INDEX(1,2)] * matrix_ptr->data[WORD_INDEX(2,3)] * matrix_ptr->data[WORD_INDEX(3,0)] -
                                    matrix_ptr->data[WORD_INDEX(0,3)] * matrix_ptr->data[WORD_INDEX(1,2)] * matrix_ptr->data[WORD_INDEX(2,0)] * matrix_ptr->data[WORD_INDEX(3,1)] +
                                    matrix_ptr->data[WORD_INDEX(0,2)] * matrix_ptr->data[WORD_INDEX(1,3)] * matrix_ptr->data[WORD_INDEX(2,0)] * matrix_ptr->data[WORD_INDEX(3,1)] +
                                    matrix_ptr->data[WORD_INDEX(0,3)] * matrix_ptr->data[WORD_INDEX(1,0)] * matrix_ptr->data[WORD_INDEX(2,2)] * matrix_ptr->data[WORD_INDEX(3,1)] -
                                    matrix_ptr->data[WORD_INDEX(0,0)] * matrix_ptr->data[WORD_INDEX(1,3)] * matrix_ptr->data[WORD_INDEX(2,2)] * matrix_ptr->data[WORD_INDEX(3,1)] -
                                    matrix_ptr->data[WORD_INDEX(0,2)] * matrix_ptr->data[WORD_INDEX(1,0)] * matrix_ptr->data[WORD_INDEX(2,3)] * matrix_ptr->data[WORD_INDEX(3,1)] +
                                    matrix_ptr->data[WORD_INDEX(0,0)] * matrix_ptr->data[WORD_INDEX(1,2)] * matrix_ptr->data[WORD_INDEX(2,3)] * matrix_ptr->data[WORD_INDEX(3,1)] +
                                    matrix_ptr->data[WORD_INDEX(0,3)] * matrix_ptr->data[WORD_INDEX(1,1)] * matrix_ptr->data[WORD_INDEX(2,0)] * matrix_ptr->data[WORD_INDEX(3,2)] -
                                    matrix_ptr->data[WORD_INDEX(0,1)] * matrix_ptr->data[WORD_INDEX(1,3)] * matrix_ptr->data[WORD_INDEX(2,0)] * matrix_ptr->data[WORD_INDEX(3,2)] -
                                    matrix_ptr->data[WORD_INDEX(0,3)] * matrix_ptr->data[WORD_INDEX(1,0)] * matrix_ptr->data[WORD_INDEX(2,1)] * matrix_ptr->data[WORD_INDEX(3,2)] +
                                    matrix_ptr->data[WORD_INDEX(0,0)] * matrix_ptr->data[WORD_INDEX(1,3)] * matrix_ptr->data[WORD_INDEX(2,1)] * matrix_ptr->data[WORD_INDEX(3,2)] +
                                    matrix_ptr->data[WORD_INDEX(0,1)] * matrix_ptr->data[WORD_INDEX(1,0)] * matrix_ptr->data[WORD_INDEX(2,3)] * matrix_ptr->data[WORD_INDEX(3,2)] -
                                    matrix_ptr->data[WORD_INDEX(0,0)] * matrix_ptr->data[WORD_INDEX(1,1)] * matrix_ptr->data[WORD_INDEX(2,3)] * matrix_ptr->data[WORD_INDEX(3,2)] -
                                    matrix_ptr->data[WORD_INDEX(0,2)] * matrix_ptr->data[WORD_INDEX(1,1)] * matrix_ptr->data[WORD_INDEX(2,0)] * matrix_ptr->data[WORD_INDEX(3,3)] +
                                    matrix_ptr->data[WORD_INDEX(0,1)] * matrix_ptr->data[WORD_INDEX(1,2)] * matrix_ptr->data[WORD_INDEX(2,0)] * matrix_ptr->data[WORD_INDEX(3,3)] +
                                    matrix_ptr->data[WORD_INDEX(0,2)] * matrix_ptr->data[WORD_INDEX(1,0)] * matrix_ptr->data[WORD_INDEX(2,1)] * matrix_ptr->data[WORD_INDEX(3,3)] -
                                    matrix_ptr->data[WORD_INDEX(0,0)] * matrix_ptr->data[WORD_INDEX(1,2)] * matrix_ptr->data[WORD_INDEX(2,1)] * matrix_ptr->data[WORD_INDEX(3,3)] -
                                    matrix_ptr->data[WORD_INDEX(0,1)] * matrix_ptr->data[WORD_INDEX(1,0)] * matrix_ptr->data[WORD_INDEX(2,2)] * matrix_ptr->data[WORD_INDEX(3,3)] +
                                    matrix_ptr->data[WORD_INDEX(0,0)] * matrix_ptr->data[WORD_INDEX(1,1)] * matrix_ptr->data[WORD_INDEX(2,2)] * matrix_ptr->data[WORD_INDEX(3,3)];

    if (fabs(det) <= 0.0000001f)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Determinant is very close to 0, this matrix is most likely non-invertable.");

        return false;
    }

    m[WORD_INDEX(0, 0)] = matrix_ptr->data[WORD_INDEX(1,2)] * matrix_ptr->data[WORD_INDEX(2,3)] * matrix_ptr->data[WORD_INDEX(3,1)] -
                          matrix_ptr->data[WORD_INDEX(1,3)] * matrix_ptr->data[WORD_INDEX(2,2)] * matrix_ptr->data[WORD_INDEX(3,1)] +
                          matrix_ptr->data[WORD_INDEX(1,3)] * matrix_ptr->data[WORD_INDEX(2,1)] * matrix_ptr->data[WORD_INDEX(3,2)] -
                          matrix_ptr->data[WORD_INDEX(1,1)] * matrix_ptr->data[WORD_INDEX(2,3)] * matrix_ptr->data[WORD_INDEX(3,2)] -
                          matrix_ptr->data[WORD_INDEX(1,2)] * matrix_ptr->data[WORD_INDEX(2,1)] * matrix_ptr->data[WORD_INDEX(3,3)] +
                          matrix_ptr->data[WORD_INDEX(1,1)] * matrix_ptr->data[WORD_INDEX(2,2)] * matrix_ptr->data[WORD_INDEX(3,3)];
    m[WORD_INDEX(0, 1)] = matrix_ptr->data[WORD_INDEX(0,3)] * matrix_ptr->data[WORD_INDEX(2,2)] * matrix_ptr->data[WORD_INDEX(3,1)] -
                          matrix_ptr->data[WORD_INDEX(0,2)] * matrix_ptr->data[WORD_INDEX(2,3)] * matrix_ptr->data[WORD_INDEX(3,1)] -
                          matrix_ptr->data[WORD_INDEX(0,3)] * matrix_ptr->data[WORD_INDEX(2,1)] * matrix_ptr->data[WORD_INDEX(3,2)] +
                          matrix_ptr->data[WORD_INDEX(0,1)] * matrix_ptr->data[WORD_INDEX(2,3)] * matrix_ptr->data[WORD_INDEX(3,2)] +
                          matrix_ptr->data[WORD_INDEX(0,2)] * matrix_ptr->data[WORD_INDEX(2,1)] * matrix_ptr->data[WORD_INDEX(3,3)] -
                          matrix_ptr->data[WORD_INDEX(0,1)] * matrix_ptr->data[WORD_INDEX(2,2)] * matrix_ptr->data[WORD_INDEX(3,3)];
    m[WORD_INDEX(0, 2)] = matrix_ptr->data[WORD_INDEX(0,2)] * matrix_ptr->data[WORD_INDEX(1,3)] * matrix_ptr->data[WORD_INDEX(3,1)] -
                          matrix_ptr->data[WORD_INDEX(0,3)] * matrix_ptr->data[WORD_INDEX(1,2)] * matrix_ptr->data[WORD_INDEX(3,1)] +
                          matrix_ptr->data[WORD_INDEX(0,3)] * matrix_ptr->data[WORD_INDEX(1,1)] * matrix_ptr->data[WORD_INDEX(3,2)] -
                          matrix_ptr->data[WORD_INDEX(0,1)] * matrix_ptr->data[WORD_INDEX(1,3)] * matrix_ptr->data[WORD_INDEX(3,2)] -
                          matrix_ptr->data[WORD_INDEX(0,2)] * matrix_ptr->data[WORD_INDEX(1,1)] * matrix_ptr->data[WORD_INDEX(3,3)] +
                          matrix_ptr->data[WORD_INDEX(0,1)] * matrix_ptr->data[WORD_INDEX(1,2)] * matrix_ptr->data[WORD_INDEX(3,3)];
    m[WORD_INDEX(0, 3)] = matrix_ptr->data[WORD_INDEX(0,3)] * matrix_ptr->data[WORD_INDEX(1,2)] * matrix_ptr->data[WORD_INDEX(2,1)] -
                          matrix_ptr->data[WORD_INDEX(0,2)] * matrix_ptr->data[WORD_INDEX(1,3)] * matrix_ptr->data[WORD_INDEX(2,1)] -
                          matrix_ptr->data[WORD_INDEX(0,3)] * matrix_ptr->data[WORD_INDEX(1,1)] * matrix_ptr->data[WORD_INDEX(2,2)] +
                          matrix_ptr->data[WORD_INDEX(0,1)] * matrix_ptr->data[WORD_INDEX(1,3)] * matrix_ptr->data[WORD_INDEX(2,2)] +
                          matrix_ptr->data[WORD_INDEX(0,2)] * matrix_ptr->data[WORD_INDEX(1,1)] * matrix_ptr->data[WORD_INDEX(2,3)] -
                          matrix_ptr->data[WORD_INDEX(0,1)] * matrix_ptr->data[WORD_INDEX(1,2)] * matrix_ptr->data[WORD_INDEX(2,3)];
    m[WORD_INDEX(1, 0)] = matrix_ptr->data[WORD_INDEX(1,3)] * matrix_ptr->data[WORD_INDEX(2,2)] * matrix_ptr->data[WORD_INDEX(3,0)] -
                          matrix_ptr->data[WORD_INDEX(1,2)] * matrix_ptr->data[WORD_INDEX(2,3)] * matrix_ptr->data[WORD_INDEX(3,0)] -
                          matrix_ptr->data[WORD_INDEX(1,3)] * matrix_ptr->data[WORD_INDEX(2,0)] * matrix_ptr->data[WORD_INDEX(3,2)] +
                          matrix_ptr->data[WORD_INDEX(1,0)] * matrix_ptr->data[WORD_INDEX(2,3)] * matrix_ptr->data[WORD_INDEX(3,2)] +
                          matrix_ptr->data[WORD_INDEX(1,2)] * matrix_ptr->data[WORD_INDEX(2,0)] * matrix_ptr->data[WORD_INDEX(3,3)] -
                          matrix_ptr->data[WORD_INDEX(1,0)] * matrix_ptr->data[WORD_INDEX(2,2)] * matrix_ptr->data[WORD_INDEX(3,3)];
    m[WORD_INDEX(1, 1)] = matrix_ptr->data[WORD_INDEX(0,2)] * matrix_ptr->data[WORD_INDEX(2,3)] * matrix_ptr->data[WORD_INDEX(3,0)] -
                          matrix_ptr->data[WORD_INDEX(0,3)] * matrix_ptr->data[WORD_INDEX(2,2)] * matrix_ptr->data[WORD_INDEX(3,0)] +
                          matrix_ptr->data[WORD_INDEX(0,3)] * matrix_ptr->data[WORD_INDEX(2,0)] * matrix_ptr->data[WORD_INDEX(3,2)] -
                          matrix_ptr->data[WORD_INDEX(0,0)] * matrix_ptr->data[WORD_INDEX(2,3)] * matrix_ptr->data[WORD_INDEX(3,2)] -
                          matrix_ptr->data[WORD_INDEX(0,2)] * matrix_ptr->data[WORD_INDEX(2,0)] * matrix_ptr->data[WORD_INDEX(3,3)] +
                          matrix_ptr->data[WORD_INDEX(0,0)] * matrix_ptr->data[WORD_INDEX(2,2)] * matrix_ptr->data[WORD_INDEX(3,3)];
    m[WORD_INDEX(1, 2)] = matrix_ptr->data[WORD_INDEX(0,3)] * matrix_ptr->data[WORD_INDEX(1,2)] * matrix_ptr->data[WORD_INDEX(3,0)] -
                          matrix_ptr->data[WORD_INDEX(0,2)] * matrix_ptr->data[WORD_INDEX(1,3)] * matrix_ptr->data[WORD_INDEX(3,0)] -
                          matrix_ptr->data[WORD_INDEX(0,3)] * matrix_ptr->data[WORD_INDEX(1,0)] * matrix_ptr->data[WORD_INDEX(3,2)] +
                          matrix_ptr->data[WORD_INDEX(0,0)] * matrix_ptr->data[WORD_INDEX(1,3)] * matrix_ptr->data[WORD_INDEX(3,2)] +
                          matrix_ptr->data[WORD_INDEX(0,2)] * matrix_ptr->data[WORD_INDEX(1,0)] * matrix_ptr->data[WORD_INDEX(3,3)] -
                          matrix_ptr->data[WORD_INDEX(0,0)] * matrix_ptr->data[WORD_INDEX(1,2)] * matrix_ptr->data[WORD_INDEX(3,3)];
    m[WORD_INDEX(1, 3)] = matrix_ptr->data[WORD_INDEX(0,2)] * matrix_ptr->data[WORD_INDEX(1,3)] * matrix_ptr->data[WORD_INDEX(2,0)] -
                          matrix_ptr->data[WORD_INDEX(0,3)] * matrix_ptr->data[WORD_INDEX(1,2)] * matrix_ptr->data[WORD_INDEX(2,0)] +
                          matrix_ptr->data[WORD_INDEX(0,3)] * matrix_ptr->data[WORD_INDEX(1,0)] * matrix_ptr->data[WORD_INDEX(2,2)] -
                          matrix_ptr->data[WORD_INDEX(0,0)] * matrix_ptr->data[WORD_INDEX(1,3)] * matrix_ptr->data[WORD_INDEX(2,2)] -
                          matrix_ptr->data[WORD_INDEX(0,2)] * matrix_ptr->data[WORD_INDEX(1,0)] * matrix_ptr->data[WORD_INDEX(2,3)] +
                          matrix_ptr->data[WORD_INDEX(0,0)] * matrix_ptr->data[WORD_INDEX(1,2)] * matrix_ptr->data[WORD_INDEX(2,3)];
    m[WORD_INDEX(2, 0)] = matrix_ptr->data[WORD_INDEX(1,1)] * matrix_ptr->data[WORD_INDEX(2,3)] * matrix_ptr->data[WORD_INDEX(3,0)] -
                          matrix_ptr->data[WORD_INDEX(1,3)] * matrix_ptr->data[WORD_INDEX(2,1)] * matrix_ptr->data[WORD_INDEX(3,0)] +
                          matrix_ptr->data[WORD_INDEX(1,3)] * matrix_ptr->data[WORD_INDEX(2,0)] * matrix_ptr->data[WORD_INDEX(3,1)] -
                          matrix_ptr->data[WORD_INDEX(1,0)] * matrix_ptr->data[WORD_INDEX(2,3)] * matrix_ptr->data[WORD_INDEX(3,1)] -
                          matrix_ptr->data[WORD_INDEX(1,1)] * matrix_ptr->data[WORD_INDEX(2,0)] * matrix_ptr->data[WORD_INDEX(3,3)] +
                          matrix_ptr->data[WORD_INDEX(1,0)] * matrix_ptr->data[WORD_INDEX(2,1)] * matrix_ptr->data[WORD_INDEX(3,3)];
    m[WORD_INDEX(2, 1)] = matrix_ptr->data[WORD_INDEX(0,3)] * matrix_ptr->data[WORD_INDEX(2,1)] * matrix_ptr->data[WORD_INDEX(3,0)] -
                          matrix_ptr->data[WORD_INDEX(0,1)] * matrix_ptr->data[WORD_INDEX(2,3)] * matrix_ptr->data[WORD_INDEX(3,0)] -
                          matrix_ptr->data[WORD_INDEX(0,3)] * matrix_ptr->data[WORD_INDEX(2,0)] * matrix_ptr->data[WORD_INDEX(3,1)] +
                          matrix_ptr->data[WORD_INDEX(0,0)] * matrix_ptr->data[WORD_INDEX(2,3)] * matrix_ptr->data[WORD_INDEX(3,1)] +
                          matrix_ptr->data[WORD_INDEX(0,1)] * matrix_ptr->data[WORD_INDEX(2,0)] * matrix_ptr->data[WORD_INDEX(3,3)] -
                          matrix_ptr->data[WORD_INDEX(0,0)] * matrix_ptr->data[WORD_INDEX(2,1)] * matrix_ptr->data[WORD_INDEX(3,3)];
    m[WORD_INDEX(2, 2)] = matrix_ptr->data[WORD_INDEX(0,1)] * matrix_ptr->data[WORD_INDEX(1,3)] * matrix_ptr->data[WORD_INDEX(3,0)] -
                          matrix_ptr->data[WORD_INDEX(0,3)] * matrix_ptr->data[WORD_INDEX(1,1)] * matrix_ptr->data[WORD_INDEX(3,0)] +
                          matrix_ptr->data[WORD_INDEX(0,3)] * matrix_ptr->data[WORD_INDEX(1,0)] * matrix_ptr->data[WORD_INDEX(3,1)] -
                          matrix_ptr->data[WORD_INDEX(0,0)] * matrix_ptr->data[WORD_INDEX(1,3)] * matrix_ptr->data[WORD_INDEX(3,1)] -
                          matrix_ptr->data[WORD_INDEX(0,1)] * matrix_ptr->data[WORD_INDEX(1,0)] * matrix_ptr->data[WORD_INDEX(3,3)] +
                          matrix_ptr->data[WORD_INDEX(0,0)] * matrix_ptr->data[WORD_INDEX(1,1)] * matrix_ptr->data[WORD_INDEX(3,3)];
    m[WORD_INDEX(2, 3)] = matrix_ptr->data[WORD_INDEX(0,3)] * matrix_ptr->data[WORD_INDEX(1,1)] * matrix_ptr->data[WORD_INDEX(2,0)] -
                          matrix_ptr->data[WORD_INDEX(0,1)] * matrix_ptr->data[WORD_INDEX(1,3)] * matrix_ptr->data[WORD_INDEX(2,0)] -
                          matrix_ptr->data[WORD_INDEX(0,3)] * matrix_ptr->data[WORD_INDEX(1,0)] * matrix_ptr->data[WORD_INDEX(2,1)] +
                          matrix_ptr->data[WORD_INDEX(0,0)] * matrix_ptr->data[WORD_INDEX(1,3)] * matrix_ptr->data[WORD_INDEX(2,1)] +
                          matrix_ptr->data[WORD_INDEX(0,1)] * matrix_ptr->data[WORD_INDEX(1,0)] * matrix_ptr->data[WORD_INDEX(2,3)] -
                          matrix_ptr->data[WORD_INDEX(0,0)] * matrix_ptr->data[WORD_INDEX(1,1)] * matrix_ptr->data[WORD_INDEX(2,3)];
    m[WORD_INDEX(3, 0)] = matrix_ptr->data[WORD_INDEX(1,2)] * matrix_ptr->data[WORD_INDEX(2,1)] * matrix_ptr->data[WORD_INDEX(3,0)] -
                          matrix_ptr->data[WORD_INDEX(1,1)] * matrix_ptr->data[WORD_INDEX(2,2)] * matrix_ptr->data[WORD_INDEX(3,0)] -
                          matrix_ptr->data[WORD_INDEX(1,2)] * matrix_ptr->data[WORD_INDEX(2,0)] * matrix_ptr->data[WORD_INDEX(3,1)] +
                          matrix_ptr->data[WORD_INDEX(1,0)] * matrix_ptr->data[WORD_INDEX(2,2)] * matrix_ptr->data[WORD_INDEX(3,1)] +
                          matrix_ptr->data[WORD_INDEX(1,1)] * matrix_ptr->data[WORD_INDEX(2,0)] * matrix_ptr->data[WORD_INDEX(3,2)] -
                          matrix_ptr->data[WORD_INDEX(1,0)] * matrix_ptr->data[WORD_INDEX(2,1)] * matrix_ptr->data[WORD_INDEX(3,2)];
    m[WORD_INDEX(3, 1)] = matrix_ptr->data[WORD_INDEX(0,1)] * matrix_ptr->data[WORD_INDEX(2,2)] * matrix_ptr->data[WORD_INDEX(3,0)] -
                          matrix_ptr->data[WORD_INDEX(0,2)] * matrix_ptr->data[WORD_INDEX(2,1)] * matrix_ptr->data[WORD_INDEX(3,0)] +
                          matrix_ptr->data[WORD_INDEX(0,2)] * matrix_ptr->data[WORD_INDEX(2,0)] * matrix_ptr->data[WORD_INDEX(3,1)] -
                          matrix_ptr->data[WORD_INDEX(0,0)] * matrix_ptr->data[WORD_INDEX(2,2)] * matrix_ptr->data[WORD_INDEX(3,1)] -
                          matrix_ptr->data[WORD_INDEX(0,1)] * matrix_ptr->data[WORD_INDEX(2,0)] * matrix_ptr->data[WORD_INDEX(3,2)] +
                          matrix_ptr->data[WORD_INDEX(0,0)] * matrix_ptr->data[WORD_INDEX(2,1)] * matrix_ptr->data[WORD_INDEX(3,2)];
    m[WORD_INDEX(3, 2)] = matrix_ptr->data[WORD_INDEX(0,2)] * matrix_ptr->data[WORD_INDEX(1,1)] * matrix_ptr->data[WORD_INDEX(3,0)] -
                          matrix_ptr->data[WORD_INDEX(0,1)] * matrix_ptr->data[WORD_INDEX(1,2)] * matrix_ptr->data[WORD_INDEX(3,0)] -
                          matrix_ptr->data[WORD_INDEX(0,2)] * matrix_ptr->data[WORD_INDEX(1,0)] * matrix_ptr->data[WORD_INDEX(3,1)] +
                          matrix_ptr->data[WORD_INDEX(0,0)] * matrix_ptr->data[WORD_INDEX(1,2)] * matrix_ptr->data[WORD_INDEX(3,1)] +
                          matrix_ptr->data[WORD_INDEX(0,1)] * matrix_ptr->data[WORD_INDEX(1,0)] * matrix_ptr->data[WORD_INDEX(3,2)] -
                          matrix_ptr->data[WORD_INDEX(0,0)] * matrix_ptr->data[WORD_INDEX(1,1)] * matrix_ptr->data[WORD_INDEX(3,2)];
    m[WORD_INDEX(3, 3)] = matrix_ptr->data[WORD_INDEX(0,1)] * matrix_ptr->data[WORD_INDEX(1,2)] * matrix_ptr->data[WORD_INDEX(2,0)] -
                          matrix_ptr->data[WORD_INDEX(0,2)] * matrix_ptr->data[WORD_INDEX(1,1)] * matrix_ptr->data[WORD_INDEX(2,0)] +
                          matrix_ptr->data[WORD_INDEX(0,2)] * matrix_ptr->data[WORD_INDEX(1,0)] * matrix_ptr->data[WORD_INDEX(2,1)] -
                          matrix_ptr->data[WORD_INDEX(0,0)] * matrix_ptr->data[WORD_INDEX(1,2)] * matrix_ptr->data[WORD_INDEX(2,1)] -
                          matrix_ptr->data[WORD_INDEX(0,1)] * matrix_ptr->data[WORD_INDEX(1,0)] * matrix_ptr->data[WORD_INDEX(2,2)] +
                          matrix_ptr->data[WORD_INDEX(0,0)] * matrix_ptr->data[WORD_INDEX(1,1)] * matrix_ptr->data[WORD_INDEX(2,2)];

    const float invDet = 1.0f / det;

    for (int i = 0;
             i < 16;
             i++)
    {
        matrix_ptr->data[i] = m[i] * invDet;
    }

    matrix_ptr->is_data_dirty = true;
    return true;
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_matrix4x4_get_clipping_plane(system_matrix4x4                mvp,
                                                            system_matrix4x4_clipping_plane clipping_plane,
                                                            float*                          out_plane_coeffs_ptr)
{
    _system_matrix4x4* mvp_ptr = reinterpret_cast<_system_matrix4x4*>(mvp);

    if (mvp_ptr->is_data_dirty)
    {
        _system_matrix4x4_generate_column_major_data(mvp_ptr);
    }

    /* The idea behind this implementation is a fairly straightforward derivation of how clipping works:
     *
     * Let p = (x, y, z, 1)
     *     A = MVP = [ a_11 a_21 a_31 a_41]
     *               [ a_12 a_22 a_32 a_42]
     *               [ a_13 a_23 a_33 a_43]
     *               [ a_14 a_24 a_34 a_44]
     *
     * x_c = pA.x = x * a_11 + y * a_12 + z * a_13 + a_14
     * w_c = pA.w = x * a_41 + y * a_42 + z * a_43 + a_43
     *
     * Since:
     *
     * -w_c < x_c
     *
     * Then for the left plane we have:
     *
     * -(x * a_41 + y * a_42 + z * a_43 + a_43) <  * a_11 + y * a_12 + z * a_13 + a_14)
     *
     * which leads to:
     *
     * x(a_41 + a_11) + y(a_42 + a_12) + z(a_43 + a_13) + (a_44 + a_14) > 0
     *
     * therefore left clipping plane coeffs are:
     *
     * A = a_41 + a_11
     * B = a_42 + a_12
     * C = a_43 + a_13
     * D = a_44 + a_14
     */
    switch (clipping_plane)
    {
        case SYSTEM_MATRIX4X4_CLIPPING_PLANE_BOTTOM:
        {
            out_plane_coeffs_ptr[0] = mvp_ptr->data[4] + mvp_ptr->data[12];
            out_plane_coeffs_ptr[1] = mvp_ptr->data[5] + mvp_ptr->data[13];
            out_plane_coeffs_ptr[2] = mvp_ptr->data[6] + mvp_ptr->data[14];
            out_plane_coeffs_ptr[3] = mvp_ptr->data[7] + mvp_ptr->data[15];

            break;
        }

        case SYSTEM_MATRIX4X4_CLIPPING_PLANE_FAR:
        {
            out_plane_coeffs_ptr[0] = -mvp_ptr->data[8]  + mvp_ptr->data[12];
            out_plane_coeffs_ptr[1] = -mvp_ptr->data[9]  + mvp_ptr->data[13];
            out_plane_coeffs_ptr[2] = -mvp_ptr->data[10] + mvp_ptr->data[14];
            out_plane_coeffs_ptr[3] = -mvp_ptr->data[11] + mvp_ptr->data[15];

            break;
        }

        case SYSTEM_MATRIX4X4_CLIPPING_PLANE_LEFT:
        {
            out_plane_coeffs_ptr[0] = mvp_ptr->data[0] + mvp_ptr->data[12];
            out_plane_coeffs_ptr[1] = mvp_ptr->data[1] + mvp_ptr->data[13];
            out_plane_coeffs_ptr[2] = mvp_ptr->data[2] + mvp_ptr->data[14];
            out_plane_coeffs_ptr[3] = mvp_ptr->data[3] + mvp_ptr->data[15];

            break;
        }

        case SYSTEM_MATRIX4X4_CLIPPING_PLANE_NEAR:
        {
            out_plane_coeffs_ptr[0] = mvp_ptr->data[8]  + mvp_ptr->data[12];
            out_plane_coeffs_ptr[1] = mvp_ptr->data[9]  + mvp_ptr->data[13];
            out_plane_coeffs_ptr[2] = mvp_ptr->data[10] + mvp_ptr->data[14];
            out_plane_coeffs_ptr[3] = mvp_ptr->data[11] + mvp_ptr->data[15];

            break;
        }

        case SYSTEM_MATRIX4X4_CLIPPING_PLANE_RIGHT:
        {
            out_plane_coeffs_ptr[0] = -mvp_ptr->data[0] + mvp_ptr->data[12];
            out_plane_coeffs_ptr[1] = -mvp_ptr->data[1] + mvp_ptr->data[13];
            out_plane_coeffs_ptr[2] = -mvp_ptr->data[2] + mvp_ptr->data[14];
            out_plane_coeffs_ptr[3] = -mvp_ptr->data[3] + mvp_ptr->data[15];

            break;
        }

        case SYSTEM_MATRIX4X4_CLIPPING_PLANE_TOP:
        {
            out_plane_coeffs_ptr[0] = -mvp_ptr->data[4] + mvp_ptr->data[12];
            out_plane_coeffs_ptr[1] = -mvp_ptr->data[5] + mvp_ptr->data[13];
            out_plane_coeffs_ptr[2] = -mvp_ptr->data[6] + mvp_ptr->data[14];
            out_plane_coeffs_ptr[3] = -mvp_ptr->data[7] + mvp_ptr->data[15];

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized clipping plane requested");
        }
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API bool system_matrix4x4_is_equal(const system_matrix4x4 a,
                                                  const system_matrix4x4 b)
{
    const _system_matrix4x4* a_ptr  = reinterpret_cast<const _system_matrix4x4*>(a);
    const _system_matrix4x4* b_ptr  = reinterpret_cast<const _system_matrix4x4*>(b);
    bool                     result = true;

    for (int n = 0;
             n < 16;
           ++n)
    {
        if (fabs(a_ptr->data[n] - b_ptr->data[n]) > 1e-5f)
        {
            result = false;

            break;
        }
    }

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_matrix4x4_multiply_by_lookat(system_matrix4x4 matrix,
                                                            const float*     camera_position_ptr,
                                                            const float*     look_at_point_ptr,
                                                            const float*     up_vector_ptr)
{
    float x_vector[3];
    float y_vector[3];
    float z_vector[3] =
    {
        look_at_point_ptr[0] - camera_position_ptr[0],
        look_at_point_ptr[1] - camera_position_ptr[1],
        look_at_point_ptr[2] - camera_position_ptr[2]
    };

    float vector_magnitude = sqrt(z_vector[0] * z_vector[0] + z_vector[1] * z_vector[1] + z_vector[2] * z_vector[2]);

    if (vector_magnitude >= 0.00001f)
    {
        z_vector[0] /= vector_magnitude;
        z_vector[1] /= vector_magnitude;
        z_vector[2] /= vector_magnitude;
    }

    // f x up
    x_vector[0] = up_vector_ptr[2] * z_vector[1] -
                  up_vector_ptr[1] * z_vector[2];
    x_vector[1] = up_vector_ptr[0] * z_vector[2] -
                  up_vector_ptr[2] * z_vector[0];
    x_vector[2] = up_vector_ptr[1] * z_vector[0] -
                  up_vector_ptr[0] * z_vector[1];

    vector_magnitude = sqrt(x_vector[0] * x_vector[0] +
                            x_vector[1] * x_vector[1] +
                            x_vector[2] * x_vector[2]);

    if (vector_magnitude >= 0.00001f)
    {
        x_vector[0] /= vector_magnitude;
        x_vector[1] /= vector_magnitude;
        x_vector[2] /= vector_magnitude;
    }

    // s x f
    y_vector[0] = z_vector[2] * x_vector[1] -
                  z_vector[1] * x_vector[2];
    y_vector[1] = z_vector[0] * x_vector[2] -
                  z_vector[2] * x_vector[0];
    y_vector[2] = z_vector[1] * x_vector[0] -
                  z_vector[0] * x_vector[1];

    //
    float translation_vec [3]  =
    {
        -camera_position_ptr[0],
        -camera_position_ptr[1],
        -camera_position_ptr[2]
    };
    float view_matrix_data[16] =
    {
        x_vector[0],  x_vector[1],  x_vector[2], 0,
        y_vector[0],  y_vector[1],  y_vector[2], 0,
       -z_vector[0], -z_vector[1], -z_vector[2], 0,
       0,             0,            0,           1};

    //
    system_matrix4x4 view_matrix   = system_matrix4x4_create();
    system_matrix4x4 result_matrix = nullptr;

    system_matrix4x4_set_from_row_major_raw       (view_matrix,
                                                   view_matrix_data);
    result_matrix = system_matrix4x4_create_by_mul(matrix,
                                                   view_matrix);

    system_matrix4x4_translate         (result_matrix,
                                        translation_vec);
    system_matrix4x4_set_from_matrix4x4(matrix,
                                        result_matrix);

    system_matrix4x4_release(view_matrix);
    system_matrix4x4_release(result_matrix);
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_matrix4x4_multiply_by_matrix4x4(system_matrix4x4 a,
                                                               system_matrix4x4 b)
{
    _system_matrix4x4* a_ptr = reinterpret_cast<_system_matrix4x4*>(a);
    _system_matrix4x4* b_ptr = reinterpret_cast<_system_matrix4x4*>(b);
    _system_matrix4x4  temp;

    for (unsigned char column = 0;
                       column < 4;
                     ++column)
    {
        for (unsigned char row = 0;
                           row < 4;
                         ++row)
        {
            temp.data[WORD_INDEX(column, row)] = a_ptr->data[WORD_INDEX(0, row)] * b_ptr->data[WORD_INDEX(column, 0)] +
                                                 a_ptr->data[WORD_INDEX(1, row)] * b_ptr->data[WORD_INDEX(column, 1)] +
                                                 a_ptr->data[WORD_INDEX(2, row)] * b_ptr->data[WORD_INDEX(column, 2)] +
                                                 a_ptr->data[WORD_INDEX(3, row)] * b_ptr->data[WORD_INDEX(column, 3)];
        }
    }

    memcpy(&a_ptr->data,
           temp.data,
           sizeof(temp.data) );

    a_ptr->is_data_dirty = true;
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_matrix4x4_multiply_by_vector4(system_matrix4x4 matrix,
                                                             const float*     vector_ptr,
                                                             float*           out_result_ptr)
{
    _system_matrix4x4* matrix_ptr = reinterpret_cast<_system_matrix4x4*>(matrix);

    ASSERT_DEBUG_SYNC(vector_ptr != out_result_ptr,
                      "In data cannot be equal to out data!");

    out_result_ptr[0] = matrix_ptr->data[WORD_INDEX(0, 0)] * vector_ptr[0] +
                        matrix_ptr->data[WORD_INDEX(1, 0)] * vector_ptr[1] +
                        matrix_ptr->data[WORD_INDEX(2, 0)] * vector_ptr[2] +
                        matrix_ptr->data[WORD_INDEX(3, 0)] * vector_ptr[3];
    out_result_ptr[1] = matrix_ptr->data[WORD_INDEX(0, 1)] * vector_ptr[0] +
                        matrix_ptr->data[WORD_INDEX(1, 1)] * vector_ptr[1] +
                        matrix_ptr->data[WORD_INDEX(2, 1)] * vector_ptr[2] +
                        matrix_ptr->data[WORD_INDEX(3, 1)] * vector_ptr[3];
    out_result_ptr[2] = matrix_ptr->data[WORD_INDEX(0, 2)] * vector_ptr[0] +
                        matrix_ptr->data[WORD_INDEX(1, 2)] * vector_ptr[1] +
                        matrix_ptr->data[WORD_INDEX(2, 2)] * vector_ptr[2] +
                        matrix_ptr->data[WORD_INDEX(3, 2)] * vector_ptr[3];
    out_result_ptr[3] = matrix_ptr->data[WORD_INDEX(0, 3)] * vector_ptr[0] +
                        matrix_ptr->data[WORD_INDEX(1, 3)] * vector_ptr[1] +
                        matrix_ptr->data[WORD_INDEX(2, 3)] * vector_ptr[2] +
                        matrix_ptr->data[WORD_INDEX(3, 3)] * vector_ptr[3];
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_matrix4x4_multiply_by_vector3(system_matrix4x4 matrix,
                                                             const float*     vector_ptr,
                                                             float*           out_result_ptr)
{
    _system_matrix4x4* matrix_ptr = reinterpret_cast<_system_matrix4x4*>(matrix);

    ASSERT_DEBUG_SYNC(vector_ptr != out_result_ptr,
                      "In data cannot be equal to out data!");

    out_result_ptr[0] = matrix_ptr->data[WORD_INDEX(0, 0)] * vector_ptr[0] +
                        matrix_ptr->data[WORD_INDEX(1, 0)] * vector_ptr[1] +
                        matrix_ptr->data[WORD_INDEX(2, 0)] * vector_ptr[2];
    out_result_ptr[1] = matrix_ptr->data[WORD_INDEX(0, 1)] * vector_ptr[0] +
                        matrix_ptr->data[WORD_INDEX(1, 1)] * vector_ptr[1] +
                        matrix_ptr->data[WORD_INDEX(2, 1)] * vector_ptr[2];
    out_result_ptr[2] = matrix_ptr->data[WORD_INDEX(0, 2)] * vector_ptr[0] +
                        matrix_ptr->data[WORD_INDEX(1, 2)] * vector_ptr[1] +
                        matrix_ptr->data[WORD_INDEX(2, 2)] * vector_ptr[2];
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_matrix4x4_rotate(system_matrix4x4 matrix,
                                                float            angle,
                                                const float*     xyz_ptr)
{
    system_matrix4x4   rotation_matrix     = system_matrix4x4_create();
    _system_matrix4x4* rotation_matrix_ptr = reinterpret_cast<_system_matrix4x4*>(rotation_matrix);

    float c   = cos(angle);
    float s   = sin(angle);
    float x   = xyz_ptr[0];
    float y   = xyz_ptr[1];
    float z   = xyz_ptr[2];
    float x_2 = xyz_ptr[0] * xyz_ptr[0];
    float y_2 = xyz_ptr[1] * xyz_ptr[1];
    float z_2 = xyz_ptr[2] * xyz_ptr[2];

    if (x_2 + y_2 + z_2 > 1)
    {
        // Need to normalize.
        float length     = sqrt(x_2 + y_2 + z_2);
        float inv_length = 1.0f / length;

        x   *= inv_length;
        y   *= inv_length;
        z   *= inv_length;
        x_2 *= inv_length;
        y_2 *= inv_length;
        z_2 *= inv_length;
    }

    float xy        = x * y;
    float xz        = x * z;
    float xs        = x * s;
    float ys        = y * s;
    float yz        = y * z;
    float zs        = z * s;
    float oneMinusC = 1 - c;

    rotation_matrix_ptr->data[WORD_INDEX(0, 0)] = x_2 * oneMinusC + c;
    rotation_matrix_ptr->data[WORD_INDEX(1, 0)] = xy  * oneMinusC - zs;
    rotation_matrix_ptr->data[WORD_INDEX(2, 0)] = xz  * oneMinusC + ys;
    rotation_matrix_ptr->data[WORD_INDEX(3, 0)] = 0;
    rotation_matrix_ptr->data[WORD_INDEX(0, 1)] = xy  * oneMinusC + zs;
    rotation_matrix_ptr->data[WORD_INDEX(1, 1)] = y_2 * oneMinusC + c;
    rotation_matrix_ptr->data[WORD_INDEX(2, 1)] = yz  * oneMinusC - xs;
    rotation_matrix_ptr->data[WORD_INDEX(3, 1)] = 0;
    rotation_matrix_ptr->data[WORD_INDEX(0, 2)] = xz  * oneMinusC - ys;
    rotation_matrix_ptr->data[WORD_INDEX(1, 2)] = yz  * oneMinusC + xs;
    rotation_matrix_ptr->data[WORD_INDEX(2, 2)] = z_2 * oneMinusC + c;
    rotation_matrix_ptr->data[WORD_INDEX(3, 2)] = 0;
    rotation_matrix_ptr->data[WORD_INDEX(0, 3)] = 0;
    rotation_matrix_ptr->data[WORD_INDEX(1, 3)] = 0;
    rotation_matrix_ptr->data[WORD_INDEX(2, 3)] = 0;
    rotation_matrix_ptr->data[WORD_INDEX(3, 3)] = 1;

    system_matrix4x4   result_matrix     = system_matrix4x4_create_by_mul(matrix,
                                                                          rotation_matrix);
    _system_matrix4x4* result_matrix_ptr = reinterpret_cast<_system_matrix4x4*>(result_matrix);
    _system_matrix4x4* matrix_ptr        = reinterpret_cast<_system_matrix4x4*>(matrix);

    memcpy(matrix_ptr->data,
           result_matrix_ptr->data,
           sizeof(float) * 16);

    matrix_ptr->is_data_dirty = true;

    system_matrix4x4_release(result_matrix);
    system_matrix4x4_release(rotation_matrix);
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_matrix4x4_scale(system_matrix4x4 matrix,
                                               const float*     xyz_ptr)
{
    system_matrix4x4   scale_matrix     = system_matrix4x4_create();
    _system_matrix4x4* scale_matrix_ptr = reinterpret_cast<_system_matrix4x4*>(scale_matrix);

    system_matrix4x4_set_to_identity(scale_matrix);

    scale_matrix_ptr->data[WORD_INDEX(0, 0)] = xyz_ptr[0];
    scale_matrix_ptr->data[WORD_INDEX(1, 1)] = xyz_ptr[1];
    scale_matrix_ptr->data[WORD_INDEX(2, 2)] = xyz_ptr[2];

    system_matrix4x4   result_matrix     = system_matrix4x4_create_by_mul(matrix,
                                                                          scale_matrix);
    _system_matrix4x4* result_matrix_ptr = reinterpret_cast<_system_matrix4x4*>(result_matrix);
    _system_matrix4x4* matrix_ptr        = reinterpret_cast<_system_matrix4x4*>(matrix);

    memcpy(matrix_ptr->data,
           result_matrix_ptr->data,
           sizeof(float) * 16);

    matrix_ptr->is_data_dirty = true;

    system_matrix4x4_release(result_matrix);
    system_matrix4x4_release(scale_matrix);
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_matrix4x4_set_to_float(system_matrix4x4 matrix,
                                                      float            value)
{
    _system_matrix4x4* matrix_ptr = reinterpret_cast<_system_matrix4x4*>(matrix);

    for (unsigned char n = 0;
                       n < 16;
                     ++n)
    {
        matrix_ptr->data[n] = value;
    }

    memcpy(matrix_ptr->column_major_data,
           matrix_ptr->data,
           sizeof(float) * 16);
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_matrix4x4_set_to_identity(system_matrix4x4 matrix)
{
    _system_matrix4x4* matrix_ptr = reinterpret_cast<_system_matrix4x4*>(matrix);

    for (unsigned char n = 0;
                       n < 16;
                     ++n)
    {
        matrix_ptr->data[n] = 0.0f;
    }

    matrix_ptr->data[0]  = 1.0f;
    matrix_ptr->data[5]  = 1.0f;
    matrix_ptr->data[10] = 1.0f;
    matrix_ptr->data[15] = 1.0f;

    memcpy(matrix_ptr->column_major_data,
           matrix_ptr->data,
           sizeof(float) * 16);

    matrix_ptr->is_data_dirty = false;
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_matrix4x4_set_element(system_matrix4x4 matrix,
                                                     unsigned char    location_data,
                                                     float            value)
{
    _system_matrix4x4* matrix_ptr = reinterpret_cast<_system_matrix4x4*>(matrix);

    ASSERT_ALWAYS_SYNC(false,
                       "verify");

    matrix_ptr->data[WORD_INDEX(COL_FROM_ELEMENT_INDEX(location_data),
                                ROW_FROM_ELEMENT_INDEX(location_data) )] = value;
    matrix_ptr->is_data_dirty                                            = true;
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_matrix4x4_set_from_column_major_raw(system_matrix4x4 matrix,
                                                                   const float*     data_ptr)
{
    _system_matrix4x4* matrix_ptr = reinterpret_cast<_system_matrix4x4*>(matrix);

    matrix_ptr->data[0] = data_ptr[0];  matrix_ptr->data[4] = data_ptr[1];  matrix_ptr->data[8]  = data_ptr[2];  matrix_ptr->data[12] = data_ptr[3];
    matrix_ptr->data[1] = data_ptr[4];  matrix_ptr->data[5] = data_ptr[5];  matrix_ptr->data[9]  = data_ptr[6];  matrix_ptr->data[13] = data_ptr[7];
    matrix_ptr->data[2] = data_ptr[8];  matrix_ptr->data[6] = data_ptr[9];  matrix_ptr->data[10] = data_ptr[10]; matrix_ptr->data[14] = data_ptr[11];
    matrix_ptr->data[3] = data_ptr[12]; matrix_ptr->data[7] = data_ptr[13]; matrix_ptr->data[11] = data_ptr[14]; matrix_ptr->data[15] = data_ptr[15];

    matrix_ptr->is_data_dirty = true;
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_matrix4x4_set_from_row_major_raw(system_matrix4x4 matrix,
                                                                const float*     data_ptr)
{
    _system_matrix4x4* matrix_ptr = reinterpret_cast<_system_matrix4x4*>(matrix);

    memcpy(matrix_ptr->data,
           data_ptr,
           sizeof(float) * 16);

    matrix_ptr->is_data_dirty = true;
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_matrix4x4_set_from_matrix4x4(system_matrix4x4 dst_matrix,
                                                            system_matrix4x4 src_matrix)
{
    _system_matrix4x4* dst_matrix_ptr = reinterpret_cast<_system_matrix4x4*>(dst_matrix);
    _system_matrix4x4* src_matrix_ptr = reinterpret_cast<_system_matrix4x4*>(src_matrix);

    memcpy(dst_matrix_ptr,
           src_matrix_ptr,
           sizeof(_system_matrix4x4) );
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_matrix4x4_translate(system_matrix4x4 matrix,
                                                   const float*     xyz_ptr)
{
    system_matrix4x4   result_matrix          = nullptr;
    system_matrix4x4   translation_matrix     = system_matrix4x4_create();
    _system_matrix4x4* matrix_ptr             = reinterpret_cast<_system_matrix4x4*>(matrix);
    _system_matrix4x4* result_matrix_ptr      = nullptr;
    _system_matrix4x4* translation_matrix_ptr = reinterpret_cast<_system_matrix4x4*>(translation_matrix);

    system_matrix4x4_set_to_identity(translation_matrix);

    translation_matrix_ptr->data[WORD_INDEX(3, 0)] = xyz_ptr[0];
    translation_matrix_ptr->data[WORD_INDEX(3, 1)] = xyz_ptr[1];
    translation_matrix_ptr->data[WORD_INDEX(3, 2)] = xyz_ptr[2];

    result_matrix     = system_matrix4x4_create_by_mul(matrix,
                                                       translation_matrix);
    result_matrix_ptr = reinterpret_cast<_system_matrix4x4*>(result_matrix);

    memcpy(matrix_ptr->data,
           result_matrix_ptr->data,
           sizeof(float) * 16);

    matrix_ptr->is_data_dirty = true;

    system_matrix4x4_release(result_matrix);
    system_matrix4x4_release(translation_matrix);
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_matrix4x4_transpose(system_matrix4x4 matrix)
{
    _system_matrix4x4* matrix_ptr = reinterpret_cast<_system_matrix4x4*>(matrix);
    float              data[16];

    for (unsigned char x = 0;
                       x < 4;
                     ++x)
    {
        for (unsigned char y = 0;
                           y < 4;
                         ++y)
        {
            data[WORD_INDEX(y, x)] = matrix_ptr->data[WORD_INDEX(x, y)];
        }
    }

    memcpy(matrix_ptr->data,
           data,
           sizeof(float) * 16);

    matrix_ptr->is_data_dirty = true;
}

/** Please see header for specification */
PUBLIC void _system_matrix4x4_init()
{
    ASSERT_DEBUG_SYNC(matrix_pool == nullptr,
                      "4x4 matrix pool already initialized");

    if (matrix_pool == nullptr)
    {
        matrix_pool = system_resource_pool_create(sizeof(_system_matrix4x4),
                                                  MATRIX4X4_BASE_CAPACITY,
                                                  nullptr, /* init_fn   */
                                                  0);   /* deinit_fn */

        ASSERT_ALWAYS_SYNC(matrix_pool != nullptr,
                           "Could not create a 4x4 matrix pool");
    }
}

/** Please see header for specification */
PUBLIC void _system_matrix4x4_deinit()
{
    ASSERT_DEBUG_SYNC(matrix_pool != nullptr,
                      "4x4 matrix pool not initialized");

    if (matrix_pool != nullptr)
    {
        system_resource_pool_release(matrix_pool);

        matrix_pool = nullptr;
    }
}
