/**
 *
 * Emerald (kbi/elude @2014-2016)
 *
 */
#include "shared.h"
#include "collada/collada_data_transformation.h"
#include "collada/collada_value.h"
#include "system/system_assertions.h"
#include "system/system_resizable_vector.h"

/** Describes a single transformation as found under
 *  <visual_scene>/<node> */
typedef struct _collada_data_transformation
{
    system_hashed_ansi_string         sid;
    _collada_data_transformation_type type;

    /* Corresponds to an instance of:
     *
     * _collada_data_transformation_lookat    if LOOKAT    type is used.
     * _collada_data_transformation_matrix    if MATRIX    type is used.
     * _collada_data_transformation_rotate    if ROTATE    type is used.
     * _collada_data_transformation_scale     if SCALE     type is used.
     * _collada_data_transformation_skew      if SKEW      type is used.
     * _collada_data_transformation_translate if TRANSLATE type is used.
     */
    void* data;

    explicit _collada_data_transformation(_collada_data_transformation_type in_type);
            ~_collada_data_transformation();
} _collada_data_transformation;

/** Describes a lookat transformation (as per <lookat>) */
typedef struct _collada_data_transformation_lookat
{
    collada_value eye              [3];
    collada_value interest_position[3];
    collada_value up               [3];

    /* Constructor */
    explicit _collada_data_transformation_lookat(const float* in_eye_ptr,
                                                 const float* in_interest_position_ptr,
                                                 const float* in_up_ptr);
    /* Destructor */
    ~_collada_data_transformation_lookat();
} _collada_data_transformation_lookat;

/** Describes a matrix transformation (as per <matrix>) */
typedef struct _collada_data_transformation_matrix
{
    /* NOTE: Row-by-row data ordering */
    collada_value data[4 * 4];

    /* Constructor */
    explicit _collada_data_transformation_matrix(const float* in_data_ptr)
    {
        for (uint32_t n = 0;
                      n < 4 * 4;
                    ++n)
        {
            data[n] = collada_value_create(COLLADA_VALUE_TYPE_FLOAT,
                                           in_data_ptr + n);
        }
    }

    /* Destructor */
    ~_collada_data_transformation_matrix()
    {
        for (uint32_t n = 0;
                      n < 4 * 4;
                    ++n)
        {
            if (data[n] != nullptr)
            {
                collada_value_release(data[n]);

                data[n] = nullptr;
            }
        }
    }
} _collada_data_transformation_matrix;

/** Describes a rotate transformation (as per <rotate>) */
typedef struct _collada_data_transformation_rotate
{
    collada_value axis[3];
    collada_value angle; /* expressed in degrees */

    /* Constructor */
    explicit _collada_data_transformation_rotate(const float* in_axis_ptr,
                                                       float  in_angle)
    {
        axis[0] = collada_value_create(COLLADA_VALUE_TYPE_FLOAT,
                                       in_axis_ptr + 0);
        axis[1] = collada_value_create(COLLADA_VALUE_TYPE_FLOAT,
                                       in_axis_ptr + 1);
        axis[2] = collada_value_create(COLLADA_VALUE_TYPE_FLOAT,
                                       in_axis_ptr + 2);
        angle   = collada_value_create(COLLADA_VALUE_TYPE_FLOAT,
                                      &in_angle);
    }

    ~_collada_data_transformation_rotate()
    {
        if (axis[0] != nullptr)
        {
            collada_value_release(axis[0]);

            axis[0] = nullptr;
        }

        if (axis[1] != nullptr)
        {
            collada_value_release(axis[1]);

            axis[1] = nullptr;
        }

        if (axis[2] != nullptr)
        {
            collada_value_release(axis[2]);

            axis[2] = nullptr;
        }

        if (angle != nullptr)
        {
            collada_value_release(angle);

            angle = nullptr;
        }
    }
} _collada_data_transformation_rotate;

