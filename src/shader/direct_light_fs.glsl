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

uniform sampler2D s_WorldPos;
uniform sampler2D s_Normals;
uniform sampler2D s_Albedo;
uniform sampler2D s_ShadowMap;

uniform vec3 u_LightPos;
uniform vec3 u_LightDirection;
uniform vec3  u_LightColor;
uniform float u_LightInnerCutoff;
uniform float u_LightOuterCutoff;
uniform float u_LightIntensity;
uniform float u_LightRange;

// ------------------------------------------------------------------
// MAIN  ------------------------------------------------------------
// ------------------------------------------------------------------

const float kAmbient = 0.1;

void main(void)
{
    vec3 albedo   = texture(s_Albedo, FS_IN_TexCoord).rgb;
    vec3 frag_pos = texture(s_WorldPos, FS_IN_TexCoord).rgb;
    vec3 N        = texture(s_Normals, FS_IN_TexCoord).rgb;
    vec3 L        = normalize(u_LightPos - frag_pos); // FragPos -> LightPos vector

    float theta       = dot(L, normalize(-u_LightDirection));
    float distance    = length(u_LightPos - u_LightPos);
    float epsilon     = u_LightInnerCutoff - u_LightOuterCutoff;
    float attenuation = smoothstep(u_LightRange, 0, distance) * clamp((theta - u_LightOuterCutoff) / epsilon, 0.0, 1.0);

    vec3 color   = albedo * dot(N, L) * attenuation * u_LightIntensity * u_LightColor + albedo * kAmbient;
    FS_OUT_Color = vec4(color, 1.0);
}

// ------------------------------------------------------------------
