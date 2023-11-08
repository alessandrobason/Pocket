#version 450

layout (location = 0) in vec3 in_colour;
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec2 in_uv;
layout (location = 3) in vec3 frag_pos;
// layout (location = 3) in float time;

layout (location = 0) out vec4 out_colour;

layout (set=0, binding=1) uniform SceneData {
	vec4 fog_colour; // w is for exponent
	vec4 fog_distances; //x for min, y for max, zw unused.
	vec4 ambient_colour;
	vec4 sunlight_dir; //w for sun power
	vec4 sunlight_colour;
} scene_data;

layout (set=2, binding=0) uniform sampler2D tex_sampler;

void main() {
#if 0
    const vec3 light_pos = vec3(cos(time) * 10, 20, sin(time) * 10);
    const vec3 light_dir = normalize(light_pos - frag_pos);
    const vec3 diffuse = vec3(max(dot(in_normal, light_dir), 0));

    out_colour = vec4(diffuse * in_colour, 1);
#endif
    const vec4 tex_col = texture(tex_sampler, in_uv);
    out_colour = tex_col;
}