#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <list> // NOWOŚĆ: Do przechowywania chunków
#include <cstdio> // Do licznika FPS
// #include <map> // <<< NOWOŚĆ
#include <glm/gtc/integer.hpp> // <<< NOWOŚĆ (dla klucza mapy)
#include <vector>
#include <cmath> // Upewnij się, że to masz
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#define STB_PERLIN_IMPLEMENTATION
#include <stb_perlin.h>

#include "Shader.h"
#include "Camera.h"
#include "Chunk.h" // Ten plik jest teraz bardzo ważny

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>


// Prototypy funkcji
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow *window);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods); // <<< NOWOŚĆ
bool Raycast(glm::vec3 startPos, glm::vec3 direction, float maxDist, glm::ivec3& out_hitBlock, glm::ivec3& out_prevBlock);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset); // <<< NOWOŚĆ

// Ustawienia
const unsigned int SCR_WIDTH = 1280; // Zwiększmy trochę rozdzielczość
const unsigned int SCR_HEIGHT = 720;


// Kamera (ustawiona wyżej, żeby widzieć świat)
Camera camera(glm::vec3(8.0f, 40.0f, 8.0f));
float lastX = SCR_WIDTH / 2.0f;
// --- NOWOŚĆ: GLOBALNA MAPA ŚWIATA ---
// Będziemy przechowywać wskaźniki do chunków w tej mapie.
// Kluczem jest pozycja chunka (np. 0,0 lub 1,0).
class Chunk; // Forward-declaration
std::map<glm::ivec2, Chunk*, CompareIveco2> g_WorldChunks;
std::list<Chunk> chunk_storage;
// ... (zaraz po 'std::list<Chunk> chunk_storage;') s...
//jdhsakhdkjas
// --- NOWOŚĆ: Funkcje Interakcji ze Światem ---

// Funkcja pomocnicza do pobierania bloku z dowolnego miejsca
uint8_t GetBlockGlobal(int x, int y, int z) {
    int chunkX = static_cast<int>(floor(static_cast<float>(x) / CHUNK_WIDTH));
    int chunkZ = static_cast<int>(floor(static_cast<float>(z) / CHUNK_DEPTH));

    auto it = g_WorldChunks.find(glm::ivec2(chunkX, chunkZ));
    if (it == g_WorldChunks.end()) {
        return BLOCK_ID_AIR; // Zakładamy, że niezaładowany = powietrze
    }

    Chunk* chunk = it->second;
    int localX = x - (chunkX * CHUNK_WIDTH);
    int localZ = z - (chunkZ * CHUNK_DEPTH);

    // Musimy użyć 'getBlock' chunka, aby poprawnie obsłużyć y
    return chunk->getBlock(localX, y, localZ);
}

// Funkcja do ustawiania bloku (i aktualizowania meshy)
void SetBlock(int x, int y, int z, uint8_t blockID) {
    int chunkX = static_cast<int>(floor(static_cast<float>(x) / CHUNK_WIDTH));
    int chunkZ = static_cast<int>(floor(static_cast<float>(z) / CHUNK_DEPTH));

    auto it = g_WorldChunks.find(glm::ivec2(chunkX, chunkZ));
    if (it == g_WorldChunks.end()) {
        return; // Nie można edytować niezaładowanego chunka
    }

    Chunk* chunk = it->second;
    int localX = x - (chunkX * CHUNK_WIDTH);
    int localZ = z - (chunkZ * CHUNK_DEPTH);

    // Ustaw blok
    chunk->blocks[localX][y][localZ] = blockID;

    // Przebuduj ten chunk
    chunk->buildMesh();

    // --- KLUCZOWY KROK: Przebuduj sąsiadów, jeśli jesteśmy na krawędzi ---

    // Sprawdź krawędź -X
    if (localX == 0) {
        auto neighborIt = g_WorldChunks.find(glm::ivec2(chunkX - 1, chunkZ));
        if (neighborIt != g_WorldChunks.end()) neighborIt->second->buildMesh();
    }
    // Sprawdź krawędź +X
    if (localX == CHUNK_WIDTH - 1) {
        auto neighborIt = g_WorldChunks.find(glm::ivec2(chunkX + 1, chunkZ));
        if (neighborIt != g_WorldChunks.end()) neighborIt->second->buildMesh();
    }
    // Sprawdź krawędź -Z
    if (localZ == 0) {
        auto neighborIt = g_WorldChunks.find(glm::ivec2(chunkX, chunkZ - 1));
        if (neighborIt != g_WorldChunks.end()) neighborIt->second->buildMesh();
    }
    // Sprawdź krawędź +Z
    if (localZ == CHUNK_DEPTH - 1) {
        auto neighborIt = g_WorldChunks.find(glm::ivec2(chunkX, chunkZ + 1));
        if (neighborIt != g_WorldChunks.end()) neighborIt->second->buildMesh();
    }
}
unsigned int loadCubemapDummy()
{
    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

    // DUMMY DATA: Jeden biały piksel dla każdej ściany
    unsigned char data[] = { 255, 255, 255 };

    // Ładowanie DUMMY DATA dla wszystkich 6 ścian
    for (unsigned int i = 0; i < 6; i++)
    {
        // Używamy formatu RGB (3 kanały)
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
    }

    // Ustaw opcje (jak dla prawdziwego Skyboxa)
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    return textureID;
}
unsigned int loadCubemap(std::vector<std::string> faces)
{
    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

    int width, height, nrChannels;
    for (unsigned int i = 0; i < faces.size(); i++)
    {
        unsigned char *data = stbi_load(faces[i].c_str(), &width, &height, &nrChannels, 0);
        if (data)
        {
            GLenum internalFormat = (nrChannels == 4) ? GL_RGBA : GL_RGB;
            GLenum dataFormat = (nrChannels == 4) ? GL_RGBA : GL_RGB;

            // Kluczowe: musimy sprawdzić, czy dane są ładowane
            if (width == 0 || height == 0) {
                std::cout << "BLAD: Wymiary tekstury wynosza 0 dla: " << faces[i] << std::endl;
                stbi_image_free(data);
                return 0; // Wracamy z błędem
            }

            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, internalFormat, width, height, 0, dataFormat, GL_UNSIGNED_BYTE, data);
            stbi_image_free(data);
        }
        else
        {
            // KRTYCZNE: Jeśli nie wczytało pliku, wyjście jest konieczne
            std::cout << "BLAD 0xC0000005: Cubemap nie wczytal sie w scianie: " << faces[i] << ". Sprawdz NAZWE i ROZSZERZENIE pliku!" << std::endl;
            stbi_image_free(data);
            return 0; // <<< POWRÓĆ Z BŁĘDEM
        }
    }

    // Ustaw opcje
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    return textureID;
}
unsigned int loadUITexture(const char* path)
{
    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrChannels;
    unsigned char *data = stbi_load(path, &width, &height, &nrChannels, 0);

    if (data)
    {
        GLenum internalFormat = (nrChannels == 4) ? GL_RGBA : GL_RGB;
        GLenum dataFormat = (nrChannels == 4) ? GL_RGBA : GL_RGB;

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, dataFormat, GL_UNSIGNED_BYTE, data);

        // Brak mipmap (UI jest zawsze blisko)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        stbi_image_free(data);
        return textureID;
    }

    std::cout << "BLAD: Nie mozna zaladowac tekstury UI: " << path << std::endl;
    stbi_image_free(data);
    return 0;
}

