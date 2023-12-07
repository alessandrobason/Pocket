#version 450

layout (location = 0) in PerVertexData {
    vec4 colour;
} frag_in;

layout (location = 0) out vec4 frag_col;

void main() {
    frag_col = frag_in.colour;
}