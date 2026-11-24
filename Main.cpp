#include "libs/glad_loader_docs.h"
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <string>
#include <vector>
#include "tools/Polygon.h"
#include "tools/Cube.h"
#include <learnopengl/shader_m.h>
#include <learnopengl/camera.h>
#include "stb_image.h"

using namespace std;
using namespace glm;

// ---------------------------------------------------------------------------
// Window
// ---------------------------------------------------------------------------
#pragma region window variables
int SCR_WIDTH = 1280;
int SCR_HEIGHT = 720;
#pragma endregion

#pragma region camera explanation
// ---------------------------------------------------------------------------
// Two fixed cameras using the learnopengl Camera class.
// Constructor: Camera(position, worldUp, yaw, pitch)
//   yaw   = -90 -> display the object in the center (left,right)
//   pitch = -18.4 -> looking down at the object with a slight angle (up,down)
// ---------------------------------------------------------------------------

const float TABLE_Z = 0.7f;

Camera povCamera(vec3(0.0f, 2.2f, 5.8f), vec3(0.0f, 1.0f, 0.0f), -90.0f, -15.0f);

// Closer overhead camera centered over the longer & wider table
Camera aboveCamera(vec3(0.0f, 7.6f, TABLE_Z), vec3(0.0f, 1.0f, 0.0f), -90.0f, -89.9f);

#pragma endregion

int activeView = 0; // 0 = POV (key 1), 1 = above (key 2)

// Which of the 5 cards are still in the hand.
bool cardInHand[6] = {true, true, true, true, true, true};

// Player slots (row 1, left-to-right): -1 = empty, 0..4 = card index.
int placedPlayerSlots[5] = {-1, -1, -1, -1, -1};
int nextPlayerSlotIndex = 0;

// Card-play animation (hand -> slot)
int animCard = -1;
int animSlot = -1;
double animStartTime = 0.0;
const float ANIM_DURATION = 0.8f;
const float ANIM_ARC_HEIGHT = 0.7f;
const float PI_F = 3.14159265f;

#pragma region functions
void processInput(GLFWwindow *window, int numPlayerCards);
void framebuffer_size_callback(GLFWwindow *window, int width, int height);
bool keyPressedOnce(GLFWwindow *window, int key);
void playRandomCardToNextSlot(int numPlayerCards);
vector<GLuint> loadTextures(const vector<string> &paths,
    GLint wrapOption = GL_REPEAT,
    GLint minFilter = GL_LINEAR_MIPMAP_LINEAR,
    GLint magFilter = GL_LINEAR);
#pragma endregion

// ---------------------------------------------------------------------------
// Pose helpers (used to blend between two card transformations)
// ---------------------------------------------------------------------------
struct Pose
{
    vec3 position;
    quat rotation;
    vec3 scaleXYZ;
};

Pose poseFromMatrix(const mat4 &m)
{
    Pose p;
    p.position = vec3(m[3]);
    p.scaleXYZ = vec3(length(vec3(m[0])), length(vec3(m[1])), length(vec3(m[2])));
    p.scaleXYZ.x = (p.scaleXYZ.x > 0.0001f) ? p.scaleXYZ.x : 1.0f;
    p.scaleXYZ.y = (p.scaleXYZ.y > 0.0001f) ? p.scaleXYZ.y : 1.0f;
    p.scaleXYZ.z = (p.scaleXYZ.z > 0.0001f) ? p.scaleXYZ.z : 1.0f;
    mat3 rot(vec3(m[0]) / p.scaleXYZ.x,
             vec3(m[1]) / p.scaleXYZ.y,
             vec3(m[2]) / p.scaleXYZ.z);
    p.rotation = quat_cast(rot);
    return p;
}

mat4 matrixFromPose(const Pose &p)
{
    return translate(mat4(1.0f), p.position) * mat4_cast(p.rotation) * scale(mat4(1.0f), p.scaleXYZ);
}

