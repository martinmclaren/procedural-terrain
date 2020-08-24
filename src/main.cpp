#include <OpenGP/GL/Application.h>
#include "OpenGP/GL/Eigen.h"

#include "loadTexture.h"
#include "noise.h"

using namespace OpenGP;
const int width=1280;
const int height=720;

const char* skybox_vshader =
#include "skybox_vshader.glsl"
;
const char* skybox_fshader =
#include "skybox_fshader.glsl"
;

const char* terrain_vshader =
#include "terrain_vshader.glsl"
;
const char* terrain_fshader =
#include "terrain_fshader.glsl"
;

const unsigned restartPrimitive = 999999; // The index at which we begin a new triangle strip
constexpr float PI = 3.14159265359f;

void init();
void genTerrainMesh();
void genCubeMesh();
void drawSkybox();
void drawTerrain();
float waveOffset = 0.0f;

std::unique_ptr<Shader> skyboxShader;
std::unique_ptr<GPUMesh> skyboxMesh;
GLuint skyboxTexture;

std::unique_ptr<Shader> terrainShader;
std::unique_ptr<GPUMesh> terrainMesh;
std::unique_ptr<R32FTexture> heightTexture;
std::map<std::string, std::unique_ptr<RGBA8Texture>> terrainTextures;

// Camera position attributes
Vec3 cameraPos;
Vec3 cameraFront;
Vec3 cameraUp;

// Camera movement attributes
float halflife;
float speed;
float yaw;
float pitch;

// Viewing attributes
float fov = 100.0f;
float aspect = width/(float)height;
float zFar = 100.0f;
float zNear =  0.1f;

int main(int, char**){

    // Declare OpenGL application
    Application app;

    // Initialise scene, generate meshes
    init();
    genCubeMesh();
    genTerrainMesh();

    // Set camera positioning
    cameraFront = Vec3(0.0f, -1.0f, 0.0f);
    cameraPos = Vec3(0.0f, 0.0f, 3.0f);
    cameraUp = Vec3(0.0f, 0.0f, 1.0f);

    // Set camera movement attributes
    speed = 0.1f;
    yaw = 0.0f;
    pitch = 0.0f;

    // Display callback
    Window& window = app.create_window([&](Window&){
        // Mac OSX Configuration (2:1 pixel density)
        glViewport(0, 0, width*2, height*2);

        // Windows Configuration (1:1 pixel density)
        //glViewport(0, 0, width, height);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        drawSkybox();
        glClear(GL_DEPTH_BUFFER_BIT);
        drawTerrain();
    });
    window.set_title("Virtual Landscape");
    window.set_size(width, height);

    // Mouse input for camera spin
    Vec2 mouse(0.0f, 0.0f);
    window.add_listener<MouseMoveEvent>([&](const MouseMoveEvent &m){
        ///--- Camera control
        Vec2 delta = m.position - mouse;
        delta[1] = -delta[1];
        float sensitivity = 0.005f;
        delta = sensitivity * delta;

        yaw += delta[0];
        pitch += delta[1];

        if(pitch > PI/2.0f - 0.01f)  pitch =  PI/2.0f - 0.01f;
        if(pitch <  -PI/2.0f + 0.01f) pitch =  -PI/2.0f + 0.01f;

        Vec3 front(0.0f, 0.0f, 0.0f);
        front[0] = sin(yaw) * cos(pitch);
        front[1] = cos(yaw) * cos(pitch);
        front[2] = sin(pitch);

        cameraFront = front.normalized();
        mouse = m.position;
    });

    // Key input for camera movement
    window.add_listener<KeyEvent>([&](const KeyEvent &k){

        switch(k.key) {
            case GLFW_KEY_W:
                cameraPos = cameraPos + speed * cameraFront.normalized();
                break;

            case GLFW_KEY_A:
                cameraPos = cameraPos - speed * cameraFront.normalized().cross(cameraUp);
                break;

            case GLFW_KEY_S:
                cameraPos = cameraPos - speed * cameraFront.normalized();
                break;

             case GLFW_KEY_D:
                cameraPos = cameraPos + speed * cameraFront.normalized().cross(cameraUp);
                break;
        }
    });

    // Run application
    return app.run();
}

void init(){
    // White
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);

    // OpenGL cubemap for sky texture
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

    // Compile and link shaders for skybox
    skyboxShader = std::unique_ptr<Shader>(new Shader());
    skyboxShader->verbose = true;
    skyboxShader->add_vshader_from_source(skybox_vshader);
    skyboxShader->add_fshader_from_source(skybox_fshader);
    skyboxShader->link();

    // Compile and link shaders for terrain
    terrainShader = std::unique_ptr<Shader>(new Shader());
    terrainShader->verbose = true;
    terrainShader->add_vshader_from_source(terrain_vshader);
    terrainShader->add_fshader_from_source(terrain_fshader);
    terrainShader->link();

    // Use noise generating basis for height texture (choose between fBm or hmf)
    heightTexture = std::unique_ptr<R32FTexture>(generateNoise());

    // Load terrain textures
    const std::string list[] = {"grass", "rock", "sand", "snow", "water"};
    for (int i=0 ; i < 5 ; ++i) {
        loadTexture(terrainTextures[list[i]], (list[i]+".png").c_str());
        terrainTextures[list[i]]->bind();
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    }

    // Load skybox textures
    const std::string skyList[] = {"miramar_ft", "miramar_bk", "miramar_dn", "miramar_up", "miramar_rt", "miramar_lf"};
    glGenTextures(1, &skyboxTexture);
    glBindTexture(GL_TEXTURE_CUBE_MAP, skyboxTexture);
    int tex_wh = 1024;
    for(int i=0; i < 6; ++i) {
        std::vector<unsigned char> image;
        loadTexture(image, (skyList[i]+".png").c_str());
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+i, 0, GL_RGBA, tex_wh, tex_wh, 0, GL_RGBA, GL_UNSIGNED_BYTE, &image[0]);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
}

