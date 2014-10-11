/**
 *
 * Emerald (kbi/elude @2012)
 *
 */
#include "shared.h"
#include "sh/sh_config.h"
#include "sh/sh_rot_precalc.h"
#include "system/system_log.h"

#ifdef INCLUDE_KRIVANEK_FAST_SH_Y_ROTATION

/* Internal variables */
typedef struct
{
    int32_t  current_band; /* >= 0 */
    uint32_t current_rotation_angle_degrees;
    float    current_rotation_angle_radians;
    int32_t  n_bands;
    uint32_t n_indexes_written;
    uint32_t n_matrix_coeffs_for_rotation;

    float* raw_data;
} _sh_derivative_rotation_matrices;

const double _sh_rot_precalc_factorial[] = {1.0,    /* 0! */
                                            1.0,
                                            2.0,
                                            6.0,
                                            24.0,
                                            120.0,  /* 5! */
                                            720.0,
                                            5040.0,
                                            40320.0,
                                            362880.0,
                                            3628800.0, /* 10! */
                                            39916800.0,
                                            479001600.0,
                                            6227020800.0,
                                            87178291200.0,
                                            1307674368000.0, /* 15! */
                                            20922789888000.0,
                                            355687428096000.0,
                                            6402373705728000.0,
                                            121645100408832000.0,
                                            2432902008176640000.0, /* 20! */
                                            51090942171709440000.0,
                                            1124000727777607680000.0,
                                            25852016738884976640000.0,
                                            620448401733239439360000.0,
                                            15511210043330985984000000.0, /* 25! */
                                            403291461126605635584000000.0,
                                            10888869450418352160768000000.0,
                                            304888344611713860501504000000.0,
                                            8841761993739701954543616000000.0,
                                            265252859812191058636308480000000.0, /* 30! */
                                            8222838654177922817725562880000000.0,
                                            263130836933693530167218012160000000.0,
                                            8683317618811886495518194401280000000.0};


/* Forward declarationss */
float _sh_rot_precalc_cos_k                                 (int k, float angle);
float _sh_rot_precalc_krivanek_calculate_coeff              (_sh_derivative_rotation_matrices* tmp, int k, int l, int m, int n);
float _sh_rot_precalc_krivanek_get_matrix_coeff_for_rotation(_sh_derivative_rotation_matrices* data, int k, int l, int m, int n);
float _sh_rot_precalc_krivanek_predefined_R                 (int k, int l, int m_1, int m_2, float beta);
float _sh_rot_precalc_krivanek_d_P                          (_sh_derivative_rotation_matrices* data_ptr, int k, int l, int m_1, int m_2, int i);
float _sh_rot_precalc_krivanek_d_T                          (_sh_derivative_rotation_matrices* data_ptr, int k, int l, int m_1, int m_2, int l2, int m_12, int m_22);
float _sh_rot_precalc_krivanek_d_u                          (_sh_derivative_rotation_matrices* data_ptr, int k, int l, int m_1, int m_2);
float _sh_rot_precalc_krivanek_d_v                          (_sh_derivative_rotation_matrices* data_ptr, int k, int l, int m_1, int m_2);
float _sh_rot_precalc_krivanek_d_w                          (_sh_derivative_rotation_matrices* data_ptr, int k, int l, int m_1, int m_2);
float _sh_rot_precalc_sin_k                                 (int k, float angle);
float _sh_rot_precalc_u                                     (int l, int m, int n);
float _sh_rot_precalc_v                                     (int l, int m, int n);
float _sh_rot_precalc_w                                     (int l, int m, int n);


