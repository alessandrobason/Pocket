#version 450

layout (location = 0) out vec3 out_colour;

void main() {
    const vec3 positions[3] = vec3[3](
        vec3(+1, +1, 0),
        vec3(-1, +1, 0),
        vec3(+0, -1, 0)
    );

    const vec3 colours[3] = vec3[3](
        vec3(1, 0, 0),
        vec3(0, 1, 0),
        vec3(0, 0, 1)
    );

    gl_Position = vec4(positions[gl_VertexIndex], 1);
    out_colour = colours[gl_VertexIndex];
}