/** Describes a scale transformation (as per <scale>) */
typedef struct _collada_data_transformation_scale
{
    collada_value data[3];

    /* Constructor */
    explicit _collada_data_transformation_scale(const float* in_data_ptr)
    {
        for (uint32_t n = 0;
                      n < 3;
                    ++n)
        {
            data[n] = collada_value_create(COLLADA_VALUE_TYPE_FLOAT,
                                           in_data_ptr + n);
        }
    }

    ~_collada_data_transformation_scale()
    {
        for (uint32_t n = 0;
                      n < 3;
                    ++n)
        {
            if (data[n] != nullptr)
            {
                collada_value_release(data[n]);

                data[n] = nullptr;
            }
        }
    }
} _collada_data_transformation_scale;

/** Describes a skew transformation (as per <skew>) */
typedef struct _collada_data_transformation_skew
{
    collada_value angle;              /* degrees */
    collada_value rotation_axis   [3];
    collada_value translation_axis[3];

    /* Constructor */
    explicit _collada_data_transformation_skew(float        in_angle,
                                               const float* in_rotation_axis_ptr,
                                               const float* in_translation_axis_ptr)
    {
        angle = collada_value_create(COLLADA_VALUE_TYPE_FLOAT,
                                   &in_angle);

        for (uint32_t n = 0;
                      n < 3;
                    ++n)
        {
            rotation_axis   [n] = collada_value_create(COLLADA_VALUE_TYPE_FLOAT,
                                                       in_rotation_axis_ptr + n);
            translation_axis[n] = collada_value_create(COLLADA_VALUE_TYPE_FLOAT,
                                                       in_translation_axis_ptr + n);
        }
    }

    /* Destructor */
    ~_collada_data_transformation_skew()
    {
        if (angle != nullptr)
        {
            collada_value_release(angle);

            angle = nullptr;
        }

        for (uint32_t n = 0;
                      n < 3;
                    ++n)
        {
            if (rotation_axis[n] != nullptr)
            {
                collada_value_release(rotation_axis[n]);

                rotation_axis[n] = nullptr;
            }

            if (translation_axis[n] != nullptr)
            {
                collada_value_release(translation_axis[n]);

                translation_axis[n] = nullptr;
            }
        }
    }
} _collada_data_transformation_skew;

/** Describes a translate transformation (as per <translate>) */
typedef struct _collada_data_transformation_translate
{
    collada_value data[3];

    /* Constructor */
    explicit _collada_data_transformation_translate(const float* in_data)
    {
        for (uint32_t n = 0;
                      n < 3;
                    ++n)
        {
            data[n] = collada_value_create(COLLADA_VALUE_TYPE_FLOAT,
                                           in_data + n);
        }
    }

    ~_collada_data_transformation_translate()
    {
        for (uint32_t n = 0;
                      n < 3;
                    ++n)
        {
            if (data[n] != nullptr)
            {
                collada_value_release(data[n]);

                data[n] = nullptr;
            }
        }
    }
} _collada_data_transformation_translate;


/** TODO */
_collada_data_transformation::_collada_data_transformation(_collada_data_transformation_type in_type)
{
    data = nullptr;
    sid  = system_hashed_ansi_string_get_default_empty_string();
    type = in_type;
}

/** TODO */
_collada_data_transformation::~_collada_data_transformation()
{
    if (data != nullptr)
    {
        switch (type)
        {
            case COLLADA_DATA_TRANSFORMATION_TYPE_LOOKAT:
            {
                delete reinterpret_cast<_collada_data_transformation_lookat*>(data);

                break;
            }

            case COLLADA_DATA_TRANSFORMATION_TYPE_MATRIX:
            {
                delete reinterpret_cast<_collada_data_transformation_matrix*>(data);

                break;
            }

            case COLLADA_DATA_TRANSFORMATION_TYPE_ROTATE:
            {
                delete reinterpret_cast<_collada_data_transformation_rotate*>(data);

                break;
            }

            case COLLADA_DATA_TRANSFORMATION_TYPE_SCALE:
            {
                delete reinterpret_cast<_collada_data_transformation_scale*>(data);

                break;
            }

            case COLLADA_DATA_TRANSFORMATION_TYPE_SKEW:
            {
                delete reinterpret_cast<_collada_data_transformation_skew*>(data);

                break;
            }

            case COLLADA_DATA_TRANSFORMATION_TYPE_TRANSLATE:
            {
                delete reinterpret_cast<_collada_data_transformation_translate*>(data);

                break;
            }

            default:
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Unrecognized transformation type encountered");
            }
        }
    }
}

