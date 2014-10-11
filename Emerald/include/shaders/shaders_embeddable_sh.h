/**
 *
 * Emerald (kbi/elude @2012)
 *
 * TODO
 */
#ifndef SHADERS_EMBEDDABLE_SH_H
#define SHADERS_EMBEDDABLE_SH_H

/** Rotates SH function in Y axis. Based on Irisa-Krivanek work. Relatievly fast but ONLY works for
 *  small rotation angles!
 *
 *  http://cgg.mff.cuni.cz/~jaroslav/papers/2006-sccg/2006-sccg-krivanek-shrot.pdf
 */
static const char* glsl_embeddable_rot_krivanek_y =
    "uniform samplerBuffer dy_subdiagonal;\n"
    "uniform samplerBuffer ddy_diagonal;\n"
    "\n"
    "void sh_rot_krivanek_y(inout vec3 input_output[N_BANDS * N_BANDS], in float angle)\n"
    "{\n"
    "    float bbeta = 0.5 * angle * angle;\n"
    "\n"
    "    vec3 prev_tmp = input_output[0];\n"
    "    vec3 curr_tmp = input_output[1];\n"
    "    vec3 next_tmp = input_output[2];\n"
    "\n"
    "    for (int i = 1; i < N_BANDS * N_BANDS - 1; i++)\n"
    "    {\n"
    "        float ddy_diagonal_i     = texelFetch(ddy_diagonal,   i).x;\n"
    "        float dy_subdiagonal_i   = texelFetch(dy_subdiagonal, i).x;\n"
    "        float dy_subdiagonal_i_1 = texelFetch(dy_subdiagonal, i+1).x;\n"
    "\n"
    "        input_output[i] = curr_tmp * (1.0 + bbeta * ddy_diagonal_i) + angle * (dy_subdiagonal_i * prev_tmp - dy_subdiagonal_i_1 * next_tmp);\n"
    "\n"
    "        prev_tmp = curr_tmp;\n"
    "        curr_tmp = next_tmp;\n"
    "        next_tmp = input_output[i+2];\n"
    "    }\n"
    "\n"
    "    float ddy_diagonal_i   = texelFetch(ddy_diagonal,   N_BANDS * N_BANDS - 1).x;\n"
    "    float dy_subdiagonal_i = texelFetch(dy_subdiagonal, N_BANDS * N_BANDS - 1).x;\n"
    "\n"
    "    input_output[N_BANDS * N_BANDS - 1] = curr_tmp * (1.0 + bbeta * ddy_diagonal_i) + angle * (dy_subdiagonal_i * prev_tmp);\n"
    "}\n";

/** Rotates SH function in Z axis. This is largely based on Irisa-Krivanek work, although the solution
 *  expands to what Green had proposed between the lines in his "Gritty details" piece.
 *
 *   http://www.irisa.fr/prive/kadi/SiteEquipeAsociee/Site_RTR2A/Papiers/rr1728-irisa-krivanek.pdf
 * 
 */
#define glsl_embeddable_rot_z                                                                                            \
    "void sh_rot_z(inout vec4 input_output[N_BANDS * N_BANDS], in float angle)\n"                                        \
    "{\n"                                                                                                                \
    "    vec4 input[N_BANDS * N_BANDS];\n"                                                                               \
    "\n"                                                                                                                 \
    "   for (int n = 0; n < N_BANDS * N_BANDS; ++n)\n"                                                                   \
    "   {\n"                                                                                                             \
    "       input[n] = input_output[n];\n"                                                                               \
    "   }\n"                                                                                                             \
    "   for (int l = 0; l < N_BANDS; l++)\n"                                                                             \
    "   {\n"                                                                                                             \
    "       for (int m = 1; m <= l; m++)\n"                                                                              \
    "       {\n"                                                                                                         \
    "           float m_float = float(m);\n"                                                                             \
    "           vec4 tmp1 = input[l * (l+1) - m];\n"                                                                     \
    "           vec4 tmp2 = input[l * (l+1) + m];\n"                                                                     \
    "\n"                                                                                                                 \
    "           input_output[l * (l+1) - m] = tmp1 * vec4(cos(m_float * angle)) - tmp2 * vec4(sin(m_float * angle));\n"  \
    "           input_output[l * (l+1) + m] = tmp1 * vec4(sin(m_float * angle)) + tmp2 * vec4(cos(m_float * angle));\n"  \
    "       }\n"                                                                                                         \
    "   }\n"                                                                                                             \
    "}\n"
    
