#version 450

layout (location = 0) in vec3 in_colour;
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec3 frag_pos;
layout (location = 3) in float time;

layout (location = 0) out vec4 out_colour;

void main() {
    const vec3 light_pos = vec3(cos(time) * 10, 20, sin(time) * 10);
    const vec3 light_dir = normalize(light_pos - frag_pos);
    const vec3 diffuse = vec3(max(dot(in_normal, light_dir), 0));

    out_colour = vec4(diffuse * in_colour, 1);
}