/** TODO */
_collada_data_transformation_lookat::_collada_data_transformation_lookat(const float* in_eye_ptr,
                                                                         const float* in_interest_position_ptr,
                                                                         const float* in_up_ptr)
{
    for (uint32_t n_component = 0;
                  n_component < 3;
                ++n_component)
    {
        eye              [n_component] = collada_value_create(COLLADA_VALUE_TYPE_FLOAT,
                                                              in_eye_ptr + n_component);
        interest_position[n_component] = collada_value_create(COLLADA_VALUE_TYPE_FLOAT,
                                                              in_interest_position_ptr + n_component);
        up               [n_component] = collada_value_create(COLLADA_VALUE_TYPE_FLOAT,
                                                              in_up_ptr + n_component);
    }
}

/** TODO */
_collada_data_transformation_lookat::~_collada_data_transformation_lookat()
{
    for (uint32_t n_component = 0;
                  n_component < 3;
                ++n_component)
    {
        if (eye[n_component] != nullptr)
        {
            collada_value_release(eye[n_component]);

            eye[n_component] = nullptr;
        }

        if (interest_position[n_component] != nullptr)
        {
            collada_value_release(interest_position[n_component]);

            interest_position[n_component] = nullptr;
        }

        if (up[n_component] != nullptr)
        {
            collada_value_release(up[n_component]);

            up[n_component] = nullptr;
        }
    }
}


/** Please see header for specification */
PUBLIC collada_data_transformation collada_data_transformation_create_lookat(tinyxml2::XMLElement* element_ptr,
                                                                             const float*          data_ptr)
{
    _collada_data_transformation*        new_transformation_ptr      = new (std::nothrow) _collada_data_transformation       (COLLADA_DATA_TRANSFORMATION_TYPE_LOOKAT);
    _collada_data_transformation_lookat* new_transformation_data_ptr = new (std::nothrow) _collada_data_transformation_lookat(data_ptr,
                                                                                                                              data_ptr + 3,
                                                                                                                              data_ptr + 6);

    ASSERT_DEBUG_SYNC(new_transformation_ptr      != nullptr &&
                      new_transformation_data_ptr != nullptr,
                      "Out of memory");

    if (new_transformation_ptr      == nullptr ||
        new_transformation_data_ptr == nullptr)
    {
        goto end;
    }

    new_transformation_ptr->data = new_transformation_ptr;
    new_transformation_ptr->sid  = system_hashed_ansi_string_create(element_ptr->Attribute("sid") );

end:
    return (collada_data_transformation) new_transformation_ptr;
}

/** Please see header for specification */
PUBLIC collada_data_transformation collada_data_transformation_create_matrix(tinyxml2::XMLElement* element_ptr,
                                                                             const float*          data_ptr)
{
    _collada_data_transformation*        new_transformation_ptr      = new (std::nothrow) _collada_data_transformation       (COLLADA_DATA_TRANSFORMATION_TYPE_MATRIX);
    _collada_data_transformation_matrix* new_transformation_data_ptr = new (std::nothrow) _collada_data_transformation_matrix(data_ptr);

    ASSERT_DEBUG_SYNC(new_transformation_ptr      != nullptr &&
                      new_transformation_data_ptr != nullptr,
                      "Out of memory");

    if (new_transformation_ptr      == nullptr ||
        new_transformation_data_ptr == nullptr)
    {
        goto end;
    }

    new_transformation_ptr->data = new_transformation_data_ptr;
    new_transformation_ptr->sid  = system_hashed_ansi_string_create(element_ptr->Attribute("sid") );

end:
    return (collada_data_transformation) new_transformation_ptr;
}

/** Please see header for specification */
PUBLIC collada_data_transformation collada_data_transformation_create_rotate(tinyxml2::XMLElement* element_ptr,
                                                                             const float*          data_ptr)
{
    _collada_data_transformation*        new_transformation_ptr      = new (std::nothrow) _collada_data_transformation       (COLLADA_DATA_TRANSFORMATION_TYPE_ROTATE);
    _collada_data_transformation_rotate* new_transformation_data_ptr = new (std::nothrow) _collada_data_transformation_rotate(data_ptr,
                                                                                                                              *(data_ptr + 3) );

    ASSERT_DEBUG_SYNC(new_transformation_ptr      != nullptr &&
                      new_transformation_data_ptr != nullptr,
                      "Out of memory");

    if (new_transformation_ptr      == nullptr ||
        new_transformation_data_ptr == nullptr)
    {
        goto end;
    }

    new_transformation_ptr->data = new_transformation_data_ptr;
    new_transformation_ptr->sid  = system_hashed_ansi_string_create(element_ptr->Attribute("sid") );

end:
    return (collada_data_transformation) new_transformation_ptr;
}