const int RENDER_DISTANCE = 12;
const glm::vec3 SKY_COLOR(0.5f, 0.8f, 1.0f);
// --- KONIEC NOWOŚCI ---
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;
// --- NOWOŚĆ: Globalne stałe i stan fizyki ---
const float GRAVITY = -18.0f;
const float JUMP_FORCE = 7.0f;
const float PLAYER_HEIGHT = 1.8f; // Całkowita wysokość gracza (od stóp do czubka głowy)
const int HOTBAR_SIZE = 9;
// Tablica ID bloków w pasku (indeks 0 to slot 1, itd.)
const uint8_t g_hotbarSlots[HOTBAR_SIZE] = {
    BLOCK_ID_GRASS,
    BLOCK_ID_DIRT,
    BLOCK_ID_STONE,
    BLOCK_ID_WATER,
    BLOCK_ID_GRASS,
    BLOCK_ID_DIRT,
    BLOCK_ID_STONE,
    BLOCK_ID_WATER,
    BLOCK_ID_GRASS
};

int g_activeSlot = 0; // Aktywny slot (0 do 8)
uint8_t g_selectedBlockID = BLOCK_ID_GRASS;
glm::vec3 playerVelocity(0.0f); // Prędkość gracza (dla grawitacji, skoku)
bool onGround = false;          // Czy gracz stoi na ziemi
glm::vec3 wishDir(0.0f);        // Kierunek, w którym gracz "chce" iść (z WSAD)
bool wishJump = false;          // Czy gracz "chce" skoczyć (ze Spacji)
// --- KONIEC NOWOŚCI ---
// DeltaTime
float deltaTime = 0.0f;
float lastFrame = 0.0f;
float cameraSpeed = 4.0f;
// FPS
double lastFPSTime = 0.0;
int frameCount = 0;
unsigned int crosshairVAO, crosshairVBO;
unsigned int highlighterVAO, highlighterVBO;
unsigned int skyboxVAO, skyboxVBO; // <<< NOWOŚĆ
unsigned int cubemapTexture;       // <<< NOWOŚĆ
unsigned int hotbarVAO, hotbarVBO;
unsigned int slotTextureID;      // ID dla ui_slot.png
unsigned int selectorTextureID;  // ID dla ui_selector.png// Nowy shader
// --- KONIEC NOWOŚCI ---
unsigned int iconsTextureID;
bool g_raycastHit = false; // Czy raycast w ogóle w coś trafił?
glm::ivec3 g_hitBlockPos; // Pozycja trafionego bloku
glm::ivec3 g_prevBlockPos; // Pozycja "przed" trafieniem
const float SLOT_SIZE = 64.0f;
const float SELECTOR_SIZE = 70.0f;
const float GAP = 8.0f;
const float BAR_WIDTH = (SLOT_SIZE * HOTBAR_SIZE) + (GAP * (HOTBAR_SIZE - 1));
const float BAR_START_X = (SCR_WIDTH - BAR_WIDTH) / 2.0f; // Wyśrodkowanie paska
int main()
{
    const float G_SKYBOX_VERTICES[] = {
        -1.0f,  1.0f, -1.0f, -1.0f, -1.0f, -1.0f,  1.0f, -1.0f, -1.0f,  1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f, -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f, -1.0f, -1.0f, -1.0f, -1.0f,  1.0f, -1.0f, -1.0f,  1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f, -1.0f,  1.0f, -1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f, -1.0f,  1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f, -1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f, -1.0f,  1.0f, -1.0f, -1.0f,  1.0f,
        -1.0f,  1.0f, -1.0f,  1.0f,  1.0f, -1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f, -1.0f,  1.0f,  1.0f, -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f, -1.0f,  1.0f, -1.0f, -1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f,  1.0f
    };
    g_selectedBlockID = g_hotbarSlots[g_activeSlot];
    // --- 1. Inicjalizacja ---
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Krok 8: Culling!", nullptr, nullptr);
    if (window == nullptr) { /* ... błąd ... */ return -1; }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSwapInterval(0); // Wyłącz V-Sync
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetScrollCallback(window, scroll_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Błąd: Nie można zainicjować GLAD" << std::endl;
        return -1;
    }
    // Włączanie debugowania komunikatów OpenGL
    #ifdef DEBUG
        glEnable(GL_DEBUG_OUTPUT);
        glDebugMessageCallback(NULL, 0); // Ustaw własny callback dla komunikatów
    #endif
    // Włączamy Culling "od tyłu" (standardowa optymalizacja)
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // --- 2. Budowanie shaderów ---
    Shader ourShader("shader.vert", "shader.frag");

    // --- 3. Ładowanie tekstury (ATLAS) ---
    unsigned int textureAtlas;
    glGenTextures(1, &textureAtlas);
    glBindTexture(GL_TEXTURE_2D, textureAtlas);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    int width, height, nrChannels;
    stbi_set_flip_vertically_on_load(true);
    unsigned char *data = stbi_load("atlas.png", &width, &height, &nrChannels, 0);
    if (data)
    {
        // TA LINIA JEST KLUCZOWA:
        // Poprawnie wykrywa, czy plik ma 3 kanały (RGB) czy 4 (RGBA)
        GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;

        // Użyj 'format' w obu miejscach
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
    }
    else { std::cout << "Błąd: Nie można załadować tekstury 'atlas.png'" << std::endl; }
    stbi_image_free(data);
    Shader uiShader("ui.vert", "ui.frag");
    int success;
    glGetProgramiv(uiShader.ID, GL_LINK_STATUS, &success);
    if (!success) {
        // W przypadku błędu, konsola pokaże, co poszło nie tak
        std::cout << "BLAD KRYTYCZNY: Shader UI nie skompilowal sie poprawnie." << std::endl;
        return -1;
    }
    slotTextureID = loadUITexture("ui_slot.png");
    selectorTextureID = loadUITexture("ui_selector.png");
    iconsTextureID = loadUITexture("ui_icons.png");
    ourShader.use();
    ourShader.setInt("ourTexture", 0);
    Shader crosshairShader("crosshair.vert", "crosshair.frag");

    // Definiujemy prosty krzyżyk (+)
    // Współrzędne są w pikselach ekranu (wyśrodkowane)
    float crosshairSize = 10.0f; // 10 pikseli w każdą stronę
    float vertices[] = {
        // Linia pozioma
        (SCR_WIDTH / 2.0f) - crosshairSize, (SCR_HEIGHT / 2.0f),
        (SCR_WIDTH / 2.0f) + crosshairSize, (SCR_HEIGHT / 2.0f),
        // Linia pionowa
        (SCR_WIDTH / 2.0f), (SCR_HEIGHT / 2.0f) - crosshairSize,
        (SCR_WIDTH / 2.0f), (SCR_HEIGHT / 2.0f) + crosshairSize
    };

    glGenVertexArrays(1, &crosshairVAO);
    glGenBuffers(1, &crosshairVBO);
    glBindVertexArray(crosshairVAO);
    glBindBuffer(GL_ARRAY_BUFFER, crosshairVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // Mamy tylko 2 współrzędne (X, Y)
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0); // Od-binduj
    // --- NOWOŚĆ: KONFIGURACJA HOTBARA (Geometria + VAO) ---
    std::vector<float> hotbarGeometry;

    for (int i = 0; i < HOTBAR_SIZE; i++) {
        float x_offset = BAR_START_X + i * (SLOT_SIZE + GAP);
        float y_offset = 10.0f; // 10px od dołu ekranu

        // --- 1. SLOT TŁA (9 Slotów) ---
        // Kwadrat 64x64, UV (0,0) do (1,1)
        hotbarGeometry.push_back(x_offset);               hotbarGeometry.push_back(y_offset);
        hotbarGeometry.push_back(0.0f);                   hotbarGeometry.push_back(0.0f);                 // UV (0, 0)
        hotbarGeometry.push_back(x_offset + SLOT_SIZE);   hotbarGeometry.push_back(y_offset);
        hotbarGeometry.push_back(1.0f);                   hotbarGeometry.push_back(0.0f);                 // UV (1, 0)
        hotbarGeometry.push_back(x_offset + SLOT_SIZE);   hotbarGeometry.push_back(y_offset + SLOT_SIZE);
        hotbarGeometry.push_back(1.0f);                   hotbarGeometry.push_back(1.0f);                 // UV (1, 1)

        hotbarGeometry.push_back(x_offset);               hotbarGeometry.push_back(y_offset);
        hotbarGeometry.push_back(0.0f);                   hotbarGeometry.push_back(0.0f);                 // UV (0, 0)
        hotbarGeometry.push_back(x_offset + SLOT_SIZE);   hotbarGeometry.push_back(y_offset + SLOT_SIZE);
        hotbarGeometry.push_back(1.0f);                   hotbarGeometry.push_back(1.0f);                 // UV (1, 1)
        hotbarGeometry.push_back(x_offset);               hotbarGeometry.push_back(y_offset + SLOT_SIZE);
        hotbarGeometry.push_back(0.0f);                   hotbarGeometry.push_back(1.0f);                 // UV (0, 1)

        // --- 2. IKONY BLOKÓW (9 Ikon) ---
        // Kwadrat 52x52, wyśrodkowany
        float ICON_SIZE = 52.0f;
        float icon_x_start = x_offset + (SLOT_SIZE - ICON_SIZE) / 2.0f;
        float icon_y_start = y_offset + (SLOT_SIZE - ICON_SIZE) / 2.0f;

        // Obliczanie offsetu UV dla ikony z UI_ICONS.png
        uint8_t blockID = g_hotbarSlots[i];

        // Zakładamy: Ikony są ułożone w rzędzie (0, 1, 2, 3...)
        float uv_icon_start_u = (blockID - 1) * UV_STEP_U;

        // Geometria ikony (ta sama geometria, ale inne przesunięcia)
        hotbarGeometry.push_back(icon_x_start);               hotbarGeometry.push_back(icon_y_start);             // V1
        hotbarGeometry.push_back(uv_icon_start_u);            hotbarGeometry.push_back(0.0f);                     // UV (0, 0)
        hotbarGeometry.push_back(icon_x_start + ICON_SIZE);   hotbarGeometry.push_back(icon_y_start);             // V2
        hotbarGeometry.push_back(uv_icon_start_u + UV_STEP_U);hotbarGeometry.push_back(0.0f);                     // UV (1, 0)
        hotbarGeometry.push_back(icon_x_start + ICON_SIZE);   hotbarGeometry.push_back(icon_y_start + ICON_SIZE); // V3
        hotbarGeometry.push_back(uv_icon_start_u + UV_STEP_U);hotbarGeometry.push_back(1.0f);                     // UV (1, 1)

        hotbarGeometry.push_back(icon_x_start);               hotbarGeometry.push_back(icon_y_start);             // V4 (V1)
        hotbarGeometry.push_back(uv_icon_start_u);            hotbarGeometry.push_back(0.0f);                     // UV (0, 0)
        hotbarGeometry.push_back(icon_x_start + ICON_SIZE);   hotbarGeometry.push_back(icon_y_start + ICON_SIZE); // V5 (V3)
        hotbarGeometry.push_back(uv_icon_start_u + UV_STEP_U);hotbarGeometry.push_back(1.0f);                     // UV (1, 1)
        hotbarGeometry.push_back(icon_x_start);               hotbarGeometry.push_back(icon_y_start + ICON_SIZE); // V6
        hotbarGeometry.push_back(uv_icon_start_u);            hotbarGeometry.push_back(1.0f);                     // UV (0, 1)
    }

    // --- 3. Stwórz VAO/VBO dla Hotbara ---
    glGenVertexArrays(1, &hotbarVAO);
    glGenBuffers(1, &hotbarVBO);
    glBindVertexArray(hotbarVAO);
    glBindBuffer(GL_ARRAY_BUFFER, hotbarVBO);
    glBufferData(GL_ARRAY_BUFFER, hotbarGeometry.size() * sizeof(float), hotbarGeometry.data(), GL_STATIC_DRAW);

    // Konfiguracja (4 floaty na wierzchołek: X, Y, U, V)
    GLsizei stride = 4 * sizeof(float);

    // 0: Pozycja (X, Y)
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(0);

    // 1: Tekstura (U, V)
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
    Shader highlighterShader("highlighter.vert", "highlighter.frag");

    // Wierzchołki dla SZKIELETU sześcianu (12 linii * 2 punkty)
    float highlighterVertices[] = {
        // Dolna ściana
        -0.5f, -0.5f, -0.5f,  0.5f, -0.5f, -0.5f,
         0.5f, -0.5f, -0.5f,  0.5f, -0.5f,  0.5f,
         0.5f, -0.5f,  0.5f, -0.5f, -0.5f,  0.5f,
        -0.5f, -0.5f,  0.5f, -0.5f, -0.5f, -0.5f,
        // Górna ściana
        -0.5f,  0.5f, -0.5f,  0.5f,  0.5f, -0.5f,
         0.5f,  0.5f, -0.5f,  0.5f,  0.5f,  0.5f,
         0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f,
        -0.5f,  0.5f,  0.5f, -0.5f,  0.5f, -0.5f,
        // Krawędzie pionowe
        -0.5f, -0.5f, -0.5f, -0.5f,  0.5f, -0.5f,
         0.5f, -0.5f, -0.5f,  0.5f,  0.5f, -0.5f,
         0.5f, -0.5f,  0.5f,  0.5f,  0.5f,  0.5f,
        -0.5f, -0.5f,  0.5f, -0.5f,  0.5f,  0.5f
    };
    glGenVertexArrays(1, &highlighterVAO);
    glGenBuffers(1, &highlighterVBO);
    glBindVertexArray(highlighterVAO);
    glBindBuffer(GL_ARRAY_BUFFER, highlighterVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(highlighterVertices), highlighterVertices, GL_STATIC_DRAW);
    // Mamy tylko pozycję (X, Y, Z)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);


    // 2. Utwórz Shader dla Skyboxa
    Shader skyboxShader("skybox.vert", "skybox.frag");
    std::vector<std::string> faces = {
        "right.png", "left.png",
        "top.png", "bottom.png",
        "front.png", "back.png"
    };
    // --- KONIEC NOWOŚCI ---

    // cubemapTexture = loadCubemap(faces); // <<< Ta linia już widzi 'faces'
    cubemapTexture=loadCubemap(faces);
    if (cubemapTexture == 0) {
        std::cout << "Blad: Skybox nie załadował się. Koniec programu." << std::endl;
        return -1; // Zakończ program bezpiecznie
    }
    skyboxShader.use();
    skyboxShader.setInt("skybox", 0); // Używaj jednostki tekstury 0


    glGenVertexArrays(1, &skyboxVAO);
    glGenBuffers(1, &skyboxVBO);
    glBindVertexArray(skyboxVAO);
    glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    // Używamy globalnej stałej (G_SKYBOX_VERTICES), którą przenieśliśmy na górę
    glBufferData(GL_ARRAY_BUFFER, sizeof(G_SKYBOX_VERTICES), G_SKYBOX_VERTICES, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
   while (!glfwWindowShouldClose(window))
    {
        // --- 1. OBLICZENIA CZASU I INPUTU ---
        auto currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // Licznik FPS
        frameCount++;
        if (currentFrame - lastFPSTime >= 1.0) {
            char title[256];
            sprintf(title, "Crafters | FPS: %d", frameCount);
            glfwSetWindowTitle(window, title);
            frameCount = 0;
            lastFPSTime = currentFrame;
        }
       // --- NOWOŚĆ: KOMPLETNA PĘTLA FIZYKI (zastępuje starą) ---
       float currentSpeed = camera.MovementSpeed;
       // Sprawdź, czy lewy control jest wciśnięty
       if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) {
           currentSpeed *= 1.8f; // Mnożnik sprintu (1.8x szybszy)
       }
       if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS && !camera.flyingMode) {
           currentSpeed *= 0.6f; // Mnożnik sprintu (1.8x szybszy)
       }
        if (camera.flyingMode)
        {
            // --- FIZYKA LATANIA (prosta) ---
            camera.Position += wishDir * currentSpeed * deltaTime;
            onGround = false; // W trybie latania nigdy nie jesteś "na ziemi"
            playerVelocity = glm::vec3(0.0f);
        }
        else
        {
            // --- FIZYKA CHODZENIA (skomplikowana) ---

            // 1. Zastosuj grawitację
            if (!onGround) {
                playerVelocity.y += GRAVITY * deltaTime;
            }

            // 2. Obsłuż skok
            if (wishJump && onGround) {
                playerVelocity.y = JUMP_FORCE;
                onGround = false;
            }

            // 3. Zastosuj "życzenie" ruchu (WSAD) do prędkości
            playerVelocity.x = wishDir.x * currentSpeed;
            playerVelocity.z = wishDir.z * currentSpeed;

            // 4. Oblicz nową pozycję
            glm::vec3 newPos = camera.Position + playerVelocity * deltaTime;

            // 5. Sprawdź kolizje (prosty AABB)
            // Sprawdzamy 2 punkty na wysokości (stopy i głowa)
            float playerWidthHalf = PLAYER_WIDTH / 2.0f;
            glm::vec3 feetCheck = newPos;
            glm::vec3 headCheck = newPos + glm::vec3(0.0f, PLAYER_HEIGHT, 0.0f);

            // Kolizja w osi Y (Góra/Dół)
            if (playerVelocity.y > 0) { // Lecimy w GÓRĘ (skok)
                if (GetBlockGlobal(round(headCheck.x), round(headCheck.y), round(headCheck.z)) != BLOCK_ID_AIR) {
                    playerVelocity.y = 0; // Uderzyliśmy w sufit
                }
            } else { // Lecimy w DÓŁ (grawitacja)
                if (GetBlockGlobal(round(feetCheck.x), round(feetCheck.y - 0.1f), round(feetCheck.z)) != BLOCK_ID_AIR) {
                    playerVelocity.y = 0;
                    onGround = true;
                    newPos.y = round(feetCheck.y - 0.1f) + 0.5f; // Ląduj na bloku
                } else {
                    onGround = false;
                }
            }

            // Kolizja w osi X
            if (playerVelocity.x > 0) { // Idziemy w prawo (+X)
                if (GetBlockGlobal(round(feetCheck.x + playerWidthHalf), round(feetCheck.y), round(feetCheck.z)) != BLOCK_ID_AIR ||
                    GetBlockGlobal(round(headCheck.x + playerWidthHalf), round(headCheck.y), round(headCheck.z)) != BLOCK_ID_AIR) {
                    newPos.x = camera.Position.x; // Zablokuj ruch X
                }
            } else if (playerVelocity.x < 0) { // Idziemy w lewo (-X)
                if (GetBlockGlobal(round(feetCheck.x - playerWidthHalf), round(feetCheck.y), round(feetCheck.z)) != BLOCK_ID_AIR ||
                    GetBlockGlobal(round(headCheck.x - playerWidthHalf), round(headCheck.y), round(headCheck.z)) != BLOCK_ID_AIR) {
                    newPos.x = camera.Position.x; // Zablokuj ruch X
                }
            }

            // Kolizja w osi Z
            if (playerVelocity.z > 0) { // Idziemy do przodu (+Z)
                if (GetBlockGlobal(round(feetCheck.x), round(feetCheck.y), round(feetCheck.z + playerWidthHalf)) != BLOCK_ID_AIR ||
                    GetBlockGlobal(round(headCheck.x), round(headCheck.y), round(headCheck.z + playerWidthHalf)) != BLOCK_ID_AIR) {
                    newPos.z = camera.Position.z; // Zablokuj ruch Z
                }
            } else if (playerVelocity.z < 0) { // Idziemy do tyłu (-Z)
                if (GetBlockGlobal(round(feetCheck.x), round(feetCheck.y), round(feetCheck.z - playerWidthHalf)) != BLOCK_ID_AIR ||
                    GetBlockGlobal(round(headCheck.x), round(headCheck.y), round(headCheck.z - playerWidthHalf)) != BLOCK_ID_AIR) {
                    newPos.z = camera.Position.z; // Zablokuj ruch Z
                }
            }

            // 6. Ostatecznie zaktualizuj pozycję kamery (stóp)
            camera.Position = newPos;
        }
        // --- KONIEC FIZYKI ---
        processInput(window); // Obsługa WSAD i ESC

        // --- 2. RAYCASTING (w każdej klatce) ---
        float raycastDistance = 5.0f;
        glm::vec3 eyePos = camera.Position + glm::vec3(0.0f, PLAYER_EYE_HEIGHT, 0.0f);
        g_raycastHit = Raycast(eyePos, camera.Front, raycastDistance, g_hitBlockPos, g_prevBlockPos);

        // --- 3. ŁADOWANIE CHUNKÓW (bez zmian) ---
        int playerChunkX = static_cast<int>(floor(camera.Position.x / CHUNK_WIDTH));
        int playerChunkZ = static_cast<int>(floor(camera.Position.z / CHUNK_DEPTH));

        std::vector<Chunk*> chunksToBuild;
        for (int x = playerChunkX - RENDER_DISTANCE; x <= playerChunkX + RENDER_DISTANCE; x++) {
            for (int z = playerChunkZ - RENDER_DISTANCE; z <= playerChunkZ + RENDER_DISTANCE; z++) {
                glm::ivec2 chunkPos(x, z);
                if (g_WorldChunks.find(chunkPos) == g_WorldChunks.end()) {
                    chunk_storage.emplace_back(glm::vec3(x * CHUNK_WIDTH, 0.0f, z * CHUNK_DEPTH));
                    Chunk* newChunk = &chunk_storage.back();
                    g_WorldChunks[chunkPos] = newChunk;
                    chunksToBuild.push_back(newChunk);
                }
            }
        }
        for (Chunk* chunk : chunksToBuild) {
            chunk->buildMesh();
        }

        // --- 4. RENDEROWANIE ---
        glClearColor(0.5f, 0.8f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Ustaw widok i projekcję RAZ
        ourShader.use();
       float fogStart = (RENDER_DISTANCE - 2) * CHUNK_WIDTH; // Mgła zaczyna się 2 chunky od krawędzi
       float fogEnd = (RENDER_DISTANCE + 1) * CHUNK_WIDTH;   // Mgła jest pełna na krawędzi
       ourShader.setVec3("u_fogColor", SKY_COLOR);
       ourShader.setFloat("u_fogStart", fogStart);
       ourShader.setFloat("u_fogEnd", fogEnd);
       float viewDistance = (RENDER_DISTANCE + 1) * CHUNK_WIDTH; // +1 dla bezpieczeństwa
       glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, viewDistance);
       glm::mat4 view = camera.GetViewMatrix();
        ourShader.setMat4("projection", projection);
        ourShader.setMat4("view", view);
       if (cubemapTexture != 0)
       {
           // 1. Zmień stan głębi: GL_LEQUAL rysuje obiekty na krawędzi dalekiego planu
           glDepthFunc(GL_LEQUAL);

           // 2. Użyj shadera
           skyboxShader.use();
           skyboxShader.setMat4("projection", projection);

           // Macierz widoku (View) bez translacji: Skybox podąża za kamerą
           glm::mat4 viewNoTranslation = glm::mat4(glm::mat3(view));
           skyboxShader.setMat4("view", viewNoTranslation);

           // 3. Rysuj
           glBindVertexArray(skyboxVAO);
           glActiveTexture(GL_TEXTURE0);
           glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
           glDrawArrays(GL_TRIANGLES, 0, 36);
           glBindVertexArray(0);

           // 4. Przywróć stan głębi (KLUCZOWE)
           glDepthFunc(GL_LESS);
       }

       ourShader.use();
        // PRZEBIEG 1: Solid
        glDepthMask(GL_TRUE);
        glEnable(GL_CULL_FACE);
        for (int x = playerChunkX - RENDER_DISTANCE; x <= playerChunkX + RENDER_DISTANCE; x++) {
            for (int z = playerChunkZ - RENDER_DISTANCE; z <= playerChunkZ + RENDER_DISTANCE; z++) {
                auto it = g_WorldChunks.find(glm::ivec2(x, z));
                if (it != g_WorldChunks.end()) {
                    it->second->DrawSolid(ourShader, textureAtlas);
                }
            }
        }

        // PRZEBIEG 1.5: PODŚWIETLENIE
        if (g_raycastHit)
        {
            glDisable(GL_DEPTH_TEST);
            glDisable(GL_CULL_FACE);

            highlighterShader.use();
            highlighterShader.setMat4("projection", projection);
            highlighterShader.setMat4("view", view);

            // POPRAWKA: Prostsza transformacja
            glm::mat4 model = glm::mat4(1.0f);
            // 1. Przesuń obwódkę (wyśrodkowaną na 0,0,0)
            //    na pozycję bloku (wyśrodkowanego na (x,y,z))
            model = glm::translate(model, glm::vec3(g_hitBlockPos));
            // 2. Lekko powiększ, aby uniknąć Z-fightingu
            model = glm::scale(model, glm::vec3(1.002f, 1.002f, 1.002f));

            highlighterShader.setMat4("model", model);

            glBindVertexArray(highlighterVAO);
            glDrawArrays(GL_LINES, 0, 24); // Rysuj 24 wierzchołki (12 linii)
            glBindVertexArray(0);

            glEnable(GL_DEPTH_TEST);
            glEnable(GL_CULL_FACE);
        }

        // PRZEBIEG 2: Transparent (Woda) - POPRAWIONY
        ourShader.use();
        glDepthMask(GL_FALSE);
        for (int x = playerChunkX - RENDER_DISTANCE; x <= playerChunkX + RENDER_DISTANCE; x++) {
            for (int z = playerChunkZ - RENDER_DISTANCE; z <= playerChunkZ + RENDER_DISTANCE; z++) {
                // TUTAJ BYŁ BŁĄD - TERAZ JEST POPRAWNIE
                auto it = g_WorldChunks.find(glm::ivec2(x, z));
                if (it != g_WorldChunks.end()) {
                    it->second->DrawTransparent(ourShader, textureAtlas);
                }
            }
        }

        // PRZEBIEG 3: UI (Celownik)
        glDisable(GL_DEPTH_TEST);
        crosshairShader.use();
        glm::mat4 ortho_projection = glm::ortho(0.0f, (float)SCR_WIDTH, 0.0f, (float)SCR_HEIGHT);
        crosshairShader.setMat4("projection", ortho_projection);
        glBindVertexArray(crosshairVAO);
        glDrawArrays(GL_LINES, 0, 4);
        glBindVertexArray(0);
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        glDisable(GL_DEPTH_TEST);
        uiShader.use();

       glDisable(GL_DEPTH_TEST);
       glDisable(GL_CULL_FACE);
       uiShader.use();

       // Ustaw matrycę ortograficzn
       uiShader.setMat4("projection", ortho_projection);

       // 1. Rysuj Tła Slotów (9 Slotów)
       glBindVertexArray(hotbarVAO);
       glActiveTexture(GL_TEXTURE0);
       glBindTexture(GL_TEXTURE_2D, slotTextureID); // Tekstura slotu
       uiShader.setInt("uiTexture", 0);
       glDrawArrays(GL_TRIANGLES, 0, HOTBAR_SIZE * 6);

       // 2. Rysuj Ikony Bloków
       glActiveTexture(GL_TEXTURE0);
       glBindTexture(GL_TEXTURE_2D, iconsTextureID); // Tekstura ikon
       int icon_offset_vertices = HOTBAR_SIZE * 6; // Przesunięcie do sekcji ikon w VBO
       glDrawArrays(GL_TRIANGLES, icon_offset_vertices, HOTBAR_SIZE * 6);

       // 3. Rysuj Selekter (Ramka podświetlenia)
       glActiveTexture(GL_TEXTURE0);
       glBindTexture(GL_TEXTURE_2D, selectorTextureID); // Tekstura selektora

       // Oblicz, od którego wierzchołka zacząć rysować aktywny selektor (6 wierzchołków na slot)
       int selector_render_offset = g_activeSlot * 6;
       glDrawArrays(GL_TRIANGLES, selector_render_offset, 6); // Rysuj jeden kwadrat (ramka)

       glBindVertexArray(0);
       glEnable(GL_DEPTH_TEST);
       glEnable(GL_CULL_FACE);
       glDepthMask(GL_TRUE);
        // --- 5. SWAP BUFFERS ---
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // --- 6. Sprzątanie ---
    // Destruktory chunków (`~Chunk()`) zostaną wywołane automatycznie
    // gdy wektor 'chunks' wyjdzie poza zakres.
    glDeleteProgram(ourShader.ID);
    glDeleteProgram(crosshairShader.ID); // <<< NOWOŚĆ
    glDeleteVertexArrays(1, &crosshairVAO); // <<< NOWOŚĆ
    glDeleteBuffers(1, &crosshairVBO); // <<< NOWOŚĆ
    glDeleteVertexArrays(1, &highlighterVAO); // <<< NOWOŚĆ
    glDeleteBuffers(1, &highlighterVBO);
    glfwTerminate();
    return 0;
}

