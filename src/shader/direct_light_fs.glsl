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
uniform float u_LightBias;

// ------------------------------------------------------------------
// FUNCTIONS  -------------------------------------------------------
// ------------------------------------------------------------------

// Convert an exponential depth value from an arbitrary views' projection to linear 0..1 depth
float exp_01_to_linear_01_depth(float z, float n, float f)
{
    float z_buffer_params_y = f / n;
    float z_buffer_params_x = 1.0 - z_buffer_params_y;

    return 1.0 / (z_buffer_params_x * z + z_buffer_params_y);
}

// ------------------------------------------------------------------

float spot_light_shadows(vec3 p)
{
    // Transform frag position into Light-space.
    vec4 light_space_pos = light_view_proj * vec4(p, 1.0);

    vec3 proj_coords = light_space_pos.xyz / light_space_pos.w;
    // transform to [0,1] range
    proj_coords = proj_coords * 0.5 + 0.5;
    // get closest depth value from light's perspective (using [0,1] range fragPosLight as coords)
    float closest_depth = texture(s_ShadowMap, proj_coords.xy).r;
    // get depth of current fragment from light's perspective
    float current_depth = proj_coords.z;
    // linearize depth values so that the bias can be applied
    float linear_closest_depth = exp_01_to_linear_01_depth(closest_depth, 1.0, u_LightRange);
    float linear_current_depth = exp_01_to_linear_01_depth(current_depth, 1.0, u_LightRange);
    // check whether current frag pos is in shadow
    float bias   = u_LightBias;
    float shadow = linear_current_depth - bias > linear_closest_depth ? 1.0 : 0.0;

    return 1.0 - shadow;
}

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
    float distance    = length(frag_pos - u_LightPos);
    float epsilon     = u_LightInnerCutoff - u_LightOuterCutoff;
    float attenuation = smoothstep(u_LightRange, 0, distance) * clamp((theta - u_LightOuterCutoff) / epsilon, 0.0, 1.0) * spot_light_shadows(frag_pos);

    vec3 color   = albedo * dot(N, L) * attenuation * u_LightIntensity * u_LightColor + albedo * kAmbient;
    FS_OUT_Color = vec4(color, 1.0);
}

// ------------------------------------------------------------------
