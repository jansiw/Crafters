#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <list> // NOWOŚĆ: Do przechowywania chunków
#include <cstdio> // Do licznika FPS
// #include <map> // <<< NOWOŚĆ
#include <glm/gtc/integer.hpp> // <<< NOWOŚĆ (dla klucza mapy)
#include <vector>
#include <cmath> // Upewnij się, że to masz
#include <filesystem> // <<< NOWOŚĆ
#include <sstream>    // <<< NOWOŚĆ
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
#include <cstdlib> // Dla rand() i srand()
#include <ctime>   // Dla time()
enum GameState {
    STATE_MAIN_MENU,
    STATE_NEW_WORLD_MENU,
    STATE_LOADING_WORLD,
    STATE_IN_GAME,
    STATE_EXITING
};
GameState g_currentState = STATE_MAIN_MENU;

// Prototypy funkcji
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow *window);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods); // <<< NOWOŚĆ
bool Raycast(glm::vec3 startPos, glm::vec3 direction, float maxDist, glm::ivec3& out_hitBlock, glm::ivec3& out_prevBlock);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset); // <<< NOWOŚĆ
bool IsMouseOver(double mouseX, double mouseY, float x, float y, float w, float h);
void DrawMenuButton(Shader& uiShader, unsigned int vao, unsigned int textureID, float x, float y, float w, float h);
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
void char_callback(GLFWwindow* window, unsigned int codepoint);
void RenderText(Shader& shader, unsigned int vao, const std::string& text,
                float x, float y, float size);
// Ustawienia
const unsigned int SCR_WIDTH = 1280; // Zwiększmy trochę rozdzielczość
const unsigned int SCR_HEIGHT = 720;
std::string g_selectedWorldName;
int g_selectedWorldSeed;
std::vector<std::string> existingWorlds;
int currentMenuPage = 0;

// Tekstury przycisków
unsigned int texBtnNew, texBtnLoad, texBtnQuit;

// Stałe przycisków (muszą być globalne dla callbacka myszy)
const float BTN_W = 300.0f;
const float BTN_H = 60.0f;
const float BTN_X = (SCR_WIDTH - BTN_W) / 2.0f;
const float BTN_Y_NEW = SCR_HEIGHT / 2.0f + 70.0f;
const float BTN_Y_LOAD = SCR_HEIGHT / 2.0f;
const float BTN_Y_QUIT = SCR_HEIGHT / 2.0f - 70.0f;
std::string g_inputWorldName = "NowySwiat";
std::string g_inputSeedString;
int g_activeInput = 0; // 0 = Nazwa, 1 = Seed, -1 = Brak

// Tekstury dla nowego menu
unsigned int texBtnStart;
unsigned int texBtnBack;
unsigned int texInputBox; // Będziemy reużywać ui_slot.png
unsigned int texFont;

// Pozycje dla nowego menu (ustaw je jak chcesz)
const float INPUT_W = 400.0f;
const float INPUT_H = 50.0f;
const float INPUT_X = (SCR_WIDTH - INPUT_W) / 2.0f;
const float INPUT_NAME_Y = SCR_HEIGHT / 2.0f + 60.0f;
const float INPUT_SEED_Y = SCR_HEIGHT / 2.0f;

