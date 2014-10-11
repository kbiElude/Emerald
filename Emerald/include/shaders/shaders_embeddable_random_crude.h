/**
 *
 * Emerald (kbi/elude @2012)
 *
 * TODO
 */
#ifndef SHADERS_EMBEDDABLE_RANDOM_CRUDE_H
#define SHADERS_EMBEDDABLE_RANDOM_CRUDE_H

/* Relies on GL_VertexID. Is more of a number generator, rather than a random number generator. */
static const char* glsl_embeddable_random_crude = 
    "int random(int delta)\n"
    "{\n"
    "    int a = 36969 * ((gl_VertexID + delta) & 65535) + ((gl_VertexID + delta) >> 16);\n"
    "    int b = 18000 * ((gl_VertexID + delta) & 65535) + ((gl_VertexID + delta) >> 16);\n"
    "    return (a + b) & 65535;\n"
    "}\n"
    "\n";

#endif /* SHADERS_EMBEDDABLE_RANDOM_CRUDE_H */