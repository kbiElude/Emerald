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
PUBLIC EMERALD_API system_matrix4x4 system_matrix4x4_create_lookat_matrix(__in_ecount(3) float* camera_location,
                                                                          __in_ecount(3) float* look_at_point,
                                                                          __in_ecount(3) float* up_vector);

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
PUBLIC EMERALD_API void system_matrix4x4_get_clipping_plane(__in            __notnull system_matrix4x4                mvp,
                                                            __in                      system_matrix4x4_clipping_plane clipping_plane,
                                                            __out_ecount(4)           float*                          out_plane_coeffs);

/** TODO */
PUBLIC EMERALD_API bool system_matrix4x4_is_equal(__in __notnull const system_matrix4x4 a,
                                                  __in __notnull const system_matrix4x4 b);

/** Releases ane xisting instance of 4x4 matrix object.
 *
 *  @param system_matrix4x4 Isntance of a 4x4 matrix object. Cannot be null.
 */
PUBLIC EMERALD_API void system_matrix4x4_release(__in __notnull __deallocate(mem) system_matrix4x4);

/** Retrieves column-major data stored in 4x4 matrix object.
 *
 *  @param system_matrix4x4 4x4 matrix to retrieve the data from.
 *
 *  @return Matrix contents in column-major order.
 */
PUBLIC EMERALD_API const float* system_matrix4x4_get_column_major_data(__in __notnull system_matrix4x4);

/** Retrieves row-major data stored in 4x4 matrix object.
 *
 *  @param system_matrix4x4 4x4 matrix to retrieve the data from.
 *
 *  @return Matrix contents in row-major order.
 */
PUBLIC EMERALD_API const float* system_matrix4x4_get_row_major_data(__in __notnull system_matrix4x4);

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
PUBLIC EMERALD_API system_matrix4x4 system_matrix4x4_create_by_mul(__in __notnull system_matrix4x4,
                                                                   __in __notnull system_matrix4x4);

/** Inverts a 4x4 matrix object.
 *
 *  @param system_matrix4x4 4x4 matrix object to invert.
 *
 *  @return True if inversion operation was successful, false otherwise.
 */
PUBLIC EMERALD_API bool system_matrix4x4_invert(__in __notnull system_matrix4x4);

/** Multiplies 4x4 matrix object by a look-at matrix. Result is stored within the matrix.
 *
 *  NOTE: Up vector must be normalized.
 *
 *  @param system_matrix4x4 4x4 matrix object to use for multiplication.
 *  @param const float*     Camera position.
 *  @param const float*     Look at point.
 *  @param const float*     Up vector.
 */
PUBLIC EMERALD_API void system_matrix4x4_multiply_by_lookat(__in __notnull             system_matrix4x4,
                                                            __in __notnull __ecount(3) const float*,
                                                            __in __notnull __ecount(3) const float*,
                                                            __in __notnull __ecount(3) const float*);

/** Multiples 4x4 matrix object by 4-dimensional user vector.
 *  Result is stored in user-provided vector.
 *
 *  @param system_matrix4x4 4x4 matrix object to use for multiplication.
 *  @param const float*     Vector to multiply the matrix with.
 *  @param const float*     Result will be stored in dereference of the pointer. Msut have space for 4 elements.
 */
PUBLIC EMERALD_API void system_matrix4x4_multiply_by_vector4(__in  __notnull             system_matrix4x4,
                                                             __in  __notnull __ecount(4) const float*,
                                                             __out __notnull __ecount(4) float*);

/** Multiples 4x4 matrix object by 3-dimensional user vector. 4th element is set to 0. 
 *  Result is stored in user-provided vector.
 *
 *  @param system_matrix4x4 4x4 matrix object to use for multiplication.
 *  @param const float*     Vector to multiply the matrix with.
 *  @param const float*     Result will be stored in dereference of the pointer. Must have space for 3 elements.
 */
PUBLIC EMERALD_API void system_matrix4x4_multiply_by_vector3(__in  __notnull             system_matrix4x4,
                                                             __in  __notnull __ecount(3) const float*,
                                                             __out __notnull __ecount(3) float*);