const float BTN_START_X = (SCR_WIDTH - BTN_W) / 2.0f;
const float BTN_START_Y = SCR_HEIGHT / 2.0f - 70.0f;
const float BTN_BACK_X = (SCR_WIDTH - BTN_W) / 2.0f;
const float BTN_BACK_Y = SCR_HEIGHT / 2.0f - 140.0f;
const float FONT_ATLAS_COLS = 16.0f;
const float FONT_ATLAS_ROWS = 8.0f;
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
const std::string G_WORLD_NAME = "MojPierwszySwiat";
const int G_WORLD_SEED = 12345; // Zmień tę liczbę, aby dostać inny świat
// --- KONIEC NOWOŚCI ---
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;
// --- NOWOŚĆ: Globalne stałe i stan fizyki ---
const float GRAVITY = -18.0f;
const float JUMP_FORCE = 7.0f;
const float PLAYER_HEIGHT = 1.8f; // Całkowita wysokość gracza (od stóp do czubka głowy)
const int HOTBAR_SIZE = 9;
const float BUOYANCY = 9.0f;    // Siła wyporu (trochę słabsza niż grawitacja)
const float WATER_DRAG = 0.85f; // Spowolnienie w wodzie
// Tablica ID bloków w pasku (indeks 0 to slot 1, itd.)
const uint8_t g_hotbarSlots[HOTBAR_SIZE] = {
    BLOCK_ID_GRASS,
    BLOCK_ID_DIRT,
    BLOCK_ID_STONE,
    BLOCK_ID_WATER,
    BLOCK_ID_BEDROCK,
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
unsigned int loadingTextureID;
bool g_raycastHit = false; // Czy raycast w ogóle w coś trafił?
glm::ivec3 g_hitBlockPos; // Pozycja trafionego bloku
glm::ivec3 g_prevBlockPos; // Pozycja "przed" trafieniem
const float SLOT_SIZE = 64.0f;
const float SELECTOR_SIZE = 70.0f;
const float GAP = 0.0f;
const float BAR_WIDTH = (SLOT_SIZE * HOTBAR_SIZE) + (GAP * (HOTBAR_SIZE - 1));
const float BAR_START_X = (SCR_WIDTH - BAR_WIDTH) / 2.0f; // Wyśrodkowanie paska
const float ICON_ATLAS_COLS = 8.0f; // Ile jest ikonek w rzędzie w pliku ui_icons.png
const float ICON_ATLAS_ROWS = 1.0f; // Ile jest rzędów
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
    std::string worldFolderPath = "worlds/" + G_WORLD_NAME;
    if (!std::filesystem::exists("worlds")) {
        std::filesystem::create_directory("worlds");
    }
    if (!std::filesystem::exists(worldFolderPath)) {
        try {
            std::filesystem::create_directory(worldFolderPath);
            std::cout << "Utworzono folder swiata: '" << worldFolderPath << "'" << std::endl;
        } catch (const std::filesystem::filesystem_error& e) {
            std::cout << "Blad: Nie mozna utworzyc folderu swiata: " << e.what() << std::endl;
        }
    }
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
    glfwSetCharCallback(window, char_callback);
    glfwSetKeyCallback(window, key_callback);

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
    loadingTextureID = loadUITexture("loading.png");
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
    // std::vector<float> hotbarGeometry;

    // --- NOWOŚĆ: KONFIGURACJA HOTBARA (JEDEN KWADRAT) ---
    // Stworzymy jeden kwadrat "unit quad" od (0,0) do (1,1).
    // Będziemy go skalować i przesuwać za pomocą macierzy "model".
    float quadVertices[] = {
        // Pozycja (X,Y)   // UV
        0.0f, 0.0f,       0.0f, 0.0f, // Dół-Lewo
        1.0f, 0.0f,       1.0f, 0.0f, // Dół-Prawo
        1.0f, 1.0f,       1.0f, 1.0f, // Góra-Prawo

        0.0f, 0.0f,       0.0f, 0.0f, // Dół-Lewo
        1.0f, 1.0f,       1.0f, 1.0f, // Góra-Prawo
        0.0f, 1.0f,       0.0f, 1.0f  // Góra-Lewo
    };

    glGenVertexArrays(1, &hotbarVAO);
    glGenBuffers(1, &hotbarVBO);
    glBindVertexArray(hotbarVAO);
    glBindBuffer(GL_ARRAY_BUFFER, hotbarVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

    // Atrybuty: 4 floaty (X, Y, U, V)
    GLsizei stride = 4 * sizeof(float);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(0);
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
    // ... (kod odglBindVertexArray(0); dla Skyboxa)
    // Inicjalizacja generatora liczb losowych
    srand(static_cast<unsigned int>(time(NULL)));

    // Wczytaj tekstury przycisków
    texBtnNew = loadUITexture("button_new_world.png");
    texBtnLoad = loadUITexture("button_load_world.png");
    texBtnQuit = loadUITexture("button_quit.png");
    texBtnStart = loadUITexture("button_start.png");
    texBtnBack = loadUITexture("button_back.png");
    texInputBox = loadUITexture("ui_slot.png"); // Reużywamy starej tekstury
    texFont = loadUITexture("font.png");
    // --- NOWOŚĆ: PĘTLA ŁADOWANIA I WYGASZANIA ---
    glm::mat4 ortho_projection = glm::ortho(0.0f, (float)SCR_WIDTH, 0.0f, (float)SCR_HEIGHT);


   while (g_currentState != STATE_EXITING)
{
    // Obliczanie deltaTime (wspólne dla wszystkich stanów)
    // auto currentFrame = static_cast<float>(glfwGetTime());
    // deltaTime = currentFrame - lastFrame;
    // lastFrame = currentFrame;

    switch (g_currentState)
    {
        case STATE_MAIN_MENU:
        {
            glDisable(GL_DEPTH_TEST);
            glDisable(GL_CULL_FACE);
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL); // POKAŻ KURSOR

            glClearColor(0.1f, 0.1f, 0.1f, 1.0f); // Ciemne tło
            glClear(GL_COLOR_BUFFER_BIT);

            uiShader.use();
            uiShader.setMat4("projection", ortho_projection);
            uiShader.setFloat("u_opacity", 1.0f);
            uiShader.setVec2("u_uvScale", 1.0f, 1.0f);
            uiShader.setVec2("u_uvOffset", 0.0f, 0.0f);
            uiShader.setInt("uiTexture", 0);

            if (currentMenuPage == 0) {
                DrawMenuButton(uiShader, hotbarVAO, texBtnNew, BTN_X, BTN_Y_NEW, BTN_W, BTN_H);
                DrawMenuButton(uiShader, hotbarVAO, texBtnLoad, BTN_X, BTN_Y_LOAD, BTN_W, BTN_H);
                DrawMenuButton(uiShader, hotbarVAO, texBtnQuit, BTN_X, BTN_Y_QUIT, BTN_W, BTN_H);
            }
            // else if (currentMenuPage == 1) { /* Kod dla listy światów */ }

            break; // Koniec case STATE_MAIN_MENU
        }
        case STATE_NEW_WORLD_MENU:
        {
            glDisable(GL_DEPTH_TEST);
            glDisable(GL_CULL_FACE);
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

            glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            uiShader.use();
            uiShader.setMat4("projection", ortho_projection);
            uiShader.setFloat("u_opacity", 1.0f);
            uiShader.setVec2("u_uvScale", 1.0f, 1.0f);
            uiShader.setVec2("u_uvOffset", 0.0f, 0.0f);
            uiShader.setInt("uiTexture", 0);

            // TODO: Narysuj etykiety "Nazwa Świata:", "Seed:" (gdy będziesz miał fonty)

            // Rysuj pole nazwy
            DrawMenuButton(uiShader, hotbarVAO, texInputBox, INPUT_X, INPUT_NAME_Y, INPUT_W, INPUT_H);

            // Rysuj pole seeda
            DrawMenuButton(uiShader, hotbarVAO, texInputBox, INPUT_X, INPUT_SEED_Y, INPUT_W, INPUT_H);

            // --- NOWY KOD DO RYSOWANIA TEKSTU ---

            // Przygotuj stringi z migającym kursorem
            std::string name_to_render = g_inputWorldName;
            std::string seed_to_render = g_inputSeedString;

            // Sprawdzamy czas, aby kursor migał co pół sekundy
            if (static_cast<int>(glfwGetTime() * 2.0f) % 2 == 0) {
                if (g_activeInput == 0) name_to_render += "_";
                else if (g_activeInput == 1) seed_to_render += "_";
            }

            // Definiujemy rozmiar i padding tekstu
            float FONT_SIZE = 24.0f;
            float PADDING_X = 10.0f;
            float PADDING_Y = (INPUT_H - FONT_SIZE) / 2.0f; // Wyśrodkuj w pionie

            // Rysuj tekst
            RenderText(uiShader, hotbarVAO, name_to_render, INPUT_X + PADDING_X, INPUT_NAME_Y + PADDING_Y, FONT_SIZE);
            RenderText(uiShader, hotbarVAO, seed_to_render, INPUT_X + PADDING_X, INPUT_SEED_Y + PADDING_Y, FONT_SIZE);


            // Rysuj przycisk Start
            DrawMenuButton(uiShader, hotbarVAO, texBtnStart, BTN_START_X, BTN_START_Y, BTN_W, BTN_H);
            // Rysuj przycisk Wstecz
            DrawMenuButton(uiShader, hotbarVAO, texBtnBack, BTN_BACK_X, BTN_BACK_Y, BTN_W, BTN_H);
            break;
        }
        case STATE_LOADING_WORLD: {
        {
            // === ETAP 1: Pokaż statyczny obrazek ładowania ===
            glDisable(GL_DEPTH_TEST);
            uiShader.use();
            uiShader.setMat4("projection", ortho_projection);
            uiShader.setVec2("u_uvScale", 1.0f, 1.0f);
            uiShader.setVec2("u_uvOffset", 0.0f, 0.0f);
            uiShader.setFloat("u_opacity", 1.0f); // W pełni widoczny

            glm::mat4 model = glm::mat4(1.0f);
            model = glm::scale(model, glm::vec3(SCR_WIDTH, SCR_HEIGHT, 1.0f));
            uiShader.setMat4("model", model);

            glClearColor(0.0f, 0.0f, 0.0f, 1.0f); // Czarne tło
            glClear(GL_COLOR_BUFFER_BIT);

            glBindVertexArray(hotbarVAO); // Użyj "pędzla" (unit quad)
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, loadingTextureID);
            uiShader.setInt("uiTexture", 0);
            glDrawArrays(GL_TRIANGLES, 0, 6);

            glfwSwapBuffers(window);
            glfwPollEvents();

            // === ETAP 2: Wykonaj całą ciężką pracę (generowanie świata) ===
            // (Ten kod jest ukryty za ekranem ładowania)
            int playerChunkX = static_cast<int>(floor(camera.Position.x / CHUNK_WIDTH));
            int playerChunkZ = static_cast<int>(floor(camera.Position.z / CHUNK_DEPTH));
            std::vector<Chunk*> chunksToBuild;
            for (int x = playerChunkX - RENDER_DISTANCE; x <= playerChunkX + RENDER_DISTANCE; x++) {
                for (int z = playerChunkZ - RENDER_DISTANCE; z <= playerChunkZ + RENDER_DISTANCE; z++) {
                    glm::ivec2 chunkPos(x, z);
                    if (g_WorldChunks.find(chunkPos) == g_WorldChunks.end()) {
                        chunk_storage.emplace_back(glm::vec3(x * CHUNK_WIDTH, 0.0f, z * CHUNK_DEPTH), g_selectedWorldName, g_selectedWorldSeed);
                        Chunk* newChunk = &chunk_storage.back();
                        g_WorldChunks[chunkPos] = newChunk;
                        chunksToBuild.push_back(newChunk);
                    }
                }
            }
            for (Chunk* chunk : chunksToBuild) {
                chunk->buildMesh();
            }
        }
        std::cout << "Ladowanie zakonczone. Wygaszanie..." << std::endl;

        // === ETAP 3: Pętla Wygaszania (Fade-out) ===
        float fadeDuration = 1.0f; // Czas trwania wygaszania (w sekundach)
        float fadeTimer = fadeDuration;

        // Pobierz czas rozpoczęcia wygaszania
        float fadeStartTime = static_cast<float>(glfwGetTime());
        float lastFadeFrame = fadeStartTime;

        while (fadeTimer > 0.0f)
        {
            // Oblicz deltaTime dla tej mini-pętli
            float currentFadeFrame = static_cast<float>(glfwGetTime());
            float fadeDeltaTime = currentFadeFrame - lastFadeFrame;
            lastFadeFrame = currentFadeFrame;

            fadeTimer -= fadeDeltaTime;
            float opacity = fadeTimer / fadeDuration; // Opacity od 1.0 do 0.0

            // --- Rysuj świat (tylko Solid + Skybox) ---
            // (Musimy narysować to, co jest "pod" ekranem ładowania)
            glClearColor(SKY_COLOR.x, SKY_COLOR.y, SKY_COLOR.z, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            // Oblicz macierze (tak jak w głównej pętli)
            float viewDistance = (RENDER_DISTANCE + 1) * CHUNK_WIDTH;
            glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, viewDistance);
            glm::mat4 view = camera.GetViewMatrix();

            // Rysuj Skybox (bez zmian)
            if (cubemapTexture != 0) {
                glDepthFunc(GL_LEQUAL);
                skyboxShader.use();
                skyboxShader.setMat4("projection", projection);
                skyboxShader.setMat4("view", glm::mat4(glm::mat3(view)));
                glBindVertexArray(skyboxVAO);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
                glDrawArrays(GL_TRIANGLES, 0, 36);
                glBindVertexArray(0);
                glDepthFunc(GL_LESS);
            }

            // Rysuj Solid Pass (Ląd)
            ourShader.use();
            ourShader.setMat4("projection", projection);
            ourShader.setMat4("view", view);
            // (Ustawienia mgły dla ourShader)
            float fogStart = (RENDER_DISTANCE - 2) * CHUNK_WIDTH;
            float fogEnd = (RENDER_DISTANCE + 1) * CHUNK_WIDTH;
            ourShader.setVec3("u_fogColor", SKY_COLOR);
            ourShader.setFloat("u_fogStart", fogStart);
            ourShader.setFloat("u_fogEnd", fogEnd);

            glDepthMask(GL_TRUE);
            glEnable(GL_CULL_FACE);
            for (auto const& [pos, chunk] : g_WorldChunks) {
                chunk->DrawSolid(ourShader, textureAtlas);
            }

            // --- Rysuj Ekran Ładowania (NA WIERZCHU) ---
            glDisable(GL_DEPTH_TEST);
            uiShader.use();
            uiShader.setMat4("projection", ortho_projection); // Użyj ortho z Etapu 1
            uiShader.setVec2("u_uvScale", 1.0f, 1.0f);
            uiShader.setVec2("u_uvOffset", 0.0f, 0.0f);
            uiShader.setFloat("u_opacity", opacity); // Użyj obliczonej przezroczystości

            auto model = glm::mat4(1.0f);
            model = glm::scale(model, glm::vec3(SCR_WIDTH, SCR_HEIGHT, 1.0f));
            uiShader.setMat4("model", model);

            glBindVertexArray(hotbarVAO);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, loadingTextureID);
            uiShader.setInt("uiTexture", 0);
            glDrawArrays(GL_TRIANGLES, 0, 6);

            glEnable(GL_DEPTH_TEST);

            glfwSwapBuffers(window);
            glfwPollEvents();
        }
    // --- KONIEC PĘTLI ŁADOWANIA I WYGASZANIA ---

    // Ustaw czas startowy dla głównej pętli gry
    lastFrame = static_cast<float>(glfwGetTime());

            // 1. ZNAJDŹ I ZAMIEŃ (w kodzie który właśnie wkleiłeś):
            //    G_WORLD_NAME -> g_selectedWorldName
            //    G_WORLD_SEED -> g_selectedWorldSeed

            // 2. DODAJ NA KOŃCU TEGO BLOKU (po pętli 'while(fadeTimer > 0.0f)'):
            //    Kod do zapisywania seeda
            std::string infoPath = "worlds/" + g_selectedWorldName + "/world.info";
            if (!std::filesystem::exists(infoPath)) {
                std::ofstream infoFile(infoPath);
                if (infoFile.is_open()) {
                    infoFile << g_selectedWorldSeed; // Zapisz seed
                    infoFile.close();
                }
            }
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED); // UKRYJ KURSOR
            g_currentState = STATE_IN_GAME; // Przełącz stan!

            break; // Koniec case STATE_LOADING_WORLD
        }

        case STATE_IN_GAME:
        {
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
        if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) {
            currentSpeed *= 1.8f;
        }

        // --- Sprawdź, czy gracz jest w wodzie ---
        // Sprawdzamy blok na wysokości "oczu"
        glm::vec3 headPos = camera.Position + glm::vec3(0.0f, PLAYER_EYE_HEIGHT, 0.0f);
        bool isInWater = (GetBlockGlobal(round(headPos.x), round(headPos.y), round(headPos.z)) == BLOCK_ID_WATER);


        if (camera.flyingMode)
        {
            // --- FIZYKA LATANIA (prosta) ---
            camera.Position += wishDir * currentSpeed * deltaTime;
            onGround = false;
            playerVelocity = glm::vec3(0.0f);
        }
        else
        {
            // --- FIZYKA CHODZENIA I PŁYWANIA ---

            // 1. Zastosuj Grawitację lub Pływalność
            if (isInWater)
            {
                onGround = false;
                // Zastosuj opór wody (spowolnienie)
                playerVelocity.y *= (1.0f - WATER_DRAG * deltaTime);
                // Zastosuj wyporność
                playerVelocity.y += BUOYANCY * deltaTime;

                // Pływanie w górę (ze Spacji)
                if (wishJump) {
                    playerVelocity.y = camera.MovementSpeed * 0.6f; // Prędkość pływania w górę
                }
                // Pływanie w dół (z Shifta)
                if(wishDir.y < 0) {
                     playerVelocity.y = -camera.MovementSpeed * 0.6f;
                }

                // Ogranicz prędkość w wodzie
                playerVelocity.y = glm::clamp(playerVelocity.y, -camera.MovementSpeed * 0.6f, camera.MovementSpeed * 0.6f);
            }
            else // Nie jesteśmy w wodzie
            {
                // Zastosuj normalną grawitację
                if (!onGround) {
                    playerVelocity.y += GRAVITY * deltaTime;
                }
                // Obsłuż skok (tylko na lądzie)
                if (wishJump && onGround) {
                    playerVelocity.y = JUMP_FORCE;
                    onGround = false;
                }
            }

            // 2. Zastosuj ruch poziomy (WSAD)
            float speedMultiplier = isInWater ? 0.5f : 1.0f; // 50% prędkości w wodzie
            playerVelocity.x = wishDir.x * currentSpeed * speedMultiplier;
            playerVelocity.z = wishDir.z * currentSpeed * speedMultiplier;

            // 3. Oblicz nową pozycję
            glm::vec3 newPos = camera.Position + playerVelocity * deltaTime;

            // 4. Sprawdź kolizje (prosty AABB)

            // Funkcja pomocnicza sprawdzająca, czy blok jest SOLIDNY
            auto isSolid = [&](int x, int y, int z) {
                uint8_t block = GetBlockGlobal(x, y, z);
                return block != BLOCK_ID_AIR && block != BLOCK_ID_WATER;
            };

            float playerWidthHalf = PLAYER_WIDTH / 2.0f;
            glm::vec3 feetCheck = newPos;
            glm::vec3 headCheck = newPos + glm::vec3(0.0f, PLAYER_HEIGHT, 0.0f);

            // Kolizja w osi Y (Góra/Dół)
            if (playerVelocity.y > 0) { // Lecimy w GÓRĘ
                if (isSolid(round(headCheck.x), round(headCheck.y), round(headCheck.z))) {
                    playerVelocity.y = 0; // Uderzyliśmy w sufit
                }
            } else { // Lecimy w DÓŁ
                if (isSolid(round(feetCheck.x), round(feetCheck.y - 0.1f), round(feetCheck.z))) {
                    playerVelocity.y = 0;
                    onGround = true; // Jesteśmy na ziemi (nie w wodzie)
                    newPos.y = round(feetCheck.y - 0.1f) + 0.5f; // Ląduj na bloku
                } else {
                    onGround = false;
                }
            }

            // Kolizja w osi X
            if (playerVelocity.x > 0) { // Idziemy w prawo (+X)
                if (isSolid(round(feetCheck.x + playerWidthHalf), round(feetCheck.y), round(feetCheck.z)) ||
                    isSolid(round(headCheck.x + playerWidthHalf), round(headCheck.y), round(headCheck.z))) {
                    newPos.x = camera.Position.x;
                }
            } else if (playerVelocity.x < 0) { // Idziemy w lewo (-X)
                if (isSolid(round(feetCheck.x - playerWidthHalf), round(feetCheck.y), round(feetCheck.z)) ||
                    isSolid(round(headCheck.x - playerWidthHalf), round(headCheck.y), round(headCheck.z))) {
                    newPos.x = camera.Position.x;
                }
            }

            // Kolizja w osi Z
            if (playerVelocity.z > 0) { // Idziemy do przodu (+Z)
                if (isSolid(round(feetCheck.x), round(feetCheck.y), round(feetCheck.z + playerWidthHalf)) ||
                    isSolid(round(headCheck.x), round(headCheck.y), round(headCheck.z + playerWidthHalf))) {
                    newPos.z = camera.Position.z;
                }
            } else if (playerVelocity.z < 0) { // Idziemy do tyłu (-Z)
                if (isSolid(round(feetCheck.x), round(feetCheck.y), round(feetCheck.z - playerWidthHalf)) ||
                    isSolid(round(headCheck.x), round(headCheck.y), round(headCheck.z - playerWidthHalf))) {
                    newPos.z = camera.Position.z;
                }
            }

            // 5. Ostatecznie zaktualizuj pozycję kamery
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
                    chunk_storage.emplace_back(glm::vec3(x * CHUNK_WIDTH, 0.0f, z * CHUNK_DEPTH), G_WORLD_NAME, G_WORLD_SEED);
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
       uiShader.setFloat("u_opacity", 1.0f);
       // Używamy naszego "pędzla" (unit quad)
        glBindVertexArray(hotbarVAO);

        // --- 1. Rysuj 9 Slotów Tła ---
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, slotTextureID);
        uiShader.setInt("uiTexture", 0);

        // Resetuj UV (ważne)
        uiShader.setVec2("u_uvScale", 1.0f, 1.0f);
        uiShader.setVec2("u_uvOffset", 0.0f, 0.0f);

        for (int i = 0; i < HOTBAR_SIZE; i++) {
            float x_pos = BAR_START_X + i * (SLOT_SIZE + GAP);
            float y_pos = 10.0f; // 10px od dołu

            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(x_pos, y_pos, 0.0f));
            model = glm::scale(model, glm::vec3(SLOT_SIZE, SLOT_SIZE, 1.0f));
            uiShader.setMat4("model", model);

            glDrawArrays(GL_TRIANGLES, 0, 6); // Rysuj kwadrat
        }

        // --- 2. Rysuj 9 Ikon Bloków ---
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, iconsTextureID);
        uiShader.setInt("uiTexture", 0);

        float ICON_SIZE = 52.0f; // Ikony są mniejsze (52x52)

        // Obliczamy skalę UV (jak mały jest jeden kafelek ikony)
        glm::vec2 uvScale(1.0f / ICON_ATLAS_COLS, 1.0f / ICON_ATLAS_ROWS);
        uiShader.setVec2("u_uvScale", uvScale);

        for (int i = 0; i < HOTBAR_SIZE; i++) {
            // Pozycja ikony (wyśrodkowana w slocie)
            float x_pos = BAR_START_X + i * (SLOT_SIZE + GAP) + (SLOT_SIZE - ICON_SIZE) / 2.0f;
            float y_pos = 10.0f + (SLOT_SIZE - ICON_SIZE) / 2.0f;

            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(x_pos, y_pos, 0.0f));
            model = glm::scale(model, glm::vec3(ICON_SIZE, ICON_SIZE, 1.0f));
            uiShader.setMat4("model", model);

            // --- Logika wyboru ikony ---
            uint8_t blockID = g_hotbarSlots[i];

            float iconIndex = 0.0f;
            if (blockID == BLOCK_ID_GRASS) iconIndex = 0.0f;
            else if (blockID == BLOCK_ID_DIRT) iconIndex = 1.0f;
            else if (blockID == BLOCK_ID_STONE) iconIndex = 2.0f;
            else if (blockID == BLOCK_ID_WATER) iconIndex = 3.0f;
            // (Tu zmapuj resztę slotów 4-8, na razie będą się powtarzać)

            // Przesuwamy UV do odpowiedniej ikony
            glm::vec2 uvOffset(iconIndex / ICON_ATLAS_COLS, 0.0f);
            uiShader.setVec2("u_uvOffset", uvOffset);

            glDrawArrays(GL_TRIANGLES, 0, 6);
        }

        // --- 3. Rysuj Selekter (Ramka podświetlenia) ---
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, selectorTextureID);
        uiShader.setInt("uiTexture", 0);

        // Resetuj UV (bardzo ważne!)
        uiShader.setVec2("u_uvScale", 1.0f, 1.0f);
        uiShader.setVec2("u_uvOffset", 0.0f, 0.0f);

        // Pozycja selektora (wyśrodkowana na slocie)
        float selector_x_pos = BAR_START_X + g_activeSlot * (SLOT_SIZE + GAP) - (SELECTOR_SIZE - SLOT_SIZE) / 2.0f;
        float selector_y_pos = 10.0f - (SELECTOR_SIZE - SLOT_SIZE) / 2.0f;

        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(selector_x_pos, selector_y_pos, 0.0f));
        model = glm::scale(model, glm::vec3(SELECTOR_SIZE, SELECTOR_SIZE, 1.0f));
        uiShader.setMat4("model", model);

        glDrawArrays(GL_TRIANGLES, 0, 6); // Rysuj JEDEN kwadrat

        // Koniec
        glBindVertexArray(0);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glDepthMask(GL_TRUE);
        // --- 5. SWAP BUFFERS ---
        // glfwSwapBuffers(window);
        // glfwPollEvents();

            break; // Koniec case STATE_IN_GAME
        }

        case STATE_EXITING:
            // Pętla się zakończy
            break;
    }

    // Wspólne dla wszystkich stanów
    glfwSwapBuffers(window);
    glfwPollEvents();
}

    // --- 6. Sprzątanie ---
    // Destruktory chunków (`~Chunk()`) zostaną wywołane automatycznie
    // gdy wektor 'chunks' wyjdzie poza zakres.
    std::cout << "Zapisywanie swiata..." << std::endl;
    for (auto& chunk : chunk_storage) // Użyj chunk_storage, a nie g_WorldChunks
    {
        chunk.SaveToFile();
    }
    std::cout << "Zapisano." << std::endl;
    // --- KONIEC NOWOŚCI ---
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
    // ... (obsługa klawiszy numerycznych Hotbara bez zmian) ...
    for (int i = 0; i < HOTBAR_SIZE; i++) {
        if (glfwGetKey(window, GLFW_KEY_1 + i) == GLFW_PRESS) {
            g_activeSlot = i;
            g_selectedBlockID = g_hotbarSlots[i];
            break;
        }
    }

    // Zamknięcie okna
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        if (g_currentState == STATE_IN_GAME) {
            g_currentState = STATE_MAIN_MENU; // Wróć do menu
            // Musimy zresetować stan, aby nie iść w kierunku po powrocie do gry
            wishDir = glm::vec3(0.0f);
            wishJump = false;
        }
    }
    // Przełącznik trybu Latanie/Chodzenie
    if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS) {
        if (!f_pressed) {
            camera.ToggleFlyingMode();
            std::cout << "Tryb latania: " << (camera.flyingMode ? "ON" : "OFF") << std::endl;
            if (!camera.flyingMode) playerVelocity = glm::vec3(0.0f);
        }
        f_pressed = true;
    }
    if (glfwGetKey(window, GLFW_KEY_F) == GLFW_RELEASE) {
        f_pressed = false;
    }

    // --- Ustawianie "Życzenia" Ruchu (wishDir) ---
    wishDir = glm::vec3(0.0f);

    // --- Ruch Poziomy (zawsze taki sam) ---
    glm::vec3 front_horizontal = glm::normalize(glm::vec3(camera.Front.x, 0.0f, camera.Front.z));
    glm::vec3 right_horizontal = glm::normalize(glm::vec3(camera.Right.x, 0.0f, camera.Right.z));
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) wishDir += front_horizontal;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) wishDir -= front_horizontal;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) wishDir -= right_horizontal;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) wishDir += right_horizontal;

    // --- Ruch Pionowy (zależy od trybu) ---
    if (camera.flyingMode)
    {
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) wishDir += camera.WorldUp;
        if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) wishDir -= camera.WorldUp;
    }
    else // Chodzenie lub Pływanie
    {
        // Skok (lub pływanie w górę)
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
            wishJump = true;
        } else {
            wishJump = false;
        }

        // POPRAWKA: Pływanie w dół (lub kucanie, gdy je dodamy)
        if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
             wishDir -= camera.WorldUp; // Dodaj składową 'w dół'
        }
    }

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
    float step = 0.05f;

    glm::ivec3 prevBlockPos = glm::ivec3(
        static_cast<int>(round(startPos.x)),
        static_cast<int>(round(startPos.y)),
        static_cast<int>(round(startPos.z))
    );

    for (float t = 0.0f; t < maxDist; t += step) {
        currentPos = startPos + direction * t;
        glm::ivec3 blockPos = glm::ivec3(
            static_cast<int>(round(currentPos.x)),
            static_cast<int>(round(currentPos.y)),
            static_cast<int>(round(currentPos.z))
        );

        // POPRAWKA: Sprawdź, czy blok jest SOLIDNY
        uint8_t block = GetBlockGlobal(blockPos.x, blockPos.y, blockPos.z);
        if (block != BLOCK_ID_AIR && block != BLOCK_ID_WATER) {
            out_hitBlock = blockPos;
            out_prevBlock = prevBlockPos;
            return true;
        }

        prevBlockPos = blockPos;
    }
    return false;
}


