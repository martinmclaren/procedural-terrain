R"(
#version 330 core

// Textures from main
uniform sampler2D noiseTex;
uniform sampler2D water;
uniform sampler2D sand;
uniform sampler2D grass;
uniform sampler2D rock;
uniform sampler2D snow;
uniform float waveOffset;
uniform vec3 viewPos;

// Shader inputs
in vec2 uv;
in vec3 fragPos;
in float wHeightLim;	// Get from vertex shader

// Shader output
out vec4 texOut;

void main() {

    // Initialise environment light and view
    vec3 viewDir = viewPos - fragPos;
    vec3 lightDir = normalize(vec3(1.0f, 1.0f, 1.0f));
    vec3 lightPos = vec3(1.0f, 1.0f, 1.0f);
    vec3 lvUnitVec = normalize(lightDir + viewDir);

    // Texture size in pixels
    ivec2 size = textureSize(noiseTex, 1);

    // Get wavemap using offsets
    vec2 waveMap = texture(water, vec2(uv.x + waveOffset, uv.y)).rg * 0.01f;
    waveMap = uv + vec2( waveMap.x,  waveMap.y + waveOffset);

    // Calculate surface normals
    vec3 A = vec3(uv.x + 1.0f / size.x, uv.y, textureOffset(noiseTex, uv, ivec2(1, 0)));
    vec3 B = vec3(uv.x - 1.0f / size.x, uv.y, textureOffset(noiseTex, uv, ivec2(-1, 0)));
    vec3 C = vec3(uv.x, uv.y + 1.0f / size.y, textureOffset(noiseTex, uv, ivec2(0, 1)));
    vec3 D = vec3(uv.x, uv.y - 1.0f / size.y, textureOffset(noiseTex, uv, ivec2(0, -1)));
    vec3 normal = normalize(cross(normalize(A - B), normalize(C - D)));

    // Set slope attributes for sand and snow deposits
    float div = 1.0f / size.x;
    float slope = 0.0f;
    float saSlopeLim = 0.7f;
    float snSlopeLim = 0.85f;

    // Calculate terrain slope
    slope = max(slope, (A.y - fragPos.y) / div);
    slope = max(slope, (B.y - fragPos.y) / div);
    slope = max(slope, (C.y - fragPos.y) / div);
    slope = max(slope, (D.y - fragPos.y) / div);

    // Set height limits for materials
    float saHeightLim = wHeightLim + 0.02f;
    float grHeightLim = saHeightLim + 0.07f;
    float snHeightLim = 1.0f;
    vec4 tex = texture(sand, uv);

    // Component height
    float texHeight = (texture(noiseTex, uv).r + 1.0f) / 2.0f;

    // Apply water texture w/ waves
    if (texHeight < wHeightLim) {
        tex = texture(water,  waveMap);
    }

    // Apply sand texture
    if (saSlopeLim > slope && texHeight > wHeightLim && texHeight < saHeightLim) {
        tex = texture(sand, uv);
    }

    // Apply grass texture with smoothing
    if (texHeight > saHeightLim && texHeight < grHeightLim) {
        tex = texture(grass, uv);
    }

    // Apply rock texture with smoothing
    if (texHeight > grHeightLim) {
        tex = texture(rock, uv);
    }

    // Apply snow texture
    if (texHeight > snHeightLim && slope < snSlopeLim) {
        tex = texture(snow, uv);
    }

    // Compute scene's ambient, diffuse, and specular lighting using Phong shading
    float ambient = 0.05f;
    float diffuse = 0.2f * max(0.0f, -dot(normal, lightDir));
    float specular = 0.2f * max(0.0f, dot(normal, lvUnitVec));

    // Colour texture and send to output
    tex += ambient + diffuse + specular;
    texOut = vec4(tex);
}
)"