/* TODO */
float _sh_rot_precalc_krivanek_calculate_coeff(_sh_derivative_rotation_matrices* tmp, int k, int l, int m, int n)
{
    if (l < 2)
    {
        return _sh_rot_precalc_krivanek_predefined_R(k, l, m, n, tmp->current_rotation_angle_radians);
    }
    else
    {
        float u_coeff = _sh_rot_precalc_u(l, m, n);
        float v_coeff = _sh_rot_precalc_v(l, m, n);
        float w_coeff = _sh_rot_precalc_w(l, m, n);

        return (u_coeff != 0 ? u_coeff * _sh_rot_precalc_krivanek_d_u(tmp, k, l, m, n) : 0) +
               (v_coeff != 0 ? v_coeff * _sh_rot_precalc_krivanek_d_v(tmp, k, l, m, n) : 0) + 
               (w_coeff != 0 ? w_coeff * _sh_rot_precalc_krivanek_d_w(tmp, k, l, m, n) : 0);
    }
}

/* TODO */
float _sh_rot_precalc_cos_k(int k, float angle)
{
    register int k_mod_4 = k % 4;

    if (k_mod_4 == 0) return  cos(angle);
    if (k_mod_4 == 1) return -sin(angle);
    if (k_mod_4 == 2) return -cos(angle);

    return sin(angle);
}

/* TODO
 *
 * k >= 0
 * l >= 0
 * m >  -n_band && m <  n_band
 * n >  -n_band && n <  n_band
*/
float _sh_rot_precalc_krivanek_get_matrix_coeff_for_rotation(_sh_derivative_rotation_matrices* data, int k, int l, int m, int n)
{
    uint32_t n_coefficients_till_band_l = sh_derivative_rotation_matrices_get_n_coefficients_till_band(l);

    m += l;
    n += l;

    if (m >= 0 && n >= 0 && m < (2 * l + 1) && n < (2 * l + 1) )
    {
        uint32_t index = k * data->n_matrix_coeffs_for_rotation  * 360                                + 
                            data->current_rotation_angle_degrees * data->n_matrix_coeffs_for_rotation +
                            n_coefficients_till_band_l                                                +
                            n * (2 * l + 1) + m;

        ASSERT_DEBUG_SYNC(index < data->n_indexes_written, "");
        
        #ifdef _DEBUG
        {
            ASSERT_DEBUG_SYNC(data->raw_data[index] > -665.0f, "Accessing element, value of which has not been calculated yet!");
        }
        #endif

        return data->raw_data[index];
    }
    else
    {
        /* We reached a location that is hit due to bugs in Krivanek's paper.. */
        //ASSERT_DEBUG_SYNC(false, "This place is hit many times due to an error. It should never be reached but it is, apparently due to incorrect m_1 value being out of scope for index l..");

        /* Need to go all over again for this one.. */
        //return _sh_rot_precalc_krivanek_calculate_coeff(data, k, l, m - l, n - l);
        return 0;
    }
}

/* TODO */
float _sh_rot_precalc_sin_k(int k, float angle)
{
    register int k_mod_4 = k % 4;

    if (k_mod_4 == 0) return  sin(angle);
    if (k_mod_4 == 1) return  cos(angle);
    if (k_mod_4 == 2) return -sin(angle);
    
    return -cos(angle);
}

/* TODO */
float _sh_rot_precalc_krivanek_d_P(_sh_derivative_rotation_matrices* data_ptr, int k, int l, int m_1, int m_2, int i)
{
    if (abs(m_2) < l)
    {
        return _sh_rot_precalc_krivanek_d_T(data_ptr, k, 1, i, 0, l - 1, m_1, m_2);
    }
    else
    if (m_2 == l)
    {
        return _sh_rot_precalc_krivanek_d_T(data_ptr, k, 1, i,  1, l - 1, m_1,  l - 1) - 
               _sh_rot_precalc_krivanek_d_T(data_ptr, k, 1, i, -1, l - 1, m_1, -l + 1);
    }
    else
    if (m_2 == -l)
    {
        return _sh_rot_precalc_krivanek_d_T(data_ptr, k, 1, i,  1, l - 1, m_1, -l + 1) + 
               _sh_rot_precalc_krivanek_d_T(data_ptr, k, 1, i, -1, l - 1, m_1,  l - 1);
    }

    return 0;
}

