// ------------------------------------------------------------------
// INPUT VARIABLES  -------------------------------------------------
// ------------------------------------------------------------------

in vec2 FS_IN_TexCoord;

// ------------------------------------------------------------------
// OUTPUT VARIABLES  ------------------------------------------------
// ------------------------------------------------------------------

out vec4 FS_OUT_Color;

// ------------------------------------------------------------------
// UNIFORMS  --------------------------------------------------------
// ------------------------------------------------------------------

layout(std140) uniform GlobalUniforms
{
    mat4 view_proj;
    mat4 light_view_proj;
    vec4 cam_pos;
};

uniform sampler2D s_Normals;
uniform sampler2D s_WorldPos;
uniform sampler2D s_RSMFlux;
uniform sampler2D s_RSMNormals;
uniform sampler2D s_RSMWorldPos;
uniform sampler2D s_Samples;

uniform float u_SampleRadius;
uniform float u_IndirectLightAmount;
uniform int   u_NumSamples;

// ------------------------------------------------------------------
// DEFINES  ---------------------------------------------------------
// ------------------------------------------------------------------

#define RSM_SIZE 1024.0

// ------------------------------------------------------------------
// MAIN  ------------------------------------------------------------
// ------------------------------------------------------------------

void main(void)
{
    vec3 P = texture(s_WorldPos, FS_IN_TexCoord).rgb;
    vec3 N = texture(s_Normals, FS_IN_TexCoord).rgb;

    // Project fragment position into light's coordinate space.
    vec4 light_coord = light_view_proj * vec4(P, 1.0);

    // Perspective divide.
    light_coord.xyz /= light_coord.w;

    // Remap to [0.0 - 1.0] range.
    light_coord = light_coord * 0.5 + 0.5;

    float texel_size = 1.0 / RSM_SIZE;

    vec3 indirect = vec3(0.0);

    for (int i = 0; i < u_NumSamples; i++)
    {
        vec3 offset    = texelFetch(s_Samples, ivec2(i, 0), 0).rgb;
        vec2 tex_coord = light_coord.xy + offset.xy * u_SampleRadius * texel_size;

        vec3 vpl_pos    = texture(s_RSMWorldPos, tex_coord).rgb;
        vec3 vpl_normal = texture(s_RSMNormals, tex_coord).rgb;
        vec3 vpl_flux   = texture(s_RSMFlux, tex_coord).rgb;

        vec3 result = vpl_flux * ((max(0.0, dot(vpl_normal, (P - vpl_pos))) * max(0.0, dot(N, (vpl_pos - P)))) / pow(length(P - vpl_pos), 4.0));

        result *= offset.z * offset.z;

        indirect += result;
    }

    FS_OUT_Color = vec4(clamp(indirect * u_IndirectLightAmount, 0.0, 1.0), 1.0);
}

// ------------------------------------------------------------------