// ---------------------------------------------------------------------------
int main()
{
    srand(static_cast<unsigned int>(time(nullptr)));

    if (!glfwInit())
    {
        cerr << "Failed to initialize GLFW" << endl;
        return 1;
    }

    GLFWwindow *window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Scene From a Cards Game", NULL, NULL);
    if (!window)
    {
        cerr << "Failed to create GLFW window" << endl;
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        cerr << "Failed to initialize GLAD" << endl;
        return 1;
    }

    Shader allShader("./shaders/vs/L5.vs", "./shaders/fs/L5.fs");
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    stbi_set_flip_vertically_on_load(true);

    // -----------------------------------------------------------------------
    // WRITE YOUR IMAGE / TEXTURE PATHS HERE:
    //   [0] : floor texture
    //   [1] : wall texture
    //   [2] : table texture
    //   [3] : card slot texture
    //   [4..8]: 5 hero card textures
    // -----------------------------------------------------------------------
    vector<GLuint> textures = loadTextures({
        "./textures/wood_floor.jpg",
        "./textures/wall_paper.jpg",
        "./textures/wooden-card-table.jpg",
        "./textures/card-slot.jpg",
        "./textures/card-geralt.png",
        "./textures/card-gman.png",
        "./textures/card-gordon.png",
        "./textures/card-leon.png",
        "./textures/card-virgil.png",
        "./textures/card-dante.png",
        "./textures/lantern-texture.jpg",
        "./textures/cup-texture.jpg",
        "./textures/water-image.jpg",
        "./textures/cigs-texture.png",
        "./textures/liberation_poster.jpg"});

    const vector<vec2> texA = {vec2(0, 0), vec2(0, 1), vec2(1, 1), vec2(1, 0)};
    const vector<vec2> texB = {vec2(1, 0), vec2(1, 1), vec2(0, 1), vec2(0, 0)};

    vector<vec3> zFace = {vec3(-0.5f, -0.5f, -0.5f), vec3(-0.5f, 0.5f, -0.5f),
                          vec3(0.5f, 0.5f, -0.5f), vec3(0.5f, -0.5f, -0.5f)};
    vector<vec3> yFace = {vec3(-0.5f, -0.5f, -0.5f), vec3(-0.5f, -0.5f, 0.5f),
                          vec3(0.5f, -0.5f, 0.5f), vec3(0.5f, -0.5f, -0.5f)};
    vector<vec3> xFace = {vec3(-0.5f, -0.5f, 0.5f), vec3(-0.5f, 0.5f, 0.5f),
                          vec3(-0.5f, 0.5f, -0.5f), vec3(-0.5f, -0.5f, -0.5f)};

    PolygonWithTexture Face1(zFace, texA);
    PolygonWithTexture Face2(zFace, texB);
    PolygonWithTexture Face3(yFace, texB);
    PolygonWithTexture Face4(yFace, texB);
    PolygonWithTexture Face5(xFace, texA);
    PolygonWithTexture Face6(xFace, texB);

    auto drawBox = [&](vec3 position, vec3 size, GLuint texID)
    {
        Face1.setTexture(texID);
        Face2.setTexture(texID);
        Face3.setTexture(texID);
        Face4.setTexture(texID);
        Face5.setTexture(texID);
        Face6.setTexture(texID);

        mat4 model = mat4(1.0f);
        model = translate(model, position);
        model = scale(model, size);

        Face1.transformation(model);
        Face2.transformation(translate(model, vec3(0.0f, 0.0f, 1.0f)));
        Face3.transformation(model);
        Face4.transformation(translate(model, vec3(0.0f, 1.0f, 0.0f)));
        Face5.transformation(model);
        Face6.transformation(translate(model, vec3(1.0f, 0.0f, 0.0f)));

        Face1.draw(allShader);
        Face2.draw(allShader);
        Face3.draw(allShader);
        Face4.draw(allShader);
        Face5.draw(allShader);
        Face6.draw(allShader);
    };
        auto drawCup = [&](vec3 pos, GLuint cupTex, GLuint waterTex)
    {
        drawBox(pos + vec3(0.0f, 0.015f, 0.0f), vec3(0.35f, 0.03f, 0.35f), cupTex);
        drawBox(pos + vec3( 0.0f,  0.225f,  0.16f), vec3(0.35f, 0.45f, 0.03f), cupTex);
        drawBox(pos + vec3( 0.0f,  0.225f, -0.16f), vec3(0.35f, 0.45f, 0.03f), cupTex);
        drawBox(pos + vec3(-0.16f, 0.225f,  0.0f),  vec3(0.03f, 0.45f, 0.35f), cupTex);
        drawBox(pos + vec3( 0.16f, 0.225f,  0.0f),  vec3(0.03f, 0.45f, 0.35f), cupTex);
        drawBox(pos + vec3(0.0f, 0.38f, 0.0f), vec3(0.28f, 0.025f, 0.28f), waterTex);
    };
    auto drawLantern = [&](vec3 pos, GLuint frameTex, GLuint lightTex)
{
    drawBox(pos + vec3(0.0f, 0.05f, 0.0f), vec3(0.50f, 0.10f, 0.50f), frameTex);
    drawBox(pos + vec3(0.0f, 0.35f, 0.0f), vec3(0.36f, 0.50f, 0.36f), lightTex);
    drawBox(pos + vec3(-0.20f, 0.35f, -0.20f), vec3(0.05f, 0.50f, 0.05f), frameTex);
    drawBox(pos + vec3( 0.20f, 0.35f, -0.20f), vec3(0.05f, 0.50f, 0.05f), frameTex);
    drawBox(pos + vec3(-0.20f, 0.35f,  0.20f), vec3(0.05f, 0.50f, 0.05f), frameTex);
    drawBox(pos + vec3( 0.20f, 0.35f,  0.20f), vec3(0.05f, 0.50f, 0.05f), frameTex);
    drawBox(pos + vec3(0.0f, 0.65f, 0.0f), vec3(0.52f, 0.10f, 0.52f), frameTex);
    drawBox(pos + vec3(0.0f, 0.74f, 0.0f), vec3(0.30f, 0.08f, 0.30f), frameTex);
    drawBox(pos + vec3(0.0f, 0.83f, 0.0f), vec3(0.12f, 0.10f, 0.05f), frameTex);
};
    // Room setup
    const vector<vec2> floorUV    = {vec2(0, 0), vec2(0, 5), vec2(4, 5), vec2(4, 0)};
    const vector<vec2> backWallUV = {vec2(0, 0), vec2(0, 2), vec2(4, 2), vec2(4, 0)};
    const vector<vec2> sideWallUV = {vec2(0, 0), vec2(0, 2), vec2(5, 2), vec2(5, 0)};

    vector<vec3> floorVerts = {
        vec3(-8.0f, -2.0f,  10.0f),
        vec3(-8.0f, -2.0f, -12.0f),
        vec3( 8.0f, -2.0f, -12.0f),
        vec3( 8.0f, -2.0f,  10.0f)};
    PolygonWithTexture roomFloor(floorVerts, floorUV);
    roomFloor.setTexture(textures[0]);

    vector<vec3> backWallVerts = {
        vec3(-8.0f, -2.0f, -12.0f),
        vec3(-8.0f,  6.0f, -12.0f),
        vec3( 8.0f,  6.0f, -12.0f),
        vec3( 8.0f, -2.0f, -12.0f)};
    PolygonWithTexture backWall(backWallVerts, backWallUV);
    backWall.setTexture(textures[1]);

    vector<vec3> leftWallVerts = {
        vec3(-8.0f, -2.0f,  10.0f),
        vec3(-8.0f,  6.0f,  10.0f),
        vec3(-8.0f,  6.0f, -12.0f),
        vec3(-8.0f, -2.0f, -12.0f)};
    PolygonWithTexture leftWall(leftWallVerts, sideWallUV);
    leftWall.setTexture(textures[1]);

    vector<vec3> rightWallVerts = {
        vec3( 8.0f, -2.0f, -12.0f),
        vec3( 8.0f,  6.0f, -12.0f),
        vec3( 8.0f,  6.0f,  10.0f),
        vec3( 8.0f, -2.0f,  10.0f)};
    PolygonWithTexture rightWall(rightWallVerts, sideWallUV);
    rightWall.setTexture(textures[1]);

    vector<vec3> frontWallVerts = {
        vec3( 8.0f, -2.0f,  10.0f),
        vec3( 8.0f,  6.0f,  10.0f),
        vec3(-8.0f,  6.0f,  10.0f),
        vec3(-8.0f, -2.0f,  10.0f)};
    PolygonWithTexture frontWall(frontWallVerts, backWallUV);
    frontWall.setTexture(textures[1]);
    vector<vec3> posterVerts = {
        vec3(-1.5f,  2.8f, -11.95f),
        vec3( 1.5f,  2.8f, -11.95f),
        vec3( 1.5f,  0.7f, -11.95f),
        vec3( 0.9f,  0.3f, -11.95f),
        vec3( 0.3f, -0.7f, -11.95f),
        vec3(-1.5f, -0.7f, -11.95f)
    };
       const vector<vec2> posterUV = {
        vec2(0.0f, 1.0f),
        vec2(1.0f, 1.0f),
        vec2(1.0f, 0.4f),
        vec2(0.8f, 0.28f),
        vec2(0.6f, 0.0f),
        vec2(0.0f, 0.0f)
    };

    PolygonWithTexture liberationPoster(posterVerts, posterUV);
        liberationPoster.setTexture(textures[14]);

    // Colt pack front face (same ratio as the image, standing on the table)
    vector<vec3> packFrontVerts = {
        vec3(-0.18f, 0.0f, 0.0f),
        vec3(-0.18f, 0.5f, 0.0f),
        vec3( 0.18f, 0.5f, 0.0f),
        vec3( 0.18f, 0.0f, 0.0f)};
    PolygonWithTexture packFront(packFrontVerts, texA);
    packFront.setTexture(textures[13]);

    // Wider and longer tabletop to fit the longer slots, enemy row pushback, and hand cleanly
        const float tableW = 8.4f;
        const float tableD = 6.8f;

    // 4 player card slots on the tabletop
    vector<PolygonWithTexture> cardSlots;
    cardSlots.reserve(5);
    const float baseSlotHeight = 0.10f;
    const float slotHalfW      = 0.50f;
    const float slotHalfD      = 0.48f; // longer slot depth (0.96 total)
    const float playerRowLocalZ = 0.15f;

    for (float localZ : {playerRowLocalZ})
    {
        for (float localX : {-2.6f, -1.3f, 0.0f, 1.3f, 2.6f})
        {
            float z = TABLE_Z + localZ;
            vector<vec3> slotVertices = {
                vec3(localX - slotHalfW, baseSlotHeight, z - slotHalfD),
                vec3(localX - slotHalfW, baseSlotHeight, z + slotHalfD),
                vec3(localX + slotHalfW, baseSlotHeight, z + slotHalfD),
                vec3(localX + slotHalfW, baseSlotHeight, z - slotHalfD)};
            cardSlots.emplace_back(slotVertices, texA);
            cardSlots.back().setTexture(textures[3]);
        }
    }

    // 5 Player hero cards
    vector<PolygonWithTexture> playerCards;
    playerCards.reserve(6);
    const vector<float> fanAngles = {12.5f, 7.5f, 2.5f, -2.5f, -7.5f, -12.5f};
    for (int i = 0; i < 6; ++i)
    {
        vector<vec3> cardVertices = {
            vec3(-0.45f, 0.0f,  0.0f),
            vec3(-0.45f, 0.95f, 0.0f),
            vec3( 0.45f, 0.95f, 0.0f),
            vec3( 0.45f, 0.0f,  0.0f)};
        playerCards.emplace_back(cardVertices, texA);
        playerCards.back().setTexture(textures[4 + i]);
    }

    // -----------------------------------------------------------------------
    // Main loop
    // -----------------------------------------------------------------------
    while (!glfwWindowShouldClose(window))
    {
        processInput(window, static_cast<int>(playerCards.size()));

        float animProgress = 0.0f;
        if (animCard != -1)
        {
            animProgress = static_cast<float>((glfwGetTime() - animStartTime) / ANIM_DURATION);
            if (animProgress >= 1.0f)
            {
                if (animSlot >= 0 && animSlot < 5 && animCard >= 0 && animCard < 6)
                    placedPlayerSlots[animSlot] = animCard;
                animCard = -1;
                animSlot = -1;
            }
        }

        glClearColor(0.12f, 0.12f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        float aspect = (SCR_HEIGHT > 0) ? (float)SCR_WIDTH / (float)SCR_HEIGHT : 1.0f;
        mat4 projection = perspective(radians(45.0f), aspect, 0.1f, 100.0f);

        Camera &cam = (activeView == 0) ? povCamera : aboveCamera;
        mat4 view = cam.GetViewMatrix();

        allShader.use();
        allShader.setMat4("projection", projection);
        allShader.setMat4("view", view);

        // Draw the room
        mat4 base = mat4(1.0f);
        roomFloor.transformation(base); roomFloor.draw(allShader);
        backWall.transformation(base);  backWall.draw(allShader);
        leftWall.transformation(base);  leftWall.draw(allShader);
        rightWall.transformation(base); rightWall.draw(allShader);
        frontWall.transformation(base); frontWall.draw(allShader);

        // 1. Tabletop surface (top touches y = 0 from above)
        drawBox(vec3(0.0f, -0.1f, TABLE_Z), vec3(tableW, 0.2f, tableD), textures[2]);
        liberationPoster.transformation(base);
        liberationPoster.draw(allShader);

        // 2. Four table legs (height 1.8 - from floor y = -2 up to underside y = -0.2)
        float legX = tableW * 0.5f - 0.35f;
        float legZ = tableD * 0.5f - 0.35f;
        drawBox(vec3(-legX, -1.1f, TABLE_Z - legZ), vec3(0.22f, 1.8f, 0.22f), textures[2]);
        drawBox(vec3( legX, -1.1f, TABLE_Z - legZ), vec3(0.22f, 1.8f, 0.22f), textures[2]);
        drawBox(vec3(-legX, -1.1f, TABLE_Z + legZ), vec3(0.22f, 1.8f, 0.22f), textures[2]);
        drawBox(vec3( legX, -1.1f, TABLE_Z + legZ), vec3(0.22f, 1.8f, 0.22f), textures[2]);

        drawCup(vec3(-3.45f, 0.0f, TABLE_Z - 1.85f), textures[11], textures[12]);
        drawLantern(vec3(3.45f, 0.0f, TABLE_Z - 1.85f), textures[10], textures[10]);

        // Colt pack: box body + front face on top
        vec3 packPos(0.0f, 0.0f, TABLE_Z - 1.85f);
        drawBox(packPos + vec3(0.0f, 0.25f, 0.0f), vec3(0.36f, 0.5f, 0.14f), textures[13]);
        packFront.transformation(translate(mat4(1.0f), packPos + vec3(0.0f, 0.0f, 0.072f)));
        packFront.draw(allShader);

        // Draw slots & cards placed in the 4 player slots
            mat4 placedTarget[5] = {mat4(1.0f), mat4(1.0f), mat4(1.0f), mat4(1.0f), mat4(1.0f)};

        for (int i = 0; i < static_cast<int>(cardSlots.size()); ++i)
        {
            int row = 1;
            int column = i;
            float slotX = -2.6f + column * 1.3f;
            float baseSlotZ = TABLE_Z + playerRowLocalZ;

            // Move slots slightly up (-Z) in Above view and raise enemy slots so they never merge with the table
            float zShift = (activeView == 1) ? -0.18f : 0.0f;
            float yLift = (activeView == 1) ? 0.09f : 0.15f;
            float finalSlotZ = baseSlotZ + zShift;
            float finalSlotY = baseSlotHeight + yLift;

            mat4 slotTransform = mat4(1.0f);
            if (activeView == 1)
            {
                slotTransform = translate(slotTransform, vec3(0.0f, yLift, zShift));
            }
            else
            {
                float tilt = 25.0f;
                slotTransform = translate(slotTransform, vec3(slotX, finalSlotY, finalSlotZ));
                slotTransform = rotate(slotTransform, radians(tilt), vec3(1.0f, 0.0f, 0.0f));
                slotTransform = translate(slotTransform, vec3(-slotX, -finalSlotY, -finalSlotZ));
                slotTransform = translate(slotTransform, vec3(0.0f, yLift, 0.0f));
            }
            cardSlots[i].transformation(slotTransform);
            cardSlots[i].draw(allShader);

            if (row == 1)
            {
                mat4 laidInSlotLocal = translate(mat4(1.0f), vec3(0.0f, 0.025f, 0.5f));
                laidInSlotLocal = rotate(laidInSlotLocal, radians(-90.0f), vec3(1.0f, 0.0f, 0.0f));
                laidInSlotLocal = scale(laidInSlotLocal, vec3(1.13f, 1.05f, 1.0f)); // slightly bigger than the slot, no dark gaps

                mat4 laidFlatOverSlot = translate(mat4(1.0f), vec3(slotX, finalSlotY, finalSlotZ));
                if (activeView == 0)
                {
                    laidFlatOverSlot = rotate(laidFlatOverSlot, radians(25.0f), vec3(1.0f, 0.0f, 0.0f));
                }
                laidFlatOverSlot = laidFlatOverSlot * laidInSlotLocal;

                placedTarget[column] = laidFlatOverSlot;

                if (placedPlayerSlots[column] != -1)
                {
                    int cardIdx = placedPlayerSlots[column];
                    if (cardIdx >= 0 && cardIdx < static_cast<int>(playerCards.size()))
                    {
                        playerCards[cardIdx].transformation(laidFlatOverSlot);
                        playerCards[cardIdx].draw(allShader);
                    }
                }
            }
        }

        // Player hand:
        // Moved slightly upward (-Z) compared to before, while staying clearly BELOW the player slots
        // (bottom edge of player slots in Above view is around TABLE_Z - 0.10 - 0.18 + 0.42 = TABLE_Z + 0.14;
        //  cards at TABLE_Z + 1.38 with y in [0, 0.95] tipped -90 deg span z in [TABLE_Z + 0.43, TABLE_Z + 1.38],
        //  so they sit cleanly below the slots on their own row).
        bool povView = (activeView == 0);
        const float cardScale = povView ? 0.68f : 1.06f;
        const float handZ   = TABLE_Z + (povView ? 2.25f : 1.85f);
        const float spacing = povView ? 0.54f : 1.18f;
        const float lean    = povView ? -30.0f : -90.0f;

        auto handMatrix = [&](int i) -> mat4
        {
            float c   = i - 2.5f; // centers the 6 cards (-2.5 ... +2.5)
            float x   = c * spacing;
            float z   = handZ + (povView ? i * 0.02f + c * c * 0.012f : 0.0f);
            float fan = povView ? fanAngles[i] : 0.0f;

            mat4 cardModel = translate(mat4(1.0f), vec3(x, 0.03f, z));
            cardModel = rotate(cardModel, radians(lean), vec3(1.0f, 0.0f, 0.0f));
            cardModel = rotate(cardModel, radians(fan),  vec3(0.0f, 0.0f, 1.0f));
            cardModel = scale(cardModel, vec3(cardScale));
            return cardModel;
        };

        for (int i = 0; i < static_cast<int>(playerCards.size()); ++i)
        {
            if (!cardInHand[i])
                continue;
            playerCards[i].transformation(handMatrix(i));
            playerCards[i].draw(allShader);
        }

        // Flying card animation (G key)
        if (animCard != -1)
        {
            if (animCard < 0 || animCard >= static_cast<int>(playerCards.size()) ||
                    animSlot < 0 || animSlot >= 5)
            {
                animCard = -1;
                animSlot = -1;
            }
            else
            {
                float t = animProgress * animProgress * (3.0f - 2.0f * animProgress);

                Pose from = poseFromMatrix(handMatrix(animCard));
                Pose to   = poseFromMatrix(placedTarget[animSlot]);

                Pose current;
                current.position = mix(from.position, to.position, t);
                current.position.y += std::sin(t * PI_F) * ANIM_ARC_HEIGHT;
                current.rotation = slerp(from.rotation, to.rotation, t);
                current.scaleXYZ = mix(from.scaleXYZ, to.scaleXYZ, t);

                playerCards[animCard].transformation(matrixFromPose(current));
                playerCards[animCard].draw(allShader);
            }
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}

// ---------------------------------------------------------------------------
// Gameplay
// ---------------------------------------------------------------------------
void playRandomCardToNextSlot(int numPlayerCards)
{
    if (animCard != -1 || nextPlayerSlotIndex >= 5)
        return;

    vector<int> inHand;
    for (int i = 0; i < numPlayerCards; ++i)
        if (cardInHand[i])
            inHand.push_back(i);
    if (inHand.empty())
        return;

    int card = inHand[rand() % inHand.size()];

    cardInHand[card] = false;
    animCard = card;
    animSlot = nextPlayerSlotIndex;
    animStartTime = glfwGetTime();
    nextPlayerSlotIndex++;
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------
bool keyPressedOnce(GLFWwindow *window, int key)
{
    static bool wasDown[GLFW_KEY_LAST + 1] = {};
    bool isDown = glfwGetKey(window, key) == GLFW_PRESS;
    bool fired = isDown && !wasDown[key];
    wasDown[key] = isDown;
    return fired;
}

void processInput(GLFWwindow *window, int numPlayerCards)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (keyPressedOnce(window, GLFW_KEY_1))
    {
        activeView = 0;
    }
    if (keyPressedOnce(window, GLFW_KEY_2))
    {
        activeView = 1;
    }
    if (keyPressedOnce(window, GLFW_KEY_G))
    {
        playRandomCardToNextSlot(numPlayerCards);
    }
}

void framebuffer_size_callback(GLFWwindow *window, int width, int height)
{
    SCR_WIDTH = width;
    SCR_HEIGHT = height;
    glViewport(0, 0, width, height);
}

// ---------------------------------------------------------------------------
// Textures
// ---------------------------------------------------------------------------
vector<GLuint> loadTextures(const vector<string> &paths, GLint wrapOption, GLint minFilter, GLint magFilter)
{
    vector<GLuint> textures;
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    for (const string &path : paths)
    {
        GLuint texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapOption);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapOption);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magFilter);

        int width, height, nrChannels;
        unsigned char *data = stbi_load(path.c_str(), &width, &height, &nrChannels, 0);
        if (data)
        {
            GLenum format = (nrChannels == 4) ? GL_RGBA : (nrChannels == 1) ? GL_RED : GL_RGB;
            glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
            glGenerateMipmap(GL_TEXTURE_2D);
        }
        else
        {
            cerr << "Failed to load texture: " << path << endl;
        }
        stbi_image_free(data);

        textures.push_back(texture);
    }
    return textures;
}