void genTerrainMesh() {

    // Use triangle strips to generate flat mesh for terrain
    terrainMesh = std::unique_ptr<GPUMesh>(new GPUMesh());

    // Set resolution of grid
    int n_width = 512;
    int n_height = 512;
    // Set width of grid, center at origin
    float f_width = 5.0f;
    float f_height = 5.0f;

    // Declare vectors to store triangle strips
    std::vector<Vec3> points;
    std::vector<unsigned int> indices;
    std::vector<Vec2> texCoords;

    // Intialise vertex positions and texture coordinates
    for(int j=0; j<n_height; ++j) {
        for(int i=0; i<n_width; ++i) {

            // Calculate and store vertex positions
            float vX = -f_width/2 + j/(float)n_width * f_width;
            float vY = -f_height/2 + i/(float)n_height * f_height;
            points.push_back(Vec3(vX, vY, 0.0f));

            // Calculate and store texture coordinates
            texCoords.push_back(Vec2(i/(float)(n_width-1), j/(float)(n_height-1)));
        }
    }

    // Get element indices using triangle strips
    for(int j=0; j<n_height-1; ++j) {
        // Vertices at the base of each strip
        indices.push_back(j * n_width);
        indices.push_back((j + 1) * n_width);

        for(int i=1; i<n_width; ++i) {
            // Calculate and store next two triangles
            indices.push_back(i + j * n_width);
            indices.push_back(i + (j + 1) * n_height);
        }
        // Begin new strip
        indices.push_back(restartPrimitive);
    }

    terrainMesh->set_vbo<Vec3>("vposition", points);
    terrainMesh->set_triangles(indices);
    terrainMesh->set_vtexcoord(texCoords);
}

void genCubeMesh() {
    // Generate a cube mesh for skybox
    skyboxMesh = std::unique_ptr<GPUMesh>(new GPUMesh());
    std::vector<Vec3> points;

    points.push_back(Vec3( 1, 1, 1)); // 0
    points.push_back(Vec3(-1, 1, 1)); // 1
    points.push_back(Vec3( 1, 1,-1)); // 2
    points.push_back(Vec3(-1, 1,-1)); // 3
    points.push_back(Vec3( 1,-1, 1)); // 4
    points.push_back(Vec3(-1,-1, 1)); // 5
    points.push_back(Vec3(-1,-1,-1)); // 6
    points.push_back(Vec3( 1,-1,-1)); // 7

    std::vector<unsigned int> indices = { 3, 2, 6, 7, 4, 2, 0, 3, 1, 6, 5, 4, 1, 0 };
    skyboxMesh->set_vbo<Vec3>("vposition", points);
    skyboxMesh->set_triangles(indices);
}

void drawSkybox() {
    skyboxShader->bind();

    // Set transformations
    Vec3 look = cameraFront + cameraPos;
    Mat4x4 view = lookAt(cameraPos, look, cameraUp); // pos, look, up
    skyboxShader->set_uniform("V", view);
    Mat4x4 projection = perspective(fov, aspect, 0.1f, 100.0f);
    skyboxShader->set_uniform("P", projection);

    // Bind Textures and set uniform
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, skyboxTexture);
    skyboxShader -> set_uniform("noiseTex", 0);

    // Set atrributes, draw cube using GL_TRIANGLE_STRIP mode
    glEnable(GL_DEPTH_TEST);
    skyboxMesh -> set_attributes(*skyboxShader);
    skyboxMesh -> set_mode(GL_TRIANGLE_STRIP);
    glEnable(GL_PRIMITIVE_RESTART);
    glPrimitiveRestartIndex(restartPrimitive);
    skyboxMesh -> draw();
    skyboxShader->unbind();
}

void drawTerrain() {
    terrainShader->bind();

    // Transformation matrices for model
    Mat4x4 model = Mat4x4::Identity();
    terrainShader->set_uniform("M", model);

    // View
    Vec3 look = cameraFront + cameraPos;
    Mat4x4 view = lookAt(cameraPos, look, cameraUp);
    terrainShader->set_uniform("V", view);

    // Projection
    Mat4x4 projection = perspective(fov, aspect, zNear, zFar);
    terrainShader->set_uniform("P", projection);

    terrainShader->set_uniform("viewPos", cameraPos);

    // Bind textures
    int i = 0;
    for(std::map<std::string, std::unique_ptr<RGBA8Texture>>::iterator it = terrainTextures.begin(); it != terrainTextures.end(); ++it) {
        glActiveTexture(GL_TEXTURE1+i);
        (it->second)->bind();
        terrainShader->set_uniform(it->first.c_str(), 1+i);
        ++i;
    }
    // Bind height texture to GL_TEXTURE0 and set uniform noiseTex
    glActiveTexture(GL_TEXTURE0);
    heightTexture -> bind();
    terrainShader -> set_uniform("noiseTex", 0);

    // Draw terrain using triangle strips
    glEnable(GL_DEPTH_TEST);
    terrainMesh->set_attributes(*terrainShader);
    terrainMesh->set_mode(GL_TRIANGLE_STRIP);
    glEnable(GL_PRIMITIVE_RESTART);
    glPrimitiveRestartIndex(restartPrimitive);

    terrainMesh->draw();

    // Calculate offset for wave texture
    terrainShader->set_uniform("waveOffset", waveOffset);
    waveOffset += 0.00004f;
    if (waveOffset > 1.0f) {
        waveOffset = 0.0f;
    }

    terrainShader->unbind();
}