/** Rotates SH function in XYZ axes (per Green). Limited to 3 bands, slow as * - can be optimised (but then would have to 
 *  have 3 separate variants for cases where we use either 1/2/3 chans 
 **/
#define glsl_embeddable_rot_xyz                                                                                                                                                                          \
    "const float x_90_multipliers_precalc_table[] = float[](1,\n"                                                                                                                                        \
    "                                                       0, 1,  0, \n"                                                                                                                                \
    "                                                      -1, 0,  0, \n"                                                                                                                                \
    "                                                       0, 0,  1, \n"                                                                                                                                \
    "                                                       0, 0,  0,             1,  0, \n"                                                                                                             \
    "                                                       0, -1, 0,             0,  0, \n"                                                                                                             \
    "                                                       0, 0, -0.5,           0, -sqrt(3.0)*0.5,\n"                                                                                                  \
    "                                                      -1, 0,  0,             0,  0, \n"                                                                                                             \
    "                                                       0, 0, -sqrt(3.0)*0.5, 0,  0.5,\n"                                                                                                            \
    "                                                       /* + */\n"                                                                                                                                   \
    "                                                       1,\n"                                                                                                                                        \
    "                                                       0, -1,  0, \n"                                                                                                                               \
    "                                                       1,  0,  0, \n"                                                                                                                               \
    "                                                       0,  0,  1, \n"                                                                                                                               \
    "                                                       0,  0,  0,            -1,  0, \n"                                                                                                            \
    "                                                       0,  -1, 0,             0,  0, \n"                                                                                                            \
    "                                                       0,  0, -0.5,           0, -sqrt(3.0)*0.5,\n"                                                                                                 \
    "                                                       1,  0,  0,             0,  0, \n"                                                                                                            \
    "                                                       0,  0, -sqrt(3.0)*0.5, 0,  0.5);\n"                                                                                                          \
    "\n"                                                                                                                                                                                                 \
    "void sh_rot_x_90(inout vec4 input[N_BANDS * N_BANDS], out vec4 output[N_BANDS * N_BANDS], in float is_negative)\n"                                                                                  \
    "{\n"                                                                                                                                                                                                \
    "    int delta = 0;\n"                                                                                                                                                                               \
    "\n"                                                                                                                                                                                                 \
    "    if (is_negative < 0)\n"                                                                                                                                                                         \
    "    {\n"                                                                                                                                                                                            \
    "        delta += 35;\n"                                                                                                                                                                             \
    "    }\n"                                                                                                                                                                                            \
    "\n"                                                                                                                                                                                                 \
    "    output[0] = input[0];"                                                                                                                                                                          \
    "\n"                                                                                                                                                                                                 \
    "    if (N_BANDS >= 2)\n"                                                                                                                                                                            \
    "    {\n"                                                                                                                                                                                            \
    "        output[1] = input[1] * x_90_multipliers_precalc_table[delta + 1] + input[2] * x_90_multipliers_precalc_table[delta + 2] + input[3] * x_90_multipliers_precalc_table[delta + 3];\n"          \
    "        output[2] = input[1] * x_90_multipliers_precalc_table[delta + 4] + input[2] * x_90_multipliers_precalc_table[delta + 5] + input[3] * x_90_multipliers_precalc_table[delta + 6];\n"          \
    "        output[3] = input[1] * x_90_multipliers_precalc_table[delta + 7] + input[2] * x_90_multipliers_precalc_table[delta + 8] + input[3] * x_90_multipliers_precalc_table[delta + 9];\n"          \
    "\n"                                                                                                                                                                                                 \
    "        if (N_BANDS > 2)\n"                                                                                                                                                                         \
    "        {\n"                                                                                                                                                                                        \
    "            output[4] = input[4] * x_90_multipliers_precalc_table[delta + 10] + input[5] * x_90_multipliers_precalc_table[delta + 11] + input[6] * x_90_multipliers_precalc_table[delta + 12] + \n" \
    "                        input[7] * x_90_multipliers_precalc_table[delta + 13] + input[8] * x_90_multipliers_precalc_table[delta + 14];\n"                                                           \
    "            output[5] = input[4] * x_90_multipliers_precalc_table[delta + 15] + input[5] * x_90_multipliers_precalc_table[delta + 16] + input[6] * x_90_multipliers_precalc_table[delta + 17] + \n" \
    "                        input[7] * x_90_multipliers_precalc_table[delta + 18] + input[8] * x_90_multipliers_precalc_table[delta + 19];\n"                                                           \
    "            output[6] = input[4] * x_90_multipliers_precalc_table[delta + 20] + input[5] * x_90_multipliers_precalc_table[delta + 21] + input[6] * x_90_multipliers_precalc_table[delta + 22] + \n" \
    "                        input[7] * x_90_multipliers_precalc_table[delta + 23] + input[8] * x_90_multipliers_precalc_table[delta + 24];\n"                                                           \
    "            output[7] = input[4] * x_90_multipliers_precalc_table[delta + 25] + input[5] * x_90_multipliers_precalc_table[delta + 26] + input[6] * x_90_multipliers_precalc_table[delta + 27] + \n" \
    "                        input[7] * x_90_multipliers_precalc_table[delta + 28] + input[8] * x_90_multipliers_precalc_table[delta + 29];\n"                                                           \
    "            output[8] = input[4] * x_90_multipliers_precalc_table[delta + 30] + input[5] * x_90_multipliers_precalc_table[delta + 31] + input[6] * x_90_multipliers_precalc_table[delta + 32] + \n" \
    "                        input[7] * x_90_multipliers_precalc_table[delta + 33] + input[8] * x_90_multipliers_precalc_table[delta + 34];\n"                                                           \
    "        }\n"                                                                                                                                                                                        \
    "    }\n"                                                                                                                                                                                            \
    "}\n"                                                                                                                                                                                                \
    "\n"                                                                                                                                                                                                 \
    glsl_embeddable_rot_z                                                                                                                                                                                \
    "\n"                                                                                                                                                                                                 \
    "void sh_rot_xyz(in vec4 input[N_BANDS * N_BANDS], out vec4 output[N_BANDS * N_BANDS], in float x_angle, in float y_angle, in float z_angle)\n"                                                      \
    "{\n"                                                                                                                                                                                                \
    "   sh_rot_z   (input,          x_angle); /* input = Z_alpha,                                    output = ? */\n"                                                                                    \
    "   sh_rot_x_90(input,  output, -1);      /* input = Z_alpha,                                    output = X_+90 * Z_alpha */\n"                                                                      \
    "   sh_rot_z   (output,         y_angle); /* input = Z_alpha,                                    output = Z_beta * X_+90 * Z_alpha */\n"                                                             \
    "   sh_rot_x_90(output, input,  1);       /* input = X_-90 * Z_beta * X_+90 * Z_alpha,           output = Z_beta * X_+90 * Z_alpha */\n"                                                             \
    "   sh_rot_z   (input,          z_angle); /* input = Z_theta * X_-90 * Z_beta * X_+90 * Z_alpha, output = Z_beta * X_+90 * Z_alpha */\n"                                                             \
    "\n"                                                                                                                                                                                                 \
    "   /* Ugh */\n"                                                                                                                                                                                     \
    "   for (int n = 0; n < N_BANDS * N_BANDS; ++n)\n"                                                                                                                                                   \
    "   {\n"                                                                                                                                                                                             \
    "       output[n] = input[n];\n"                                                                                                                                                                     \
    "   }\n"                                                                                                                                                                                             \
    "}\n"