/* TODO */
float _sh_rot_precalc_krivanek_d_T(_sh_derivative_rotation_matrices* data_ptr, int k, int l, int m_1, int m_2, int l2, int m_12, int m_22)
{
    double result           = 0.0f;
    float  R_k_with_l_m1_m2 = _sh_rot_precalc_krivanek_get_matrix_coeff_for_rotation(data_ptr, k, l, m_1, m_2);

    if (R_k_with_l_m1_m2 != 0)
    {
        for (int i = 0; i <= k; i++)
        { 
            float R_k_minus_i_with_l2_m12_m22 = _sh_rot_precalc_krivanek_get_matrix_coeff_for_rotation(data_ptr, k - i, l2, m_12, m_22);

            result += double(_sh_rot_precalc_factorial[k]) / double(_sh_rot_precalc_factorial[i] * _sh_rot_precalc_factorial[k - i]) * double(R_k_with_l_m1_m2 * R_k_minus_i_with_l2_m12_m22);
        }
    }

    return float(result);
}

/* TODO */
float _sh_rot_precalc_krivanek_d_u(_sh_derivative_rotation_matrices* data_ptr, int k, int l, int m_1, int m_2)
{
    return _sh_rot_precalc_krivanek_d_P(data_ptr, k, l, m_1, m_2, 0);
}

/* TODO */
float _sh_rot_precalc_krivanek_d_v(_sh_derivative_rotation_matrices* data_ptr, int k, int l, int m_1, int m_2)
{
    if (m_1 > 1)
    {
        return _sh_rot_precalc_krivanek_d_P(data_ptr, k, l,  m_1 - 1, m_2,  1) -
               _sh_rot_precalc_krivanek_d_P(data_ptr, k, l, -m_1 + 1, m_2, -1);
    }
    else
    if (m_1 == 1)
    {
        return sqrt(2.0f) * _sh_rot_precalc_krivanek_d_P(data_ptr, k, l, 0, m_2, 1);
    }
    else
    if (m_1 == 0)
    {
        return _sh_rot_precalc_krivanek_d_P(data_ptr, k, l,  1, m_2,  1) + 
               _sh_rot_precalc_krivanek_d_P(data_ptr, k, l, -1, m_2, -1);
    }
    else
    if (m_1 == -1)
    {
        return sqrt(2.0f) * _sh_rot_precalc_krivanek_d_P(data_ptr, k, l, 0, m_2, -1);
    }
    else
    {
        return _sh_rot_precalc_krivanek_d_P(data_ptr, k, l, -m_1 - 1, m_2, -1) + 
               _sh_rot_precalc_krivanek_d_P(data_ptr, k, l,  m_1 + 1, m_2,  1);
    }
}

/* TODO */
float _sh_rot_precalc_krivanek_d_w(_sh_derivative_rotation_matrices* data_ptr, int k, int l, int m_1, int m_2)
{
    if (m_1 > 0)
    {
        return _sh_rot_precalc_krivanek_d_P(data_ptr, k, l,  m_1 + 1, m_2,  1) +
               _sh_rot_precalc_krivanek_d_P(data_ptr, k, l, -m_1 - 1, m_2, -1);
    }
    else
    {
        return _sh_rot_precalc_krivanek_d_P(data_ptr, k, l,  m_1 - 1, m_2,  1) - 
               _sh_rot_precalc_krivanek_d_P(data_ptr, k, l, -m_1 + 1, m_2, -1);
    }
}

/* TODO */
float _sh_rot_precalc_krivanek_predefined_R(int k, int l, int m_1, int m_2, float beta)
{
    if (l == 0) return float(k > 0 ? 0 : 1);
    else
    {
        if (m_1 == -1)
        {
            if (m_2 == -1) return float(k > 0 ? 0 : 1);

            return 0;
        }
        else
        if (m_1 == 0)
        {
            if (m_2 == -1) return 0;
            if (m_2 ==  0) return _sh_rot_precalc_cos_k(k, beta);

            return -_sh_rot_precalc_sin_k(k, beta);
        }

        if (m_2 == -1) return 0;
        if (m_2 ==  0) return _sh_rot_precalc_sin_k(k, beta);

        return _sh_rot_precalc_cos_k(k, beta);
    }

}

