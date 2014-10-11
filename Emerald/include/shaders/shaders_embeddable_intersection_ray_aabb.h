/**
 *
 * Emerald (kbi/elude @2012)
 *
 * TODO
 */
#ifndef SHADERS_EMBEDDABLE_INTERSECTION_RAY_AABB_H
#define SHADERS_EMBEDDABLE_INTERSECTION_RAY_AABB_H

/* Not the most suitable approach for SIMD-like architectures, sadly.. :( */
static const char* glsl_embeddable_intersection_ray_aabb = 
    "bool intersect_ray_aabb(vec3 origin, vec3 direction, vec3 min_aabb, vec3 max_aabb)\n"
    "{\n"
    "    /* Smits' intersection test */\n"
    "    float tx_min, tx_max, ty_min, ty_max, tz_min, tz_max;\n"
    "\n"
    "    float divx = 1.0 / direction.x;\n"
    "    if (divx >= 0.0)\n"
    "    {\n"
    "        tx_min = (min_aabb.x - origin.x) * divx;\n"
    "        tx_max = (max_aabb.x - origin.x) * divx;\n"
    "    }\n"
    "    else\n"
    "    {\n"
    "        tx_min = (max_aabb.x - origin.x) * divx;\n"
    "        tx_max = (min_aabb.x - origin.x) * divx;\n"
    "    }\n"
    "\n"
    "    float divy = 1.0 / direction.y;\n"
    "    if (divy >= 0.0)\n"
    "    {\n"
    "        ty_min = (min_aabb.y - origin.y) * divy;\n"
    "        ty_max = (max_aabb.y - origin.y) * divy;\n"
    "    }\n"
    "    else\n"
    "    {\n"
    "        ty_min = (max_aabb.y - origin.y) * divy;\n"
    "        ty_max = (min_aabb.y - origin.y) * divy;\n"
    "    }\n"
    "    if (!((tx_min > ty_max) || (ty_min > tx_max) ))\n"
    "    {\n"
    "        if (ty_min > tx_min)\n"
    "        {\n"
    "            tx_min = ty_min;\n"
    "        }\n"
    "        if (ty_max < tx_max)\n"
    "        {\n"
    "            tx_max = ty_max;\n"
    "        }\n"
    "        \n"
    "        float divz = 1.0 / direction.z;\n"
    "        if (divz >= 0.0)\n"
    "        {\n"
    "            tz_min = (min_aabb.z - origin.z) * divz;\n"
    "            tz_max = (max_aabb.z - origin.z) * divz;\n"
    "        }\n"
    "        else\n"
    "        {\n"
    "            tz_min = (max_aabb.z - origin.z) * divz;\n"
    "            tz_max = (min_aabb.z - origin.z) * divz;\n"
    "        }\n"
    "        if (!((tx_min > tz_max) || (tz_min > tx_max) ))\n"
    "        {\n"
    "            if (tx_max > 0) return true;\n"
    "        }\n"
    "    }\n"
    "\n"
    "    return false;\n"
    "}\n";

#endif /* SHADERS_EMBEDDABLE_INTERSECTION_RAY_AABB_H */