/**
 *
 * Emerald (kbi/elude @2012-2016)
 *
 * Internal usage only.
 */
#ifndef UI_SHARED_H
#define UI_SHARED_H

static const char* ui_general_vertex_shader_body =
    "#version 430 core\n"
    "\n"
    "layout(binding = 0, std140) uniform dataVS\n"
    "{\n"
    "    vec4 x1y1x2y2;\n"
    "};\n"
    "\n"
    "layout(location = 0) out vec2 uv;\n"
    "\n"
    "void main()\n"
    "{\n"
    "   vec4 ss_x1y1x2y2 = x1y1x2y2 * 2.0f - 1.0f;\n"
    "\n"
    "   gl_Position = vec4(gl_VertexID < 2 ? ss_x1y1x2y2.x : ss_x1y1x2y2.z, (gl_VertexID == 0 || gl_VertexID == 3) ? ss_x1y1x2y2.y : ss_x1y1x2y2.w, 0, 1);\n"
    "   uv          = vec2(gl_VertexID < 2 ? 0.0 : 1.0, (gl_VertexID == 0 || gl_VertexID == 3) ? 1.0 : 0.0);\n"
    "}\n";

#endif /* UI_SHARED_H */