/* TODO */
float _sh_rot_precalc_u(int l, int m, int n)
{
    if (abs(n) < l)
    {
        return sqrt( float((l + m) * (l - m)) / float((l + n) * (l - n)) );
    }
    else
    {
        return sqrt( float((l + m) * (l - m)) / float((2 * l) * (2 * l - 1)) );
    }
}

/* TODO */
float _sh_rot_precalc_v(int l, int m, int n)
{
    float delta_m_0 = float(m == 0 ? 1.0f : 0.0f);

    if (abs(n) < l)
    {
        return 0.5f * sqrt( (1 + delta_m_0) * (l + abs(m) - 1) * float(l + abs(m) ) / float((l + n) * (l - n)) )    * (1 - 2 * delta_m_0);
    }
    else
    {
        return 0.5f * sqrt( (1 + delta_m_0) * (l + abs(m) - 1) * float(l + abs(m) ) / float((2 * l) * (2 * l - 1)) ) * (1 - 2 * delta_m_0);
    }
}

/* TODO */
float _sh_rot_precalc_w(int l, int m, int n)
{
    float delta_m_0 = (m == 0 ? 1.0f : 0.0f);

    if (abs(n) < l)
    {
        return -0.5f * sqrt( (l - abs(m) - 1) * float(l - abs(m)) / float((l + n) * (l - n)) ) * (1 - delta_m_0);
    }
    else
    {
        return -0.5f * sqrt( (l + abs(m) - 1) * float(l + abs(m)) / float((2 * l) * (2 * l - 1)) ) * (1 - delta_m_0);
    }
}                                                   

/* Please see header for specification */
PUBLIC sh_derivative_rotation_matrices sh_derivative_rotation_matrices_create(__in uint32_t n_bands, __in uint32_t n_derivatives)
{
    _sh_derivative_rotation_matrices* tmp = new (std::nothrow) _sh_derivative_rotation_matrices;

    ASSERT_DEBUG_SYNC(tmp != NULL, "Out of memory");
    if (tmp != NULL)
    {
        uint32_t n_result_floats = 0;

        tmp->n_bands                      = n_bands;
        tmp->n_indexes_written            = 0;
        tmp->n_matrix_coeffs_for_rotation = sh_derivative_rotation_matrices_get_n_coefficients_till_band(n_bands);
        n_result_floats                   = tmp->n_matrix_coeffs_for_rotation * 360 * n_derivatives;
        tmp->raw_data                     = new float[n_result_floats];

        float* result_ptr = tmp->raw_data;

        memset(tmp->raw_data, 0, n_result_floats * sizeof(float) );

        #ifdef _DEBUG
        {
            for (uint32_t n = 0; n < n_result_floats; n++)
            {
                tmp->raw_data[n] = -666.0f;
            }
        }
        #endif
        /* Let's loop! */
        for (uint32_t n_derivative = 0; n_derivative < n_derivatives; ++n_derivative)
        {
            for (uint32_t angle = 0; angle < 360; ++angle)
            {
                tmp->current_rotation_angle_degrees = angle;
                tmp->current_rotation_angle_radians = float(angle) / 360.0f * 2 * 3.14152965f;
                
                for (int32_t n_band = 0; n_band < (int32_t) tmp->n_bands; ++n_band)
                {
                    uint32_t n_matrix_coeffs_for_band = (2 * n_band + 1) * (2 * n_band + 1);

                    tmp->current_band = n_band;

                    for (uint32_t n_matrix_coeff = 0; n_matrix_coeff < n_matrix_coeffs_for_band; ++n_matrix_coeff)
                    {
                        int m_1 = (-n_band) + n_matrix_coeff % (n_band * 2 + 1);
                        int m_2 = (-n_band) + n_matrix_coeff / (n_band * 2 + 1);

                        if (tmp->n_indexes_written == 90670)
                        {
                            int a = 1; a++;
                        }

                        *result_ptr = _sh_rot_precalc_krivanek_calculate_coeff(tmp, n_derivative, n_band, m_1, m_2);
                       
                        result_ptr            ++;
                        tmp->n_indexes_written++;
                    } /* for (uint32_t n_matrix_coeff = 0; n_matrix_coeff < tmp->n_matrix_coeffs_for_rotation; ++n_matrix_coeff) */
                } /* for (uint32_t n_band = 0; n_band < tmp->n_bands; ++n_band)*/
            } /* for (uint32_t angle = 0; angle < 360; ++angle) */
        } /* for (uint32_t n_derivative = 0; n_derivative < n_derivatives; ++n_derivative) */
    }
    
    return (sh_derivative_rotation_matrices) tmp;
}

