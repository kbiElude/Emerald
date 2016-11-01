/**
 *
 * Emerald (kbi/elude @2015-2016)
 *
 */
#ifndef SCALAR_FIELD_METABALLS_H
#define SCALAR_FIELD_METABALLS_H

DECLARE_HANDLE(scalar_field_metaballs);

REFCOUNT_INSERT_DECLARATIONS(scalar_field_metaballs,
                             scalar_field_metaballs)


typedef enum
{
    /* float; settable.*/
    SCALAR_FIELD_METABALLS_METABALL_PROPERTY_SIZE,

    /* float[3]; settable. */
    SCALAR_FIELD_METABALLS_METABALL_PROPERTY_XYZ,

} scalar_field_metaballs_metaball_property;

typedef enum
{
    /* ral_buffer; not settable */
    SCALAR_FIELD_METABALLS_PROPERTY_DATA_BO_RAL,

    /* unsigned int; settable.
     *
     * Under OpenGL 4.3, the maximum platform-cross value you could use is (16384 - 16) / sizeof(float) / 4 = 1023.
     * scalar_field_metaballs may accept larger values, depending on what the reported platform-specific value of
     * GL_MAX_UNIFORM_BLOCK_SIZE is, but the solution may not work across all hardware architectures.
     *
     * Any modification of this value will result in an UB update at scalar_field_metaballs_update() call time.
     *
     * Default value: 0.
     */
     SCALAR_FIELD_METABALLS_PROPERTY_N_METABALLS,

     /* bool; not settable */
     SCALAR_FIELD_METABALLS_PROPERTY_RESULT_BUFFER_UPDATE_NEEDED,

} scalar_field_metaballs_property;

/** TODO */
PUBLIC EMERALD_API scalar_field_metaballs scalar_field_metaballs_create(ral_context               context,
                                                                        const unsigned int*       grid_size_xyz,
                                                                        system_hashed_ansi_string name);

/** Returns a present task instance which updates the scalar field data (if necessary), and exposes
 *  the data buffer at 0th unique output.
 *
 *  @param metaballs Metaballs instance to use for the request.
 *
 *  @return As per description.
 **/
PUBLIC EMERALD_API ral_present_task scalar_field_metaballs_get_present_task(scalar_field_metaballs metaballs);

/** TODO */
PUBLIC EMERALD_API void scalar_field_metaballs_get_property(scalar_field_metaballs          metaballs,
                                                            scalar_field_metaballs_property property,
                                                            void*                           out_result);

/** TODO */
PUBLIC EMERALD_API void scalar_field_metaballs_set_metaball_property(scalar_field_metaballs                   metaballs,
                                                                     scalar_field_metaballs_metaball_property property,
                                                                     unsigned int                             n_metaball,
                                                                     const void*                              data);

/** TODO */
PUBLIC EMERALD_API void scalar_field_metaballs_set_property(scalar_field_metaballs          metaballs,
                                                            scalar_field_metaballs_property property,
                                                            const void*                     data);

#endif /* SCALAR_FIELD_METABALLS_H */