void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
    // === OBSŁUGA STANU MENU ===
    if (g_currentState == STATE_MAIN_MENU && button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
    {
        double mouseX, mouseY;
        glfwGetCursorPos(window, &mouseX, &mouseY);

        if (currentMenuPage == 0) // Główne menu
        {
            if (IsMouseOver(mouseX, mouseY, BTN_X, BTN_Y_NEW, BTN_W, BTN_H))
            {
                // --- NOWY ŚWIAT ---
                // TODO: W przyszłości zrób tu pole do wpisania nazwy
                g_inputWorldName = "NowySwiat"; // Ustaw domyślne
                g_inputSeedString = "";
                g_activeInput = 0; // Aktywuj pierwsze pole
                g_currentState = STATE_NEW_WORLD_MENU; // Przejdź do nowego menu
            }
            else if (IsMouseOver(mouseX, mouseY, BTN_X, BTN_Y_LOAD, BTN_W, BTN_H))
            {
                // --- ZAŁADUJ ŚWIAT ---
                std::cout << "Wybieranie swiata do wczytania..." << std::endl;

                existingWorlds.clear();
                std::string worldsPath = "worlds";
                if (std::filesystem::exists(worldsPath)) {
                    for (const auto& entry : std::filesystem::directory_iterator(worldsPath)) {
                        if (entry.is_directory()) {
                            existingWorlds.push_back(entry.path().filename().string());
                        }
                    }
                }

                // TODO: Zamiast ładować pierwszy, przełącz na 'currentMenuPage = 1'
                // i narysuj listę 'existingWorlds'.
                // Na razie na sztywno ładujemy pierwszy z listy lub stary świat.

                std::string worldToLoad = "MojPierwszySwiat"; // Domyślny
                if (!existingWorlds.empty()) {
                     worldToLoad = existingWorlds[0];
                }

                g_selectedWorldName = worldToLoad;
                g_selectedWorldSeed = 12345; // Domyślny seed

                std::string infoPath = "worlds/" + g_selectedWorldName + "/world.info";
                if (std::filesystem::exists(infoPath)) {
                    std::ifstream infoFile(infoPath);
                    if (infoFile.is_open()) {
                        infoFile >> g_selectedWorldSeed; // Wczytaj prawdziwy seed
                        infoFile.close();
                    }
                }
                std::cout << "Ladowanie swiata: " << g_selectedWorldName << " (Seed: " << g_selectedWorldSeed << ")" << std::endl;
                g_currentState = STATE_LOADING_WORLD;
            }
            else if (IsMouseOver(mouseX, mouseY, BTN_X, BTN_Y_QUIT, BTN_W, BTN_H))
            {
                g_currentState = STATE_EXITING;
            }
        }

        // else if (currentMenuPage == 1) { /* ... obsługa kliknięć na listę światów ... */ }
        return; // Ważne: nie wykonuj kodu gry
    }
    else if (g_currentState == STATE_NEW_WORLD_MENU && button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
    {
        double mouseX, mouseY;
        glfwGetCursorPos(window, &mouseX, &mouseY);

        // Kliknięcie na pole Nazwy
        if (IsMouseOver(mouseX, mouseY, INPUT_X, INPUT_NAME_Y, INPUT_W, INPUT_H)) {
            g_activeInput = 0;
        }
        // Kliknięcie na pole Seeda
        else if (IsMouseOver(mouseX, mouseY, INPUT_X, INPUT_SEED_Y, INPUT_W, INPUT_H)) {
            g_activeInput = 1;
        }
        // Kliknięcie "Start"
        else if (IsMouseOver(mouseX, mouseY, BTN_START_X, BTN_START_Y, BTN_W, BTN_H))
        {
            g_selectedWorldName = g_inputWorldName;
            if (g_selectedWorldName.empty()) g_selectedWorldName = "SwiatBezNazwy";

            if (g_inputSeedString.empty()) {
                g_selectedWorldSeed = rand(); // Losowy seed
            } else {
                // Spróbuj przekonwertować string na liczbę
                try {
                    g_selectedWorldSeed = std::stoi(g_inputSeedString);
                } catch (...) {
                    // Jeśli się nie uda (np. ktoś wpisał "abc"), użyj hasha stringa
                    g_selectedWorldSeed = static_cast<int>(std::hash<std::string>{}(g_inputSeedString));
                }
            }
            std::cout << "Rozpoczynanie swiata: " << g_selectedWorldName << " (Seed: " << g_selectedWorldSeed << ")" << std::endl;
            g_currentState = STATE_LOADING_WORLD;
        }
        // Kliknięcie "Wstecz"
        else if (IsMouseOver(mouseX, mouseY, BTN_BACK_X, BTN_BACK_Y, BTN_W, BTN_H)) {
            g_currentState = STATE_MAIN_MENU;
        }
        return;
    }
    // --- KONIEC NOWEGO BLOKU ---
    // === OBSŁUGA STANU GRY ===
    if (g_currentState != STATE_IN_GAME) return; // Nic nie rób, jeśli nie jesteśmy w grze

    // --- STARY KOD (zaczyna się od linii 1088) ---
    glm::vec3 eyePos = camera.Position + glm::vec3(0.0f, PLAYER_EYE_HEIGHT, 0.0f);
    glm::ivec3 hitBlockPos;
    glm::ivec3 prevBlockPos;
    float raycastDistance = 5.0f;

    bool hit = Raycast(eyePos, camera.Front, raycastDistance, hitBlockPos, prevBlockPos);
    g_raycastHit = hit;

    if (hit && action == GLFW_PRESS)
    {
        if (button == GLFW_MOUSE_BUTTON_LEFT)
        {
            uint8_t blockToDestroy = GetBlockGlobal(hitBlockPos.x, hitBlockPos.y, hitBlockPos.z);
            if (blockToDestroy != BLOCK_ID_AIR && blockToDestroy != BLOCK_ID_WATER) {
                if (blockToDestroy==BLOCK_ID_BEDROCK && camera.flyingMode ) {
                    SetBlock(hitBlockPos.x, hitBlockPos.y, hitBlockPos.z, BLOCK_ID_AIR);
                }
                else if (blockToDestroy!=BLOCK_ID_BEDROCK){
                    SetBlock(hitBlockPos.x, hitBlockPos.y, hitBlockPos.z, BLOCK_ID_AIR);
                }
            }
        }
        else if (button == GLFW_MOUSE_BUTTON_RIGHT)
        {
            uint8_t blockToPlaceIn = GetBlockGlobal(prevBlockPos.x, prevBlockPos.y, prevBlockPos.z);
            if (blockToPlaceIn == BLOCK_ID_AIR || (blockToPlaceIn == BLOCK_ID_WATER && g_selectedBlockID != BLOCK_ID_WATER))
            {
                SetBlock(prevBlockPos.x, prevBlockPos.y, prevBlockPos.z, g_selectedBlockID);
            }
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

void DrawMenuButton(Shader& uiShader, unsigned int vao, unsigned int textureID, float x, float y, float w, float h) {
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureID);

    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(x, y, 0.0f));
    model = glm::scale(model, glm::vec3(w, h, 1.0f));
    uiShader.setMat4("model", model);

    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

bool IsMouseOver(double mouseX, double mouseY, float x, float y, float w, float h) {
    // Musimy odwrócić Y myszy (GLFW liczy od góry, OpenGL od dołu)
    mouseY = SCR_HEIGHT - mouseY;
    return (mouseX >= x && mouseX <= x + w && mouseY >= y && mouseY <= y + h);
}

void char_callback(GLFWwindow* window, unsigned int codepoint)
{
    if (g_currentState != STATE_NEW_WORLD_MENU) return;

    std::string* targetString = nullptr;
    if (g_activeInput == 0) {
        targetString = &g_inputWorldName;
    } else if (g_activeInput == 1) {
        targetString = &g_inputSeedString;
    }

    if (targetString && codepoint >= 32 && codepoint <= 126) {
        // Dodaj znak do stringa (tylko podstawowe ASCII)
        *targetString += static_cast<char>(codepoint);
        std::cout << "Tekst: " << *targetString << std::endl; // Do debugowania
    }
}

// Callback dla klawiszy specjalnych (Backspace, Tab)
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (g_currentState != STATE_NEW_WORLD_MENU || (action != GLFW_PRESS && action != GLFW_REPEAT)) return;

    std::string* targetString = nullptr;
    if (g_activeInput == 0) targetString = &g_inputWorldName;
    else if (g_activeInput == 1) targetString = &g_inputSeedString;

    if (key == GLFW_KEY_BACKSPACE && targetString && !targetString->empty()) {
        targetString->pop_back();
        std::cout << "Tekst: " << *targetString << std::endl; // Do debugowania
    }

    if (key == GLFW_KEY_TAB && action == GLFW_PRESS) {
        g_activeInput = (g_activeInput + 1) % 2; // Zmień pole (0 -> 1, 1 -> 0)
    }
}
void RenderText(Shader& shader, unsigned int vao, const std::string& text,
                float x, float y, float size)
{
    // Używamy tekstury czcionki (załóżmy, że jest już zbindowana, ale dla pewności...)
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texFont);
    shader.setInt("uiTexture", 0);

    glBindVertexArray(vao);

    // Ustawiamy stały rozmiar UV dla pojedynczego znaku
    shader.setVec2("u_uvScale", 1.0f / FONT_ATLAS_COLS, 1.0f / FONT_ATLAS_ROWS);

    float cursor_x = x;

    for (char c : text)
    {
        // Pomiń znaki spoza naszego atlasu (ASCII 0-127)
        if (c < 0) continue;
        int ascii = static_cast<int>(c);
        if (ascii >= 128) continue; // Nasz atlas ma 128 znaków

        // Oblicz offset UV w atlasie
        float uv_x_offset = (ascii % static_cast<int>(FONT_ATLAS_COLS));
        float uv_y_offset = (ascii / static_cast<int>(FONT_ATLAS_COLS)); // Dzielenie całkowite przez szerokość da nam wiersz

        // Przesuń offset UV do odpowiedniego znaku
        shader.setVec2("u_uvOffset", uv_x_offset / FONT_ATLAS_COLS, (FONT_ATLAS_ROWS - 1.0 - uv_y_offset) / FONT_ATLAS_ROWS);
        // Ustaw macierz modelu dla tego znaku
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(cursor_x, y, 0.0f));
        model = glm::scale(model, glm::vec3(size, size, 1.0f));
        shader.setMat4("model", model);

        glDrawArrays(GL_TRIANGLES, 0, 6);
        // Przesuń kursor w prawo. Możesz dostosować '0.6f', jeśli znaki są zbyt blisko/daleko
        cursor_x += (size * 0.6f);
    }
}