/** Please see header for specification */
PUBLIC collada_data_transformation collada_data_transformation_create_scale(tinyxml2::XMLElement* element_ptr,
                                                                            const float*          data_ptr)
{
    _collada_data_transformation*       new_transformation_ptr      = new (std::nothrow) _collada_data_transformation      (COLLADA_DATA_TRANSFORMATION_TYPE_SCALE);
    _collada_data_transformation_scale* new_transformation_data_ptr = new (std::nothrow) _collada_data_transformation_scale(data_ptr);

    ASSERT_DEBUG_SYNC(new_transformation_ptr      != nullptr &&
                      new_transformation_data_ptr != nullptr,
                      "Out of memory");

    if (new_transformation_ptr      == nullptr ||
        new_transformation_data_ptr == nullptr)
    {
        goto end;
    }

    new_transformation_ptr->data = new_transformation_data_ptr;
    new_transformation_ptr->sid  = system_hashed_ansi_string_create(element_ptr->Attribute("sid") );

end:
    return (collada_data_transformation) new_transformation_ptr;
}

/** Please see header for specification */
PUBLIC collada_data_transformation collada_data_transformation_create_skew(tinyxml2::XMLElement* element_ptr,
                                                                           const float*          data_ptr)
{
    _collada_data_transformation*      new_transformation_ptr      = new (std::nothrow) _collada_data_transformation     (COLLADA_DATA_TRANSFORMATION_TYPE_SKEW);
    _collada_data_transformation_skew* new_transformation_data_ptr = new (std::nothrow) _collada_data_transformation_skew(*data_ptr,
                                                                                                                           data_ptr + 1,
                                                                                                                           data_ptr + 4);

    ASSERT_DEBUG_SYNC(new_transformation_ptr      != nullptr &&
                      new_transformation_data_ptr != nullptr,
                      "Out of memory");

    if (new_transformation_ptr      == nullptr ||
        new_transformation_data_ptr == nullptr)
    {
        goto end;
    }

    new_transformation_ptr->data = new_transformation_data_ptr;
    new_transformation_ptr->sid  = system_hashed_ansi_string_create(element_ptr->Attribute("sid") );

end:
    return (collada_data_transformation) new_transformation_ptr;
}

/** Please see header for specification */
PUBLIC collada_data_transformation collada_data_transformation_create_translate(tinyxml2::XMLElement* element_ptr,
                                                                                const float*          data_ptr)
{
    _collada_data_transformation*           new_transformation_ptr      = new (std::nothrow) _collada_data_transformation          (COLLADA_DATA_TRANSFORMATION_TYPE_TRANSLATE);
    _collada_data_transformation_translate* new_transformation_data_ptr = new (std::nothrow) _collada_data_transformation_translate(data_ptr);

    ASSERT_DEBUG_SYNC(new_transformation_ptr      != nullptr &&
                      new_transformation_data_ptr != nullptr,
                      "Out of memory");

    if (new_transformation_ptr      == nullptr ||
        new_transformation_data_ptr == nullptr)
    {
        goto end;
    }

    new_transformation_ptr->data = new_transformation_data_ptr;
    new_transformation_ptr->sid  = system_hashed_ansi_string_create(element_ptr->Attribute("sid") );

end:
    return (collada_data_transformation) new_transformation_ptr;
}

/* Please see header for specification */
PUBLIC EMERALD_API void collada_data_transformation_get_matrix_properties(collada_data_transformation transformation,
                                                                          collada_value*              out_data_ptr)
{
    _collada_data_transformation* transformation_ptr = reinterpret_cast<_collada_data_transformation*>(transformation);

    if (out_data_ptr != nullptr)
    {
        ASSERT_DEBUG_SYNC(transformation_ptr->type == COLLADA_DATA_TRANSFORMATION_TYPE_MATRIX,
                          "Requested matrix data from a non-matrix transformation object");

        _collada_data_transformation_matrix* data_ptr = reinterpret_cast<_collada_data_transformation_matrix*>(transformation_ptr->data);

        memcpy(out_data_ptr,
               data_ptr->data,
               sizeof(data_ptr->data) );
    }
}