void processInput(GLFWwindow *window)
{
    static bool f_pressed = false;
    // --- NOWOŚĆ: Obsługa klawiszy numerycznych ---
    for (int i = 0; i < HOTBAR_SIZE; i++) {
        // GLFW_KEY_1 jest 49, GLFW_KEY_2 jest 50, itd.
        if (glfwGetKey(window, GLFW_KEY_1 + i) == GLFW_PRESS) {
            g_activeSlot = i;
            g_selectedBlockID = g_hotbarSlots[i];
            break;
        }
    }
    // Zamknięcie okna
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    // Przełącznik trybu Latanie/Chodzenie
    if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS) {
        if (!f_pressed) {
            camera.ToggleFlyingMode();
            std::cout << "Tryb latania: " << (camera.flyingMode ? "ON" : "OFF") << std::endl;
            if (!camera.flyingMode) playerVelocity = glm::vec3(0.0f); // Resetuj prędkość
        }
        f_pressed = true;
    }
    if (glfwGetKey(window, GLFW_KEY_F) == GLFW_RELEASE) {
        f_pressed = false;
    }

    // --- Ustawianie "Życzenia" Ruchu (wishDir) ---
    wishDir = glm::vec3(0.0f); // Resetuj w każdej klatce

    if (camera.flyingMode)
    {
        // --- Tryb Latania ---
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) wishDir += camera.Front;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) wishDir -= camera.Front;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) wishDir -= camera.Right;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) wishDir += camera.Right;
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) wishDir += camera.WorldUp;
        if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) wishDir -= camera.WorldUp;
    }
    else
    {
        // --- Tryb Chodzenia (tylko XZ) ---
        glm::vec3 front_horizontal = glm::normalize(glm::vec3(camera.Front.x, 0.0f, camera.Front.z));
        glm::vec3 right_horizontal = glm::normalize(glm::vec3(camera.Right.x, 0.0f, camera.Right.z));

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) wishDir += front_horizontal;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) wishDir -= front_horizontal;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) wishDir -= right_horizontal;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) wishDir += right_horizontal;

        // Skok
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
            wishJump = true;
        } else {
            wishJump = false;
        }
    }

    // Normalizuj wektor "życzenia" (jeśli nie jest zerowy), aby ruch po przekątnej nie był szybszy
    if (glm::length(wishDir) > 0.1f) {
        wishDir = glm::normalize(wishDir);
    }
}
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}

