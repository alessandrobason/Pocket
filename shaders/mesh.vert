#version 460

layout (location = 0) in vec3 pos;
layout (location = 1) in vec3 norm;
layout (location = 2) in vec3 col;
layout (location = 3) in vec2 uv;

layout (location = 0) out vec3 out_colour;
layout (location = 1) out vec3 out_normal;
layout (location = 2) out vec2 out_uv;
layout (location = 3) out vec3 frag_pos;
// layout (location=3) out float time;

layout(set=0, binding=0) uniform CameraBuffer {
    mat4 view;
    mat4 proj;
    mat4 view_proj;
} camera;

struct ObjectData {
    mat4 model;
};

layout (std140, set=1, binding=0) readonly buffer ObjectBuffer {
    ObjectData objects[];
} obj_buf;

// layout (push_constant) uniform constants {
//     vec4 data;
//     mat4 model;
// } push_constants;

void main() {
    mat4 transform = camera.view_proj * obj_buf.objects[gl_BaseInstance].model;
    gl_Position = transform * vec4(pos, 1);

    // const vec3 offset = push_constants.data.xyz;
    // const mat4 model = mat4(
    //     1, 0, 0, 0,
    //     0, 1, 0, 0,
    //     0, 0, 1, 0,
    //     offset.xyz, 1
    // );

    // out_colour = vec3(1);
    out_colour = col;
    out_normal = norm;
    out_uv = uv;
    frag_pos = vec3(obj_buf.objects[gl_BaseInstance].model * vec4(pos, 1));
    // time = push_constants.data.w;
}