/* Please see header for specification */
PUBLIC EMERALD_API void collada_data_transformation_get_property(collada_data_transformation          transformation,
                                                                 collada_data_transformation_property property,
                                                                 void*                                out_result_ptr)
{
    _collada_data_transformation* transformation_ptr = reinterpret_cast<_collada_data_transformation*>(transformation);

    switch (property)
    {
        case COLLADA_DATA_TRANSFORMATION_PROPERTY_DATA_HANDLE:
        {
            *reinterpret_cast<void**>(out_result_ptr) = transformation_ptr->data;

            break;
        }

        case COLLADA_DATA_TRANSFORMATION_PROPERTY_SID:
        {
            *reinterpret_cast<system_hashed_ansi_string*>(out_result_ptr) = transformation_ptr->sid;

            break;
        }

        case COLLADA_DATA_TRANSFORMATION_PROPERTY_TYPE:
        {
            *reinterpret_cast<_collada_data_transformation_type*>(out_result_ptr) = transformation_ptr->type;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized collada_data_transformation_property value");
        }
    }
}

/* Please see header for specification */
PUBLIC EMERALD_API void collada_data_transformation_get_rotate_properties(collada_data_transformation transformation,
                                                                          collada_value*              out_axis_vector_ptr,
                                                                          collada_value*              out_angle_ptr)
{
    _collada_data_transformation*        transformation_ptr = reinterpret_cast<_collada_data_transformation*>       (transformation);
    _collada_data_transformation_rotate* rotate_data_ptr    = reinterpret_cast<_collada_data_transformation_rotate*>(transformation_ptr->data);

    ASSERT_DEBUG_SYNC(transformation_ptr->type == COLLADA_DATA_TRANSFORMATION_TYPE_ROTATE,
                          "Requested rotation data from a non-rotate transformation object");

    memcpy(out_axis_vector_ptr,
           rotate_data_ptr->axis,
           sizeof(rotate_data_ptr->axis) );

    *out_angle_ptr = rotate_data_ptr->angle;
}

/* Please see header for specification */
PUBLIC EMERALD_API void collada_data_transformation_get_scale_properties(collada_data_transformation transformation,
                                                                         collada_value*              out_scale_vector_ptr)
{
    _collada_data_transformation*       transformation_ptr = reinterpret_cast<_collada_data_transformation*>      (transformation);
    _collada_data_transformation_scale* scale_data_ptr     = reinterpret_cast<_collada_data_transformation_scale*>(transformation_ptr->data);

    ASSERT_DEBUG_SYNC(transformation_ptr->type == COLLADA_DATA_TRANSFORMATION_TYPE_SCALE,
                          "Requested scaling data from a non-scale transformation object");

    memcpy(out_scale_vector_ptr,
           scale_data_ptr->data,
           sizeof(scale_data_ptr->data) );
}

/* Please see header for specification */
PUBLIC EMERALD_API void collada_data_transformation_get_translate_properties(collada_data_transformation transformation,
                                                                             collada_value*              out_translation_vector_ptr)
{
    _collada_data_transformation*           transformation_ptr = reinterpret_cast<_collada_data_transformation*>          (transformation);
    _collada_data_transformation_translate* translate_data_ptr = reinterpret_cast<_collada_data_transformation_translate*>(transformation_ptr->data);

    ASSERT_DEBUG_SYNC(transformation_ptr->type == COLLADA_DATA_TRANSFORMATION_TYPE_TRANSLATE,
                          "Requested translation data from a non-translate transformation object");

    memcpy(out_translation_vector_ptr,
           translate_data_ptr->data,
           sizeof(translate_data_ptr->data) );
}

/* Please see header for spec */
PUBLIC EMERALD_API void collada_data_transformation_release(collada_data_transformation transformation)
{
    delete reinterpret_cast<_collada_data_transformation*>(transformation);

    transformation = nullptr;
}