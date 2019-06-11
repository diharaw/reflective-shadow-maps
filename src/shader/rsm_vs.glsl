// ------------------------------------------------------------------
// INPUTS VARIABLES -------------------------------------------------
// ------------------------------------------------------------------

layout (location = 0) in vec3 VS_IN_Position;
layout (location = 1) in vec2 VS_IN_TexCoord;
layout (location = 2) in vec3 VS_IN_Normal;
layout (location = 3) in vec3 VS_IN_Tangent;
layout (location = 4) in vec3 VS_IN_Bitangent;

// ------------------------------------------------------------------
// OUTPUT VARIABLES  ------------------------------------------------
// ------------------------------------------------------------------

out vec3 FS_IN_WorldPos;
out vec3 FS_IN_Normal;
out vec2 FS_IN_TexCoord;

// ------------------------------------------------------------------
// UNIFORMS ---------------------------------------------------------
// ------------------------------------------------------------------

layout(std140) uniform GlobalUniforms
{
    mat4 view_proj;
    mat4 light_view_proj;
};

layout (std140) uniform ObjectUniforms
{
    mat4 model;
};

// ------------------------------------------------------------------
// MAIN -------------------------------------------------------------
// ------------------------------------------------------------------

void main()
{
    vec4 world_pos = model * vec4(VS_IN_Position, 1.0);
    FS_IN_WorldPos = world_pos.xyz;
    FS_IN_Normal = mat3(model) * VS_IN_Position.xyz;
    FS_IN_TexCoord = VS_IN_TexCoord;
    gl_Position = light_view_proj * world_pos;
}

// ------------------------------------------------------------------
