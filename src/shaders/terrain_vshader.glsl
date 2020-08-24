R"(
#version 330 core
uniform sampler2D noiseTex;

in vec3 vposition;
in vec2 vtexcoord;

uniform mat4 M;
uniform mat4 V;
uniform mat4 P;

out vec2 uv;
out vec3 fragPos;
out float wHeightLim;

void main() {
    uv = vtexcoord;
    float height = (texture(noiseTex, uv).r + 1.0f) / 2.0f;
    height = max(height, 0.55f);

    fragPos = vposition.xyz + vec3(0.0f, 0.0f, height);
    gl_Position = P * V * M * vec4(vposition.x, vposition.y, vposition.z + height, 1.0f);
    wHeightLim = 0.55f;
}
)"
