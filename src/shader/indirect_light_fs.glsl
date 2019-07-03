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

uniform sampler2D s_DirectLight;
uniform sampler2D s_Normals;
uniform sampler2D s_WorldPos;
uniform sampler2D s_RSMFlux;
uniform sampler2D s_RSMNormals;
uniform sampler2D s_RSMWorldPos;
uniform sampler2D s_RSM;
uniform sampler2D s_Samples;

uniform vec3 u_LightPos;
uniform vec3 u_LightDirection;
uniform vec3  u_LightColor;
uniform float u_LightIntensity;
uniform int u_NumSamples;

// ------------------------------------------------------------------
// MAIN  ------------------------------------------------------------
// ------------------------------------------------------------------

void main(void)
{
    vec3 albedo   = texture(s_Albedo, FS_IN_TexCoord).rgb;
    vec3 frag_pos = texture(s_WorldPos, FS_IN_TexCoord).rgb;
    vec3 N        = texture(s_Normals, FS_IN_TexCoord).rgb;

    // @TODO: Implement deferred shading and spot light shadows
}

// ------------------------------------------------------------------