#define FACTORIAL_DEFINE                                                                    \
    "const float factorial[34] = float[34](\n"                                              \
    "                                      1.0,\n"                                          \
    "                                      1.0,\n"                                          \
    "                                      2.0,\n"                                          \
    "                                      6.0,\n"                                          \
    "                                      24.0,\n"                                         \
    "                                      120.0,\n"                                        \
    "                                      720.0,\n"                                        \
    "                                      5040.0,\n"                                       \
    "                                      40320.0,\n"                                      \
    "                                      362880.0,\n"                                     \
    "                                      3628800.0,\n"                                    \
    "                                      39916800.0,\n"                                   \
    "                                      479001600.0,\n"                                  \
    "                                      6227020800.0,\n"                                 \
    "                                      87178291200.0,\n"                                \
    "                                      1307674368000.0,\n"                              \
    "                                      20922789888000.0,\n"                             \
    "                                      355687428096000.0,\n"                            \
    "                                      6402373705728000.0,\n"                           \
    "                                      121645100408832000.0,\n"                         \
    "                                      2432902008176640000.0,\n"                        \
    "                                      51090942171709440000.0,\n"                       \
    "                                      1124000727777607680000.0,\n"                     \
    "                                      25852016738884976640000.0,\n"                    \
    "                                      620448401733239439360000.0,\n"                   \
    "                                      15511210043330985984000000.0,\n"                 \
    "                                      403291461126605635584000000.0,\n"                \
    "                                      10888869450418352160768000000.0,\n"              \
    "                                      304888344611713860501504000000.0,\n"             \
    "                                      8841761993739701954543616000000.0,\n"            \
    "                                      265252859812191058636308480000000.0,\n"          \
    "                                      8222838654177922817725562880000000.0,\n"         \
    "                                      263130836933693530167218012160000000.0,\n"       \
    "                                      8683317618811886495518194401280000000.0);\n"
    
