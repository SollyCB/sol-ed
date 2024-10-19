#define SH_CL_URI "C:/VulkanSDK/1.3.296.0/Bin/glslc.exe"

#define SH_SRC_URI "../shader.h"
#define SH_VERT_SRC_URI "shader.vert"
#define SH_FRAG_SRC_URI "shader.frag"
#define SH_VERT_OUT_URI "shader.vert.spv"
#define SH_FRAG_OUT_URI "shader.frag.spv"

#define SH_UB_CNT 1024
#define SH_SI_CNT 127

#define SH_ENTRY_POINT "main"

#define SH_VERT_BEGIN
#define SH_FRAG_BEGIN
#define SH_VERT_END
#define SH_FRAG_END
#define SH_VERT_BEGIN_STR stringify(SH_VERT_BEGIN)
#define SH_FRAG_BEGIN_STR stringify(SH_FRAG_BEGIN)
#define SH_VERT_END_STR stringify(SH_VERT_END)
#define SH_FRAG_END_STR stringify(SH_FRAG_END)

#if GL_core_profile

SH_VERT_BEGIN
#version 450
/****************************************************/
// Vertex shader

vec2 points[] = {
    vec2( 0, -1),
    vec2( 1,  1),
    vec2(-1,  1)
};

void main() {
    gl_Position = vec4(points[gl_VertexIndex], 0, 1);
}

SH_VERT_END
SH_FRAG_BEGIN
#version 450
/****************************************************/
// Fragment shader

layout(location = 0) out vec4 fc;

void main() {
    fc = vec4(0, 1, 0, 1);
}

SH_FRAG_END

#endif // #if SHADER_SRC_GUARD