/**
 *
 * Emerald (kbi/elude @2012)
 *
 */
#ifndef SYSTEM_MATRIX4X4_MUTEX
#define SYSTEM_MATRIX4X4_MUTEX

#include "system/system_types.h"

#define ELEMENT_INDEX(column, row) (column | (row << 2)

enum system_matrix4x4_clipping_plane
{
    SYSTEM_MATRIX4X4_CLIPPING_PLANE_BOTTOM,
    SYSTEM_MATRIX4X4_CLIPPING_PLANE_FAR,
    SYSTEM_MATRIX4X4_CLIPPING_PLANE_LEFT,
    SYSTEM_MATRIX4X4_CLIPPING_PLANE_NEAR,
    SYSTEM_MATRIX4X4_CLIPPING_PLANE_RIGHT,
    SYSTEM_MATRIX4X4_CLIPPING_PLANE_TOP,

};

/** Creates a new instance of 4x4 matrix object without initializing the data.
 *
 *  @return New 4x4 matrix instance.
 */
PUBLIC EMERALD_API system_matrix4x4 system_matrix4x4_create();

/** TODO */
PUBLIC EMERALD_API system_matrix4x4 system_matrix4x4_create_copy(system_matrix4x4 src_matrix);

/** TODO */
PUBLIC EMERALD_API system_matrix4x4 system_matrix4x4_create_lookat_matrix(const float* camera_location,
                                                                          const float* look_at_point,
                                                                          const float* up_vector);

/** TODO */
PUBLIC EMERALD_API system_matrix4x4 system_matrix4x4_create_ortho_projection_matrix(float left,
                                                                                    float right,
                                                                                    float bottom,
                                                                                    float top,
                                                                                    float z_near,
                                                                                    float z_far);

/** Creates a new instance of 4x4 matrix object and fills it perspective projection matrix.
 *
 *  @param fov_y  Field of view angle (in degrees) in y direction
 *  @param ar     Aspect ratio (width/height)
 *  @param z_near Distance from the viewer to the near clipping plane (always positive)
 *  @param z_far  Distance from the viewer to the far clipping plane (always positive)
 *
 *  @return Instance of a 4x4 matrix object. Cannot be null.
 **/
PUBLIC EMERALD_API system_matrix4x4 system_matrix4x4_create_perspective_projection_matrix(float fov_y,
                                                                                          float ar,
                                                                                          float z_near,
                                                                                          float z_far);

/** TODO */
PUBLIC EMERALD_API system_matrix4x4 system_matrix4x4_create_perspective_projection_matrix_custom( float left,
                                                                                                  float right,
                                                                                                  float bottom,
                                                                                                  float top,
                                                                                                  float z_near,
                                                                                                  float z_far);

/** TODO */
PUBLIC EMERALD_API void system_matrix4x4_get_clipping_plane(system_matrix4x4                mvp,
                                                            system_matrix4x4_clipping_plane clipping_plane,
                                                            float*                          out_plane_coeffs);

/** TODO */
PUBLIC EMERALD_API bool system_matrix4x4_is_equal(const system_matrix4x4 a,
                                                  const system_matrix4x4 b);

/** Releases ane xisting instance of 4x4 matrix object.
 *
 *  @param system_matrix4x4 Isntance of a 4x4 matrix object. Cannot be null.
 */
PUBLIC EMERALD_API void system_matrix4x4_release(system_matrix4x4 matrix);

/** Retrieves column-major data stored in 4x4 matrix object.
 *
 *  @param system_matrix4x4 4x4 matrix to retrieve the data from.
 *
 *  @return Matrix contents in column-major order.
 */
PUBLIC EMERALD_API const float* system_matrix4x4_get_column_major_data(system_matrix4x4 matrix);

/** Retrieves row-major data stored in 4x4 matrix object.
 *
 *  @param system_matrix4x4 4x4 matrix to retrieve the data from.
 *
 *  @return Matrix contents in row-major order.
 */
PUBLIC EMERALD_API const float* system_matrix4x4_get_row_major_data(system_matrix4x4 matrix);

/** Creates a new isntance of 4x4 matrix object by multiplying two 4x4 matrix objects (A*B).
 *
 *  NOTE: This is a new instance that you will need to release! Input matrices are
 *        NOT released.
 *
 *  @param system_matrix4x4 A matrix.
 *  @param system_matrix4x4 B matrix.
 *
 *  @return New instance of a matrix4x4 containing the result of multiplication.
 */
PUBLIC EMERALD_API system_matrix4x4 system_matrix4x4_create_by_mul(system_matrix4x4 a,
                                                                   system_matrix4x4 b);

/** Inverts a 4x4 matrix object.
 *
 *  @param system_matrix4x4 4x4 matrix object to invert.
 *
 *  @return True if inversion operation was successful, false otherwise.
 */
PUBLIC EMERALD_API bool system_matrix4x4_invert(system_matrix4x4 matrix);

/** Multiplies 4x4 matrix object by a look-at matrix. Result is stored within the matrix.
 *
 *  NOTE: Up vector must be normalized.
 *
 *  @param system_matrix4x4 4x4 matrix object to use for multiplication.
 *  @param const float*     Camera position.
 *  @param const float*     Look at point.
 *  @param const float*     Up vector.
 */