void mouse_callback(GLFWwindow* window, double xposIn, double yposIn)
{
    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);
    if (firstMouse) { lastX = xpos; lastY = ypos; firstMouse = false; }
    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;
    lastX = xpos;
    lastY = ypos;
    camera.ProcessMouseMovement(xoffset, yoffset);
}
// --- NOWOŚĆ: Funkcja do obsługi kliknięć myszy ---

// --- NOWOŚĆ: Zmodyfikowany Raycast do budowania ---

// Raycast zwraca 'true', jeśli trafił, 'false', jeśli nie.
// Wypełnia 'out_hitBlock' (blok, który trafiono)
// Wypełnia 'out_prevBlock' (blok powietrza tuż przed trafieniem)
bool Raycast(glm::vec3 startPos, glm::vec3 direction, float maxDist, glm::ivec3& out_hitBlock, glm::ivec3& out_prevBlock) {
    glm::vec3 currentPos = startPos;
    float step = 0.05f; // Krok co 5cm

    // Używamy round() do znalezienia bloku, w którym jest kamera
    glm::ivec3 prevBlockPos = glm::ivec3(
        static_cast<int>(round(startPos.x)),
        static_cast<int>(round(startPos.y)),
        static_cast<int>(round(startPos.z))
    );

    for (float t = 0.0f; t < maxDist; t += step) {
        currentPos = startPos + direction * t;

        // POPRAWKA: Używamy round() zamiast floor()
        glm::ivec3 blockPos = glm::ivec3(
            static_cast<int>(round(currentPos.x)),
            static_cast<int>(round(currentPos.y)),
            static_cast<int>(round(currentPos.z))
        );

        // Sprawdź, czy ten blok jest solidny
        if (GetBlockGlobal(blockPos.x, blockPos.y, blockPos.z) != BLOCK_ID_AIR) {
            out_hitBlock = blockPos;
            out_prevBlock = prevBlockPos;
            return true;
        }

        prevBlockPos = blockPos;
    }
    return false; // Nic nie trafiono
}


