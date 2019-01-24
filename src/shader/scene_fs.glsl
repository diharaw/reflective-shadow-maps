
out vec4 PS_OUT_Color;

in vec3 PS_IN_FragPos;
in vec3 PS_IN_Normal;
in vec2 PS_IN_TexCoord;

uniform vec4 u_Diffuse;

void main()
{
	vec3 light_pos = vec3(200.0, 200.0, 0.0);
	vec3 light_dir = normalize(light_pos - PS_IN_FragPos);

    vec4 diffuse = u_Diffuse;

	if (diffuse.w < 0.1)
		discard;

	//vec3 color = dot(PS_IN_Normal, light_dir) * diffuse.xyz;

	PS_OUT_Color = vec4(diffuse.xyz, 1.0);
}
