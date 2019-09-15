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
uniform sampler2D s_Dither;

uniform float u_SampleRadius;
uniform float u_IndirectLightAmount;
uniform int   u_NumSamples;
uniform int   u_Dither;
uniform vec3 u_LightPos;
uniform vec3  u_LightDirection;
uniform float u_LightInnerCutoff;
uniform float u_LightOuterCutoff;
uniform float u_LightRange;

// ------------------------------------------------------------------

float light_attenuation(vec3 frag_pos)
{
    vec3  L        = normalize(u_LightPos - frag_pos); // FragPos -> LightPos vector
    float theta    = dot(L, normalize(-u_LightDirection));
    float distance = length(frag_pos - u_LightPos);
    float epsilon  = u_LightInnerCutoff - u_LightOuterCutoff;

    return smoothstep(u_LightRange, 0, distance) * clamp((theta - u_LightOuterCutoff) / epsilon, 0.0, 1.0);
}

// ------------------------------------------------------------------
// MAIN  ------------------------------------------------------------
// ------------------------------------------------------------------

void main(void)
{
    vec3 P = texture(s_WorldPos, FS_IN_TexCoord).rgb;
    vec3 N = normalize(texture(s_Normals, FS_IN_TexCoord).rgb);

    // Project fragment position into light's coordinate space.
    vec4 light_coord = light_view_proj * vec4(P, 1.0);

    // Perspective divide.
    light_coord.xyz /= light_coord.w;

    // Remap to [0.0 - 1.0] range.
    light_coord = light_coord * 0.5 + 0.5;

    vec3 indirect = vec3(0.0);

#ifdef DITHER_8_8
    vec2  interleaved_pos = (mod(floor(gl_FragCoord.xy), 8.0));
    float dither_offset   = texture(s_Dither, interleaved_pos / 8.0 + vec2(0.5 / 8.0, 0.5 / 8.0)).r;
#else
    vec2  interleaved_pos = (mod(floor(gl_FragCoord.xy), 4.0));
    float dither_offset   = texture(s_Dither, interleaved_pos / 4.0 + vec2(0.5 / 4.0, 0.5 / 4.0)).r;
#endif

    if (u_Dither == 0)
        dither_offset = 0.0;

    for (int i = 0; i < u_NumSamples; i++)
    {
        vec3 offset    = texelFetch(s_Samples, ivec2(i, 0), 0).rgb;
        vec2 tex_coord = light_coord.xy + offset.xy * u_SampleRadius + (((offset.xy * u_SampleRadius) / 2.0) * dither_offset);

        vec3 vpl_pos    = texture(s_RSMWorldPos, tex_coord).rgb;
        vec3 vpl_normal = normalize(texture(s_RSMNormals, tex_coord).rgb);
        vec3 vpl_flux   = texture(s_RSMFlux, tex_coord).rgb;

        vec3 result = light_attenuation(vpl_pos) * vpl_flux * ((max(0.0, dot(vpl_normal, (P - vpl_pos))) * max(0.0, dot(N, (vpl_pos - P)))) / pow(length(P - vpl_pos), 4.0));

        result *= offset.z * offset.z;

        // Uncomment following line for debugging.
        // indirect += vec3(((max(0.0, dot(vpl_normal, normalize(P - vpl_pos))) * max(0.0, dot(N, normalize(vpl_pos - P))))));
        indirect += result;
    }

    FS_OUT_Color = vec4(clamp(indirect * u_IndirectLightAmount, 0.0, 1.0), 1.0);
}

// ------------------------------------------------------------------