void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
    if (g_raycastHit && action == GLFW_PRESS) // Sprawdź, czy trafiliśmy i czy przycisk jest wciśnięty
    {
        if (button == GLFW_MOUSE_BUTTON_LEFT)
        {
            // --- NISZCZENIE (Lewy Przycisk) ---
            std::cout << "Niszczenie bloku w: "
                      << g_hitBlockPos.x << ", " << g_hitBlockPos.y << ", " << g_hitBlockPos.z << std::endl;

            SetBlock(g_hitBlockPos.x, g_hitBlockPos.y, g_hitBlockPos.z, BLOCK_ID_AIR);
        }
        else if (button == GLFW_MOUSE_BUTTON_RIGHT)
        {
            // --- BUDOWANIE (Prawy Przycisk) ---
            std::cout << "Budowanie bloku w: "
                      << g_prevBlockPos.x << ", " << g_prevBlockPos.y << ", " << g_prevBlockPos.z << std::endl;

            SetBlock(g_prevBlockPos.x, g_prevBlockPos.y, g_prevBlockPos.z, g_selectedBlockID);
        }
    }
}
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    // yoffset to pionowy scroll (1.0 lub -1.0)
    int newSlot = g_activeSlot - static_cast<int>(yoffset);

    // Zawiń (wrap) indeks, aby pozostał w zakresie 0-8
    if (newSlot < 0) {
        newSlot = HOTBAR_SIZE - 1;
    } else if (newSlot >= HOTBAR_SIZE) {
        newSlot = 0;
    }

    g_activeSlot = newSlot;
    g_selectedBlockID = g_hotbarSlots[g_activeSlot];

    std::cout << "Wybrano slot: " << g_activeSlot + 1 << " (Block ID: " << g_selectedBlockID << ")" << std::endl;
}