/** Rotates 4x4 matrix object using user provided angle and 3-diemnsional rotation vector.
 *  Result is stored in the object.
 *
 *  @param system_matrix4x4 4x4 matrix to rotate.
 *  @param float            Rotation angle.
 *  @param const float*     3-dimensional rotation vector.
 */
PUBLIC EMERALD_API void system_matrix4x4_rotate(__in __notnull system_matrix4x4,
                                                __in                       float,
                                                __in __notnull __ecount(3) const float* xyz);

/** Scales 4x4 matrix object using user provided 3-diemnsional XYZ scale vector.
 *  Result is stored in the object.
 *
 *  @param system_matrix4x4 4x4 matrix to rotate.
 *  @param const float*     3-dimensional scale vector.
 */
PUBLIC EMERALD_API void system_matrix4x4_scale(__in __notnull             system_matrix4x4,
                                               __in __notnull __ecount(3) const float* xyz);

/** TODO */
PUBLIC EMERALD_API void system_matrix4x4_set_to_float(__in __notnull system_matrix4x4,
                                                      __in           float);

/** Resets 4x4 matrix object to identity matrix. Result is stored in the object.
 *
 *  @param system_matrix4x4 4x4 matrix object to set to identity amtrix.
 */
PUBLIC EMERALD_API void system_matrix4x4_set_to_identity(__in __notnull system_matrix4x4);

/** Sets a single element of a 4x4 matrix object to user-defined value.
 *
 *  @param system_matrix4x4 4x4 matrix to modify.
 *  @param unsigned char    <column, row> pair determining index of element to modify. Use ELEMENT_INDEX() to define
 *                          the pair.
 *  @param float            New value to use.
 */
PUBLIC EMERALD_API void system_matrix4x4_set_element(__in __notnull system_matrix4x4,
                                                     __in           unsigned char,
                                                     __in           float);

/** Sets a 4x4 matrix contents, using user-provided raw data stored in column-major order.
 *
 *  @param system_matrix4x4 4x4 matrix to set.
 *  @param const float*     Data to set, stored in column-,ajor order.
 */
PUBLIC EMERALD_API void system_matrix4x4_set_from_column_major_raw(__in __notnull              system_matrix4x4,
                                                                   __in __notnull __ecount(16) const float*);

/** Sets a 4x4 matrix contents, using user-provided raw data stored in row-major order.
 *
 *  @param system_matrix4x4 4x4 matrix to set.
 *  @param const float*     Data to set, stored in row-major order.
 */
PUBLIC EMERALD_API void system_matrix4x4_set_from_row_major_raw(__in __notnull system_matrix4x4,
    __in __notnull __ecount(16) const float*);

/** Sets a 4x4 matrix contents by copying data from another 4x4 matrix.
 *
 *  @param system_matrix4x4 Destination 4x4 matrix.
 *  @param system_matrix4x4 Source 4x4 matrix
 */
PUBLIC EMERALD_API void system_matrix4x4_set_from_matrix4x4(__in __notnull system_matrix4x4,
                                                            __in __notnull system_matrix4x4);

/** Multiples 4x4 matrix object with translatino matrix. Result is stored in the object.
 *
 *  @param system_matrix4x4 4x4 matrix to translate.
 *  @param xyz              3-dimensional vector containing the translation vector.
 */
PUBLIC EMERALD_API void system_matrix4x4_translate(__in __notnull             system_matrix4x4,
                                                   __in __notnull __ecount(3) const float* xyz);

/** Transposes a 4x4 matrix object. Result is stored in the object.
 *
 *  @param system_matrix4x4 4x4 matrix to transpose.
 */
PUBLIC EMERALD_API void system_matrix4x4_transpose(__in __notnull system_matrix4x4);

/** Initializes matrix pool. Should only be called once from DLL entry point */
PUBLIC void _system_matrix4x4_init();

/** Deinitializes matrix pool. Should only be called once from DLL entry ponit */
PUBLIC void _system_matrix4x4_deinit();

#endif /* SYSTEM_MATRIX4X4_MUTEX */