PUBLIC EMERALD_API void system_matrix4x4_multiply_by_lookat(system_matrix4x4 matrix,
                                                            const float*     camera_position,
                                                            const float*     look_at_point,
                                                            const float*     up_vector);

/** TODO
 *
 *  a *= b;
 */
PUBLIC EMERALD_API void system_matrix4x4_multiply_by_matrix4x4(system_matrix4x4 a,
                                                               system_matrix4x4 b);

/** Multiples 4x4 matrix object by 4-dimensional user vector.
 *  Result is stored in user-provided vector.
 *
 *  @param system_matrix4x4 4x4 matrix object to use for multiplication.
 *  @param const float*     Vector to multiply the matrix with.
 *  @param const float*     Result will be stored in dereference of the pointer. Msut have space for 4 elements.
 */
PUBLIC EMERALD_API void system_matrix4x4_multiply_by_vector4(system_matrix4x4 matrix,
                                                             const float*     vector,
                                                             float*           result);

/** Multiples 4x4 matrix object by 3-dimensional user vector. 4th element is set to 0. 
 *  Result is stored in user-provided vector.
 *
 *  @param system_matrix4x4 4x4 matrix object to use for multiplication.
 *  @param const float*     Vector to multiply the matrix with.
 *  @param const float*     Result will be stored in dereference of the pointer. Must have space for 3 elements.
 */
PUBLIC EMERALD_API void system_matrix4x4_multiply_by_vector3(system_matrix4x4 matrix,
                                                             const float*     vector,
                                                             float*           result);

/** Rotates 4x4 matrix object using user provided angle and 3-diemnsional rotation vector.
 *  Result is stored in the object.
 *
 *  @param system_matrix4x4 4x4 matrix to rotate.
 *  @param float            Rotation angle.
 *  @param const float*     3-dimensional rotation vector.
 */
PUBLIC EMERALD_API void system_matrix4x4_rotate(system_matrix4x4 matrix,
                                                float            angle,
                                                const float*     xyz);

/** Scales 4x4 matrix object using user provided 3-diemnsional XYZ scale vector.
 *  Result is stored in the object.
 *
 *  @param system_matrix4x4 4x4 matrix to rotate.
 *  @param const float*     3-dimensional scale vector.
 */
PUBLIC EMERALD_API void system_matrix4x4_scale(system_matrix4x4 matrix,
                                               const float*     xyz);

/** TODO */
PUBLIC EMERALD_API void system_matrix4x4_set_to_float(system_matrix4x4 matrix,
                                                      float            value);

/** Resets 4x4 matrix object to identity matrix. Result is stored in the object.
 *
 *  @param system_matrix4x4 4x4 matrix object to set to identity amtrix.
 */
PUBLIC EMERALD_API void system_matrix4x4_set_to_identity(system_matrix4x4 matrix);

/** Sets a single element of a 4x4 matrix object to user-defined value.
 *
 *  @param system_matrix4x4 4x4 matrix to modify.
 *  @param unsigned char    <column, row> pair determining index of element to modify. Use ELEMENT_INDEX() to define
 *                          the pair.
 *  @param float            New value to use.
 */
PUBLIC EMERALD_API void system_matrix4x4_set_element(system_matrix4x4 matrix,
                                                     unsigned char    location_data,
                                                     float            value);

/** Sets a 4x4 matrix contents, using user-provided raw data stored in column-major order.
 *
 *  @param system_matrix4x4 4x4 matrix to set.
 *  @param const float*     Data to set, stored in column-,ajor order.
 */
PUBLIC EMERALD_API void system_matrix4x4_set_from_column_major_raw(system_matrix4x4 matrix,
                                                                   const float*     data);

/** Sets a 4x4 matrix contents, using user-provided raw data stored in row-major order.
 *
 *  @param system_matrix4x4 4x4 matrix to set.
 *  @param const float*     Data to set, stored in row-major order.
 */
PUBLIC EMERALD_API void system_matrix4x4_set_from_row_major_raw(system_matrix4x4 matrix,
                                                                const float*     data);

/** Sets a 4x4 matrix contents by copying data from another 4x4 matrix.
 *
 *  @param system_matrix4x4 Destination 4x4 matrix.
 *  @param system_matrix4x4 Source 4x4 matrix
 */
PUBLIC EMERALD_API void system_matrix4x4_set_from_matrix4x4(system_matrix4x4 dst_matrix,
                                                            system_matrix4x4 src_matrix);

/** Multiples 4x4 matrix object with translatino matrix. Result is stored in the object.
 *
 *  @param system_matrix4x4 4x4 matrix to translate.
 *  @param xyz              3-dimensional vector containing the translation vector.
 */
PUBLIC EMERALD_API void system_matrix4x4_translate(system_matrix4x4 matrix,
                                                   const float*     xyz);

/** Transposes a 4x4 matrix object. Result is stored in the object.
 *
 *  @param system_matrix4x4 4x4 matrix to transpose.
 */
PUBLIC EMERALD_API void system_matrix4x4_transpose(system_matrix4x4 matrix);

/** Initializes matrix pool. Should only be called once from DLL entry point */
PUBLIC void _system_matrix4x4_init();

/** Deinitializes matrix pool. Should only be called once from DLL entry ponit */
PUBLIC void _system_matrix4x4_deinit();

#endif /* SYSTEM_MATRIX4X4_MUTEX */