#define DOUBLE_FACTORIAL_DEFINE \
    "const float double_factorial[33] = float[33](\n" \
    "1.0,\n"                                          \
    "2.0,\n"                                          \
    "3.0,\n"                                          \
    "8.0,\n"                                          \
    "15.0,\n"                                         \
    "48.0,\n"                                         \
    "105.0,\n"                                        \
    "384.0,\n"                                        \
    "945.0,\n"                                        \
    "3840.0,\n"                                       \
    "10395.0,\n"                                      \
    "46080.0,\n"                                      \
    "135135.0,\n"                                     \
    "645120.0,\n"                                     \
    "2027025.0,\n"                                    \
    "10321920.0,\n"                                   \
    "34459425.0,\n"                                   \
    "185794560.0,\n"                                  \
    "654729075.0,\n"                                  \
    "3715891200.0,\n"                                 \
    "13749310575.0,\n"                                \
    "81749606400.0,\n"                                \
    "316234143225.0,\n"                               \
    "1961990553600.0,\n"                              \
    "7905853580625.0,\n"                              \
    "51011754393600.0,\n"                             \
    "213458046676875.0,\n"                            \
    "1428329123020800.0,\n"                           \
    "6190283353629375.0,\n"                           \
    "42849873690624000.0,\n"                          \
    "191898783962510625.0,\n"                         \
    "1371195958099968000.0,\n"                        \
    "6332659870762850625.0);\n"

/* Requires N_BANDS define to be present.
 * Requires former glsl_embeddable_random_crude inclusion.
 **/
static const char* glsl_embeddable_sh = 
    FACTORIAL_DEFINE
    "\n"
    "float P(int l, int m, float x)\n"
    "{\n"
    "   float pmm = 1.0;\n"
    "\n"
    "   if (m > 0)\n"
    "   {\n"
    "       float temp = sqrt( (1.0 - x) * (1.0 + x) );\n"
    "       float fact = 1.0;\n"
    "\n"
    "       for (int i = 1; i <= m; ++i)\n"
    "       {\n"
    "           pmm  *= (-fact) * temp;\n"
    "           fact += 2.0;\n"
    "       }\n"
    "   }\n"
    "\n"
    "   if (l == m)\n"
    "   {\n"
    "       return pmm;\n"
    "   }\n"
    "\n"
    "   float pmmp1 = x * (2.0*m + 1.0) * pmm;\n"
    "\n"
    "   if (l == (m+1) )\n"
    "   {\n"
    "       return pmmp1;\n"
    "   }\n"
    "\n"
    "   float pll = 0.0;\n"
    "\n"
    "   for (int ll = m+2; ll <= l; ++ll)\n"
    "   {\n"
    "       pll   = ( (2.0 * ll - 1.0) * x * pmmp1 - (ll + m - 1.0) * pmm) / (ll - m);\n"
    "       pmm   = pmmp1;\n"
    "       pmmp1 = pll;\n"
    "   }\n"
    "\n"
    "   return pll;\n"
    "}\n"
    "\n"
    "float K(int l, int m)\n"
    "{\n"
    "   float temp = ((2.0 * l + 1.0) * factorial[l - m]) / (4.0 * 3.141529657 * factorial[l+m]);\n"
    "\n"
    "   return sqrt(temp);\n"
    "}\n"
    "\n"
    "float spherical_harmonics(int l, int m, float theta, float phi)\n"
    "{\n"
    "   const float sqrt2 = sqrt(2.0);\n"
    "\n"
    "   if (m == 0)\n"
    "   {\n"
    "       return K(l, 0) * P(l, m, cos(theta) );\n"
    "   }\n"
    "   else\n"
    "   if (m > 0)\n"
    "   {\n"
    "       return sqrt2 * K(l, m) * cos(m * phi) * P(l, m, cos(theta) );\n"
    "   }\n"
    "   else\n"
    "   {\n"
    "       return sqrt2 * K(l, -m) * sin(-m * phi) * P(l, -m, cos(theta) );\n"
    "   }\n"
    "}\n";

#endif /* SHADERS_EMBEDDABLE_SH_H */