/* Please see header for specification */
PUBLIC void sh_derivative_rotation_matrices_get_krivanek_ddy_diagonal(__in __notnull sh_derivative_rotation_matrices in,
                                                                      __in           float                           rotation_angle_radians,
                                                                      __deref_out    float* result)
{
    int                               cur_band  = 0;
    int                               cur_m     = -cur_band;
    _sh_derivative_rotation_matrices* internal  = (_sh_derivative_rotation_matrices*) in;
    uint32_t                          n_entries = internal->n_bands * internal->n_bands;

    memset(result, 0, n_entries * sizeof(float) );

    for (uint32_t n = 0; n < n_entries; n++)
    {
        ASSERT_DEBUG_SYNC(cur_band < internal->n_bands, "Band index exceeds the legal range!");
        
        result[n] = _sh_rot_precalc_krivanek_get_matrix_coeff_for_rotation(internal, 2, cur_band, cur_m, cur_m);
        cur_m    ++;
        
        if (cur_m > cur_band)
        {
            cur_band ++;
            cur_m    = -cur_band;
        }
    }
}

/* Please see header for specification */
PUBLIC void sh_derivative_rotation_matrices_get_krivanek_dy_subdiagonal(__in __notnull sh_derivative_rotation_matrices in,
                                                                        __in           float                           rotation_angle_radians,
                                                                        __deref_out    float* result)
{
    int                               cur_band  = 0;
    int                               cur_m     = -cur_band;
    _sh_derivative_rotation_matrices* internal  = (_sh_derivative_rotation_matrices*) in;
    uint32_t                          n_entries = internal->n_bands * internal->n_bands;

    memset(result, 0, n_entries * sizeof(float) );

    for (uint32_t n = 0; n < n_entries; n++)
    {
        ASSERT_DEBUG_SYNC(cur_band < internal->n_bands, "Band index exceeds the legal range!");

        if (cur_m < cur_band)
        {
            result[n] = _sh_rot_precalc_krivanek_get_matrix_coeff_for_rotation(internal, 1, cur_band, cur_m, cur_m + 1);
        }
        
        cur_m++;

        if (cur_m > cur_band)
        {
            cur_band ++;
            cur_m    = -cur_band;
        }
    }
}

/* Please see header for specification */
PUBLIC uint32_t sh_derivative_rotation_matrices_get_n_coefficients_till_band(int n_band)
{
    uint32_t result = 0;

    for (int n = 1; n <= n_band; ++n)
    {
        result += (2 * n - 1) * (2 * n - 1);
    }

    return result;
}

/* Please see header for specification */
PUBLIC void sh_derivative_rotation_matrices_release(__in __notnull __post_invalid sh_derivative_rotation_matrices in)
{
    _sh_derivative_rotation_matrices* internal = (_sh_derivative_rotation_matrices*) in;

    delete [] internal->raw_data;
    delete internal;
}

#endif /* INCLUDE_KRIVANEK_FAST_SH_Y_ROTATION */
