#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "ViewFrustum.h" // <<< NOWOŚĆ
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
#include <thread>
#include <mutex>
#include <queue>
#include <atomic>

// --- BEZPIECZNA KOLEJKA (Thread-Safe Queue) ---
// Pozwala przesyłać dane między wątkiem gry a wątkiem ładowania
template <typename T>
class SafeQueue {
private:
    std::queue<T> q;
    std::mutex m;
public:
    void push(T val) {
        std::lock_guard<std::mutex> lock(m);
        q.push(val);
    }
    bool try_pop(T& val) {
        std::lock_guard<std::mutex> lock(m);
        if (q.empty()) return false;
        val = q.front();
        q.pop();
        return true;
    }
    bool empty() {
        std::lock_guard<std::mutex> lock(m);
        return q.empty();
    }
};

// Kolejki komunikacyjne
SafeQueue<glm::ivec2> g_chunksToLoad;   // Main -> Worker: "Zrób ten chunk"
SafeQueue<Chunk*> g_chunksReady;        // Worker -> Main: "Zrobiłem, masz!"
std::atomic<bool> g_isRunning{true};    // Flaga do zatrzymania wątku przy wyjściu
enum GameState {
    STATE_MAIN_MENU,
    STATE_NEW_WORLD_MENU,
    STATE_LOAD_WORLD_MENU,
    STATE_LOADING_WORLD,
    STATE_PAUSE_MENU,
    STATE_IN_GAME,
    STATE_EXITING,
    STATE_INVENTORY,
    STATE_FURNACE_MENU
};
GameState g_currentState = STATE_MAIN_MENU;
bool IsAnyWater(uint8_t id) {
    // 4 = Źródło, 13-16 = Płynąca (zgodnie z Chunk.h)
    return id == BLOCK_ID_WATER || (id >= 13 && id <= 16);
}
// --- Funkcja pomocnicza dla Lawy ---
bool isAnyLava(uint8_t id) {
    // Sprawdzamy ID źródła (22) oraz płynącej lawy (23, 24, 25)
    // Upewnij się, że te numery zgadzają się z tymi w Chunk.h!
    return id == BLOCK_ID_LAVA || (id >= 23 && id <= 25);
}
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
void RenderText(Shader& shader, unsigned int vao, const std::string& text,float x, float y, float size);
void SaveWorld();
void UpdateScreenSize(int width, int height);
int GetSlotUnderMouse(double mouseX, double mouseY);
void SpawnDrop(uint8_t itemID, glm::vec3 pos, glm::vec3 velocity);
void UpdateDrops(float dt, glm::vec3 playerPos);
void UpdateParticles(float dt);
void SpawnBlockParticles(uint8_t blockID, glm::vec3 blockPos);
void UpdateMining(float dt);
void LoadInventory();
void SaveInventory();
void UpdateLiquids(float dt);
void LoadPlayerPosition();
void SavePlayerPosition();
void LoadFurnaces();
void SaveFurnaces();
void SpawnEntity(glm::vec3 pos);
void UpdateEntities(float dt);
void CheckCrafting();
bool AddItemToInventory(uint8_t itemID, int count);
void DrawDurabilityBar(Shader& uiShader, unsigned int vao, float x, float y, float w, float h, int cur, int max);
void UpdateSurvival(float dt);
void InitClouds();
void ChunkWorkerThread();
void UnloadFarChunks();

const int INVENTORY_SLOTS = 36; // 9 slotów hotbara + 27 slotów plecaka
const int MAX_STACK_SIZE = 64;
const uint8_t ITEM_ID_IRON_INGOT = 27;
const uint8_t ITEM_ID_COAL = 28; // (Jeśli jeszcze nie masz)

bool g_openglInitialized = false;
unsigned int hdrFBO=0;
unsigned int colorBuffers[2]; // 0 = Scene, 1 = Brightness
unsigned int rboDepth;
unsigned int pingpongFBO[2];
unsigned int pingpongColorbuffers[2];
unsigned int quadVAO = 0;
unsigned int quadVBO;
struct ItemStack {
    uint8_t itemID = BLOCK_ID_AIR;
    int count = 0;
    int durability = 0;    // Ile użyć zostało
    int maxDurability = 0; // Maksymalna wytrzymałość (0 = niezniszczalny blok)
};
struct FurnaceData {
    ItemStack input;  // Slot 0: Co przetapiamy
    ItemStack fuel;   // Slot 1: Paliwo
    ItemStack result; // Slot 2: Wynik

    float burnTime = 0.0f;    // Ile czasu jeszcze będzie się palić (paliwo)
    float maxBurnTime = 1.0f; // Całkowity czas spalania jednego kawałka węgla
    float cookTime = 0.0f;    // Postęp przetapiania (0.0 -> 3.0 sekundy)
};

// Komparator, żeby użyć ivec3 jako klucza w mapie
struct CompareVec3 {
    bool operator()(const glm::ivec3& a, const glm::ivec3& b) const {
        if (a.x != b.x) return a.x < b.x;
        if (a.y != b.y) return a.y < b.y;
        return a.z < b.z;
    }
};
std::map<glm::ivec3, FurnaceData, CompareVec3> g_furnaces;

// Pozycja aktualnie otwartego pieca (-1,-1,-1 oznacza brak)
glm::ivec3 g_openedFurnacePos = glm::ivec3(-1);
// Globalny ekwipunek (hotbar to pierwsze 9 slotów)
std::vector<ItemStack> g_inventory(INVENTORY_SLOTS);
std::vector<ItemStack> g_craftingGrid(4);
// Slot wyjściowy (wynik)
ItemStack g_craftingOutput = {BLOCK_ID_AIR, 0};
const uint8_t ITEM_ID_PICKAXE = 20; // Nowe ID dla Kilofa
// Specjalne indeksy slotów (żeby funkcja myszki wiedziała w co klikamy)
const int SLOT_CRAFT_START = 100; // 100, 101, 102, 103
const int SLOT_RESULT = 104;
const int SLOT_FURNACE_INPUT = 200;
const int SLOT_FURNACE_FUEL  = 201;
const int SLOT_FURNACE_RESULT = 202;
float playerHealth = 10.0f; // 10 serduszek (20 HP)
float playerMaxHealth = 10.0f;
float airTimer = 10.0f;     // Czas powietrza pod wodą (sekundy)
bool isDead = false;
unsigned int cloudTexture;
unsigned int cloudVAO, cloudVBO;
// Do obrażeń od upadku
float lastYVelocity = 0.0f;
ItemStack g_mouseItem = {BLOCK_ID_AIR, 0};
float g_lavaTimer = 0.0f;
enum EntityType {
    ENTITY_PIG,
    ENTITY_ZOMBIE
};
struct Entity {
    EntityType type;    // <<< NOWOŚĆ: Co to jest?
    glm::vec3 position;
    glm::vec3 velocity;
    float rotationY;    // Obrót w osi Y (gdzie patrzy)
    float moveTimer;    // Czas do zmiany decyzji (iść/stać)
    bool isWalking;     // Czy teraz idzie?
    float health;       // Życie (opcjonalnie)

    // Kolor (dla uproszczenia użyjemy koloru zamiast tekstury na start)
    glm::vec3 color;
    float attackCooldown; // Żeby nie bił co klatkę
};
void DrawPig(Shader& shader, const Entity& e, unsigned int vao);
std::list<Entity> g_entities; // Lista wszystkich mobów
struct DroppedItem {
    uint8_t itemID;
    glm::vec3 position;
    glm::vec3 velocity;
    float rotationTime; // Do obracania się
    float pickupDelay;  // Żeby nie podnieść od razu po wyrzuceniu
    bool isAlive;       // Czy przedmiot istnieje
};

std::list<DroppedItem> g_droppedItems;
struct Particle {
    glm::vec3 position;
    glm::vec3 velocity;
    uint8_t blockID;    // Żeby wiedzieć jaką teksturę nałożyć
    float lifetime;     // Ile czasu jeszcze pożyje
    float maxLifetime;  // Żeby obliczyć skalę (zanikanie)
};

std::list<Particle> g_particles;
bool g_isMining = false;          // Czy gracz trzyma LPM?
float g_miningTimer = 0.0f;       // Jak długo kopiemy ten blok
glm::ivec3 g_currentMiningPos;    // Który blok aktualnie kopiemy
// --- KONIEC NOWOŚCI ---
ma_engine g_audioEngine;
float g_footstepTimer = 0.0f;
// --- NOWOŚĆ: Funkcja Twardości (Czas w sekundach) ---
float GetBlockHardness(uint8_t blockID, uint8_t toolID) {
    // 1. BAZOWE CZASY (Ręką)
    float time = 1.0f; // Domyślnie

    if (blockID == BLOCK_ID_BEDROCK) return -1.0f;
    if (blockID == BLOCK_ID_LEAVES) return 0.2f;
    if (blockID == BLOCK_ID_TORCH) return 0.05f;
    if (blockID == BLOCK_ID_SAND) return 0.6f;
    if (blockID == BLOCK_ID_DIRT || blockID == BLOCK_ID_GRASS) return 0.7f;
    if (blockID == BLOCK_ID_LOG) return 2.0f;

    // Kamień i rudy są twarde bez narzędzia
    if (blockID == BLOCK_ID_STONE ||
        blockID == BLOCK_ID_COAL_ORE ||
        blockID == BLOCK_ID_IRON_ORE ||
        blockID == BLOCK_ID_GOLD_ORE ||
        blockID == BLOCK_ID_DIAMOND_ORE)
    {
        return 5.0f; // Długi czas kopania ręką
    }

    // 2. BONUSY NARZĘDZI
    if (toolID == ITEM_ID_PICKAXE) {
        // Kilof przyspiesza kopanie kamienia, rud, pieców itp.
        if (blockID == BLOCK_ID_STONE || blockID == BLOCK_ID_SANDSTONE) {
            time /= 10.0f; // 10x szybciej (0.5s zamiast 5.0s)
        }
        // (Tu dodasz rudy później: Coal, Iron itp.)
    }

    // 3. (Opcjonalnie) Siekiera
    // if (toolID == ITEM_ID_AXE && blockID == BLOCK_ID_LOG) time /= 5.0f;

    return time;
}
unsigned int dropVAO = 0, dropVBO = 0;
// Ustawienia
unsigned int SCR_WIDTH;
unsigned int SCR_HEIGHT;
std::string g_selectedWorldName;
int g_selectedWorldSeed;
std::vector<std::string> existingWorlds;
int currentMenuPage = 0;

// Tekstury przycisków
unsigned int texBtnNew, texBtnLoad, texBtnQuit;

// Stałe przycisków (teraz jako zmienne)
float BTN_W = 300.0f;
float BTN_H = 60.0f;
float BTN_X;
float BTN_Y_NEW;
float BTN_Y_LOAD;
float BTN_Y_QUIT;
std::string g_inputWorldName;
std::string g_inputSeedString;
int g_activeInput = 0; // 0 = Nazwa, 1 = Seed, -1 = Brak

// Tekstury dla nowego menu
unsigned int texBtnStart;
unsigned int texBtnBack;
unsigned int texInputBox; // Będziemy reużywać ui_slot.png
unsigned int texFont;
unsigned int texOverlay;
unsigned int texSun;

// Pozycje dla nowego menu (teraz jako zmienne)
float INPUT_W = 400.0f;
float INPUT_H = 50.0f;
float INPUT_X;
float INPUT_NAME_Y;
float INPUT_SEED_Y;

float BTN_START_X;
float BTN_START_Y;
float BTN_BACK_X;
float BTN_BACK_Y;
const float FONT_ATLAS_COLS = 16.0f;
const float FONT_ATLAS_ROWS = 8.0f;
const float LIST_ITEM_W = 400.0f;
const float LIST_ITEM_H = 50.0f;
float LIST_ITEM_X;
float LIST_START_Y;
float LIST_GAP = 60.0f;
float LIST_BACK_BTN_Y = 50.0f;
unsigned int texBtnResume;
unsigned int texBtnSaveAndMenu;
float g_waterTimer = 0.0f;
// Pozycje dla menu pauzy (teraz jako zmienne)
float PAUSE_BTN_Y_RESUME;
float PAUSE_BTN_Y_MENU;
float PAUSE_BTN_Y_QUIT;
// Kamera (ustawiona wyżej, żeby widzieć świat)
Camera camera(glm::vec3(8.0f, 40.0f, 8.0f));
float lastX; // <-- Usunięto inicjalizację
// --- NOWOŚĆ: GLOBALNA MAPA ŚWIATA ---
// Będziemy przechowywać wskaźniki do chunków w tej mapie.
// Kluczem jest pozycja chunka (np. 0,0 lub 1,0).
class Chunk; // Forward-declaration
std::map<glm::ivec2, Chunk*, CompareIveco2> g_WorldChunks;
std::list<Chunk> chunk_storage;
std::list<glm::vec3> g_torchPositions;
// ... (zaraz po 'std::list<Chunk> chunk_storage;') s...
//jdhsakhdkjas
// --- NOWOŚĆ: Funkcje Interakcji ze Światem ---
glm::mat4 ortho_projection;
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
    chunk->CalculateLighting();
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
void InitDropMesh() {
    // Tworzymy prosty sześcian 1x1x1.
    // POPRAWKA: Wszystkie ściany boczne mają teraz UV zorientowane pionowo (Y jest górą).
    float vertices[] = {
        // Pozycja (XYZ)      // UV       // Normalne

        // Tył (-Z)
         0.5f, -0.5f, -0.5f,  0.0f, 0.0f,  0.0f,  0.0f, -1.0f,1.0f, // Dół-Lewo
        -0.5f, -0.5f, -0.5f,  1.0f, 0.0f,  0.0f,  0.0f, -1.0f,1.0f,// Dół-Prawo
        -0.5f,  0.5f, -0.5f,  1.0f, 1.0f,  0.0f,  0.0f, -1.0f,1.0f, // Góra-Prawo
         0.5f,  0.5f, -0.5f,  0.0f, 1.0f,  0.0f,  0.0f, -1.0f,1.0f, // Góra-Lewo
         0.5f, -0.5f, -0.5f,  0.0f, 0.0f,  0.0f,  0.0f, -1.0f,1.0f,
        -0.5f,  0.5f, -0.5f,  1.0f, 1.0f,  0.0f,  0.0f, -1.0f,1.0f,

        // Przód (+Z)
        -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,  0.0f,  0.0f, 1.0f,1.0f, // Dół-Lewo
         0.5f, -0.5f,  0.5f,  1.0f, 0.0f,  0.0f,  0.0f, 1.0f,1.0f, // Dół-Prawo
         0.5f,  0.5f,  0.5f,  1.0f, 1.0f,  0.0f,  0.0f, 1.0f,1.0f, // Góra-Prawo
        -0.5f,  0.5f,  0.5f,  0.0f, 1.0f,  0.0f,  0.0f, 1.0f,1.0f, // Góra-Lewo
        -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,  0.0f,  0.0f, 1.0f,1.0f,
         0.5f,  0.5f,  0.5f,  1.0f, 1.0f,  0.0f,  0.0f, 1.0f,1.0f,

        // Lewo (-X) - TU BYŁY PROBLEMY
        -0.5f, -0.5f, -0.5f,  0.0f, 0.0f, -1.0f,  0.0f,  0.0f,1.0f, // Dół-Lewo (Tył)
        -0.5f, -0.5f,  0.5f,  1.0f, 0.0f, -1.0f,  0.0f,  0.0f, 1.0f,// Dół-Prawo (Przód)
        -0.5f,  0.5f,  0.5f,  1.0f, 1.0f, -1.0f,  0.0f,  0.0f,1.0f, // Góra-Prawo
        -0.5f,  0.5f, -0.5f,  0.0f, 1.0f, -1.0f,  0.0f,  0.0f,1.0f, // Góra-Lewo
        -0.5f, -0.5f, -0.5f,  0.0f, 0.0f, -1.0f,  0.0f,  0.0f,1.0f,
        -0.5f,  0.5f,  0.5f,  1.0f, 1.0f, -1.0f,  0.0f,  0.0f,1.0f,

        // Prawo (+X) - TU TEŻ
         0.5f, -0.5f,  0.5f,  0.0f, 0.0f,  1.0f,  0.0f,  0.0f,1.0f, // Dół-Lewo (Przód)
         0.5f, -0.5f, -0.5f,  1.0f, 0.0f,  1.0f,  0.0f,  0.0f,1.0f, // Dół-Prawo (Tył)
         0.5f,  0.5f, -0.5f,  1.0f, 1.0f,  1.0f,  0.0f,  0.0f,1.0f, // Góra-Prawo
         0.5f,  0.5f,  0.5f,  0.0f, 1.0f,  1.0f,  0.0f,  0.0f,1.0f, // Góra-Lewo
         0.5f, -0.5f,  0.5f,  0.0f, 0.0f,  1.0f,  0.0f,  0.0f,1.0f,
         0.5f,  0.5f, -0.5f,  1.0f, 1.0f,  1.0f,  0.0f,  0.0f,1.0f,

        // Dół (-Y)
        -0.5f, -0.5f, -0.5f,  0.0f, 1.0f,  0.0f, -1.0f,  0.0f,1.0f,
         0.5f, -0.5f, -0.5f,  1.0f, 1.0f,  0.0f, -1.0f,  0.0f,1.0f,
         0.5f, -0.5f,  0.5f,  1.0f, 0.0f,  0.0f, -1.0f,  0.0f,1.0f,
        -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,  0.0f, -1.0f,  0.0f,1.0f,
        -0.5f, -0.5f, -0.5f,  0.0f, 1.0f,  0.0f, -1.0f,  0.0f,1.0f,
         0.5f, -0.5f,  0.5f,  1.0f, 0.0f,  0.0f, -1.0f,  0.0f,1.0f,

        // Góra (+Y)
        -0.5f,  0.5f, -0.5f,  0.0f, 1.0f,  0.0f,  1.0f,  0.0f,1.0f,
         0.5f,  0.5f,  0.5f,  1.0f, 0.0f,  0.0f,  1.0f,  0.0f,1.0f,
         0.5f,  0.5f, -0.5f,  1.0f, 1.0f,  0.0f,  1.0f,  0.0f,1.0f,
         0.5f,  0.5f,  0.5f,  1.0f, 0.0f,  0.0f,  1.0f,  0.0f,1.0f,
        -0.5f,  0.5f, -0.5f,  0.0f, 1.0f,  0.0f,  1.0f,  0.0f,1.0f,
        -0.5f,  0.5f,  0.5f,  0.0f, 0.0f,  0.0f,  1.0f,  0.0f,1.0f
    };

    glGenVertexArrays(1, &dropVAO);
    glGenBuffers(1, &dropVBO);
    glBindVertexArray(dropVAO);
    glBindBuffer(GL_ARRAY_BUFFER, dropVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    GLsizei stride = 9 * sizeof(float);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, (void*)(5 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, stride, (void*)(8 * sizeof(float)));
        glEnableVertexAttribArray(3);
}


const int RENDER_DISTANCE = 12;
const int MAX_POINT_LIGHTS = 8; // <<< DODAJ TĘ LINIĘ
const glm::vec3 SKY_COLOR(0.5f, 0.8f, 1.0f);
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
unsigned int shadowMapFBO;
unsigned int shadowMapTexture;
const unsigned int SHADOW_WIDTH = 2048, SHADOW_HEIGHT = 2048; // Rozdzielczość mapy cienia
bool g_raycastHit = false; // Czy raycast w ogóle w coś trafił?
glm::ivec3 g_hitBlockPos; // Pozycja trafionego bloku
glm::ivec3 g_prevBlockPos; // Pozycja "przed" trafieniem
const float SLOT_SIZE = 64.0f;
const float SELECTOR_SIZE = 70.0f;
const float GAP = 0.0f;
const float BAR_WIDTH = (SLOT_SIZE * HOTBAR_SIZE) + (GAP * (HOTBAR_SIZE - 1));
float BAR_START_X;
const float ICON_ATLAS_COLS = 22.0f; // Ile jest ikonek w rzędzie w pliku ui_icons.png
const float ICON_ATLAS_ROWS = 1.0f; // Ile jest rzędów
void DrawZombie(Shader& shader, const Entity& e, unsigned int vao);
void InitBloom();
void RenderQuad();
void UpdateEntities(float dt, glm::vec3 playerPos);
void UpdateFurnaces(float dt);

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
    // g_inventory[0] = {BLOCK_ID_GRASS, 64};
    // g_inventory[1] = {BLOCK_ID_DIRT, 64};
    // g_inventory[2] = {BLOCK_ID_STONE, 64};
    // g_inventory[3] = {BLOCK_ID_TORCH, 64};
    // g_inventory[4] = {BLOCK_ID_LOG, 64};
    // g_inventory[5] = {BLOCK_ID_LEAVES, 64};
    // g_inventory[6] = {BLOCK_ID_LAVA, 64};
    // g_inventory[7] = {BLOCK_ID_WATER, 64};
    // g_inventory[8] = {BLOCK_ID_FURNACE, 64}; // <<< ZMIEŃ OSTATNI SLOT NA LAWĘ

    g_selectedBlockID = g_inventory[g_activeSlot].itemID;
    // --- 1. Inicjalizacja ---
    glfwInit();
    if (ma_engine_init(NULL, &g_audioEngine) != MA_SUCCESS) {
        std::cout << "BLAD: Nie udalo sie zainicjowac silnika audio!" << std::endl;
        return -1;
    }
    std::cout << "Inicjalizacja pelnego ekranu" << std::endl;
    // --- NOWY KOD: Pobierz rozdzielczość monitora ---
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    std::cout <<"Rozdzielczosc: "<< mode->width << "x" << mode->height << std::endl;
    // Ustaw GLFW, aby stworzyć okno pasujące do monitora
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RED_BITS, mode->redBits);
    glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
    glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
    glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);
    std::cout << "Wywolanie pelnego ekranu" << std::endl;
    // Wywołaj naszą funkcję, aby ustawić globalne zmienne
    UpdateScreenSize(mode->width, mode->height);
    std::cout << "Poprawnie wykonano UpdateScreenSize" << std::endl;
    // Stwórz okno w trybie PEŁNOEKRANOWYM (przekazując 'monitor')
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Crafters", monitor, nullptr);
    if (window == nullptr) { std::cout << "Nie udalo sie utworzyc okna GLFW" << std::endl; return -1; }

    glfwMakeContextCurrent(window);
    // --- KONIEC NOWEGO KODU ---

    // glfwMakeContextCurrent(window);
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
    InitDropMesh();
    InitClouds();
    InitBloom();
    g_openglInitialized = true;
    std::thread loaderThread(ChunkWorkerThread);
    Shader cloudShader("clouds.vert", "clouds.frag");
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
    Shader blurShader("blur.vert", "blur.frag");
    Shader finalShader("final.vert", "final.frag");
    finalShader.use();
    finalShader.setInt("scene", 0);
    finalShader.setInt("bloomBlur", 1);
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
    texOverlay = loadUITexture("overlay.png");
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
    texBtnResume = loadUITexture("button_resume.png");
    texBtnSaveAndMenu = loadUITexture("button_save_menu.png");
    texSun = loadUITexture("sun.png"); // <<< DODAJ TĘ LINIĘ
    // --- NOWOŚĆ: PĘTLA ŁADOWANIA I WYGASZANIA ---
    // glm::mat4 ortho_projection = glm::ortho(0.0f, (float)SCR_WIDTH, 0.0f, (float)SCR_HEIGHT);
    Shader depthShader("depth.vert", "depth.frag");

    // 1. Stwórz Framebuffer
    glGenFramebuffers(1, &shadowMapFBO);

    // 2. Stwórz Teksturę Głębi (Mapę Cienia)
    glGenTextures(1, &shadowMapTexture);
    glBindTexture(GL_TEXTURE_2D, shadowMapTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor); // Poza mapą jest jasno

    // 3. Podłącz teksturę do FBO
    glBindFramebuffer(GL_FRAMEBUFFER, shadowMapFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowMapTexture, 0);
    glDrawBuffer(GL_NONE); // Nie potrzebujemy bufora koloru
    glReadBuffer(GL_NONE); // Nie potrzebujemy bufora koloru

    // Sprawdź, czy FBO się powiodło
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "BLAD KRYTYCZNY: Framebuffer (Mapa Cienia) nie jest kompletny!" << std::endl;

    glBindFramebuffer(GL_FRAMEBUFFER, 0); // Od-binduj

    // Ustaw, której jednostki tekstury używa shader
    ourShader.use();
    ourShader.setInt("ourTexture", 0);
    ourShader.setInt("shadowMap", 1); // Mapa cienia będzie na jednostce 1
    // --- KONIEC INICJALIZACJI FBO ---

   while (g_currentState != STATE_EXITING)
{
       auto currentFrame = static_cast<float>(glfwGetTime());
       deltaTime = currentFrame - lastFrame;
       lastFrame = currentFrame;

       float physicsDelta = deltaTime;
       if (physicsDelta > 0.05f) {
           physicsDelta = 0.05f; // "Zwolnij czas" przy lagu
       }
       // Przetwarzanie inputu (wspólne dla wszystkich stanów)
       processInput(window);
    switch (g_currentState) {
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
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            // Rysuj tekst
            RenderText(uiShader, hotbarVAO, name_to_render, INPUT_X + PADDING_X, INPUT_NAME_Y + PADDING_Y, FONT_SIZE);
            RenderText(uiShader, hotbarVAO, seed_to_render, INPUT_X + PADDING_X, INPUT_SEED_Y + PADDING_Y, FONT_SIZE);
            glDisable(GL_BLEND);

            // Resetuj UV (bardzo ważne po RenderText!)
            uiShader.setVec2("u_uvScale", 1.0f, 1.0f);
            uiShader.setVec2("u_uvOffset", 0.0f, 0.0f);

            // Rysuj przycisk Start
            DrawMenuButton(uiShader, hotbarVAO, texBtnStart, BTN_START_X, BTN_START_Y, BTN_W, BTN_H);
            // Rysuj przycisk Wstecz
            DrawMenuButton(uiShader, hotbarVAO, texBtnBack, BTN_BACK_X, BTN_BACK_Y, BTN_W, BTN_H);
            break;
        }
        case STATE_LOAD_WORLD_MENU:
        {
            glDisable(GL_DEPTH_TEST);
            glDisable(GL_CULL_FACE);
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

            glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            uiShader.use();
            uiShader.setMat4("projection", ortho_projection);
            uiShader.setFloat("u_opacity", 1.0f);
            uiShader.setInt("uiTexture", 0);

            float FONT_SIZE = 24.0f;
            float PADDING_X = 10.0f;

            // --- Rysuj listę światów ---
            float current_y = LIST_START_Y;
            for (const std::string& worldName : existingWorlds)
            {
                // Rysuj tło przycisku
                uiShader.setVec2("u_uvScale", 1.0f, 1.0f);
                uiShader.setVec2("u_uvOffset", 0.0f, 0.0f);
                DrawMenuButton(uiShader, hotbarVAO, texInputBox, LIST_ITEM_X, current_y, LIST_ITEM_W, LIST_ITEM_H);

                // Rysuj tekst (nazwę świata)
                float PADDING_Y = (LIST_ITEM_H - FONT_SIZE) / 2.0f;
                glEnable(GL_BLEND);
                RenderText(uiShader, hotbarVAO, worldName, LIST_ITEM_X + PADDING_X, current_y + PADDING_Y, FONT_SIZE);
                glDisable(GL_BLEND);

                current_y -= LIST_GAP; // Przesuń w dół na następny przycisk
            }

            // --- Rysuj przycisk Wstecz ---
            uiShader.setVec2("u_uvScale", 1.0f, 1.0f);
            uiShader.setVec2("u_uvOffset", 0.0f, 0.0f);
            // Używamy stałych X i W z listy, ale Y z naszej nowej stałej
            DrawMenuButton(uiShader, hotbarVAO, texBtnBack, LIST_ITEM_X, LIST_BACK_BTN_Y, LIST_ITEM_W, LIST_ITEM_H);

            break;
        }
        case STATE_LOADING_WORLD: {
            std::string worldFolderPath = "worlds/" + g_selectedWorldName; // <-- WAŻNA ZMIANA
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
            LoadInventory();
            LoadFurnaces();
            g_selectedBlockID = g_inventory[g_activeSlot].itemID;
            LoadPlayerPosition();
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

                // Mierzymy czas, żeby nie przekroczyć limitu na klatkę
                double loadingStartTime = glfwGetTime();
                // Poświęcamy max 8ms na ładowanie (z 16ms dostępnych dla 60 FPS)
                // Jeśli masz słabszy PC, zmniejsz to do 0.004 (4ms)
                double timeBudget = 0.008;

                bool somethingLoaded = false;

                for (int x = playerChunkX - RENDER_DISTANCE; x <= playerChunkX + RENDER_DISTANCE; x++) {
                    for (int z = playerChunkZ - RENDER_DISTANCE; z <= playerChunkZ + RENDER_DISTANCE; z++) {

                        // SPRAWDZENIE CZASU: Czy przekroczyliśmy limit?
                        // Robimy to na początku, ale po wykonaniu chociaż jednego pełnego obiegu pętli
                        if (somethingLoaded && (glfwGetTime() - loadingStartTime > timeBudget)) {
                            goto stop_loading; // Przerywamy natychmiast
                        }

                        glm::ivec2 chunkPos(x, z);

                        // Jeśli chunk nie istnieje, stwórz go
                        if (g_WorldChunks.find(chunkPos) == g_WorldChunks.end())
                        {
                            chunk_storage.emplace_back(glm::vec3(x * CHUNK_WIDTH, 0.0f, z * CHUNK_DEPTH), g_selectedWorldName, g_selectedWorldSeed);
                            Chunk* newChunk = &chunk_storage.back();
                            g_WorldChunks[chunkPos] = newChunk;

                            // To jest najcięższa operacja:
                            newChunk->buildMesh();
                            newChunk->CalculateLighting();

                            // Odśwież sąsiadów (to też jest ciężkie, ale konieczne dla wody/AO)
                            auto refreshNeighbor = [&](int nx, int nz) {
                                auto it = g_WorldChunks.find(glm::ivec2(nx, nz));
                                if (it != g_WorldChunks.end()) {
                                    it->second->buildMesh();
                                }
                            };
                            refreshNeighbor(x + 1, z);
                            refreshNeighbor(x - 1, z);
                            refreshNeighbor(x, z + 1);
                            refreshNeighbor(x, z - 1);

                            somethingLoaded = true; // Zaznacz, że coś zrobiliśmy w tej klatce
                        }
                    }
                }
                stop_loading:;
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

            // Jeśli input zmienił stan (np. na STATE_PAUSE_MENU),
            // natychmiast wyjdź, nie obliczaj fizyki
            if (g_currentState != STATE_IN_GAME) {
                break;
            }
            auto currentFrame = static_cast<float>(glfwGetTime()); // Ta linia jest OK

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
            uint8_t headBlock = GetBlockGlobal(round(headPos.x), round(headPos.y), round(headPos.z));
            bool isInWater = (headBlock == BLOCK_ID_WATER || (headBlock >= 13 && headBlock <= 16));


            if (camera.flyingMode)
            {
                // --- FIZYKA LATANIA (prosta) ---
                camera.Position += wishDir * currentSpeed * physicsDelta;
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
                glm::vec3 newPos = camera.Position + playerVelocity * physicsDelta;

                // 4. Sprawdź kolizje (prosty AABB)

                // Funkcja pomocnicza sprawdzająca, czy blok jest SOLIDNY
                auto isSolid = [&](int x, int y, int z) {
                    uint8_t block = GetBlockGlobal(x, y, z);
                    return block != BLOCK_ID_AIR &&
                       block != BLOCK_ID_WATER &&
                           !(block == BLOCK_ID_WATER || (block >= 13 && block <= 16))&&
                       block != BLOCK_ID_TORCH;
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
            // processInput(window); // Obsługa WSAD i ESC
            UpdateDrops(physicsDelta, camera.Position);
            UpdateParticles(physicsDelta); // <<< DODAJ TĘ LINIĘ
            UpdateMining(deltaTime);
            UpdateLiquids(deltaTime);
            UpdateEntities(physicsDelta, camera.Position);
            UpdateSurvival(deltaTime);
            UnloadFarChunks();
            bool isMoving = (glm::length(glm::vec2(playerVelocity.x, playerVelocity.z)) > 0.1f);

            if (isMoving && onGround) {
                // Szybkość kroków zależy od tego czy biegniemy (CTRL) czy idziemy
                float stepDelay = (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) ? 0.3f : 0.5f;

                g_footstepTimer += deltaTime;

                if (g_footstepTimer >= stepDelay) {
                    // Odtwórz dźwięk (musisz mieć plik "step.mp3" lub "step.wav" w folderze projektu!)
                    ma_engine_play_sound(&g_audioEngine, "step.mp3", NULL);

                    // Zresetuj timer
                    g_footstepTimer = 0.0f;

                    // Efekt "Bobbing" kamery (opcjonalnie, jeśli jeszcze nie masz)
                }
            } else {
                // Jeśli stoimy, zresetuj timer, żeby pierwszy krok po ruszeniu był szybki
                g_footstepTimer = 0.5f;
            }
            // --- 2. RAYCASTING (w każdej klatce) ---
            float raycastDistance = 5.0f;
            glm::vec3 eyePos = camera.Position + glm::vec3(0.0f, PLAYER_EYE_HEIGHT, 0.0f);
            g_raycastHit = Raycast(eyePos, camera.Front, raycastDistance, g_hitBlockPos, g_prevBlockPos);

            // --- 3. ŁADOWANIE CHUNKÓW (bez zmian) ---
            int playerChunkX = static_cast<int>(floor(camera.Position.x / CHUNK_WIDTH));
            int playerChunkZ = static_cast<int>(floor(camera.Position.z / CHUNK_DEPTH));

            int pCX = playerChunkX;
            int pCZ = playerChunkZ;

            // A. ZLECANIE PRACY (Main -> Worker)
            // Sprawdzamy, których chunków brakuje i dodajemy je do kolejki
            // Limitujemy liczbę zleceń, żeby nie zapchać pamięci (np. max 50 w kolejce)
            int requestsSent = 0;

            // Specjalna mapa, żeby nie zlecać dwa razy tego samego (gdy worker jeszcze mieli)
            static std::map<glm::ivec2, bool, CompareIveco2> pendingChunks;

            for (int x = pCX - RENDER_DISTANCE; x <= pCX + RENDER_DISTANCE; x++) {
                for (int z = pCZ - RENDER_DISTANCE; z <= pCZ + RENDER_DISTANCE; z++) {

                    if (requestsSent > 2) break; // Max 2 zlecenia na klatkę, żeby nie zamulić kolejki

                    glm::ivec2 pos(x, z);

                    // Jeśli chunka nie ma I nie jest w trakcie robienia
                    if (g_WorldChunks.find(pos) == g_WorldChunks.end() &&
                        pendingChunks.find(pos) == pendingChunks.end())
                    {
                        g_chunksToLoad.push(pos); // Wyślij do workera
                        pendingChunks[pos] = true; // Oznacz jako "w toku"
                        requestsSent++;
                    }
                }
            }

            // B. ODBIERANIE GOTOWYCH (Worker -> Main)
            // Sprawdzamy, czy worker coś skończył
            Chunk* readyChunk = nullptr;
            int chunksProcessed = 0;

            // Przetwarzamy max 2 gotowe chunki na klatkę (żeby buildMesh nie zlagował)
            while (chunksProcessed < 2 && g_chunksReady.try_pop(readyChunk)) {

                glm::ivec2 pos(readyChunk->position.x / CHUNK_WIDTH, readyChunk->position.z / CHUNK_DEPTH);

                // Dodaj do świata (Teraz wątek główny jest właścicielem)
                g_WorldChunks[pos] = readyChunk;
                // Dodaj do listy (żeby SaveWorld działał) - chociaż chunk_storage to lista obiektów, a tu mamy wskaźnik...
                // POPRAWKA ARCHITEKTURY: Musimy zmienić chunk_storage na listę wskaźników,
                // ALBO (prościej dla Ciebie teraz) dodać wskaźnik do specjalnej listy `std::vector<Chunk*> loadedChunksPtrs;`
                // Ale Twój kod używa `chunk_storage` jako `std::list<Chunk>`.
                // Żeby nie psuć: zrobimy kopię (trochę wolniej, ale bezpieczniej w obecnym kodzie)
                chunk_storage.push_back(*readyChunk);
                // Teraz g_WorldChunks musi wskazywać na element w liście, a nie na readyChunk (który zaraz usuniemy)
                g_WorldChunks[pos] = &chunk_storage.back();
                delete readyChunk; // Usuwamy tymczasowy obiekt z workera, bo skopiowaliśmy go do listy

                // Usuń z listy oczekujących
                pendingChunks.erase(pos);

                // TERAZ (w wątku głównym) możemy bezpiecznie wywołać OpenGL
                Chunk* finalChunk = g_WorldChunks[pos];
                finalChunk->CalculateLighting();
                finalChunk->buildMesh();

                // Odśwież sąsiadów
                auto refresh = [&](int dx, int dz) {
                    auto it = g_WorldChunks.find(glm::ivec2(pos.x + dx, pos.y + dz));
                    if (it != g_WorldChunks.end()) {
                        it->second->CalculateLighting();
                        it->second->buildMesh();
                    }
                };
                refresh(1, 0); refresh(-1, 0); refresh(0, 1); refresh(0, -1);

                chunksProcessed++;
            }

            // --- 4. RENDEROWANIE ---
            float dayDuration = 100.0f;
            float startTimeOffset = dayDuration * 0.3f;
            float timeOfDay = fmod(glfwGetTime() + startTimeOffset, dayDuration) / dayDuration;
            float sunAngle = timeOfDay * 2.0f * 3.14159f;
            // s.y = 1.0 (południe), s.y = -1.0 (północ), s.y = 0.0 (wschód/zachód)
            // 'sunDirection' to wektor OD słońca
            glm::vec3 sunDirection = glm::normalize(glm::vec3(sin(sunAngle), cos(sunAngle), 0.3f));
            float sunHeight = -sunDirection.y; // Wysokość słońca (-1 do 1)

            // 2. Oblicz współczynnik oświetlenia (0.0 = noc, 1.0 = dzień)
            // Używamy .y (cos(sunAngle)), który jest > 0 tylko w dzień
            float daylightFactor = glm::smoothstep(-0.2f, 0.3f, sunHeight);

            // Kolory (takie same jak w shaderze dla spójności)
            glm::vec3 daySkyColor = glm::vec3(0.7f, 0.8f, 1.0f);   // Jasny błękit
            glm::vec3 sunsetSkyColor = glm::vec3(1.0f, 0.6f, 0.3f);// Pomarańcz
            glm::vec3 nightSkyColor = glm::vec3(0.02f, 0.02f, 0.05f); // Ciemny granat

            glm::vec3 currentSkyColor;
            if (sunHeight > 0.0f) {
                float t = glm::smoothstep(0.0f, 0.5f, sunHeight);
                currentSkyColor = glm::mix(sunsetSkyColor, daySkyColor, t);
            } else {
                float t = glm::smoothstep(0.0f, -0.3f, sunHeight);
                currentSkyColor = glm::mix(sunsetSkyColor, nightSkyColor, t);
            }

            // 4. Oblicz dynamiczną intensywność Słońca
            // W nocy (daylightFactor = 0), ambient jest słaby (księżyc), diffuse jest 0 (wyłączone).
            glm::vec3 sunAmbient = glm::mix(glm::vec3(0.05f), glm::vec3(0.4f), daylightFactor);

            // Diffuse (Słońce): W nocy 0.0, w dzień 0.9
            // Dodajemy lekki pomarańczowy odcień przy wschodzie/zachodzie
            glm::vec3 sunColor = glm::mix(glm::vec3(1.0f, 0.6f, 0.3f), glm::vec3(1.0f), daylightFactor);
            glm::vec3 sunDiffuse = sunColor * daylightFactor * 0.9f;
            // --- KONIEC CYKLU DNIA/NOCY ---


            // Użyj obliczonego koloru do czyszczenia tła
            glClearColor(currentSkyColor.x, currentSkyColor.y, currentSkyColor.z, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            // --- NAJPIERW OBLICZ MACIERZE ---
            float viewDistance = (RENDER_DISTANCE + 1) * CHUNK_WIDTH;
            glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, viewDistance);
            glm::mat4 view = camera.GetViewMatrix();
            // 'sunDirection' jest już obliczony dynamicznie

            // --- PRZEBIEG 1: RENDEROWANIE MAPY CIENIA ---
            {
                float shadowRange = 180.0f;
                glm::mat4 lightProjection = glm::ortho(-shadowRange, shadowRange, -shadowRange, shadowRange, 1.0f, 150.0f);
                glm::vec3 lightCamPos = camera.Position - (sunDirection * 50.0f);
                glm::mat4 lightView = glm::lookAt(lightCamPos, camera.Position, glm::vec3(0.0, 1.0, 0.0));
                glm::mat4 lightSpaceMatrix = lightProjection * lightView;

                depthShader.use();
                depthShader.setMat4("lightSpaceMatrix", lightSpaceMatrix);

                glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
                glBindFramebuffer(GL_FRAMEBUFFER, shadowMapFBO);
                glClear(GL_DEPTH_BUFFER_BIT);

                // --- KLUCZOWA POPRAWKA: ZMIANA CULLINGU ---
                // Rysujemy TYLNE ściany do mapy cienia.
                // To eliminuje "Shadow Acne" (paski) na oświetlonych powierzchniach.
                glCullFace(GL_FRONT);
                // ------------------------------------------

                int pCX = static_cast<int>(floor(camera.Position.x / CHUNK_WIDTH));
                int pCZ = static_cast<int>(floor(camera.Position.z / CHUNK_DEPTH));

                for (int x = pCX - RENDER_DISTANCE; x <= pCX + RENDER_DISTANCE; x++) {
                    for (int z = pCZ - RENDER_DISTANCE; z <= pCZ + RENDER_DISTANCE; z++) {
                        auto it = g_WorldChunks.find(glm::ivec2(x, z));
                        if (it != g_WorldChunks.end()) {
                            // Rysujemy tylko solidne bloki do cienia (bez wody/liści dla wydajności)
                            it->second->DrawSolid(depthShader, textureAtlas);
                        }
                    }
                }

                glBindFramebuffer(GL_FRAMEBUFFER, 0);

                // --- PRZYWRÓĆ NORMALNY CULLING ---
                glCullFace(GL_BACK);
                // ---------------------------------
            }
            glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);
            glClearColor(currentSkyColor.x, currentSkyColor.y, currentSkyColor.z, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
            // --- PRZEBIEG 2: NORMALNE RENDEROWANIE (Z PERSPEKTYWY GRACZA) ---
            ViewFrustum frustum;
            frustum.Update(projection * view); // Macierz VP (View-Projection)
            // (glClear jest już wywołany na górze)

            // --- PRZEBIEG 2A: SKYBOX ---
            glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);

            // Rysuj Skybox ZAWSZE (nawet bez tekstury, bo teraz jest proceduralny)
            {
                glDepthFunc(GL_LEQUAL);
                skyboxShader.use();

                // Macierze
                skyboxShader.setMat4("projection", projection);
                skyboxShader.setMat4("view", glm::mat4(glm::mat3(view))); // Usuń translację

                // --- NOWOŚĆ: Wysyłamy Słońce i Czas ---
                // Używamy 'sunDirection', które obliczyłeś na początku pętli
                skyboxShader.setVec3("u_sunDir", -sunDirection);
                skyboxShader.setFloat("u_time", (float)glfwGetTime());
                // --------------------------------------

                glBindVertexArray(skyboxVAO);

                // UWAGA: Nie bindujemy już żadnej tekstury (glBindTexture),
                // bo kolor liczy się matematycznie!

                glDrawArrays(GL_TRIANGLES, 0, 36);
                glBindVertexArray(0);
                glDepthFunc(GL_LESS);

            }

            //rysowanie świnki
            ourShader.use();
            ourShader.setInt("u_useUVTransform", 1);
            ourShader.setVec2("u_uvScale", 1.0f / ATLAS_COLS, 1.0f / ATLAS_ROWS);

            // Używamy dropVAO jako "cegiełki" do budowania świni
            glBindVertexArray(dropVAO);

            for (const auto& e : g_entities) {
                if (e.type == ENTITY_PIG) {
                    DrawPig(ourShader, e, dropVAO);
                } else if (e.type == ENTITY_ZOMBIE) {
                    DrawZombie(ourShader, e, dropVAO);
                }
            }

            ourShader.setInt("u_useUVTransform", 0);
            {
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glDisable(GL_CULL_FACE); // Chmury widać z dołu

                cloudShader.use();
                cloudShader.setMat4("projection", projection);
                cloudShader.setMat4("view", view);
                cloudShader.setVec3("viewPos", camera.Position);

                // Animacja wiatru
                float speed = 0.01f;
                float offset = (float)glfwGetTime() * speed;
                cloudShader.setVec2("u_cloudOffset", glm::vec2(offset, 0.0f)); // Płyń w osi X

                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, cloudTexture);

                glBindVertexArray(cloudVAO);
                glDrawArrays(GL_TRIANGLES, 0, 6);

                glEnable(GL_CULL_FACE);
            }
            // --- PRZEBIEG 2B: ŚWIAT (SOLID) ---
            ourShader.use();
            ourShader.setInt("u_isWater", 0); // Domyślnie NIE faluj
            ourShader.setFloat("u_time", (float)glfwGetTime()); // Prześlij czas

            // Ustaw wszystkie uniformy (Macierze, Mgła, Światła)
            ourShader.setMat4("projection", projection);
            ourShader.setMat4("view", view);
            ourShader.setVec3("viewPos", camera.Position);

            glm::vec3 finalFogColor = currentSkyColor;
            float finalFogStart = (RENDER_DISTANCE - 2) * CHUNK_WIDTH;
            float finalFogEnd = (RENDER_DISTANCE + 1) * CHUNK_WIDTH;

            // Sprawdź, czy kamera jest pod wodą
            // (Oczy są na Position + 0.0, 1.6, 0.0, ale lepiej sprawdzić samą pozycję kamery)
            glm::vec3 camEye = camera.Position;


            // Czy głowa jest w bloku wody?
            if (headBlock == BLOCK_ID_WATER || (headBlock >= 13 && headBlock <= 16)) {
                // JESTEŚMY POD WODĄ!
                finalFogColor = glm::vec3(0.0f, 0.1f, 0.4f); // Ciemny granat
                finalFogStart = 0.0f;    // Mgła zaczyna się od razu przy oczach
                finalFogEnd = 15.0f;     // Widoczność tylko na 15 metrów
            }
            ourShader.setVec3("u_fogColor", finalFogColor);
            ourShader.setFloat("u_fogStart", finalFogStart);
            ourShader.setFloat("u_fogEnd", finalFogEnd);

            // Słońce (DirLight) (użyj dynamicznych wartości)
            ourShader.setVec3("u_dirLight.direction", sunDirection); // <<< POPRAWKA (dynamiczny kierunek)
            ourShader.setVec3("u_dirLight.ambient", sunAmbient); // <<< POPRAWKA
            ourShader.setVec3("u_dirLight.diffuse", sunDiffuse); // <<< POPRAWKA
            ourShader.setVec3("u_dirLight.specular", 0.5f, 0.5f, 0.5f);

            // Pochodnia (PointLight) (bez zmian)
            int lightCount = 0;
            for (const auto& torchPos : g_torchPositions)
            {
                // Upewnij się, że nie przekraczamy limitu shadera
                if (lightCount >= MAX_POINT_LIGHTS) break;
                std::string base = "u_pointLights[" + std::to_string(lightCount) + "]";

                ourShader.setVec3(base + ".position", torchPos);
                ourShader.setVec3(base + ".ambient", 0.05f, 0.02f, 0.0f);

                // --- EFEKT MIGOTANIA ---
                // Używamy czasu i pozycji pochodni, żeby każda migała inaczej
                // Mieszamy dwa sinusy, żeby ruch był bardziej "chaotyczny"
                float flicker = sin(glfwGetTime() * 10.0f + torchPos.x) * 0.05f +
                                cos(glfwGetTime() * 23.0f + torchPos.z) * 0.05f;

                // Kolor ognia (Ciepły pomarańcz)
                ourShader.setVec3(base + ".diffuse", 1.5f + flicker, 0.9f + flicker, 0.4f);
                ourShader.setVec3(base + ".specular", 1.5f, 0.9f, 0.4f);

                ourShader.setFloat(base + ".constant", 1.0f);
                // Zmieniamy zasięg w rytm migotania
                ourShader.setFloat(base + ".linear", 0.09f + flicker * 0.1f);
                ourShader.setFloat(base + ".quadratic", 0.032f);

                lightCount++;
            }
            ourShader.setInt("u_pointLightCount", lightCount);

            // --- KLUCZOWE DLA CIENI ---
            // (Musimy obliczyć 'lightSpaceMatrix' DOKŁADNIE TAK SAMO jak w Przebiegu 1)
            float shadowRange = 180.0f; // Ta wartość MUSI być taka sama jak w Przebiegu 1
            glm::mat4 lightProjection = glm::ortho(-shadowRange, shadowRange, -shadowRange, shadowRange, 1.0f, 150.0f);
            glm::vec3 lightCamPos = camera.Position - (sunDirection * 50.0f); // Użyj DYNAMICZNEGO kierunku
            glm::mat4 lightView = glm::lookAt(lightCamPos, camera.Position, glm::vec3(0.0, 1.0, 0.0));
            glm::mat4 lightSpaceMatrix = lightProjection * lightView;
            ourShader.setMat4("lightSpaceMatrix", lightSpaceMatrix);

            // Powiedz shaderowi, żeby użył tekstury cienia na jednostce 1
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, shadowMapTexture);

            // Rysuj bloki stałe (używając 'ourShader')
            glDepthMask(GL_TRUE);
            glEnable(GL_CULL_FACE);
            for (int x = playerChunkX - RENDER_DISTANCE; x <= playerChunkX + RENDER_DISTANCE; x++) {
                for (int z = playerChunkZ - RENDER_DISTANCE; z <= playerChunkZ + RENDER_DISTANCE; z++) {
                    auto it = g_WorldChunks.find(glm::ivec2(x, z));
                    if (it != g_WorldChunks.end()) {
                        glm::vec3 min = it->second->position;
                        glm::vec3 max = min + glm::vec3(CHUNK_WIDTH, CHUNK_HEIGHT, CHUNK_DEPTH);

                        if (frustum.IsBoxVisible(min, max)) { // <<< TYLKO JEŚLI WIDOCZNY
                            it->second->DrawSolid(ourShader, textureAtlas);
                        }
                        // it->second->DrawSolid(ourShader, textureAtlas);
                    }
                }
            }
            ourShader.use(); // Upewnij się, że główny shader jest aktywny
            glDepthMask(GL_TRUE); // <<< WAŻNE: Włącz pisanie do głębi
            glDisable(GL_CULL_FACE); // Wyłącz culling dla pochodni

            for (int x = playerChunkX - RENDER_DISTANCE; x <= playerChunkX + RENDER_DISTANCE; x++) {
                for (int z = playerChunkZ - RENDER_DISTANCE; z <= playerChunkZ + RENDER_DISTANCE; z++) {
                    auto it = g_WorldChunks.find(glm::ivec2(x, z));
                    if (it != g_WorldChunks.end()) {
                        glm::vec3 min = it->second->position;
                        glm::vec3 max = min + glm::vec3(CHUNK_WIDTH, CHUNK_HEIGHT, CHUNK_DEPTH);
                        if (frustum.IsBoxVisible(min, max)) {
                            it->second->DrawFoliage(ourShader, textureAtlas);
                        } // <<< Użyj nowej funkcji
                    }
                }
            }
            // --- PRZEBIEG 2C: PODŚWIETLENIE ---
            if (g_raycastHit)
            {
                // ... (Twój kod podświetlenia - bez zmian) ...
                glDisable(GL_DEPTH_TEST);
                glDisable(GL_CULL_FACE);

                highlighterShader.use();
                highlighterShader.setMat4("projection", projection);
                highlighterShader.setMat4("view", view);

                glm::mat4 model = glm::mat4(1.0f);
                model = glm::translate(model, glm::vec3(g_hitBlockPos));
                model = glm::scale(model, glm::vec3(1.002f, 1.002f, 1.002f));

                highlighterShader.setMat4("model", model);

                glBindVertexArray(highlighterVAO);
                glDrawArrays(GL_LINES, 0, 24);
                glBindVertexArray(0);

                glEnable(GL_DEPTH_TEST);
                glEnable(GL_CULL_FACE);
            }

            // --- PRZEBIEG 2D: WODA (TRANSPARENT) ---
            ourShader.use();
            glDepthMask(GL_FALSE); // Nie pisz do bufora głębi
            glEnable(GL_BLEND);    // Włącz blendowanie
            glDisable(GL_CULL_FACE); // Rysuj obie strony (dla liści/pochodni)
            ourShader.setInt("u_isWater", 1);
            // --- NOWA LOGIKA SORTOWANIA ---
            // Stwórz mapę, która posortuje chunki od NAJDALSZEGO do NAJBLIŻSZEGO
            // Klucz: Dystans (float), Wartość: Wskaźnik na Chunk (Chunk*)
            std::map<float, Chunk*, std::greater<float>> sortedChunks;

            // 1. Wypełnij mapę wszystkimi widocznymi chunkami
            for (int x = playerChunkX - RENDER_DISTANCE; x <= playerChunkX + RENDER_DISTANCE; x++) {
                for (int z = playerChunkZ - RENDER_DISTANCE; z <= playerChunkZ + RENDER_DISTANCE; z++) {
                    auto it = g_WorldChunks.find(glm::ivec2(x, z));
                    if (it != g_WorldChunks.end()) {
                        Chunk* chunk = it->second;

                        // Oblicz kwadrat dystansu od kamery do środka chunka
                        // (Nie musimy liczyć pierwiastka (sqrt), sam kwadrat wystarczy do sortowania)
                        glm::vec3 min = chunk->position;
                        glm::vec3 max = min + glm::vec3(CHUNK_WIDTH, CHUNK_HEIGHT, CHUNK_DEPTH);

                        if (frustum.IsBoxVisible(min, max)) { // <<< DODAJ TO
                            glm::vec3 chunkCenter = chunk->position + glm::vec3(CHUNK_WIDTH / 2.0f, CHUNK_HEIGHT / 2.0f, CHUNK_DEPTH / 2.0f);
                            glm::vec3 vecToChunk = camera.Position - chunkCenter;
                            float distanceSq = glm::dot(vecToChunk, vecToChunk);
                            sortedChunks[distanceSq] = chunk;
                        }
                    }
                }
            }

            // 2. Rysuj chunki w posortowanej kolejności (od tyłu do przodu)
            // (std::map automatycznie sortuje klucze, a std::greater<float> odwraca kolejność)
            for (auto const& [dist, chunk] : sortedChunks)
            {
                chunk->DrawTransparent(ourShader, textureAtlas);
            }
            // --- KONIEC NOWEJ LOGIKI ---
            ourShader.setInt("u_isWater", 0);
            glEnable(GL_CULL_FACE); // Włącz culling z powrotem
            glDepthMask(GL_TRUE); // Włącz z powrotem pisanie do bufora głębi
            // --- PRZEBIEG 3: UI (SŁOŃCE, CELOWNIK, HOTBAR) ---
            ourShader.use();
            glEnable(GL_CULL_FACE);
            glDepthMask(GL_TRUE);

            // Włącz transformację UV dla dropów
            ourShader.setInt("u_useUVTransform", 1);

            glBindVertexArray(dropVAO);

            ourShader.setVec2("u_uvScale", 1.0f / ATLAS_COLS, 1.0f / ATLAS_ROWS);



            for (const auto& item : g_droppedItems) {
                glm::vec2 uvSide(0,0);
                glm::vec2 uvTop(0,0);
                glm::vec2 uvBottom(0,0); // <<< NOWOŚĆ
                int useMulti = 0;

                // --- Logika doboru tekstur ---
                if (item.itemID == BLOCK_ID_GRASS) {
                    uvSide   = glm::vec2(UV_GRASS_SIDE[0], UV_GRASS_SIDE[1]);
                    uvTop    = glm::vec2(UV_GRASS_TOP[0], UV_GRASS_TOP[1]);
                    uvBottom = glm::vec2(UV_DIRT[0], UV_DIRT[1]); // Dół trawy to ZIEMIA
                    useMulti = 1;
                }
                else if (item.itemID == BLOCK_ID_LOG) {
                    uvSide   = glm::vec2(UV_LOG_SIDE[0], UV_LOG_SIDE[1]);
                    uvTop    = glm::vec2(UV_LOG_TOP[0], UV_LOG_TOP[1]);
                    uvBottom = uvTop; // Dół pnia jest taki sam jak góra
                    useMulti = 1;
                }
                // --- Reszta bloków (Jednolita tekstura) ---
                else {
                    useMulti = 0;
                    if (item.itemID == BLOCK_ID_DIRT) uvSide = glm::vec2(UV_DIRT[0], UV_DIRT[1]);
                    else if (item.itemID == BLOCK_ID_STONE) uvSide = glm::vec2(UV_STONE[0], UV_STONE[1]);
                    else if (item.itemID == BLOCK_ID_TORCH) uvSide = glm::vec2(UV_TORCH[0], UV_TORCH[1]);
                    else if (item.itemID == BLOCK_ID_LEAVES) uvSide = glm::vec2(UV_LEAVES[0], UV_LEAVES[1]);
                    else if (item.itemID == BLOCK_ID_SAND) uvSide = glm::vec2(UV_SAND[0], UV_SAND[1]);
                    else if (item.itemID == BLOCK_ID_SANDSTONE) uvSide = glm::vec2(UV_SANDSTONE[0], UV_SANDSTONE[1]);
                    else if (item.itemID == BLOCK_ID_SNOW) uvSide = glm::vec2(UV_SNOW[0], UV_SNOW[1]);
                    else if (item.itemID == BLOCK_ID_ICE) uvSide = glm::vec2(UV_ICE[0], UV_ICE[1]);
                    else if (item.itemID == BLOCK_ID_BEDROCK) uvSide = glm::vec2(UV_BEDROCK[0], UV_BEDROCK[1]);
                    else if (item.itemID == BLOCK_ID_WATER) uvSide = glm::vec2(UV_WATER[0], UV_WATER[1]);
                    else if (item.itemID == BLOCK_ID_COAL_ORE) uvSide = glm::vec2(UV_COAL_ORE[0], UV_COAL_ORE[1]);
                    else if (item.itemID == BLOCK_ID_IRON_ORE) uvSide = glm::vec2(UV_IRON_ORE[0], UV_IRON_ORE[1]);
                    else if (item.itemID == BLOCK_ID_GOLD_ORE) uvSide = glm::vec2(UV_GOLD_ORE[0], UV_GOLD_ORE[1]);
                    else if (item.itemID == BLOCK_ID_DIAMOND_ORE) uvSide = glm::vec2(UV_DIAMOND_ORE[0], UV_DIAMOND_ORE[1]);

                    uvTop = uvSide;
                    uvBottom = uvSide;
                }

                // Wyślij wszystkie 3 offsety
                ourShader.setVec2("u_uvOffset", uvSide);
                ourShader.setVec2("u_uvOffsetTop", uvTop);
                ourShader.setVec2("u_uvOffsetBottom", uvBottom); // <<< NOWOŚĆ
                ourShader.setInt("u_multiTexture", useMulti);

                // --- Macierz Modelu (Bez zmian) ---
                glm::mat4 model = glm::mat4(1.0f);
                model = glm::translate(model, item.position);
                model = glm::rotate(model, item.rotationTime, glm::vec3(0, 1, 0));
                // model = glm::rotate(model, glm::radians(20.0f), glm::vec3(1, 0, 1));

                float hoverY = sin(item.rotationTime * 3.0f) * 0.1f;
                model = glm::translate(model, glm::vec3(0, hoverY, 0));

                if (item.itemID == BLOCK_ID_TORCH) {
                    model = glm::scale(model, glm::vec3(0.15f, 0.4f, 0.15f));
                } else {
                    model = glm::scale(model, glm::vec3(0.25f));
                }

                ourShader.setMat4("model", model);
                glDrawArrays(GL_TRIANGLES, 0, 36);
            }
            uint8_t handItemID = g_inventory[g_activeSlot].itemID;

            if (handItemID != BLOCK_ID_AIR) {
                glClear(GL_DEPTH_BUFFER_BIT); // Czyścimy głębię - ręka zawsze na wierzchu
                ourShader.use();

                // Włącz transformację UV (używamy atlasu jak w dropach)
                ourShader.setInt("u_useUVTransform", 1);
                ourShader.setVec2("u_uvScale", 1.0f / ATLAS_COLS, 1.0f / ATLAS_ROWS);

                // --- 1. Dobór Tekstur (Skopiowana logika z dropów) ---
                glm::vec2 uvSide(0,0), uvTop(0,0), uvBottom(0,0);
                int useMulti = 0;

                if (handItemID == BLOCK_ID_GRASS) {
                    uvSide = glm::vec2(UV_GRASS_SIDE[0], UV_GRASS_SIDE[1]);
                    uvTop  = glm::vec2(UV_GRASS_TOP[0], UV_GRASS_TOP[1]);
                    uvBottom = glm::vec2(UV_DIRT[0], UV_DIRT[1]);
                    useMulti = 1;
                }
                else if (handItemID == BLOCK_ID_LOG) {
                    uvSide = glm::vec2(UV_LOG_SIDE[0], UV_LOG_SIDE[1]);
                    uvTop  = glm::vec2(UV_LOG_TOP[0], UV_LOG_TOP[1]);
                    uvBottom = uvTop;
                    useMulti = 1;
                }
                else {
                    // Reszta bloków (Skrócona wersja - dodaj inne jeśli brakuje)
                    useMulti = 0;
                    if (handItemID == BLOCK_ID_DIRT) uvSide = glm::vec2(UV_DIRT[0], UV_DIRT[1]);
                    else if (handItemID == BLOCK_ID_STONE) uvSide = glm::vec2(UV_STONE[0], UV_STONE[1]);
                    else if (handItemID == BLOCK_ID_TORCH) uvSide = glm::vec2(UV_TORCH[0], UV_TORCH[1]);
                    else if (handItemID == BLOCK_ID_LEAVES) uvSide = glm::vec2(UV_LEAVES[0], UV_LEAVES[1]);
                    else if (handItemID == BLOCK_ID_SAND) uvSide = glm::vec2(UV_SAND[0], UV_SAND[1]);
                    else if (handItemID == BLOCK_ID_WATER) uvSide = glm::vec2(UV_WATER[0], UV_WATER[1]);
                    else if (handItemID == BLOCK_ID_BEDROCK) uvSide = glm::vec2(UV_BEDROCK[0], UV_BEDROCK[1]);
                    else if (handItemID == BLOCK_ID_SNOW) uvSide = glm::vec2(UV_SNOW[0], UV_SNOW[1]);
                    else if (handItemID == ITEM_ID_PICKAXE) uvSide = glm::vec2(UV_PICKAXE[0], UV_PICKAXE[1]); // Np. 13. ikona

                    uvTop = uvSide; uvBottom = uvSide;
                }

                ourShader.setVec2("u_uvOffset", uvSide);
                ourShader.setVec2("u_uvOffsetTop", uvTop);
                ourShader.setVec2("u_uvOffsetBottom", uvBottom);
                ourShader.setInt("u_multiTexture", useMulti);

                // --- 2. Pozycja Ręki (Matematyka) ---
                glm::mat4 handModel = glm::mat4(1.0f);

                // Bazowa pozycja: przyklejona do kamery
                glm::vec3 handPos = camera.Position + glm::vec3(0.0f, PLAYER_EYE_HEIGHT, 0.0f);

                // Przesunięcie "w rękę" (Prawo, Dół, Przód)
                // (Używamy wektorów kamery, żeby przedmiot się obracał razem z nami)
                handPos += camera.Front * 0.4f;  // 0.4m przed twarzą
                handPos += camera.Right * 0.25f; // 0.25m w prawo
                handPos += camera.Up * -0.25f;   // 0.25m w dół

                // Efekt "Bobbing" (Bujanie podczas chodzenia)
                if (glm::length(playerVelocity.x) > 0.1f || glm::length(playerVelocity.z) > 0.1f) {
                    float bobSpeed = 12.0f;
                    float bobAmount = 0.02f;
                    // Bujanie góra-dół
                    handPos += camera.Up * (float)sin(glfwGetTime() * bobSpeed) * bobAmount;
                    // Bujanie lewo-prawo
                    handPos += camera.Right * (float)cos(glfwGetTime() * bobSpeed * 0.5f) * bobAmount;
                }

                handModel = glm::translate(handModel, handPos);

                // Obrót: Przedmiot musi się obracać razem z kamerą
                // Najpierw obracamy go tak jak patrzy gracz
                handModel = glm::rotate(handModel, glm::radians(-camera.Yaw - 90.0f), glm::vec3(0, 1, 0));
                handModel = glm::rotate(handModel, glm::radians(camera.Pitch), glm::vec3(1, 0, 0));

                // Dodatkowy obrót "w nadgarstku" (żeby wyglądał naturalnie)
                handModel = glm::rotate(handModel, glm::radians(30.0f), glm::vec3(0, 1, 0));
                handModel = glm::rotate(handModel, glm::radians(-10.0f), glm::vec3(0, 0, 1));

                // Skala
                if (handItemID == BLOCK_ID_TORCH)
                    handModel = glm::scale(handModel, glm::vec3(0.12f, 0.35f, 0.12f));
                else
                    handModel = glm::scale(handModel, glm::vec3(0.2f));

                ourShader.setMat4("model", handModel);

                // Rysuj
                glBindVertexArray(dropVAO);
                glDrawArrays(GL_TRIANGLES, 0, 36);

                ourShader.setInt("u_useUVTransform", 0); // Reset
            }
            ourShader.use();
            ourShader.setInt("u_useUVTransform", 1);
            ourShader.setVec2("u_uvScale", 1.0f / ATLAS_COLS, 1.0f / ATLAS_ROWS);

            glBindVertexArray(dropVAO);

            for (const auto& p : g_particles) {
                // 1. Wybór tekstury (Kopiujemy logikę z dropów/chunków)
                glm::vec2 uvOff(0,0);

                // Używamy tej samej logiki co dla dropów (tekstura boczna)
                if (p.blockID == BLOCK_ID_GRASS) uvOff = glm::vec2(UV_GRASS_SIDE[0], UV_GRASS_SIDE[1]);
                else if (p.blockID == BLOCK_ID_DIRT) uvOff = glm::vec2(UV_DIRT[0], UV_DIRT[1]);
                else if (p.blockID == BLOCK_ID_STONE) uvOff = glm::vec2(UV_STONE[0], UV_STONE[1]);
                else if (p.blockID == BLOCK_ID_LOG) uvOff = glm::vec2(UV_LOG_SIDE[0], UV_LOG_SIDE[1]);
                else if (p.blockID == BLOCK_ID_LEAVES) uvOff = glm::vec2(UV_LEAVES[0], UV_LEAVES[1]);
                else if (p.blockID == BLOCK_ID_TORCH) uvOff = glm::vec2(UV_TORCH[0], UV_TORCH[1]);
                else if (p.blockID == BLOCK_ID_SAND) uvOff = glm::vec2(UV_SAND[0], UV_SAND[1]);
                else if (p.blockID == BLOCK_ID_SANDSTONE) uvOff = glm::vec2(UV_SANDSTONE[0], UV_SANDSTONE[1]);
                else if (p.blockID == BLOCK_ID_SNOW) uvOff = glm::vec2(UV_SNOW[0], UV_SNOW[1]);
                else if (p.blockID == BLOCK_ID_ICE) uvOff = glm::vec2(UV_ICE[0], UV_ICE[1]);
                else if (p.blockID == BLOCK_ID_BEDROCK) uvOff = glm::vec2(UV_BEDROCK[0], UV_BEDROCK[1]);
                else if (p.blockID == BLOCK_ID_WATER) uvOff = glm::vec2(UV_WATER[0], UV_WATER[1]);

                ourShader.setVec2("u_uvOffset", uvOff);

                // 2. Macierz Modelu
                glm::mat4 model = glm::mat4(1.0f);
                model = glm::translate(model, p.position);

                // Cząsteczki się obracają losowo w czasie lotu (prosty trik)
                model = glm::rotate(model, p.lifetime * 10.0f, glm::vec3(1, 1, 0));

                // SKALA: Cząsteczki maleją, gdy umierają
                float scale = (p.lifetime / p.maxLifetime) * 0.15f; // Startują jako 0.15, kończą jako 0.0
                model = glm::scale(model, glm::vec3(scale));

                ourShader.setMat4("model", model);
                glDrawArrays(GL_TRIANGLES, 0, 36);
            }
            // Wyłącz transformację UV
            ourShader.setInt("u_useUVTransform", 0);
            ourShader.use();
            // Wyłączamy tekstury z atlasu, żeby użyć jednolitego koloru
            // (W shaderze musielibyśmy dodać obsługę koloru, ale na razie użyjmy triku:
            // Wybierzemy biały fragment atlasu, np. Śnieg, i zmieszamy go z kolorem światła)

            // Lepsza opcja na szybko: Użyjmy tekstury DESEK (Planks) lub SAND dla świnki
            // Zakładam, że Sandstone to świnka ;)

            ourShader.setInt("u_useUVTransform", 1);
            ourShader.setVec2("u_uvScale", 1.0f / ATLAS_COLS, 1.0f / ATLAS_ROWS);

            glBindVertexArray(dropVAO);


            // 3A: Słońce 2D
            bool horizontal = true, first_iteration = true;
            int amount = 10;
            blurShader.use();
            for (int i = 0; i < amount; i++)
            {
                glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[horizontal]);
                blurShader.setInt("horizontal", horizontal);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, first_iteration ? colorBuffers[1] : pingpongColorbuffers[!horizontal]);
                RenderQuad();
                horizontal = !horizontal;
                if (first_iteration) first_iteration = false;
            }

            // B. Rysowanie na Ekran (Final Quad)
            glBindFramebuffer(GL_FRAMEBUFFER, 0); // <<< WRACAMY NA EKRAN!
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // Czyścimy ekran

            finalShader.use();
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, colorBuffers[0]); // Scena
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, pingpongColorbuffers[!horizontal]); // Bloom

            // (Ważne: Wyłącz Depth Test dla Quada)
            glDisable(GL_DEPTH_TEST);
            RenderQuad();
            glEnable(GL_DEPTH_TEST);
            // PRZEBIEG 3: UI (Celownik)
            glDisable(GL_DEPTH_TEST);
            crosshairShader.use();
            // glm::mat4 ortho_projection = glm::ortho(0.0f, (float)SCR_WIDTH, 0.0f, (float)SCR_HEIGHT);
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
            // glBindTexture(GL_TEXTURE_2D, iconsTextureID);
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
                uint8_t blockID = g_inventory[i].itemID;;

                float iconIndex = 0.0f;
                if (blockID == BLOCK_ID_GRASS) iconIndex = 0.0f;
                else if (blockID == BLOCK_ID_DIRT) iconIndex = 1.0f;
                else if (blockID == BLOCK_ID_STONE) iconIndex = 2.0f;
                else if (blockID == BLOCK_ID_WATER) iconIndex = 3.0f;
                else if (blockID == BLOCK_ID_SAND ) iconIndex = 4.0f;
                else if (blockID == BLOCK_ID_SANDSTONE) iconIndex = 5.0f;
                else if (blockID == BLOCK_ID_SNOW) iconIndex = 6.0f;
                else if (blockID == BLOCK_ID_ICE) iconIndex = 7.0f;
                else if (blockID == BLOCK_ID_BEDROCK) iconIndex = 8.0f;
                else if (blockID == BLOCK_ID_TORCH) iconIndex = 9.0f; // Pamiętaj o dodaniu mapowania dla TORCH i LOG/LEAVES
                else if (blockID == BLOCK_ID_LOG) iconIndex = 10.0f;
                else if (blockID == BLOCK_ID_LEAVES) iconIndex = 11.0f;
                else if (blockID == ITEM_ID_PICKAXE) iconIndex = 12.0f; // Np. 13. ikona
                // (Tu zmapuj resztę slotów 4-8, na razie będą się powtarzać)
                else if (blockID == BLOCK_ID_COAL_ORE)    iconIndex = 13.0f;
                else if (blockID == BLOCK_ID_IRON_ORE)    iconIndex = 14.0f;
                else if (blockID == BLOCK_ID_GOLD_ORE)    iconIndex = 15.0f;
                else if (blockID == BLOCK_ID_DIAMOND_ORE) iconIndex = 16.0f;
                else if (blockID == BLOCK_ID_LAVA) iconIndex = 3.0f;
                else if (blockID == BLOCK_ID_FURNACE) iconIndex = 18.0f;
                else if (blockID == ITEM_ID_COAL) iconIndex = 20.0f;
                else if (blockID == ITEM_ID_IRON_INGOT) iconIndex = 19.0f;

                // Przesuwamy UV do odpowiedniej ikony
                glm::vec2 uvOffset(iconIndex / ICON_ATLAS_COLS, 0.0f);
                uiShader.setVec2("u_uvOffset", uvOffset);
                glBindTexture(GL_TEXTURE_2D, iconsTextureID);
                if (blockID != BLOCK_ID_AIR) {
                    glDrawArrays(GL_TRIANGLES, 0, 6);
                }
                if (g_inventory[i].maxDurability > 0) {
                    DrawDurabilityBar(uiShader, hotbarVAO, x_pos, y_pos, SLOT_SIZE, SLOT_SIZE,
                                      g_inventory[i].durability, g_inventory[i].maxDurability);
                }
                else if (g_inventory[i].count > 1) {
                    std::string countStr = std::to_string(g_inventory[i].count);
                    float FONT_SIZE = 16.0f;
                    // Rysuj w prawym dolnym rogu slotu
                    float textX = x_pos + ICON_SIZE - (countStr.length() * FONT_SIZE * 0.6f) - 2.0f;
                    float textY = y_pos + 2.0f;

                    glEnable(GL_BLEND);
                    RenderText(uiShader, hotbarVAO, countStr, textX, textY, FONT_SIZE);
                    glDisable(GL_BLEND);
                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, iconsTextureID);
                    uiShader.setVec2("u_uvScale", 1.0f / ICON_ATLAS_COLS, 1.0f / ICON_ATLAS_ROWS); // Reset skali
                }
            }
            if (!isDead && !camera.flyingMode) { // Nie pokazuj w Creative
                float heartSize = 27.0f;
                float startX = SCR_WIDTH / 2.0f - 150.0f; // Na lewo od hotbara
                float startY = 80.0f; // Nad hotbarem

                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, iconsTextureID); // Używamy atlasu ikon

                // Zakładam, że serce to jakaś ikona w atlasie.
                // Jeśli nie masz serca, użyjmy np. indeksu 12 (jeśli masz tam coś czerwonego)
                // lub dodaj serce do ui_icons.png!
                float heartIndex = 17.0f;

                uiShader.setVec2("u_uvScale", 1.0f / ICON_ATLAS_COLS, 1.0f / ICON_ATLAS_ROWS);
                uiShader.setVec2("u_uvOffset", heartIndex / ICON_ATLAS_COLS, 0.0f);

                // Rysujemy tyle serc, ile mamy zdrowia
                for (int i = 0; i < (int)playerHealth; i++) {
                    DrawMenuButton(uiShader, hotbarVAO, iconsTextureID,
                                  startX + (i * (heartSize + 2)), startY,
                                  heartSize, heartSize);
                }
            }
            if (isDead) {
                // 1. Czerwony filtr na ekran
                glDisable(GL_DEPTH_TEST);
                glEnable(GL_BLEND);
                uiShader.use();
                uiShader.setFloat("u_opacity", 0.5f); // Półprzezroczysty

                // (Używamy texOverlay, ale możesz dodać czerwoną teksturę "death.png")
                DrawMenuButton(uiShader, hotbarVAO, texOverlay, 0, 0, SCR_WIDTH, SCR_HEIGHT);

                // 2. Napis "Nie żyjesz!"
                uiShader.setFloat("u_opacity", 1.0f);
                RenderText(uiShader, hotbarVAO, "GAME OVER", SCR_WIDTH/2 - 100, SCR_HEIGHT/2 + 50, 40.0f);
                RenderText(uiShader, hotbarVAO, "Nacisnij R aby odrodzic", SCR_WIDTH/2 - 180, SCR_HEIGHT/2 - 20, 20.0f);

                // 3. Logika Respawnu (Klawisz R)
                if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) {
                    isDead = false;
                    playerHealth = playerMaxHealth;
                    airTimer = 10.0f;
                    playerVelocity = glm::vec3(0.0f);

                    // Teleport na spawn (wysoko)
                    camera.Position = glm::vec3(8.0f, 50.0f, 8.0f);
                }
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
        case STATE_INVENTORY:
        {
            // --- KROK 1: Narysuj "zamrożony" świat w tle (PEŁEN KOD) ---
            {
                int playerChunkX = static_cast<int>(floor(camera.Position.x / CHUNK_WIDTH));
                int playerChunkZ = static_cast<int>(floor(camera.Position.z / CHUNK_DEPTH));
                // --- Obliczenia Cyklu Dnia/Nocy ---
                // (Musimy je obliczyć, aby tło wyglądało identycznie jak w grze)
                float dayDuration = 300.0f;
                float startTimeOffset = dayDuration * 0.3f;
                float timeOfDay = fmod(glfwGetTime() + startTimeOffset, dayDuration) / dayDuration;
                float sunAngle = timeOfDay * 2.0f * 3.14159f;

                // Słońce
                glm::vec3 sunDirection = glm::normalize(glm::vec3(sin(sunAngle), cos(sunAngle), 0.3f));
                float daylightFactor = std::clamp(-sunDirection.y, 0.0f, 1.0f);

                // Kolory
                glm::vec3 daySkyColor = glm::vec3(0.5f, 0.8f, 1.0f);
                glm::vec3 nightSkyColor = glm::vec3(0.02f, 0.02f, 0.08f);
                glm::vec3 currentSkyColor = mix(nightSkyColor, daySkyColor, daylightFactor);
                glm::vec3 sunAmbient = mix(glm::vec3(0.05f), glm::vec3(0.3f), daylightFactor);
                glm::vec3 sunDiffuse = mix(glm::vec3(0.0f), glm::vec3(0.7f), daylightFactor);

                // Macierze
                float viewDistance = (RENDER_DISTANCE + 1) * CHUNK_WIDTH;
                glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, viewDistance);
                glm::mat4 view = camera.GetViewMatrix();

                // Czyść Ekran
                glClearColor(currentSkyColor.x, currentSkyColor.y, currentSkyColor.z, 1.0f);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


                // 2. MAPA CIENIA (PRZEBIEG 1)
                {
                    float shadowRange = 180.0f;
                    glm::mat4 lightProjection = glm::ortho(-shadowRange, shadowRange, -shadowRange, shadowRange, 1.0f, 150.0f);
                    glm::vec3 lightCamPos = camera.Position - (sunDirection * 50.0f);
                    glm::mat4 lightView = glm::lookAt(lightCamPos, camera.Position, glm::vec3(0.0, 1.0, 0.0));
                    glm::mat4 lightSpaceMatrix = lightProjection * lightView;

                    depthShader.use();
                    depthShader.setMat4("lightSpaceMatrix", lightSpaceMatrix);

                    glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
                    glBindFramebuffer(GL_FRAMEBUFFER, shadowMapFBO);
                    glClear(GL_DEPTH_BUFFER_BIT);

                    // --- KLUCZOWA POPRAWKA: ZMIANA CULLINGU ---
                    // Rysujemy TYLNE ściany do mapy cienia.
                    // To eliminuje "Shadow Acne" (paski) na oświetlonych powierzchniach.
                    glCullFace(GL_FRONT);
                    // ------------------------------------------

                    int pCX = static_cast<int>(floor(camera.Position.x / CHUNK_WIDTH));
                    int pCZ = static_cast<int>(floor(camera.Position.z / CHUNK_DEPTH));

                    for (int x = pCX - RENDER_DISTANCE; x <= pCX + RENDER_DISTANCE; x++) {
                        for (int z = pCZ - RENDER_DISTANCE; z <= pCZ + RENDER_DISTANCE; z++) {
                            auto it = g_WorldChunks.find(glm::ivec2(x, z));
                            if (it != g_WorldChunks.end()) {
                                // Rysujemy tylko solidne bloki do cienia (bez wody/liści dla wydajności)
                                it->second->DrawSolid(depthShader, textureAtlas);
                            }
                        }
                    }

                    glBindFramebuffer(GL_FRAMEBUFFER, 0);

                    // --- PRZYWRÓĆ NORMALNY CULLING ---
                    glCullFace(GL_BACK);
                    // ---------------------------------
                }


                // 3. SKYBOX (PRZEBIEG 2A)
                glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT); // Reset viewportu

                if (cubemapTexture != 0)
                {
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



                // --- PRZEBIEG 2B: ŚWIAT (SOLID) ---
                ourShader.use();
                ourShader.setMat4("projection", projection);
                ourShader.setMat4("view", view);
                ourShader.setVec3("viewPos", camera.Position);
                ourShader.setVec3("u_fogColor", currentSkyColor);
                ourShader.setFloat("u_fogStart", (RENDER_DISTANCE - 2) * CHUNK_WIDTH);
                ourShader.setFloat("u_fogEnd", (RENDER_DISTANCE + 1) * CHUNK_WIDTH);
                ourShader.setVec3("u_dirLight.direction", sunDirection);
                ourShader.setVec3("u_dirLight.ambient", sunAmbient);
                ourShader.setVec3("u_dirLight.diffuse", sunDiffuse);
                ourShader.setVec3("u_dirLight.specular", 0.5f, 0.5f, 0.5f);

                int lightCount = 0;
                for (const auto& torchPos : g_torchPositions)
                {
                    if (lightCount >= MAX_POINT_LIGHTS) break;
                    std::string base = "u_pointLights[" + std::to_string(lightCount) + "]";

                    ourShader.setVec3(base + ".position", torchPos);
                    ourShader.setVec3(base + ".ambient", 0.05f, 0.02f, 0.0f);

                    // --- EFEKT MIGOTANIA ---
                    // Używamy czasu i pozycji pochodni, żeby każda migała inaczej
                    // Mieszamy dwa sinusy, żeby ruch był bardziej "chaotyczny"
                    float flicker = sin(glfwGetTime() * 10.0f + torchPos.x) * 0.05f +
                                    cos(glfwGetTime() * 23.0f + torchPos.z) * 0.05f;

                    // Kolor ognia (Ciepły pomarańcz)
                    ourShader.setVec3(base + ".diffuse", 1.5f + flicker, 0.9f + flicker, 0.4f);
                    ourShader.setVec3(base + ".specular", 1.5f, 0.9f, 0.4f);

                    ourShader.setFloat(base + ".constant", 1.0f);
                    // Zmieniamy zasięg w rytm migotania
                    ourShader.setFloat(base + ".linear", 0.09f + flicker * 0.1f);
                    ourShader.setFloat(base + ".quadratic", 0.032f);

                    lightCount++;
                }
                ourShader.setInt("u_pointLightCount", lightCount);

                float shadowRange = 180.0f;
                glm::mat4 lightProjection = glm::ortho(-shadowRange, shadowRange, -shadowRange, shadowRange, 1.0f, 150.0f);
                glm::vec3 lightCamPos = camera.Position - (sunDirection * 50.0f);
                glm::mat4 lightView = glm::lookAt(lightCamPos, camera.Position, glm::vec3(0.0, 1.0, 0.0));
                glm::mat4 lightSpaceMatrix = lightProjection * lightView;
                ourShader.setMat4("lightSpaceMatrix", lightSpaceMatrix);

                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_2D, shadowMapTexture);

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

                // --- PRZEBIEG 2C: LIŚCIE I POCHODNIE ---
                ourShader.use();
                glDepthMask(GL_TRUE);
                glDisable(GL_CULL_FACE);
                for (int x = playerChunkX - RENDER_DISTANCE; x <= playerChunkX + RENDER_DISTANCE; x++) {
                    for (int z = playerChunkZ - RENDER_DISTANCE; z <= playerChunkZ + RENDER_DISTANCE; z++) {
                        auto it = g_WorldChunks.find(glm::ivec2(x, z));
                        if (it != g_WorldChunks.end()) {
                            it->second->DrawFoliage(ourShader, textureAtlas);
                        }
                    }
                }

                // (Nie rysujemy podświetlenia w menu pauzy/ekwipunku)

                // --- PRZEBIEG 2E: WODA (SORTOWANA) ---
                ourShader.use();
                glDepthMask(GL_FALSE);
                glEnable(GL_BLEND);
                // glDisable(GL_CULL_FACE); // Już wyłączone z przebiegu 2C

                std::map<float, Chunk*, std::greater<float>> sortedChunks;
                for (int x = playerChunkX - RENDER_DISTANCE; x <= playerChunkX + RENDER_DISTANCE; x++) {
                    for (int z = playerChunkZ - RENDER_DISTANCE; z <= playerChunkZ + RENDER_DISTANCE; z++) {
                        auto it = g_WorldChunks.find(glm::ivec2(x, z));
                        if (it != g_WorldChunks.end()) {
                            Chunk* chunk = it->second;
                            glm::vec3 chunkCenter = chunk->position + glm::vec3(CHUNK_WIDTH / 2.0f, CHUNK_HEIGHT / 2.0f, CHUNK_DEPTH / 2.0f);
                            glm::vec3 vecToChunk = camera.Position - chunkCenter;
                            float distanceSq = glm::dot(vecToChunk, vecToChunk);
                            sortedChunks[distanceSq] = chunk;
                        }
                    }
                }
                for (auto const& [dist, chunk] : sortedChunks)
                {
                    chunk->DrawTransparent(ourShader, textureAtlas);
                }
                glEnable(GL_CULL_FACE);
                glDepthMask(GL_TRUE);
                ourShader.use();
                glEnable(GL_CULL_FACE);
                glDepthMask(GL_TRUE);

                // Włącz transformację UV dla dropów
                ourShader.setInt("u_useUVTransform", 1);

                glBindVertexArray(dropVAO); // Używamy naszego małego sześcianu

                for (const auto& item : g_droppedItems) {
                    // 1. Oblicz UV Offset na podstawie ID
                    float iconIndex = 0.0f;
                    if (item.itemID == BLOCK_ID_GRASS) iconIndex = 0.0f;
                    else if (item.itemID == BLOCK_ID_DIRT) iconIndex = 1.0f;
                    else if (item.itemID == BLOCK_ID_STONE) iconIndex = 2.0f;
                    else if (item.itemID == BLOCK_ID_TORCH) iconIndex = 9.0f;
                    // ... (reszta Twoich if-ów)

                    // Dla testu - po prostu rysujmy (tu możesz dodać dokładniejszą logikę)
                    glm::vec2 uvOff(0,0);
                    // Przykład manualnego mapowania (dostosuj do swojego atlasu):
                    if (item.itemID == BLOCK_ID_DIRT) uvOff = glm::vec2(0.0f, 1.0f/3.0f);
                    else if (item.itemID == BLOCK_ID_STONE) uvOff = glm::vec2(0.25f, 1.0f/3.0f);

                    // UWAGA: To jest przykładowe mapowanie.
                    // Docelowo iconIndex powinien sterować uvOff tak jak w UI.

                    ourShader.setVec2("u_uvOffset", uvOff);
                    ourShader.setVec2("u_uvScale", 1.0f/4.0f, 1.0f/3.0f); // Skala atlasu bloków (4 kolumny, 3 rzędy)

                    // 2. Macierz Modelu (Ruch + Obrót + Skala)
                    glm::mat4 model = glm::mat4(1.0f);
                    model = glm::translate(model, item.position);
                    model = glm::rotate(model, item.rotationTime, glm::vec3(0, 1, 0)); // Obrót wokół Y

                    // Efekt lewitowania (Bobbing)
                    float hoverY = sin(item.rotationTime * 3.0f) * 0.1f;
                    model = glm::translate(model, glm::vec3(0, hoverY, 0));

                    model = glm::scale(model, glm::vec3(0.25f)); // Mały blok (1/4 wielkości)

                    ourShader.setMat4("model", model);
                    glDrawArrays(GL_TRIANGLES, 0, 36);
                }

                // Wyłącz transformację UV (żeby nie popsuć chunków w następnej klatce)
                ourShader.setInt("u_useUVTransform", 0);
            } // Koniec bloku renderowania tła 3D
            glDisable(GL_DEPTH_TEST);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            uiShader.use();
            uiShader.setMat4("projection", ortho_projection);

            uiShader.setFloat("u_opacity", 0.77f); // Półprzezroczystość
            uiShader.setVec2("u_uvScale", 1.0f, 1.0f);
            uiShader.setVec2("u_uvOffset", 0.0f, 0.0f);

            // Użyj tej samej tekstury 'texOverlay' co w menu pauzy
            DrawMenuButton(uiShader, hotbarVAO, texOverlay, 0.0f, 0.0f, SCR_WIDTH, SCR_HEIGHT);
            // --- KONIEC NOWEGO BLOKU ---
            // --- KROK 2: Narysuj UI Ekwipunku ---
            glDisable(GL_DEPTH_TEST);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            uiShader.use();
            uiShader.setMat4("projection", ortho_projection);
            uiShader.setFloat("u_opacity", 1.0f);

            // Definicje siatki (3 wiersze po 9 slotów)
            float invSlotSize = SLOT_SIZE;
            float invSlotGap = 4.0f;
            float invWidth = (invSlotSize * 9) + (invSlotGap * 8);
            float invStartX = (SCR_WIDTH - invWidth) / 2.0f;
            float invStartY = (SCR_HEIGHT / 2.0f) - 50.0f;

            // Pętla rysująca 27 slotów "plecaka"
            for (int i = 0; i < 27; i++) {
                int row = i / 9;
                int col = i % 9;
                int slotIndex = i + 9;

                float x_pos = invStartX + col * (invSlotSize + invSlotGap);
                float y_pos = invStartY - row * (invSlotSize + invSlotGap);

                // Rysuj tło slotu
                uiShader.setVec2("u_uvScale", 1.0f, 1.0f);
                uiShader.setVec2("u_uvOffset", 0.0f, 0.0f);
                DrawMenuButton(uiShader, hotbarVAO, slotTextureID, x_pos, y_pos, invSlotSize, invSlotSize);

                // Rysuj ikonę przedmiotu (jeśli jest)
                uint8_t itemID = g_inventory[slotIndex].itemID;
                if (itemID != BLOCK_ID_AIR) {
                    float iconSize = 52.0f;
                    float iconX = x_pos + (invSlotSize - iconSize) / 2.0f;
                    float iconY = y_pos + (invSlotSize - iconSize) / 2.0f;

                    float iconIndex = 0.0f;
                    if (itemID == BLOCK_ID_GRASS) iconIndex = 0.0f;
                    else if (itemID == BLOCK_ID_DIRT) iconIndex = 1.0f;
                    else if (itemID == BLOCK_ID_STONE) iconIndex = 2.0f;
                    else if (itemID == BLOCK_ID_WATER) iconIndex = 3.0f;
                    else if (itemID == BLOCK_ID_SAND ) iconIndex = 4.0f;
                    else if (itemID == BLOCK_ID_SANDSTONE) iconIndex = 5.0f;
                    else if (itemID == BLOCK_ID_SNOW) iconIndex = 6.0f;
                    else if (itemID == BLOCK_ID_ICE) iconIndex = 7.0f;
                    else if (itemID == BLOCK_ID_BEDROCK) iconIndex = 8.0f;
                    else if (itemID == BLOCK_ID_TORCH) iconIndex = 9.0f;
                    else if (itemID == BLOCK_ID_LOG) iconIndex = 10.0f;
                    else if (itemID == BLOCK_ID_LEAVES) iconIndex = 11.0f;
                    else if (itemID == ITEM_ID_PICKAXE) iconIndex = 12.0f; // Np. 13. ikona
                    else if (itemID == BLOCK_ID_COAL_ORE)    iconIndex = 13.0f;
                    else if (itemID == BLOCK_ID_IRON_ORE)    iconIndex = 14.0f;
                    else if (itemID == BLOCK_ID_GOLD_ORE)    iconIndex = 15.0f;
                    else if (itemID == BLOCK_ID_DIAMOND_ORE) iconIndex = 16.0f;
                    else if (itemID == BLOCK_ID_FURNACE) iconIndex = 18.0f;
                    else if (itemID == ITEM_ID_COAL) iconIndex = 20.0f;
                    else if (itemID == ITEM_ID_IRON_INGOT) iconIndex = 19.0f;


                    glm::vec2 uvScale(1.0f / ICON_ATLAS_COLS, 1.0f / ICON_ATLAS_ROWS);
                    glm::vec2 uvOffset(iconIndex / ICON_ATLAS_COLS, 0.0f);
                    uiShader.setVec2("u_uvScale", uvScale);
                    uiShader.setVec2("u_uvOffset", uvOffset);

                    // --- POPRAWKA: Ręczne rysowanie zamiast DrawMenuButton ---
                    // DrawMenuButton resetuje teksturę, psując RenderText
                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, iconsTextureID);
                    glm::mat4 model = glm::mat4(1.0f);
                    model = glm::translate(model, glm::vec3(iconX, iconY, 0.0f));
                    model = glm::scale(model, glm::vec3(iconSize, iconSize, 1.0f));
                    uiShader.setMat4("model", model);
                    glDrawArrays(GL_TRIANGLES, 0, 6);
                    // --- KONIEC POPRAWKI ---

                    // Rysuj licznik
                    if (g_inventory[slotIndex].count > 1) {
                        std::string countStr = std::to_string(g_inventory[slotIndex].count);
                        float FONT_SIZE = 16.0f;
                        float textX = iconX + iconSize - (countStr.length() * FONT_SIZE * 0.6f) - 2.0f;
                        float textY = iconY + 2.0f;
                        RenderText(uiShader, hotbarVAO, countStr, textX, textY, FONT_SIZE);

                        // --- POPRAWKA: Zresetuj stan dla następnej iteracji pętli ---
                        // (RenderText zmienia teksturę i skalę UV)
                        glActiveTexture(GL_TEXTURE0);
                        glBindTexture(GL_TEXTURE_2D, slotTextureID); // Przygotuj dla DrawMenuButton
                        uiShader.setVec2("u_uvScale", 1.0f, 1.0f); // DrawMenuButton oczekuje (1,1)
                        // --- KONIEC POPRAWKI ---
                    }
                }
            }
            float craftStartX = invStartX + (invSlotSize + invSlotGap) * 5.0f;
            float craftStartY = invStartY + (invSlotSize + invSlotGap) * 4.0f;

            // 1. Siatka Wejściowa
            for (int i = 0; i < 4; i++) {
                int row = i / 2;
                int col = i % 2;
                float x = craftStartX + col * (invSlotSize + invSlotGap);
                float y = craftStartY - row * (invSlotSize + invSlotGap);

                // Tło
                uiShader.setVec2("u_uvScale", 1.0f, 1.0f);
                uiShader.setVec2("u_uvOffset", 0.0f, 0.0f);
                DrawMenuButton(uiShader, hotbarVAO, slotTextureID, x, y, invSlotSize, invSlotSize);

                // Przedmiot
                if (g_craftingGrid[i].itemID != BLOCK_ID_AIR) {
                    float iconSize = 52.0f;
                    float iconX = x + (invSlotSize - iconSize) / 2.0f;
                    float iconY = y + (invSlotSize - iconSize) / 2.0f;

                    // 2. Mapowanie ID na ikonę (To samo co w reszcie gry)
                    uint8_t itemID = g_craftingGrid[i].itemID;
                    float iconIndex = 0.0f;

                    if (itemID == BLOCK_ID_GRASS) iconIndex = 0.0f;
                    else if (itemID == BLOCK_ID_DIRT) iconIndex = 1.0f;
                    else if (itemID == BLOCK_ID_STONE) iconIndex = 2.0f;
                    else if (itemID == BLOCK_ID_WATER) iconIndex = 3.0f;
                    else if (itemID == BLOCK_ID_BEDROCK) iconIndex = 4.0f;
                    else if (itemID == BLOCK_ID_SAND ) iconIndex = 5.0f;
                    else if (itemID == BLOCK_ID_SANDSTONE) iconIndex = 6.0f;
                    else if (itemID == BLOCK_ID_SNOW) iconIndex = 7.0f;
                    else if (itemID == BLOCK_ID_ICE) iconIndex = 8.0f;
                    else if (itemID == BLOCK_ID_TORCH) iconIndex = 9.0f;
                    else if (itemID == BLOCK_ID_LOG) iconIndex = 10.0f;
                    else if (itemID == BLOCK_ID_LEAVES) iconIndex = 11.0f;
                    else if (itemID == ITEM_ID_PICKAXE) iconIndex = 12.0f; // Np. 13. ikona
                    else if (itemID == BLOCK_ID_COAL_ORE)    iconIndex = 13.0f;
                    else if (itemID == BLOCK_ID_IRON_ORE)    iconIndex = 14.0f;
                    else if (itemID == BLOCK_ID_GOLD_ORE)    iconIndex = 15.0f;
                    else if (itemID == BLOCK_ID_DIAMOND_ORE) iconIndex = 16.0f;

                    // 3. Rysowanie Ikony
                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, iconsTextureID);

                    uiShader.setVec2("u_uvScale", 1.0f / ICON_ATLAS_COLS, 1.0f / ICON_ATLAS_ROWS);
                    uiShader.setVec2("u_uvOffset", iconIndex / ICON_ATLAS_COLS, 0.0f);

                    glm::mat4 model = glm::mat4(1.0f);
                    model = glm::translate(model, glm::vec3(iconX, iconY, 0.0f));
                    model = glm::scale(model, glm::vec3(iconSize, iconSize, 1.0f));
                    uiShader.setMat4("model", model);

                    glDrawArrays(GL_TRIANGLES, 0, 6);

                    // 4. Rysowanie Licznika (jeśli > 1)
                    if (g_craftingGrid[i].count > 1) {
                        std::string c = std::to_string(g_craftingGrid[i].count);
                        float FONT_SIZE = 16.0f;
                        float textX = iconX + iconSize - (c.length() * FONT_SIZE * 0.6f) - 2.0f;
                        float textY = iconY + 2.0f;

                        glEnable(GL_BLEND);
                        RenderText(uiShader, hotbarVAO, c, textX, textY, FONT_SIZE);
                        glDisable(GL_BLEND);

                        // WAŻNE: Reset po RenderText, żeby następna ikona się dobrze narysowała
                        glActiveTexture(GL_TEXTURE0);
                        glBindTexture(GL_TEXTURE_2D, iconsTextureID);
                    }
                }
            }

            // 2. Slot Wyniku
            float resultX = craftStartX + (invSlotSize + invSlotGap) * 3.0f;
            float resultY = craftStartY - 0.5f * (invSlotSize + invSlotGap);

            // Tło
            uiShader.setVec2("u_uvScale", 1.0f, 1.0f);
            uiShader.setVec2("u_uvOffset", 0.0f, 0.0f);
            DrawMenuButton(uiShader, hotbarVAO, slotTextureID, resultX, resultY, invSlotSize, invSlotSize);

            // Wynik
            if (g_craftingOutput.itemID != BLOCK_ID_AIR) {
                float iconSize = 52.0f;
                float iconX = resultX + (invSlotSize - iconSize) / 2.0f;
                float iconY = resultY + (invSlotSize - iconSize) / 2.0f;

                // 2. Mapowanie ID na indeks w atlasie
                // (Musi być zgodne z resztą gry)
                uint8_t outID = g_craftingOutput.itemID;
                float iconIndex = 0.0f;

                if (outID == BLOCK_ID_GRASS) iconIndex = 0.0f;
                else if (outID == BLOCK_ID_DIRT) iconIndex = 1.0f;
                else if (outID == BLOCK_ID_STONE) iconIndex = 2.0f;
                else if (outID == BLOCK_ID_WATER) iconIndex = 3.0f;
                else if (outID == BLOCK_ID_BEDROCK) iconIndex = 4.0f;
                else if (outID == BLOCK_ID_SAND ) iconIndex = 5.0f;
                else if (outID == BLOCK_ID_SANDSTONE) iconIndex = 6.0f;
                else if (outID == BLOCK_ID_SNOW) iconIndex = 7.0f;
                else if (outID == BLOCK_ID_ICE) iconIndex = 8.0f;
                else if (outID == BLOCK_ID_TORCH) iconIndex = 9.0f;
                else if (outID == BLOCK_ID_LOG) iconIndex = 10.0f;
                else if (outID == BLOCK_ID_LEAVES) iconIndex = 11.0f;
                else if (outID == ITEM_ID_PICKAXE) iconIndex = 12.0f; // Np. 13. ikona

                // 3. Ustawienia Shadera (Tekstura i UV)
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, iconsTextureID);
                uiShader.setVec2("u_uvScale", 1.0f / ICON_ATLAS_COLS, 1.0f / ICON_ATLAS_ROWS);
                uiShader.setVec2("u_uvOffset", iconIndex / ICON_ATLAS_COLS, 0.0f);

                // 4. Rysowanie Ikony
                glm::mat4 model = glm::mat4(1.0f);
                model = glm::translate(model, glm::vec3(iconX, iconY, 0.0f));
                model = glm::scale(model, glm::vec3(iconSize, iconSize, 1.0f));
                uiShader.setMat4("model", model);
                glDrawArrays(GL_TRIANGLES, 0, 6);

                // 5. Rysowanie Licznika (jeśli > 1)
                if (g_craftingOutput.count > 1) {
                    std::string c = std::to_string(g_craftingOutput.count);
                    float FONT_SIZE = 16.0f;
                    float textX = iconX + iconSize - (c.length() * FONT_SIZE * 0.6f) - 2.0f;
                    float textY = iconY + 2.0f;

                    RenderText(uiShader, hotbarVAO, c, textX, textY, FONT_SIZE);

                    // Reset po tekście
                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, iconsTextureID);
                }
            }
            // --- KROK 3: Rysuj Hotbar (w UI Ekwipunku) ---
            // (Skopiowany kod z STATE_IN_GAME)
            {
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, slotTextureID);
                uiShader.setInt("uiTexture", 0);
                uiShader.setVec2("u_uvScale", 1.0f, 1.0f);
                uiShader.setVec2("u_uvOffset", 0.0f, 0.0f);

                for (int i = 0; i < HOTBAR_SIZE; i++) {
                    float x_pos = BAR_START_X + i * (SLOT_SIZE + GAP);
                    float y_pos = 10.0f;
                    DrawMenuButton(uiShader, hotbarVAO, slotTextureID, x_pos, y_pos, SLOT_SIZE, SLOT_SIZE);
                }

                float ICON_SIZE = 52.0f;
                glm::vec2 uvScale(1.0f / ICON_ATLAS_COLS, 1.0f / ICON_ATLAS_ROWS);
                uiShader.setVec2("u_uvScale", uvScale);

                for (int i = 0; i < HOTBAR_SIZE; i++) {
                    float x_pos = BAR_START_X + i * (SLOT_SIZE + GAP) + (SLOT_SIZE - ICON_SIZE) / 2.0f;
                    float y_pos = 10.0f + (SLOT_SIZE - ICON_SIZE) / 2.0f;

                    uint8_t blockID = g_inventory[i].itemID;
                    float iconIndex = 0.0f;
                    if (blockID == BLOCK_ID_GRASS) iconIndex = 0.0f;
                    else if (blockID == BLOCK_ID_DIRT) iconIndex = 1.0f;
                    else if (blockID == BLOCK_ID_STONE) iconIndex = 2.0f;
                    else if (blockID == BLOCK_ID_WATER) iconIndex = 3.0f;
                    else if (blockID == BLOCK_ID_SAND ) iconIndex = 4.0f;
                    else if (blockID == BLOCK_ID_SANDSTONE) iconIndex = 5.0f;
                    else if (blockID == BLOCK_ID_SNOW) iconIndex = 6.0f;
                    else if (blockID == BLOCK_ID_ICE) iconIndex = 7.0f;
                    else if (blockID == BLOCK_ID_BEDROCK) iconIndex = 8.0f;
                    else if (blockID == BLOCK_ID_TORCH) iconIndex = 9.0f;
                    else if (blockID == BLOCK_ID_LOG) iconIndex = 10.0f;
                    else if (blockID == BLOCK_ID_LEAVES) iconIndex = 11.0f;
                    else if (blockID == ITEM_ID_PICKAXE) iconIndex = 12.0f; // Np. 13. ikona
                    else if (blockID == BLOCK_ID_COAL_ORE)    iconIndex = 13.0f;
                    else if (blockID == BLOCK_ID_IRON_ORE)    iconIndex = 14.0f;
                    else if (blockID == BLOCK_ID_GOLD_ORE)    iconIndex = 15.0f;
                    else if (blockID == BLOCK_ID_DIAMOND_ORE) iconIndex = 16.0f;
                    else if (blockID == BLOCK_ID_FURNACE) iconIndex = 18.0f;
                    else if (blockID == ITEM_ID_COAL) iconIndex = 20.0f;
                    else if (blockID == ITEM_ID_IRON_INGOT) iconIndex = 19.0f;


                    glm::vec2 uvOffset(iconIndex / ICON_ATLAS_COLS, 0.0f);
                    uiShader.setVec2("u_uvOffset", uvOffset);

                    // Ręczne rysowanie ikony
                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, iconsTextureID);
                    glm::mat4 model = glm::mat4(1.0f);
                    model = glm::translate(model, glm::vec3(x_pos, y_pos, 0.0f));
                    model = glm::scale(model, glm::vec3(ICON_SIZE, ICON_SIZE, 1.0f));
                    uiShader.setMat4("model", model);

                    if (blockID != BLOCK_ID_AIR) {
                        glDrawArrays(GL_TRIANGLES, 0, 6);
                    }
                    if (g_inventory[i].maxDurability > 0) {
                        DrawDurabilityBar(uiShader, hotbarVAO, x_pos, y_pos, SLOT_SIZE, SLOT_SIZE,
                                          g_inventory[i].durability, g_inventory[i].maxDurability);
                    }
                    else if (g_inventory[i].count > 1) {
                        std::string countStr = std::to_string(g_inventory[i].count);
                        float FONT_SIZE = 16.0f;
                        float textX = x_pos + ICON_SIZE - (countStr.length() * FONT_SIZE * 0.6f) - 2.0f;
                        float textY = y_pos + 2.0f;

                        RenderText(uiShader, hotbarVAO, countStr, textX, textY, FONT_SIZE);

                        // Resetuj stan dla następnej iteracji
                        glActiveTexture(GL_TEXTURE0);
                        glBindTexture(GL_TEXTURE_2D, iconsTextureID);
                        uiShader.setVec2("u_uvScale", 1.0f / ICON_ATLAS_COLS, 1.0f / ICON_ATLAS_ROWS);
                    }
                }

                // Rysuj selektor
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, selectorTextureID);
                uiShader.setVec2("u_uvScale", 1.0f, 1.0f);
                uiShader.setVec2("u_uvOffset", 0.0f, 0.0f);
                float selector_x_pos = BAR_START_X + g_activeSlot * (SLOT_SIZE + GAP) - (SELECTOR_SIZE - SLOT_SIZE) / 2.0f;
                float selector_y_pos = 10.0f - (SELECTOR_SIZE - SLOT_SIZE) / 2.0f;
                glm::mat4 model = glm::mat4(1.0f);
                model = glm::translate(model, glm::vec3(selector_x_pos, selector_y_pos, 0.0f));
                model = glm::scale(model, glm::vec3(SELECTOR_SIZE, SELECTOR_SIZE, 1.0f));
                uiShader.setMat4("model", model);
                glDrawArrays(GL_TRIANGLES, 0, 6);
            }
            if (g_mouseItem.itemID != BLOCK_ID_AIR)
            {
                double mx, my;
                glfwGetCursorPos(window, &mx, &my);

                // Konwertuj współrzędne myszy na UI (od dołu)
                float mouseX = static_cast<float>(mx);
                float mouseY = SCR_HEIGHT - static_cast<float>(my);

                // Wyśrodkuj ikonę na kursorze
                float iconSize = 52.0f;
                float drawX = mouseX - iconSize / 2.0f;
                float drawY = mouseY - iconSize / 2.0f;

                // Ustaw teksturę ikon
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, iconsTextureID);
                uiShader.setVec2("u_uvScale", 1.0f / ICON_ATLAS_COLS, 1.0f / ICON_ATLAS_ROWS);

                // Znajdź UV dla przedmiotu
                uint8_t heldID = g_mouseItem.itemID;
                float iconIndex = 0.0f;
                if (heldID == BLOCK_ID_GRASS) iconIndex = 0.0f;
                else if (heldID == BLOCK_ID_DIRT) iconIndex = 1.0f;
                // ... (dodaj mapowania, lub lepiej: zrób funkcję pomocniczą GetIconIndex(id)) ...
                // Na razie skopiuj logikę mapowania stąd, co masz wyżej w pętlach:
                else if (heldID == BLOCK_ID_STONE) iconIndex = 2.0f;
                else if (heldID == BLOCK_ID_WATER) iconIndex = 3.0f;
                else if (heldID == BLOCK_ID_SAND ) iconIndex = 4.0f;
                else if (heldID == BLOCK_ID_SANDSTONE) iconIndex = 5.0f;
                else if (heldID == BLOCK_ID_SNOW) iconIndex = 6.0f;
                else if (heldID == BLOCK_ID_ICE) iconIndex = 7.0f;
                else if (heldID == BLOCK_ID_BEDROCK) iconIndex = 8.0f;
                else if (heldID == BLOCK_ID_TORCH) iconIndex = 9.0f;
                else if (heldID == BLOCK_ID_LOG) iconIndex = 10.0f;
                else if (heldID == BLOCK_ID_LEAVES) iconIndex = 11.0f;
                else if (heldID == ITEM_ID_PICKAXE) iconIndex = 12.0f; // Np. 13. ikona
                else if (heldID == BLOCK_ID_COAL_ORE)    iconIndex = 13.0f;
                else if (heldID == BLOCK_ID_IRON_ORE)    iconIndex = 14.0f;
                else if (heldID == BLOCK_ID_GOLD_ORE)    iconIndex = 15.0f;
                else if (heldID == BLOCK_ID_DIAMOND_ORE) iconIndex = 16.0f;
                else if (heldID == BLOCK_ID_FURNACE) iconIndex = 18.0f;
                else if (heldID == ITEM_ID_COAL) iconIndex = 20.0f;
                else if (heldID == ITEM_ID_IRON_INGOT) iconIndex = 19.0f;


                uiShader.setVec2("u_uvOffset", iconIndex / ICON_ATLAS_COLS, 0.0f);

                // Rysuj (ręcznie, żeby nie psuć stanu)
                glm::mat4 model = glm::mat4(1.0f);
                model = glm::translate(model, glm::vec3(drawX, drawY, 0.0f));
                model = glm::scale(model, glm::vec3(iconSize, iconSize, 1.0f));
                uiShader.setMat4("model", model);
                glDrawArrays(GL_TRIANGLES, 0, 6);

                // Rysuj licznik (jeśli > 1)
                if (g_mouseItem.count > 1) {
                    std::string countStr = std::to_string(g_mouseItem.count);
                    RenderText(uiShader, hotbarVAO, countStr, drawX + iconSize - 10, drawY + 2, 16.0f);

                    // Reset po RenderText
                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, iconsTextureID);
                }
            }
            glDisable(GL_BLEND);
            glEnable(GL_DEPTH_TEST);
            break;
        }
        case STATE_FURNACE_MENU:
        {
            // =========================================================
            // 1. TŁO (ŚWIAT 3D - ZAMROŻONY)
            // =========================================================

            // Oblicz niezbędne zmienne (żeby świat wyglądał tak samo jak w grze)
            int playerChunkX = static_cast<int>(floor(camera.Position.x / CHUNK_WIDTH));
            int playerChunkZ = static_cast<int>(floor(camera.Position.z / CHUNK_DEPTH));
            float dayDuration = 300.0f;
            float startTimeOffset = dayDuration * 0.3f;
            float timeOfDay = fmod(glfwGetTime() + startTimeOffset, dayDuration) / dayDuration;
            float sunAngle = timeOfDay * 2.0f * 3.14159f;
            glm::vec3 sunDirection = glm::normalize(glm::vec3(sin(sunAngle), cos(sunAngle), 0.3f));
            float daylightFactor = std::clamp(-sunDirection.y, 0.0f, 1.0f);

            // Kolory
            glm::vec3 daySkyColor = glm::vec3(0.5f, 0.8f, 1.0f);
            glm::vec3 nightSkyColor = glm::vec3(0.02f, 0.02f, 0.08f);
            glm::vec3 currentSkyColor = mix(nightSkyColor, daySkyColor, daylightFactor);
            glm::vec3 sunAmbient = mix(glm::vec3(0.05f), glm::vec3(0.3f), daylightFactor);
            glm::vec3 sunDiffuse = mix(glm::vec3(0.0f), glm::vec3(0.7f), daylightFactor);

            // Macierze
            float viewDistance = (RENDER_DISTANCE + 1) * CHUNK_WIDTH;
            glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, viewDistance);
            glm::mat4 view = camera.GetViewMatrix();

            // --- RYSOWANIE TŁA ---
            // Wracamy do domyślnego bufora ekranu (omijamy Bloom dla prostoty w menu)
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);

            glClearColor(currentSkyColor.x, currentSkyColor.y, currentSkyColor.z, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            // >>> KLUCZOWA POPRAWKA: WŁĄCZ TEST GŁĘBI <<<
            glEnable(GL_DEPTH_TEST);
            glEnable(GL_CULL_FACE);
            // >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

            // A. Skybox
            if (cubemapTexture != 0) {
                glDepthFunc(GL_LEQUAL);
                skyboxShader.use();
                skyboxShader.setMat4("projection", projection);
                skyboxShader.setMat4("view", glm::mat4(glm::mat3(view)));
                skyboxShader.setVec3("u_sunDir", -sunDirection);
                skyboxShader.setFloat("u_time", (float)glfwGetTime());
                glBindVertexArray(skyboxVAO);
                glDrawArrays(GL_TRIANGLES, 0, 36);
                glBindVertexArray(0);
                glDepthFunc(GL_LESS);
            }

            // B. Świat (Solid)
            ourShader.use();
            ourShader.setInt("u_useUVTransform", 0);
            ourShader.setInt("u_isWater", 0);
            ourShader.setInt("u_multiTexture", 0);
            ourShader.setMat4("projection", projection);
            ourShader.setMat4("view", view);
            ourShader.setVec3("viewPos", camera.Position);
            ourShader.setVec3("u_fogColor", currentSkyColor);
            ourShader.setFloat("u_fogStart", (RENDER_DISTANCE - 2) * CHUNK_WIDTH);
            ourShader.setFloat("u_fogEnd", (RENDER_DISTANCE + 1) * CHUNK_WIDTH);
            ourShader.setVec3("u_dirLight.direction", sunDirection);
            ourShader.setVec3("u_dirLight.ambient", sunAmbient);
            ourShader.setVec3("u_dirLight.diffuse", sunDiffuse);
            ourShader.setVec3("u_dirLight.specular", 0.5f, 0.5f, 0.5f);
            ourShader.setInt("u_pointLightCount", 0); // Wyłączamy światła punktowe w menu dla wydajności

            // Cienie (Wymagane, żeby shader nie wariował, ale dajemy pustą mapę lub starą)
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, shadowMapTexture);
            ourShader.setMat4("lightSpaceMatrix", glm::mat4(1.0f)); // Atrapa

            // Rysuj Chunki (Solid)
            for (auto const& [pos, chunk] : g_WorldChunks) {
                // Opcjonalnie: Dodaj tu Frustum Culling jeśli masz
                chunk->DrawSolid(ourShader, textureAtlas);
            }

            // Rysuj Rośliny (Foliage)
            glDisable(GL_CULL_FACE);
            for (auto const& [pos, chunk] : g_WorldChunks) {
                chunk->DrawFoliage(ourShader, textureAtlas);
            }
            glEnable(GL_CULL_FACE);


            // =========================================================
            // 2. UI PIECA (NAKŁADKA)
            // =========================================================

            // Wyłącz głębię dla UI
            glDisable(GL_DEPTH_TEST);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            uiShader.use();
            uiShader.setMat4("projection", ortho_projection);

            // Przyciemnienie tła
            uiShader.setFloat("u_opacity", 0.7f);
            uiShader.setVec2("u_uvScale", 1.0f, 1.0f);
            uiShader.setVec2("u_uvOffset", 0.0f, 0.0f);
            DrawMenuButton(uiShader, hotbarVAO, texOverlay, 0, 0, SCR_WIDTH, SCR_HEIGHT);

            uiShader.setFloat("u_opacity", 1.0f);

            // --- RYSOWANIE EKWIPUNKU GRACZA (Dół) ---
            // (Skopiuj tu pętlę rysowania 36 slotów z STATE_INVENTORY)
            // ... Pamiętaj, żeby skopiować też logikę Drag&Drop! ...


            // --- RYSOWANIE OKNA PIECA (Środek) ---
           float cx = SCR_WIDTH / 2.0f;
            float cy = SCR_HEIGHT / 2.0f + 100.0f;

            FurnaceData* furn = &g_furnaces[g_openedFurnacePos];

            // Funkcja pomocnicza do rysowania ikony w slocie (żeby nie pisać tego 3 razy)
            auto drawFurnaceItem = [&](ItemStack& stack, float slotX, float slotY) {
                if (stack.itemID == BLOCK_ID_AIR) return;

                float iconSize = 52.0f;
                float iconX = slotX + (64.0f - iconSize) / 2.0f;
                float iconY = slotY + (64.0f - iconSize) / 2.0f;

                // Mapowanie ID na Ikonę (Skopiowane z ekwipunku)
                float iconIndex = 0.0f;
                if (stack.itemID == BLOCK_ID_GRASS) iconIndex = 0.0f;
                else if (stack.itemID == BLOCK_ID_DIRT) iconIndex = 1.0f;
                else if (stack.itemID == BLOCK_ID_STONE) iconIndex = 2.0f;
                else if (stack.itemID == BLOCK_ID_WATER) iconIndex = 3.0f;
                else if (stack.itemID == BLOCK_ID_BEDROCK) iconIndex = 4.0f;
                else if (stack.itemID == BLOCK_ID_SAND ) iconIndex = 5.0f;
                else if (stack.itemID == BLOCK_ID_SANDSTONE) iconIndex = 6.0f;
                else if (stack.itemID == BLOCK_ID_SNOW) iconIndex = 7.0f;
                else if (stack.itemID == BLOCK_ID_ICE) iconIndex = 8.0f;
                else if (stack.itemID == BLOCK_ID_TORCH) iconIndex = 9.0f;
                else if (stack.itemID == BLOCK_ID_LOG) iconIndex = 10.0f;
                else if (stack.itemID == BLOCK_ID_LEAVES) iconIndex = 11.0f;
                else if (stack.itemID == ITEM_ID_PICKAXE) iconIndex = 12.0f;
                else if (stack.itemID == BLOCK_ID_COAL_ORE)    iconIndex = 13.0f;
                else if (stack.itemID == BLOCK_ID_IRON_ORE)    iconIndex = 14.0f;
                else if (stack.itemID == BLOCK_ID_GOLD_ORE)    iconIndex = 15.0f;
                else if (stack.itemID == BLOCK_ID_DIAMOND_ORE) iconIndex = 16.0f;
                // Dodaj nowe itemy:
                else if (stack.itemID == ITEM_ID_IRON_INGOT)   iconIndex = 19.0f; // Zakładam, że masz to w atlasie!
                else if (stack.itemID == BLOCK_ID_FURNACE) iconIndex = 18.0f;
                else if (stack.itemID == ITEM_ID_COAL) iconIndex = 20.0f;
                else if (stack.itemID == ITEM_ID_IRON_INGOT) iconIndex = 19.0f;


                // Rysowanie
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, iconsTextureID);
                uiShader.setVec2("u_uvScale", 1.0f / ICON_ATLAS_COLS, 1.0f / ICON_ATLAS_ROWS);
                uiShader.setVec2("u_uvOffset", iconIndex / ICON_ATLAS_COLS, 0.0f);

                glm::mat4 model = glm::mat4(1.0f);
                model = glm::translate(model, glm::vec3(iconX, iconY, 0.0f));
                model = glm::scale(model, glm::vec3(iconSize, iconSize, 1.0f));
                uiShader.setMat4("model", model);
                glDrawArrays(GL_TRIANGLES, 0, 6);

                // Licznik
                if (stack.count > 1) {
                    std::string c = std::to_string(stack.count);
                    float FONT_SIZE = 16.0f;
                    float textX = iconX + iconSize - (c.length() * FONT_SIZE * 0.6f) - 2.0f;
                    float textY = iconY + 2.0f;
                    RenderText(uiShader, hotbarVAO, c, textX, textY, FONT_SIZE);

                    // Reset po tekście
                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, iconsTextureID);
                }
            };

            // 1. Slot INPUT (Góra)
            // Najpierw tło (reset UV do 1,1 dla tła)
            uiShader.setVec2("u_uvScale", 1.0f, 1.0f);
            uiShader.setVec2("u_uvOffset", 0.0f, 0.0f);
            DrawMenuButton(uiShader, hotbarVAO, slotTextureID, cx - 32, cy + 60, 64, 64);
            // Potem ikona
            drawFurnaceItem(furn->input, cx - 32, cy + 60);

            // 2. Slot FUEL (Dół)
            uiShader.setVec2("u_uvScale", 1.0f, 1.0f);
            uiShader.setVec2("u_uvOffset", 0.0f, 0.0f);
            DrawMenuButton(uiShader, hotbarVAO, slotTextureID, cx - 32, cy - 60, 64, 64);
            drawFurnaceItem(furn->fuel, cx - 32, cy - 60);

            // 3. Slot RESULT (Prawo)
            uiShader.setVec2("u_uvScale", 1.0f, 1.0f);
            uiShader.setVec2("u_uvOffset", 0.0f, 0.0f);
            DrawMenuButton(uiShader, hotbarVAO, slotTextureID, cx + 80, cy, 64, 64);
            drawFurnaceItem(furn->result, cx + 80, cy);

            // 4. Pasek Ognia (Między Input a Fuel)
            // 4. Pasek Ognia (Między Input a Fuel)
            float firePercent = 0.0f;
            if (furn->maxBurnTime > 0.0f) firePercent = furn->burnTime / furn->maxBurnTime;



            // 5. Pasek Strzałki (Postęp)
            float arrowPercent = furn->cookTime / 3.0f; // Zakładamy 3 sekundy na przetopienie
            if (arrowPercent > 0.0f) {
                float arrowW = 50.0f * arrowPercent;
                // Rysujemy biały pasek (używając texSun jako białego prostokąta)
                DrawMenuButton(uiShader, hotbarVAO, texSun, cx + 10, cy + 20, arrowW, 10);
            }
            uiShader.use();
            uiShader.setMat4("projection", ortho_projection);
            uiShader.setFloat("u_opacity", 1.0f);

            // Definicje siatki (3 wiersze po 9 slotów)
            float invSlotSize = SLOT_SIZE;
            float invSlotGap = 4.0f;
            float invWidth = (invSlotSize * 9) + (invSlotGap * 8);
            float invStartX = (SCR_WIDTH - invWidth) / 2.0f;
            float invStartY = (SCR_HEIGHT / 2.0f) - 50.0f;

            // Pętla rysująca 27 slotów "plecaka"
            for (int i = 0; i < 27; i++) {
                int row = i / 9;
                int col = i % 9;
                int slotIndex = i + 9;

                float x_pos = invStartX + col * (invSlotSize + invSlotGap);
                float y_pos = invStartY - row * (invSlotSize + invSlotGap);

                // Rysuj tło slotu
                uiShader.setVec2("u_uvScale", 1.0f, 1.0f);
                uiShader.setVec2("u_uvOffset", 0.0f, 0.0f);
                DrawMenuButton(uiShader, hotbarVAO, slotTextureID, x_pos, y_pos, invSlotSize, invSlotSize);

                // Rysuj ikonę przedmiotu (jeśli jest)
                uint8_t itemID = g_inventory[slotIndex].itemID;
                if (itemID != BLOCK_ID_AIR) {
                    float iconSize = 52.0f;
                    float iconX = x_pos + (invSlotSize - iconSize) / 2.0f;
                    float iconY = y_pos + (invSlotSize - iconSize) / 2.0f;

                    float iconIndex = 0.0f;
                    if (itemID == BLOCK_ID_GRASS) iconIndex = 0.0f;
                    else if (itemID == BLOCK_ID_DIRT) iconIndex = 1.0f;
                    else if (itemID == BLOCK_ID_STONE) iconIndex = 2.0f;
                    else if (itemID == BLOCK_ID_WATER) iconIndex = 3.0f;
                    else if (itemID == BLOCK_ID_SAND ) iconIndex = 4.0f;
                    else if (itemID == BLOCK_ID_SANDSTONE) iconIndex = 5.0f;
                    else if (itemID == BLOCK_ID_SNOW) iconIndex = 6.0f;
                    else if (itemID == BLOCK_ID_ICE) iconIndex = 7.0f;
                    else if (itemID == BLOCK_ID_BEDROCK) iconIndex = 8.0f;
                    else if (itemID == BLOCK_ID_TORCH) iconIndex = 9.0f;
                    else if (itemID == BLOCK_ID_LOG) iconIndex = 10.0f;
                    else if (itemID == BLOCK_ID_LEAVES) iconIndex = 11.0f;
                    else if (itemID == ITEM_ID_PICKAXE) iconIndex = 12.0f; // Np. 13. ikona
                    else if (itemID == BLOCK_ID_COAL_ORE)    iconIndex = 13.0f;
                    else if (itemID == BLOCK_ID_IRON_ORE)    iconIndex = 14.0f;
                    else if (itemID == BLOCK_ID_GOLD_ORE)    iconIndex = 15.0f;
                    else if (itemID == BLOCK_ID_DIAMOND_ORE) iconIndex = 16.0f;
                    else if (itemID == BLOCK_ID_FURNACE) iconIndex = 18.0f;
                    else if (itemID == ITEM_ID_COAL) iconIndex = 20.0f;
                    else if (itemID == ITEM_ID_IRON_INGOT) iconIndex = 19.0f;


                    glm::vec2 uvScale(1.0f / ICON_ATLAS_COLS, 1.0f / ICON_ATLAS_ROWS);
                    glm::vec2 uvOffset(iconIndex / ICON_ATLAS_COLS, 0.0f);
                    uiShader.setVec2("u_uvScale", uvScale);
                    uiShader.setVec2("u_uvOffset", uvOffset);

                    // --- POPRAWKA: Ręczne rysowanie zamiast DrawMenuButton ---
                    // DrawMenuButton resetuje teksturę, psując RenderText
                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, iconsTextureID);
                    glm::mat4 model = glm::mat4(1.0f);
                    model = glm::translate(model, glm::vec3(iconX, iconY, 0.0f));
                    model = glm::scale(model, glm::vec3(iconSize, iconSize, 1.0f));
                    uiShader.setMat4("model", model);
                    glDrawArrays(GL_TRIANGLES, 0, 6);
                    // --- KONIEC POPRAWKI ---

                    // Rysuj licznik
                    if (g_inventory[slotIndex].count > 1) {
                        std::string countStr = std::to_string(g_inventory[slotIndex].count);
                        float FONT_SIZE = 16.0f;
                        float textX = iconX + iconSize - (countStr.length() * FONT_SIZE * 0.6f) - 2.0f;
                        float textY = iconY + 2.0f;
                        RenderText(uiShader, hotbarVAO, countStr, textX, textY, FONT_SIZE);

                        // --- POPRAWKA: Zresetuj stan dla następnej iteracji pętli ---
                        // (RenderText zmienia teksturę i skalę UV)
                        glActiveTexture(GL_TEXTURE0);
                        glBindTexture(GL_TEXTURE_2D, slotTextureID); // Przygotuj dla DrawMenuButton
                        uiShader.setVec2("u_uvScale", 1.0f, 1.0f); // DrawMenuButton oczekuje (1,1)
                        // --- KONIEC POPRAWKI ---
                    }
                }
            }
            // --- Myszka z przedmiotem (Drag&Drop) ---
            {
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, slotTextureID);
                uiShader.setInt("uiTexture", 0);
                uiShader.setVec2("u_uvScale", 1.0f, 1.0f);
                uiShader.setVec2("u_uvOffset", 0.0f, 0.0f);

                for (int i = 0; i < HOTBAR_SIZE; i++) {
                    float x_pos = BAR_START_X + i * (SLOT_SIZE + GAP);
                    float y_pos = 10.0f;
                    DrawMenuButton(uiShader, hotbarVAO, slotTextureID, x_pos, y_pos, SLOT_SIZE, SLOT_SIZE);
                }

                float ICON_SIZE = 52.0f;
                glm::vec2 uvScale(1.0f / ICON_ATLAS_COLS, 1.0f / ICON_ATLAS_ROWS);
                uiShader.setVec2("u_uvScale", uvScale);

                for (int i = 0; i < HOTBAR_SIZE; i++) {
                    float x_pos = BAR_START_X + i * (SLOT_SIZE + GAP) + (SLOT_SIZE - ICON_SIZE) / 2.0f;
                    float y_pos = 10.0f + (SLOT_SIZE - ICON_SIZE) / 2.0f;

                    uint8_t blockID = g_inventory[i].itemID;
                    float iconIndex = 0.0f;
                    if (blockID == BLOCK_ID_GRASS) iconIndex = 0.0f;
                    else if (blockID == BLOCK_ID_DIRT) iconIndex = 1.0f;
                    else if (blockID == BLOCK_ID_STONE) iconIndex = 2.0f;
                    else if (blockID == BLOCK_ID_WATER) iconIndex = 3.0f;
                    else if (blockID == BLOCK_ID_SAND ) iconIndex = 4.0f;
                    else if (blockID == BLOCK_ID_SANDSTONE) iconIndex = 5.0f;
                    else if (blockID == BLOCK_ID_SNOW) iconIndex = 6.0f;
                    else if (blockID == BLOCK_ID_ICE) iconIndex = 7.0f;
                    else if (blockID == BLOCK_ID_BEDROCK) iconIndex = 8.0f;
                    else if (blockID == BLOCK_ID_TORCH) iconIndex = 9.0f;
                    else if (blockID == BLOCK_ID_LOG) iconIndex = 10.0f;
                    else if (blockID == BLOCK_ID_LEAVES) iconIndex = 11.0f;
                    else if (blockID == ITEM_ID_PICKAXE) iconIndex = 12.0f; // Np. 13. ikona
                    else if (blockID == BLOCK_ID_COAL_ORE)    iconIndex = 13.0f;
                    else if (blockID == BLOCK_ID_IRON_ORE)    iconIndex = 14.0f;
                    else if (blockID == BLOCK_ID_GOLD_ORE)    iconIndex = 15.0f;
                    else if (blockID == BLOCK_ID_DIAMOND_ORE) iconIndex = 16.0f;
                    else if (blockID == BLOCK_ID_FURNACE) iconIndex = 18.0f;
                    else if (blockID == ITEM_ID_COAL) iconIndex = 20.0f;
                    else if (blockID == ITEM_ID_IRON_INGOT) iconIndex = 19.0f;


                    glm::vec2 uvOffset(iconIndex / ICON_ATLAS_COLS, 0.0f);
                    uiShader.setVec2("u_uvOffset", uvOffset);

                    // Ręczne rysowanie ikony
                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, iconsTextureID);
                    glm::mat4 model = glm::mat4(1.0f);
                    model = glm::translate(model, glm::vec3(x_pos, y_pos, 0.0f));
                    model = glm::scale(model, glm::vec3(ICON_SIZE, ICON_SIZE, 1.0f));
                    uiShader.setMat4("model", model);

                    if (blockID != BLOCK_ID_AIR) {
                        glDrawArrays(GL_TRIANGLES, 0, 6);
                    }
                    if (g_inventory[i].maxDurability > 0) {
                        DrawDurabilityBar(uiShader, hotbarVAO, x_pos, y_pos, SLOT_SIZE, SLOT_SIZE,
                                          g_inventory[i].durability, g_inventory[i].maxDurability);
                    }
                    else if (g_inventory[i].count > 1) {
                        std::string countStr = std::to_string(g_inventory[i].count);
                        float FONT_SIZE = 16.0f;
                        float textX = x_pos + ICON_SIZE - (countStr.length() * FONT_SIZE * 0.6f) - 2.0f;
                        float textY = y_pos + 2.0f;

                        RenderText(uiShader, hotbarVAO, countStr, textX, textY, FONT_SIZE);

                        // Resetuj stan dla następnej iteracji
                        glActiveTexture(GL_TEXTURE0);
                        glBindTexture(GL_TEXTURE_2D, iconsTextureID);
                        uiShader.setVec2("u_uvScale", 1.0f / ICON_ATLAS_COLS, 1.0f / ICON_ATLAS_ROWS);
                    }
                }

                // Rysuj selektor
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, selectorTextureID);
                uiShader.setVec2("u_uvScale", 1.0f, 1.0f);
                uiShader.setVec2("u_uvOffset", 0.0f, 0.0f);
                float selector_x_pos = BAR_START_X + g_activeSlot * (SLOT_SIZE + GAP) - (SELECTOR_SIZE - SLOT_SIZE) / 2.0f;
                float selector_y_pos = 10.0f - (SELECTOR_SIZE - SLOT_SIZE) / 2.0f;
                glm::mat4 model = glm::mat4(1.0f);
                model = glm::translate(model, glm::vec3(selector_x_pos, selector_y_pos, 0.0f));
                model = glm::scale(model, glm::vec3(SELECTOR_SIZE, SELECTOR_SIZE, 1.0f));
                uiShader.setMat4("model", model);
                glDrawArrays(GL_TRIANGLES, 0, 6);
            }
            if (g_mouseItem.itemID != BLOCK_ID_AIR)
            {
                double mx, my;
                glfwGetCursorPos(window, &mx, &my);

                // Konwertuj współrzędne myszy na UI (od dołu)
                float mouseX = static_cast<float>(mx);
                float mouseY = SCR_HEIGHT - static_cast<float>(my);

                // Wyśrodkuj ikonę na kursorze
                float iconSize = 52.0f;
                float drawX = mouseX - iconSize / 2.0f;
                float drawY = mouseY - iconSize / 2.0f;

                // Ustaw teksturę ikon
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, iconsTextureID);
                uiShader.setVec2("u_uvScale", 1.0f / ICON_ATLAS_COLS, 1.0f / ICON_ATLAS_ROWS);

                // Znajdź UV dla przedmiotu
                uint8_t heldID = g_mouseItem.itemID;
                float iconIndex = 0.0f;
                if (heldID == BLOCK_ID_GRASS) iconIndex = 0.0f;
                else if (heldID == BLOCK_ID_DIRT) iconIndex = 1.0f;
                // ... (dodaj mapowania, lub lepiej: zrób funkcję pomocniczą GetIconIndex(id)) ...
                // Na razie skopiuj logikę mapowania stąd, co masz wyżej w pętlach:
                else if (heldID == BLOCK_ID_STONE) iconIndex = 2.0f;
                else if (heldID == BLOCK_ID_WATER) iconIndex = 3.0f;
                else if (heldID == BLOCK_ID_SAND ) iconIndex = 4.0f;
                else if (heldID == BLOCK_ID_SANDSTONE) iconIndex = 5.0f;
                else if (heldID == BLOCK_ID_SNOW) iconIndex = 6.0f;
                else if (heldID == BLOCK_ID_ICE) iconIndex = 7.0f;
                else if (heldID == BLOCK_ID_BEDROCK) iconIndex = 8.0f;
                else if (heldID == BLOCK_ID_TORCH) iconIndex = 9.0f;
                else if (heldID == BLOCK_ID_LOG) iconIndex = 10.0f;
                else if (heldID == BLOCK_ID_LEAVES) iconIndex = 11.0f;
                else if (heldID == ITEM_ID_PICKAXE) iconIndex = 12.0f; // Np. 13. ikona
                else if (heldID == BLOCK_ID_COAL_ORE)    iconIndex = 13.0f;
                else if (heldID == BLOCK_ID_IRON_ORE)    iconIndex = 14.0f;
                else if (heldID == BLOCK_ID_GOLD_ORE)    iconIndex = 15.0f;
                else if (heldID == BLOCK_ID_DIAMOND_ORE) iconIndex = 16.0f;
                else if (heldID == BLOCK_ID_FURNACE) iconIndex = 18.0f;
                else if (heldID == ITEM_ID_COAL) iconIndex = 20.0f;
                else if (heldID == ITEM_ID_IRON_INGOT) iconIndex = 19.0f;


                uiShader.setVec2("u_uvOffset", iconIndex / ICON_ATLAS_COLS, 0.0f);

                // Rysuj (ręcznie, żeby nie psuć stanu)
                glm::mat4 model = glm::mat4(1.0f);
                model = glm::translate(model, glm::vec3(drawX, drawY, 0.0f));
                model = glm::scale(model, glm::vec3(iconSize, iconSize, 1.0f));
                uiShader.setMat4("model", model);
                glDrawArrays(GL_TRIANGLES, 0, 6);

                // Rysuj licznik (jeśli > 1)
                if (g_mouseItem.count > 1) {
                    std::string countStr = std::to_string(g_mouseItem.count);
                    RenderText(uiShader, hotbarVAO, countStr, drawX + iconSize - 10, drawY + 2, 16.0f);

                    // Reset po RenderText
                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, iconsTextureID);
                }
            }

            // 3. LOGIKA PIECA
            // (Ważne: UpdateFurnaces musi być wywoływane także tutaj, żeby piec działał jak na niego patrzysz)
            UpdateFurnaces(deltaTime);

            break;
        }
        case STATE_PAUSE_MENU:
        {
            // --- KROK 1: NARYSUJ CAŁY ŚWIAT (TAK JAKBY GRA TRWAŁA) ---
            // (Pełny kod renderowania, skopiowany z STATE_IN_GAME)
            {
                // --- NOWA POPRAWKA: Zdefiniuj pozycję gracza ---
                int playerChunkX = static_cast<int>(floor(camera.Position.x / CHUNK_WIDTH));
                int playerChunkZ = static_cast<int>(floor(camera.Position.z / CHUNK_DEPTH));
                // --- KONIEC POPRAWKI ---

                // --- Obliczenia Cyklu Dnia/Nocy ---
                float dayDuration = 300.0f;
                float startTimeOffset = dayDuration * 0.3f;
                float timeOfDay = fmod(glfwGetTime() + startTimeOffset, dayDuration) / dayDuration;
                float sunAngle = timeOfDay * 2.0f * 3.14159f;
                glm::vec3 sunDirection = glm::normalize(glm::vec3(sin(sunAngle), cos(sunAngle), 0.3f));
                float daylightFactor = std::clamp(-sunDirection.y, 0.0f, 1.0f);
                glm::vec3 daySkyColor = glm::vec3(0.5f, 0.8f, 1.0f);
                glm::vec3 nightSkyColor = glm::vec3(0.02f, 0.02f, 0.08f);
                glm::vec3 currentSkyColor = mix(nightSkyColor, daySkyColor, daylightFactor);
                glm::vec3 sunAmbient = mix(glm::vec3(0.05f), glm::vec3(0.3f), daylightFactor);
                glm::vec3 sunDiffuse = mix(glm::vec3(0.0f), glm::vec3(0.7f), daylightFactor);

                glClearColor(currentSkyColor.x, currentSkyColor.y, currentSkyColor.z, 1.0f);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

                // --- Oblicz Macierze ---
                float viewDistance = (RENDER_DISTANCE + 1) * CHUNK_WIDTH;
                glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, viewDistance);
                glm::mat4 view = camera.GetViewMatrix();

                // --- PRZEBIEG 1: RENDEROWANIE MAPY CIENIA ---
                {
                    float shadowRange = 180.0f;
                    glm::mat4 lightProjection = glm::ortho(-shadowRange, shadowRange, -shadowRange, shadowRange, 1.0f, 150.0f);
                    glm::vec3 lightCamPos = camera.Position - (sunDirection * 50.0f);
                    glm::mat4 lightView = glm::lookAt(lightCamPos, camera.Position, glm::vec3(0.0, 1.0, 0.0));
                    glm::mat4 lightSpaceMatrix = lightProjection * lightView;

                    depthShader.use();
                    depthShader.setMat4("lightSpaceMatrix", lightSpaceMatrix);

                    glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
                    glBindFramebuffer(GL_FRAMEBUFFER, shadowMapFBO);
                    glClear(GL_DEPTH_BUFFER_BIT);
                    glCullFace(GL_FRONT);

                    for (int x = playerChunkX - RENDER_DISTANCE; x <= playerChunkX + RENDER_DISTANCE; x++) {
                        for (int z = playerChunkZ - RENDER_DISTANCE; z <= playerChunkZ + RENDER_DISTANCE; z++) {
                            auto it = g_WorldChunks.find(glm::ivec2(x, z));
                            if (it != g_WorldChunks.end()) {
                                it->second->DrawSolid(depthShader, textureAtlas);
                            }
                        }
                    }
                    glBindFramebuffer(GL_FRAMEBUFFER, 0);
                    glCullFace(GL_BACK);
                }

                // --- PRZEBIEG 2: NORMALNE RENDEROWANIE ---
                glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);

                // --- PRZEBIEG 2A: SKYBOX ---
                if (cubemapTexture != 0)
                {
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

                // --- PRZEBIEG 2B: ŚWIAT (SOLID) ---
                ourShader.use();
                ourShader.setInt("u_useUVTransform", 0);
                ourShader.setInt("u_isWater", 0);
                ourShader.setInt("u_multiTexture", 0);
                ourShader.setMat4("projection", projection);
                ourShader.setMat4("view", view);
                ourShader.setVec3("viewPos", camera.Position);
                ourShader.setVec3("u_fogColor", currentSkyColor);
                ourShader.setFloat("u_fogStart", (RENDER_DISTANCE - 2) * CHUNK_WIDTH);
                ourShader.setFloat("u_fogEnd", (RENDER_DISTANCE + 1) * CHUNK_WIDTH);
                ourShader.setVec3("u_dirLight.direction", sunDirection);
                ourShader.setVec3("u_dirLight.ambient", sunAmbient);
                ourShader.setVec3("u_dirLight.diffuse", sunDiffuse);
                ourShader.setVec3("u_dirLight.specular", 0.5f, 0.5f, 0.5f);

                int lightCount = 0;
                for (const auto& torchPos : g_torchPositions)
                {
                    if (lightCount >= MAX_POINT_LIGHTS) break;
                    std::string base = "u_pointLights[" + std::to_string(lightCount) + "]";
                    ourShader.setVec3(base + ".position", torchPos);
                    ourShader.setVec3(base + ".ambient", 0.05f, 0.02f, 0.0f);
                    ourShader.setVec3(base + ".diffuse", 2.0f, 1.2f, 0.4f);
                    ourShader.setVec3(base + ".specular", 2.0f, 1.2f, 0.4f);
                    ourShader.setFloat(base + ".constant", 1.0f);
                    ourShader.setFloat(base + ".linear", 0.06f);
                    ourShader.setFloat(base + ".quadratic", 0.015f);
                    lightCount++;
                }
                ourShader.setInt("u_pointLightCount", lightCount);

                float shadowRange = 180.0f;
                glm::mat4 lightProjection = glm::ortho(-shadowRange, shadowRange, -shadowRange, shadowRange, 1.0f, 150.0f);
                glm::vec3 lightCamPos = camera.Position - (sunDirection * 50.0f);
                glm::mat4 lightView = glm::lookAt(lightCamPos, camera.Position, glm::vec3(0.0, 1.0, 0.0));
                glm::mat4 lightSpaceMatrix = lightProjection * lightView;
                ourShader.setMat4("lightSpaceMatrix", lightSpaceMatrix);

                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_2D, shadowMapTexture);

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

                // --- PRZEBIEG 2C: LIŚCIE I POCHODNIE ---
                ourShader.use();
                glDepthMask(GL_TRUE);
                glDisable(GL_CULL_FACE);
                for (int x = playerChunkX - RENDER_DISTANCE; x <= playerChunkX + RENDER_DISTANCE; x++) {
                    for (int z = playerChunkZ - RENDER_DISTANCE; z <= playerChunkZ + RENDER_DISTANCE; z++) {
                        auto it = g_WorldChunks.find(glm::ivec2(x, z));
                        if (it != g_WorldChunks.end()) {
                            it->second->DrawFoliage(ourShader, textureAtlas);
                        }
                    }
                }

                // (Nie rysujemy podświetlenia w menu pauzy)

                // --- PRZEBIEG 2E: WODA (SORTOWANA) ---
                ourShader.use();
                glDepthMask(GL_FALSE);
                glEnable(GL_BLEND);
                // glDisable(GL_CULL_FACE); // Już wyłączone z przebiegu 2C

                std::map<float, Chunk*, std::greater<float>> sortedChunks;
                for (int x = playerChunkX - RENDER_DISTANCE; x <= playerChunkX + RENDER_DISTANCE; x++) {
                    for (int z = playerChunkZ - RENDER_DISTANCE; z <= playerChunkZ + RENDER_DISTANCE; z++) {
                        auto it = g_WorldChunks.find(glm::ivec2(x, z));
                        if (it != g_WorldChunks.end()) {
                            Chunk* chunk = it->second;
                            glm::vec3 chunkCenter = chunk->position + glm::vec3(CHUNK_WIDTH / 2.0f, CHUNK_HEIGHT / 2.0f, CHUNK_DEPTH / 2.0f);
                            glm::vec3 vecToChunk = camera.Position - chunkCenter;
                            float distanceSq = glm::dot(vecToChunk, vecToChunk);
                            sortedChunks[distanceSq] = chunk;
                        }
                    }
                }
                for (auto const& [dist, chunk] : sortedChunks)
                {
                    chunk->DrawTransparent(ourShader, textureAtlas);
                }

                glEnable(GL_CULL_FACE);
                glDepthMask(GL_TRUE);

                // --- KROK 1.5: Rysuj Hotbar (w tle menu pauzy) ---
                // (Ten kod jest skopiowany z 'STATE_IN_GAME', aby hotbar był widoczny)
                {
                    glDisable(GL_DEPTH_TEST);
                    glDisable(GL_CULL_FACE);
                    uiShader.use();
                    uiShader.setMat4("projection", ortho_projection);
                    uiShader.setFloat("u_opacity", 1.0f);
                    glBindVertexArray(hotbarVAO);

                    // 1. Tła slotów
                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, slotTextureID);
                    uiShader.setInt("uiTexture", 0);
                    uiShader.setVec2("u_uvScale", 1.0f, 1.0f);
                    uiShader.setVec2("u_uvOffset", 0.0f, 0.0f);
                    for (int i = 0; i < HOTBAR_SIZE; i++) {
                        float x_pos = BAR_START_X + i * (SLOT_SIZE + GAP);
                        float y_pos = 10.0f;
                        DrawMenuButton(uiShader, hotbarVAO, slotTextureID, x_pos, y_pos, SLOT_SIZE, SLOT_SIZE);
                    }

                    // 2. Ikony
                    float ICON_SIZE = 52.0f;
                    glm::vec2 uvScale(1.0f / ICON_ATLAS_COLS, 1.0f / ICON_ATLAS_ROWS);
                    uiShader.setVec2("u_uvScale", uvScale);
                    for (int i = 0; i < HOTBAR_SIZE; i++) {
                        float x_pos = BAR_START_X + i * (SLOT_SIZE + GAP) + (ICON_SIZE - SLOT_SIZE) / 2.0f;
                        float y_pos = 10.0f + (ICON_SIZE - SLOT_SIZE) / 2.0f;
                        glm::mat4 model = glm::mat4(1.0f);
                        model = glm::translate(model, glm::vec3(x_pos, y_pos, 0.0f));
                        model = glm::scale(model, glm::vec3(ICON_SIZE, ICON_SIZE, 1.0f));
                        uiShader.setMat4("model", model);

                        uint8_t blockID = g_inventory[i].itemID;
                        float iconIndex = 0.0f;
                        if (blockID == BLOCK_ID_GRASS) iconIndex = 0.0f;
                        else if (blockID == BLOCK_ID_DIRT) iconIndex = 1.0f;
                        else if (blockID == BLOCK_ID_STONE) iconIndex = 2.0f;
                        else if (blockID == BLOCK_ID_WATER) iconIndex = 3.0f;
                        else if (blockID == BLOCK_ID_SAND ) iconIndex = 5.0f;
                        else if (blockID == BLOCK_ID_SANDSTONE) iconIndex = 6.0f;
                        else if (blockID == BLOCK_ID_SNOW) iconIndex = 7.0f;
                        else if (blockID == BLOCK_ID_ICE) iconIndex = 8.0f;
                        else if (blockID == BLOCK_ID_BEDROCK) iconIndex = 4.0f;
                        else if (blockID == BLOCK_ID_TORCH) iconIndex = 9.0f;
                        else if (blockID == BLOCK_ID_LOG) iconIndex = 10.0f;
                        else if (blockID == BLOCK_ID_LEAVES) iconIndex = 11.0f;
                        else if (blockID == ITEM_ID_PICKAXE) iconIndex = 12.0f; // Np. 13. ikona
                        else if (blockID == BLOCK_ID_COAL_ORE)    iconIndex = 13.0f;
                        else if (blockID == BLOCK_ID_IRON_ORE)    iconIndex = 14.0f;
                        else if (blockID == BLOCK_ID_GOLD_ORE)    iconIndex = 15.0f;
                        else if (blockID == BLOCK_ID_DIAMOND_ORE) iconIndex = 16.0f;
                        else if (blockID == BLOCK_ID_FURNACE) iconIndex = 18.0f;
                        else if (blockID == ITEM_ID_COAL) iconIndex = 20.0f;
                        else if (blockID == ITEM_ID_IRON_INGOT) iconIndex = 19.0f;


                        glm::vec2 uvOffset(iconIndex / ICON_ATLAS_COLS, 0.0f);
                        uiShader.setVec2("u_uvOffset", uvOffset);

                        glBindTexture(GL_TEXTURE_2D, iconsTextureID);
                        if (blockID != BLOCK_ID_AIR) {
                            glDrawArrays(GL_TRIANGLES, 0, 6);
                        }
                        if (g_inventory[i].maxDurability > 0) {
                            DrawDurabilityBar(uiShader, hotbarVAO, x_pos, y_pos, SLOT_SIZE, SLOT_SIZE,
                                              g_inventory[i].durability, g_inventory[i].maxDurability);
                        }
                        else if (g_inventory[i].count > 1) {
                            std::string countStr = std::to_string(g_inventory[i].count);
                            float FONT_SIZE = 16.0f;
                            float textX = x_pos + ICON_SIZE - (countStr.length() * FONT_SIZE * 0.6f) - 2.0f;
                            float textY = y_pos + 2.0f;
                            glEnable(GL_BLEND);
                            RenderText(uiShader, hotbarVAO, countStr, textX, textY, FONT_SIZE);
                            glDisable(GL_BLEND);
                            glActiveTexture(GL_TEXTURE0);
                            glBindTexture(GL_TEXTURE_2D, iconsTextureID);
                            uiShader.setVec2("u_uvScale", 1.0f / ICON_ATLAS_COLS, 1.0f / ICON_ATLAS_ROWS);
                        }
                    }

                    // 3. Selektor
                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, selectorTextureID);
                    uiShader.setVec2("u_uvScale", 1.0f, 1.0f);
                    uiShader.setVec2("u_uvOffset", 0.0f, 0.0f);
                    float selector_x_pos = BAR_START_X + g_activeSlot * (SLOT_SIZE + GAP) - (SELECTOR_SIZE - SLOT_SIZE) / 2.0f;
                    float selector_y_pos = 10.0f - (SELECTOR_SIZE - SLOT_SIZE) / 2.0f;
                    glm::mat4 model = glm::mat4(1.0f);
                    model = glm::translate(model, glm::vec3(selector_x_pos, selector_y_pos, 0.0f));
                    model = glm::scale(model, glm::vec3(SELECTOR_SIZE, SELECTOR_SIZE, 1.0f));
                    uiShader.setMat4("model", model);
                    glDrawArrays(GL_TRIANGLES, 0, 6);
                    glBindVertexArray(0);
                }
            } // Koniec bloku renderowania tła 3D i hotbara


            // --- KROK 2: NARYSUJ PÓŁPRZEZROCZYSTĄ NAKŁADKĘ ---
            glDisable(GL_DEPTH_TEST);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            uiShader.use();
            uiShader.setMat4("projection", ortho_projection);
            uiShader.setFloat("u_opacity", 0.77f);
            uiShader.setVec2("u_uvScale", 1.0f, 1.0f);
            uiShader.setVec2("u_uvOffset", 0.0f, 0.0f);

            DrawMenuButton(uiShader, hotbarVAO, texOverlay, 0.0f, 0.0f, SCR_WIDTH, SCR_HEIGHT);

            // --- KROK 3: NARYSUJ PRZYCISKI MENU PAUZY ---
            uiShader.setFloat("u_opacity", 1.0f);

            DrawMenuButton(uiShader, hotbarVAO, texBtnResume, BTN_X, PAUSE_BTN_Y_RESUME, BTN_W, BTN_H);
            DrawMenuButton(uiShader, hotbarVAO, texBtnSaveAndMenu, BTN_X, PAUSE_BTN_Y_MENU, BTN_W, BTN_H);
            DrawMenuButton(uiShader, hotbarVAO, texBtnQuit, BTN_X, PAUSE_BTN_Y_QUIT, BTN_W, BTN_H);

            glDisable(GL_BLEND);
            glEnable(GL_DEPTH_TEST);
            glEnable(GL_CULL_FACE);
            glDepthMask(GL_TRUE);
            break;
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
    SaveWorld();
    std::cout << "Zapisano." << std::endl;
    // --- KONIEC NOWOŚCI ---
    glDeleteProgram(ourShader.ID);
    glDeleteProgram(crosshairShader.ID); // <<< NOWOŚĆ
    glDeleteVertexArrays(1, &crosshairVAO); // <<< NOWOŚĆ
    glDeleteBuffers(1, &crosshairVBO); // <<< NOWOŚĆ
    glDeleteVertexArrays(1, &highlighterVAO); // <<< NOWOŚĆ
    glDeleteBuffers(1, &highlighterVBO);
    g_isRunning = false;
    loaderThread.join(); // Czekaj aż wątek skończy
    ma_engine_uninit(&g_audioEngine);
    glfwTerminate();
    return 0;
}

void SpawnEntity(glm::vec3 pos, EntityType type);

void processInput(GLFWwindow *window)
{
    if (isDead) return;
    static bool f_pressed = false;
    static bool esc_pressed_last_frame = false; // <<< NOWA ZMIENNA ŚLEDZĄCA
    static bool e_pressed_last_frame = false; // <<< NOWA ZMIENNA ŚLEDZĄCA

    // --- OBSŁUGA KLAWISZA ESCAPE (Działa zawsze) ---

    bool esc_is_pressed_now = (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS);

    // SPRAWDŹ, CZY KLAWISZ ZOSTAŁ WŁAŚNIE WCIŚNIĘTY (a nie był trzymany)
    if (esc_is_pressed_now && !esc_pressed_last_frame)
    {
        if (g_currentState == STATE_IN_GAME) {

            // PRZEJŚCIE DO MENU PAUZY
            g_currentState = STATE_PAUSE_MENU;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            wishDir = glm::vec3(0.0f);
            wishJump = false;
        }
        else if (g_currentState == STATE_PAUSE_MENU) {
            // WYJŚCIE Z MENU PAUZY
            g_currentState = STATE_IN_GAME;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            firstMouse = true;
        }
    }

    // Zaktualizuj stan na następną klatkę
    esc_pressed_last_frame = esc_is_pressed_now;
    static bool m_pressed = false;
    static bool z_pressed = false;
    if (glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS) { // Z jak Zombie
        if (!z_pressed) {
            SpawnEntity(camera.Position + camera.Front * 5.0f, ENTITY_ZOMBIE);
            z_pressed = true;
        }
    } else z_pressed = false;
    if (glfwGetKey(window, GLFW_KEY_M) == GLFW_PRESS) {
        if (!m_pressed) {
            glm::vec3 spawnPos = camera.Position + (camera.Front * 3.0f); // 3 metry przed graczem
            SpawnEntity(spawnPos, ENTITY_PIG);
            std::cout << "Zespawnowano Swinke!" << std::endl;
            m_pressed = true;
        }
    } else { m_pressed = false; }
    // --- KONIEC OBSŁUGI ESCAPE ---
    bool e_is_pressed_now = (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS);
    if (e_is_pressed_now && !e_pressed_last_frame)
    {
        if (g_currentState == STATE_IN_GAME) {

            g_currentState = STATE_INVENTORY;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            wishDir = glm::vec3(0.0f);
            wishJump = false;
        }
        else if (g_currentState == STATE_INVENTORY) {
            g_currentState = STATE_IN_GAME;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            firstMouse = true;
        }
        else if (g_currentState == STATE_FURNACE_MENU) {
            g_currentState = STATE_IN_GAME;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }
    }
    e_pressed_last_frame = e_is_pressed_now;

    // --- OBSŁUGA RUCHU (Działa tylko w grze) ---
    if (g_currentState != STATE_IN_GAME)
    {
        return; // Jesteśmy w menu, nie przetwarzaj ruchu
    }

    // ... (obsługa klawiszy numerycznych Hotbara bez zmian) ...
    for (int i = 0; i < HOTBAR_SIZE; i++) {
        if (glfwGetKey(window, GLFW_KEY_1 + i) == GLFW_PRESS) {
            g_activeSlot = i;
            g_selectedBlockID = g_inventory[i].itemID;
            break;
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
    static bool q_pressed = false;
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) {
        if (!q_pressed) {
            // Sprawdź czy mamy przedmiot w ręce
            if (g_inventory[g_activeSlot].itemID != BLOCK_ID_AIR) {
                // Zmniejsz ilość
                uint8_t itemID = g_inventory[g_activeSlot].itemID;
                g_inventory[g_activeSlot].count--;
                if (g_inventory[g_activeSlot].count <= 0)
                    g_inventory[g_activeSlot].itemID = BLOCK_ID_AIR;

                // Wyrzuć przed siebie
                glm::vec3 spawnPos = camera.Position + glm::vec3(0, 1.5f, 0) + (camera.Front * 0.5f);
                glm::vec3 throwVel = camera.Front * 6.0f; // Mocno do przodu
                SpawnDrop(itemID, spawnPos, throwVel);
            }
            q_pressed = true;
        }
    } else { q_pressed = false; }
    // --- Ustawianie "Życzenia" Ruchu (wishDir) ---
    wishDir = glm::vec3(0.0f);

    // --- Ruch Poziomy (zawsze taki sam) ---
    glm::vec3 front_horizontal = glm::normalize(glm::vec3(camera.Front.x, 0.0f, camera.Front.z));
    glm::vec3 right_horizontal = glm::normalize(glm::vec3(camera.Right.x, 0.0f, camera.Right.z));
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) wishDir += front_horizontal;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) wishDir -= front_horizontal;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) wishDir -= right_horizontal;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) wishDir += right_horizontal;

    // --- Ruch Poziomy (zależy od trybu) ---
    if (camera.flyingMode)
    {
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) wishDir += camera.WorldUp;
        if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) wishDir -= camera.WorldUp;
    }
    else // Chodzenie lub Pływanie
    {
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
            wishJump = true;
        } else {
            wishJump = false;
        }
        if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
            wishDir -= camera.WorldUp;
        }
    }

    if (glm::length(wishDir) > 0.1f) {
        wishDir = glm::normalize(wishDir);
    }
}
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    // Za każdym razem, gdy okno zmieni rozmiar,
    // przelicz wszystkie pozycje UI i viewport
    UpdateScreenSize(width, height);
    glViewport(0, 0, width, height);
    std::cout << "Poprawnie wykonano glViewport" << std::endl;
}

void mouse_callback(GLFWwindow* window, double xposIn, double yposIn)
{
    if (g_currentState != STATE_IN_GAME)
    {
        return;
    }
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

        uint8_t block = GetBlockGlobal(blockPos.x, blockPos.y, blockPos.z);

        // POPRAWKA: Zatrzymuj się tylko na blokach, które NIE są powietrzem I NIE są wodą
        // (Chcemy budować "wewnątrz" wody, więc musimy trafić w dno/ścianę za nią)
        if (block != BLOCK_ID_AIR && !IsAnyWater(block)) {
            out_hitBlock = blockPos;
            out_prevBlock = prevBlockPos; // To będzie pozycja WODY (jeśli celujemy przez wodę)
            return true;
        }

        prevBlockPos = blockPos;
    }
    return false;
}


void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
    if (g_currentState == STATE_IN_GAME) {
        if (isDead) return; // Nie można budować po śmierci
        // ...
    }
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
                g_inputWorldName = "New World"; // Ustaw domyślne
                g_inputSeedString = "";
                g_activeInput = 0; // Aktywuj pierwsze pole
                g_currentState = STATE_NEW_WORLD_MENU; // Przejdź do nowego menu
            }
            else if (IsMouseOver(mouseX, mouseY, BTN_X, BTN_Y_LOAD, BTN_W, BTN_H))
            {
                // --- WCZYTAJ ŚWIAT ---
                std::cout << "Otwieranie menu wczytywania swiata..." << std::endl;

                // Wyczyść starą listę i zeskanuj folder 'worlds'
                existingWorlds.clear();
                std::string worldsPath = "worlds";
                if (std::filesystem::exists(worldsPath)) {
                    for (const auto& entry : std::filesystem::directory_iterator(worldsPath)) {
                        if (entry.is_directory()) {
                            existingWorlds.push_back(entry.path().filename().string());
                        }
                    }
                }

                // Przejdź do nowego stanu (menu wyboru świata)
                g_currentState = STATE_LOAD_WORLD_MENU;
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
            // --- POPRAWNA LOGIKA DLA "START" ---
            g_selectedWorldName = g_inputWorldName;
            if (g_selectedWorldName.empty()) g_selectedWorldName = "SwiatBezNazwy";

            if (g_inputSeedString.empty()) {
                g_selectedWorldSeed = rand(); // Losowy seed
            } else {
                // Spróbuj przekonwertować string na liczbę
                try {
                    g_selectedWorldSeed = std::stoi(g_inputSeedString);
                } catch (...) {
                    g_selectedWorldSeed = static_cast<int>(std::hash<std::string>{}(g_inputSeedString));
                }
            }
            std::cout << "Rozpoczynanie swiata: " << g_selectedWorldName << " (Seed: " << g_selectedWorldSeed << ")" << std::endl;

            // Przejdź do nowego stanu (menu wyboru świata)
            g_currentState = STATE_LOADING_WORLD;
        }
        // Kliknięcie "Wstecz"
        else if (IsMouseOver(mouseX, mouseY, BTN_BACK_X, BTN_BACK_Y, BTN_W, BTN_H)) {
            g_currentState = STATE_MAIN_MENU;
        }
        return;

    }
    else if (g_currentState == STATE_LOAD_WORLD_MENU && button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
    {
        double mouseX, mouseY;
        glfwGetCursorPos(window, &mouseX, &mouseY);

        // Sprawdź kliknięcie na przycisk "Wstecz"
        if (IsMouseOver(mouseX, mouseY, LIST_ITEM_X, LIST_BACK_BTN_Y, LIST_ITEM_W, LIST_ITEM_H)) {
            g_currentState = STATE_MAIN_MENU;
            return;
        }

        // Sprawdź kliknięcie na listę światów
        float current_y = LIST_START_Y;
        for (const std::string& worldName : existingWorlds)
        {
            if (IsMouseOver(mouseX, mouseY, LIST_ITEM_X, current_y, LIST_ITEM_W, LIST_ITEM_H))
            {
                // --- ZNALEŹLIŚMY KLIKNIĘTY ŚWIAT ---
                g_selectedWorldName = worldName;

                // Spróbuj wczytać seed z pliku world.info
                g_selectedWorldSeed = 12345; // Domyślny, na wszelki wypadek
                std::string infoPath = "worlds/" + g_selectedWorldName + "/world.info";

                if (std::filesystem::exists(infoPath)) {
                    std::ifstream infoFile(infoPath);
                    if (infoFile.is_open()) {
                        infoFile >> g_selectedWorldSeed;
                        infoFile.close();
                    }
                }

                std::cout << "Ladowanie swiata: " << g_selectedWorldName << " (Seed: " << g_selectedWorldSeed << ")" << std::endl;
                g_currentState = STATE_LOADING_WORLD;
                return; // Zakończ pętlę
            }
            current_y -= LIST_GAP; // Przesuń w dół do następnej pozycji
        }
        return;
    }
    else if (g_currentState == STATE_INVENTORY && action == GLFW_PRESS)
    {
        double mouseX, mouseY;
        glfwGetCursorPos(window, &mouseX, &mouseY);
        int slotIndex = GetSlotUnderMouse(mouseX, mouseY);

        if (slotIndex != -1)
        {
            // 1. Zidentyfikuj, w który slot klikamy (wskaźnik ułatwia sprawę)
            ItemStack* targetSlot = nullptr;

            if (slotIndex == SLOT_RESULT) {
                targetSlot = &g_craftingOutput;
            } else if (slotIndex >= SLOT_CRAFT_START) {
                targetSlot = &g_craftingGrid[slotIndex - SLOT_CRAFT_START];
            } else {
                targetSlot = &g_inventory[slotIndex];
            }

            // ============================================================
            // LEWY PRZYCISK MYSZY (LPM) - Standardowe przenoszenie
            // ============================================================
            if (button == GLFW_MOUSE_BUTTON_LEFT)
            {
                // Specjalna obsługa slotu WYNIKU (Crafting)
                if (slotIndex == SLOT_RESULT)
                {
                    if (targetSlot->itemID != BLOCK_ID_AIR)
                    {
                        // Logika Shift+Klik (Craft All)
                        bool isShiftHeld = (mods & GLFW_MOD_SHIFT);

                        if (isShiftHeld) {
                            while (targetSlot->itemID != BLOCK_ID_AIR) {
                                if (!AddItemToInventory(targetSlot->itemID, targetSlot->count)) break;
                                // Zabierz surowce
                                for (int i = 0; i < 4; i++) {
                                    if (g_craftingGrid[i].itemID != BLOCK_ID_AIR) {
                                        g_craftingGrid[i].count--;
                                        if (g_craftingGrid[i].count <= 0) g_craftingGrid[i].itemID = BLOCK_ID_AIR;
                                    }
                                }
                                CheckCrafting();
                            }
                        }
                        // Zwykłe podniesienie wyniku
                        else if (g_mouseItem.itemID == BLOCK_ID_AIR) {
                            g_mouseItem = *targetSlot;
                            *targetSlot = {BLOCK_ID_AIR, 0}; // Wizualne czyszczenie

                            // Zabierz surowce
                            for (int i = 0; i < 4; i++) {
                                if (g_craftingGrid[i].itemID != BLOCK_ID_AIR) {
                                    g_craftingGrid[i].count--;
                                    if (g_craftingGrid[i].count <= 0) g_craftingGrid[i].itemID = BLOCK_ID_AIR;
                                }
                            }
                            CheckCrafting();
                        }
                        ma_engine_play_sound(&g_audioEngine, "pop.mp3", NULL);
                    }
                }
                // Obsługa zwykłych slotów (Ekwipunek i Siatka Craftingu)
                else
                {
                    // Mysz pusta -> Podnieś
                    if (g_mouseItem.itemID == BLOCK_ID_AIR) {
                        if (targetSlot->itemID != BLOCK_ID_AIR) {
                            g_mouseItem = *targetSlot;
                            *targetSlot = {BLOCK_ID_AIR, 0};
                        }
                    }
                    // Mysz pełna -> Połóż / Zamień / Dodaj
                    else {
                        if (targetSlot->itemID == BLOCK_ID_AIR) {
                            *targetSlot = g_mouseItem;
                            g_mouseItem = {BLOCK_ID_AIR, 0};
                        } else if (targetSlot->itemID == g_mouseItem.itemID) {
                            int space = MAX_STACK_SIZE - targetSlot->count;
                            int toAdd = std::min(space, g_mouseItem.count);
                            targetSlot->count += toAdd;
                            g_mouseItem.count -= toAdd;
                            if (g_mouseItem.count <= 0) g_mouseItem = {BLOCK_ID_AIR, 0};
                        } else {
                            ItemStack temp = *targetSlot;
                            *targetSlot = g_mouseItem;
                            g_mouseItem = temp;
                        }
                    }
                    CheckCrafting(); // Sprawdź receptury po zmianie
                }
            }

            // ============================================================
            // PRAWY PRZYCISK MYSZY (PPM) - Dzielenie i Stawianie po 1
            // ============================================================
            else if (button == GLFW_MOUSE_BUTTON_RIGHT)
            {
                // Ignorujemy PPM na slocie wyniku (dla uproszczenia)
                if (slotIndex == SLOT_RESULT) return;

                // PRZYPADEK 1: Trzymamy przedmiot -> Połóż JEDEN (Place One)
                if (g_mouseItem.itemID != BLOCK_ID_AIR)
                {
                    // A. Slot pusty -> połóż 1 sztukę
                    if (targetSlot->itemID == BLOCK_ID_AIR) {
                        targetSlot->itemID = g_mouseItem.itemID;
                        targetSlot->count = 1;
                        g_mouseItem.count--;
                    }
                    // B. Slot ma ten sam przedmiot -> dodaj 1 sztukę
                    else if (targetSlot->itemID == g_mouseItem.itemID) {
                        if (targetSlot->count < MAX_STACK_SIZE) {
                            targetSlot->count++;
                            g_mouseItem.count--;
                        }
                    }
                    // C. Inny przedmiot -> Zamień (tak samo jak lewym)
                    else {
                        ItemStack temp = *targetSlot;
                        *targetSlot = g_mouseItem;
                        g_mouseItem = temp;
                    }

                    // Czyszczenie myszki jeśli pusta
                    if (g_mouseItem.count <= 0) g_mouseItem = {BLOCK_ID_AIR, 0};
                }
                // PRZYPADEK 2: Mysz pusta -> Weź POŁOWĘ (Split Stack)
                else if (targetSlot->itemID != BLOCK_ID_AIR)
                {
                    // Oblicz połowę (zaokrąglając w górę dla myszki)
                    int total = targetSlot->count;
                    int toTake = (total + 1) / 2;

                    g_mouseItem.itemID = targetSlot->itemID;
                    g_mouseItem.count = toTake;

                    targetSlot->count -= toTake;
                    if (targetSlot->count <= 0) *targetSlot = {BLOCK_ID_AIR, 0};
                }

                CheckCrafting(); // Zawsze sprawdzaj receptury po kliknięciu
            }
        }
    }
    else if (g_currentState == STATE_FURNACE_MENU && action == GLFW_PRESS)
    {
        double mouseX, mouseY;
        glfwGetCursorPos(window, &mouseX, &mouseY);
        int slotIndex = GetSlotUnderMouse(mouseX, mouseY);

        if (slotIndex != -1)
        {
            // 1. Zidentyfikuj docelowy slot
            ItemStack* targetSlot = nullptr;
            FurnaceData* furn = &g_furnaces[g_openedFurnacePos]; // Aktualny piec

            if (slotIndex == SLOT_FURNACE_INPUT) targetSlot = &furn->input;
            else if (slotIndex == SLOT_FURNACE_FUEL) targetSlot = &furn->fuel;
            else if (slotIndex == SLOT_FURNACE_RESULT) targetSlot = &furn->result;
            else if (slotIndex >= 0 && slotIndex < INVENTORY_SLOTS) targetSlot = &g_inventory[slotIndex];

            if (targetSlot == nullptr) return; // Błąd

            // 2. LEWY PRZYCISK (LPM)
            if (button == GLFW_MOUSE_BUTTON_LEFT)
            {
                // Specjalna zasada: Nie można wkładać DO slotu wyniku
                if (slotIndex == SLOT_FURNACE_RESULT && g_mouseItem.itemID != BLOCK_ID_AIR) {
                    return; // Nie wkładaj nic do wyniku
                }

                // Standardowa zamiana / podnoszenie
                if (g_mouseItem.itemID == BLOCK_ID_AIR) {
                    if (targetSlot->itemID != BLOCK_ID_AIR) {
                        g_mouseItem = *targetSlot;
                        *targetSlot = {BLOCK_ID_AIR, 0};
                    }
                } else {
                    if (targetSlot->itemID == BLOCK_ID_AIR) {
                        *targetSlot = g_mouseItem;
                        g_mouseItem = {BLOCK_ID_AIR, 0};
                    } else if (targetSlot->itemID == g_mouseItem.itemID) {
                        int space = MAX_STACK_SIZE - targetSlot->count;
                        int toAdd = std::min(space, g_mouseItem.count);
                        targetSlot->count += toAdd;
                        g_mouseItem.count -= toAdd;
                        if (g_mouseItem.count <= 0) g_mouseItem = {BLOCK_ID_AIR, 0};
                    } else {
                        // Swap (tylko jeśli nie wynik)
                        if (slotIndex != SLOT_FURNACE_RESULT) {
                            ItemStack temp = *targetSlot;
                            *targetSlot = g_mouseItem;
                            g_mouseItem = temp;
                        }
                    }
                }

                // Dźwięk kliknięcia
                if(g_mouseItem.itemID != BLOCK_ID_AIR || targetSlot->itemID != BLOCK_ID_AIR)
                     ma_engine_play_sound(&g_audioEngine, "pop.mp3", NULL);
            }

            // 3. PRAWY PRZYCISK (PPM) - Place One / Split
            else if (button == GLFW_MOUSE_BUTTON_RIGHT)
            {
                if (slotIndex == SLOT_FURNACE_RESULT) return; // Ignoruj PPM na wyniku

                if (g_mouseItem.itemID != BLOCK_ID_AIR) // Place One
                {
                    if (targetSlot->itemID == BLOCK_ID_AIR) {
                        targetSlot->itemID = g_mouseItem.itemID;
                        targetSlot->count = 1;
                        g_mouseItem.count--;
                    } else if (targetSlot->itemID == g_mouseItem.itemID) {
                        if (targetSlot->count < MAX_STACK_SIZE) {
                            targetSlot->count++;
                            g_mouseItem.count--;
                        }
                    }
                    if (g_mouseItem.count <= 0) g_mouseItem = {BLOCK_ID_AIR, 0};
                }
                else if (targetSlot->itemID != BLOCK_ID_AIR) // Split
                {
                    int half = (targetSlot->count + 1) / 2;
                    g_mouseItem.itemID = targetSlot->itemID;
                    g_mouseItem.count = half;
                    targetSlot->count -= half;
                    if (targetSlot->count <= 0) *targetSlot = {BLOCK_ID_AIR, 0};
                }

                 ma_engine_play_sound(&g_audioEngine, "pop.mp3", NULL);
            }
        }
    }
    else if (g_currentState == STATE_PAUSE_MENU && button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
    {
        double mouseX, mouseY;
        glfwGetCursorPos(window, &mouseX, &mouseY);

        // 1. Przycisk "Wznów Grę"
        if (IsMouseOver(mouseX, mouseY, BTN_X, PAUSE_BTN_Y_RESUME, BTN_W, BTN_H))
        {
            g_currentState = STATE_IN_GAME;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            firstMouse = true; // Resetuj myszkę
        }
        // 2. Przycisk "Zapisz i wyjdź do Menu"
        else if (IsMouseOver(mouseX, mouseY, BTN_X, PAUSE_BTN_Y_MENU, BTN_W, BTN_H))
        {
            SaveWorld(); // Zapisz świat
            g_currentState = STATE_MAIN_MENU;
            // Kursor jest już widoczny, więc nie trzeba go zmieniać
        }
        // 3. Przycisk "Wyjdź z Gry"
        else if (IsMouseOver(mouseX, mouseY, BTN_X, PAUSE_BTN_Y_QUIT, BTN_W, BTN_H))
        {
            SaveWorld(); // Zapisz świat
            g_currentState = STATE_EXITING;
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

    if (button == GLFW_MOUSE_BUTTON_LEFT)
    {
        if (action == GLFW_PRESS) {
            g_isMining = true;       // Zacznij kopać
            g_miningTimer = 0.0f;    // Resetuj timer
            if(hit) g_currentMiningPos = hitBlockPos; // Zapisz cel
        }
        else if (action == GLFW_RELEASE) {
            g_isMining = false;      // PRZESTAŃ kopać (To teraz zadziała!)
            g_miningTimer = 0.0f;
        }
    }

    // 2. Obsługa PRAWEGO przycisku (Stawianie - TYLKO PRESS)
    else if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS)
    {
        if (hit)
        {
            uint8_t clickedBlock = GetBlockGlobal(hitBlockPos.x, hitBlockPos.y, hitBlockPos.z);
            if (clickedBlock == BLOCK_ID_FURNACE && !glfwGetKey(window, GLFW_KEY_LEFT_SHIFT)) {
                g_currentState = STATE_FURNACE_MENU; // <<< Musisz dodać ten stan do enuma!
                g_openedFurnacePos = hitBlockPos;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                return; // Nie stawiaj bloku, jeśli otwieramy piec
            }
            // POPRAWKA: Pozwól stawiać w Powietrzu LUB w dowolnej Wodzie
            // (pod warunkiem, że nie stawiamy wody w wodzie, co byłoby dziwne, chyba że to wiadro)
            uint8_t blockToPlaceIn = GetBlockGlobal(prevBlockPos.x, prevBlockPos.y, prevBlockPos.z);
            bool canPlace = (blockToPlaceIn == BLOCK_ID_AIR) ||
                            (IsAnyWater(blockToPlaceIn) && g_selectedBlockID != BLOCK_ID_WATER);
            if (canPlace)
            {
                // Sprawdź kolizję z graczem (żeby nie postawić bloku w sobie)
                // Prosty AABB (Hitbox gracza to około 0.6x1.8)
                glm::vec3 playerFoot = camera.Position;
                glm::vec3 playerHead = camera.Position + glm::vec3(0, 1.8f, 0);

                // Środki bloku
                float bx = (float)prevBlockPos.x + 0.5f;
                float by = (float)prevBlockPos.y + 0.5f;
                float bz = (float)prevBlockPos.z + 0.5f;

                // Czy gracz jest wewnątrz tego bloku?
                bool collision = (abs(playerFoot.x - bx) < 0.8f && abs(playerFoot.y - by) < 1.4f && abs(playerFoot.z - bz) < 0.8f);

                if (!collision || g_selectedBlockID == BLOCK_ID_TORCH) // Pochodnie nie mają kolizji
                {
                    SetBlock(prevBlockPos.x, prevBlockPos.y, prevBlockPos.z, g_selectedBlockID);
                    ma_engine_play_sound(&g_audioEngine, "place.mp3", NULL);
                    // Dodaj światło dla pochodni
                    if (g_selectedBlockID == BLOCK_ID_TORCH) {
                        g_torchPositions.push_back(glm::vec3(prevBlockPos) + glm::vec3(0.0f, 0.2f, 0.0f));
                    }
                    if (g_selectedBlockID == BLOCK_ID_FURNACE) {
                        g_furnaces[prevBlockPos] = FurnaceData();
                    }

                    // Odejmij z ekwipunku (opcjonalne, jeśli chcesz survival)
                    if (!camera.flyingMode) {
                        g_inventory[g_activeSlot].count--;
                        if(g_inventory[g_activeSlot].count <= 0) g_inventory[g_activeSlot].itemID = BLOCK_ID_AIR;
                    }
                }
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
    g_selectedBlockID = g_inventory[g_activeSlot].itemID;

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
void SaveWorld() {
    std::cout << "Zapisywanie swiata..." << std::endl;
    for (auto& chunk : chunk_storage) // Użyj chunk_storage
    {
        chunk.SaveToFile();
    }
    SaveInventory();
    SavePlayerPosition();
    SaveFurnaces();
    std::cout << "Zapisano." << std::endl;

    // BARDZO WAŻNE: Wyczyść stary świat, aby zrobić miejsce na nowy,
    // jeśli gracz wróci do menu głównego.
    g_WorldChunks.clear();
    chunk_storage.clear();
}
void UpdateScreenSize(int width, int height)
{
    // 1. Zaktualizuj globalne zmienne rozmiaru
    SCR_WIDTH = width;
    SCR_HEIGHT = height;

    // 2. Zaktualizuj pozycje myszy (aby uniknąć skoków)
    lastX = SCR_WIDTH / 2.0f;
    lastY = SCR_HEIGHT / 2.0f;
    firstMouse = true; // Wymuś reset myszy

    // 3. Oblicz ponownie wszystkie pozycje UI
    BTN_X = (SCR_WIDTH - BTN_W) / 2.0f;
    BTN_Y_NEW = SCR_HEIGHT / 2.0f + 70.0f;
    BTN_Y_LOAD = SCR_HEIGHT / 2.0f;
    BTN_Y_QUIT = SCR_HEIGHT / 2.0f - 70.0f;

    INPUT_X = (SCR_WIDTH - INPUT_W) / 2.0f;
    INPUT_NAME_Y = SCR_HEIGHT / 2.0f + 60.0f;
    INPUT_SEED_Y = SCR_HEIGHT / 2.0f;

    BTN_START_X = (SCR_WIDTH - BTN_W) / 2.0f;
    BTN_START_Y = SCR_HEIGHT / 2.0f - 70.0f;
    BTN_BACK_X = (SCR_WIDTH - BTN_W) / 2.0f;
    BTN_BACK_Y = SCR_HEIGHT / 2.0f - 140.0f;

    LIST_ITEM_X = (SCR_WIDTH - LIST_ITEM_W) / 2.0f;
    LIST_START_Y = SCR_HEIGHT - 150.0f;
    LIST_BACK_BTN_Y = 50.0f;

    PAUSE_BTN_Y_RESUME = SCR_HEIGHT / 2.0f + 70.0f;
    PAUSE_BTN_Y_MENU = SCR_HEIGHT / 2.0f;
    PAUSE_BTN_Y_QUIT = SCR_HEIGHT / 2.0f - 70.0f;

    BAR_START_X = (SCR_WIDTH - BAR_WIDTH) / 2.0f; // <-- WAŻNE: Dodaj też to

    // 4. Zaktualizuj macierz projekcji 2D
    ortho_projection = glm::ortho(0.0f, (float)SCR_WIDTH, 0.0f, (float)SCR_HEIGHT);
    std::cout <<"Dzialanie UpdateScreenSize" << std::endl;
    // 5. Zaktualizuj viewport OpenGL
    // glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
    std::cout << "Poprawnie wykonano glViewport" << std::endl;
    if (g_openglInitialized) { // <<< TYLKO JEŚLI OPENGL JEST GOTOWY
        glViewport(0, 0, width, height);
        InitBloom(); // Przebuduj bufory dla nowej rozdzielczości
    }
    std::cout << "Poprawnie wykonano InitBloom" << std::endl;
}
// Zmodyfikowana funkcja: zwraca 'true', jeśli udało się dodać CAŁOŚĆ
bool AddItemToInventory(uint8_t itemID, int count)
{
    // 1. Najpierw spróbuj dodać do istniejących stosów
    for (int i = 0; i < INVENTORY_SLOTS; i++) {
        if (g_inventory[i].itemID == itemID && g_inventory[i].count < MAX_STACK_SIZE) {
            int space = MAX_STACK_SIZE - g_inventory[i].count;
            int toAdd = std::min(space, count);

            g_inventory[i].count += toAdd;
            count -= toAdd;

            if (count == 0) return true; // Wszystko dodano
        }
    }

    // 2. Jeśli nadal coś zostało, szukaj pustych slotów
    while (count > 0) {
        int emptySlot = -1;
        for (int i = 0; i < INVENTORY_SLOTS; i++) {
            if (g_inventory[i].itemID == BLOCK_ID_AIR) {
                emptySlot = i;
                break;
            }
        }

        if (emptySlot != -1) {
            g_inventory[emptySlot].itemID = itemID;
            int toAdd = std::min(MAX_STACK_SIZE, count);
            g_inventory[emptySlot].count = toAdd;
            count -= toAdd;
        } else {
            // Brak miejsca w ekwipunku!
            // (Opcjonalnie: tutaj można by wyrzucić resztę na ziemię jako drop)
            return false;
        }
    }

    return true;
}
int GetSlotUnderMouse(double mouseX, double mouseY) {
    // Definicje muszą być IDENTYCZNE jak w pętli rysowania (STATE_INVENTORY)
    float invSlotSize = SLOT_SIZE;
    float invSlotGap = 4.0f;
    float invWidth = (invSlotSize * 9) + (invSlotGap * 8);
    float invStartX = (SCR_WIDTH - invWidth) / 2.0f;
    float invStartY = (SCR_HEIGHT / 2.0f) - 50.0f;

    // Sprawdź sloty PLECAKA (indeksy 9-35)
    for (int i = 0; i < 27; i++) {
        int row = i / 9;
        int col = i % 9;
        int slotIndex = i + 9;

        float x = invStartX + col * (invSlotSize + invSlotGap);
        float y = invStartY - row * (invSlotSize + invSlotGap);

        if (IsMouseOver(mouseX, mouseY, x, y, invSlotSize, invSlotSize)) {
            return slotIndex;
        }
    }

    // Sprawdź sloty HOTBARA (indeksy 0-8)
    // (Hotbar w inventory jest rysowany w tym samym miejscu co w grze)
    for (int i = 0; i < HOTBAR_SIZE; i++) {
        float x = BAR_START_X + i * (SLOT_SIZE + GAP);
        float y = 10.0f;

        if (IsMouseOver(mouseX, mouseY, x, y, SLOT_SIZE, SLOT_SIZE)) {
            return i;
        }
    }
    // float craftStartX = invStartX + (invSlotSize + invSlotGap) * 5.0f; // Przesuń w prawo
    // float craftStartY = invStartY + (invSlotSize + invSlotGap) * 4.0f; // Przesuń w górę
    //
    // // Siatka 2x2 (Input)
    // for (int i = 0; i < 4; i++) {
    //     int row = i / 2; // 0 lub 1
    //     int col = i % 2; // 0 lub 1
    //
    //     float x = craftStartX + col * (invSlotSize + invSlotGap);
    //     float y = craftStartY - row * (invSlotSize + invSlotGap);
    //
    //     if (IsMouseOver(mouseX, mouseY, x, y, invSlotSize, invSlotSize)) {
    //         return SLOT_CRAFT_START + i; // Zwróć 100, 101, 102 lub 103
    //     }
    // }
    //
    // // Slot Wyniku (Duży slot po prawej)
    // float resultX = craftStartX + (invSlotSize + invSlotGap) * 3.0f;
    // float resultY = craftStartY - 0.5f * (invSlotSize + invSlotGap); // Wyśrodkowany w pionie
    //
    // if (IsMouseOver(mouseX, mouseY, resultX, resultY, invSlotSize, invSlotSize)) {
    //     return SLOT_RESULT; // Zwróć 104
    // }
    if (g_currentState == STATE_INVENTORY) {
        float craftStartX = invStartX + (invSlotSize + invSlotGap) * 5.0f; // Przesuń w prawo
        float craftStartY = invStartY + (invSlotSize + invSlotGap) * 4.0f; // Przesuń w górę

        // Siatka 2x2 (Input)
        for (int i = 0; i < 4; i++) {
            int row = i / 2; // 0 lub 1
            int col = i % 2; // 0 lub 1

            float x = craftStartX + col * (invSlotSize + invSlotGap);
            float y = craftStartY - row * (invSlotSize + invSlotGap);

            if (IsMouseOver(mouseX, mouseY, x, y, invSlotSize, invSlotSize)) {
                return SLOT_CRAFT_START + i; // Zwróć 100, 101, 102 lub 103
            }
        }

        // Slot Wyniku (Duży slot po prawej)
        float resultX = craftStartX + (invSlotSize + invSlotGap) * 3.0f;
        float resultY = craftStartY - 0.5f * (invSlotSize + invSlotGap); // Wyśrodkowany w pionie

        if (IsMouseOver(mouseX, mouseY, resultX, resultY, invSlotSize, invSlotSize)) {
            return SLOT_RESULT; // Zwróć 104
        }
    }

    // 3. Sprawdź Sloty PIECA (Tylko w Furnace Menu)
    if (g_currentState == STATE_FURNACE_MENU) {
        float cx = SCR_WIDTH / 2.0f;
        float cy = SCR_HEIGHT / 2.0f + 100.0f;

        // Input (Góra)
        if (IsMouseOver(mouseX, mouseY, cx - 32, cy + 60, 64, 64)) return SLOT_FURNACE_INPUT;
        // Fuel (Dół)
        if (IsMouseOver(mouseX, mouseY, cx - 32, cy - 60, 64, 64)) return SLOT_FURNACE_FUEL;
        // Result (Prawo)
        if (IsMouseOver(mouseX, mouseY, cx + 80, cy, 64, 64)) return SLOT_FURNACE_RESULT;
    }

    return -1; // Nie kliknięto w żaden slot
}
void SpawnDrop(uint8_t itemID, glm::vec3 pos, glm::vec3 velocity) {
    if (itemID == BLOCK_ID_AIR) return;
    DroppedItem item;
    item.itemID = itemID;
    item.position = pos;
    item.velocity = velocity;
    item.rotationTime = (float)(rand() % 100) / 10.0f; // Losowy start obrotu
    item.pickupDelay = 1.0f; // Nie można podnieść przez 1 sekundę
    item.isAlive = true;
    g_droppedItems.push_back(item);
}
void UpdateDrops(float dt, glm::vec3 playerPos) {
    for (auto it = g_droppedItems.begin(); it != g_droppedItems.end(); ) {

        // 1. Sprawdź, co jest POD przedmiotem
        int blX = round(it->position.x);
        int blY_under = floor(it->position.y - 0.25f); // Sprawdź blok pod stopami dropu (0.25 to połowa wysokości dropu)
        int blZ = round(it->position.z);

        uint8_t blockUnder = GetBlockGlobal(blX, blY_under, blZ);
        bool isSolidGround = (blockUnder != BLOCK_ID_AIR &&
                      blockUnder != BLOCK_ID_WATER &&
                      !(blockUnder == BLOCK_ID_WATER || (blockUnder >= 13 && blockUnder <= 16))&&
                      blockUnder != BLOCK_ID_TORCH &&
                      blockUnder != BLOCK_ID_LEAVES);

        // 2. Fizyka Grawitacji
        if (isSolidGround && it->position.y - (float)blY_under < 1.25f && it->velocity.y <= 0.0f) {
            // LEŻY NA ZIEMI: Zatrzymaj grawitację
            it->velocity.y = 0.0f;
            it->position.y = (float)blY_under + 1.0f+0.2f; // Ustaw idealnie na bloku + mały margines lewitacji

            // Silne tarcie (żeby się nie ślizgał)
            it->velocity.x *= 0.5f;
            it->velocity.z *= 0.5f;
        } else {
            // JEST W POWIETRZU: Zastosuj grawitację
            it->velocity.y += GRAVITY * dt;

            // Opór powietrza
            it->velocity.x *= 0.98f;
            it->velocity.z *= 0.98f;
        }

        // 3. Aplikuj ruch
        it->position += it->velocity * dt;

        // 4. Rotacja (zawsze się kręci dla efektu)
        it->rotationTime += dt;

        // 5. Zbieranie (Pickup) - BEZ ZMIAN
        it->pickupDelay -= dt;
        float distToPlayer = glm::distance(it->position, playerPos + glm::vec3(0,1,0));

        if (distToPlayer < 3.0f && it->pickupDelay <= 0.0f) {
            glm::vec3 dirToPlayer = normalize((playerPos + glm::vec3(0,1,0)) - it->position);
            it->position += dirToPlayer * 8.0f * dt; // Szybsze przyciąganie
        }

        if (distToPlayer < 1.0f && it->pickupDelay <= 0.0f) {
            if (AddItemToInventory(it->itemID,1)) {
                ma_engine_play_sound(&g_audioEngine, "pop.mp3", NULL);
                it = g_droppedItems.erase(it);
                continue;
            }
        }

        ++it;
    }
}
void SpawnBlockParticles(uint8_t blockID, glm::vec3 blockPos) {
    if (blockID == BLOCK_ID_AIR) return;

    int particleCount = 10; // Ile kawałków wylatuje

    for (int i = 0; i < particleCount; i++) {
        Particle p;
        // Losowa pozycja wewnątrz niszczonego bloku
        float rx = (rand() % 100) / 100.0f;
        float ry = (rand() % 100) / 100.0f;
        float rz = (rand() % 100) / 100.0f;
        p.position = blockPos + glm::vec3(rx, ry, rz);

        // Losowa prędkość (eksplozja we wszystkie strony)
        float vx = ((rand() % 100) - 50) / 100.0f * 2.0f;
        float vy = ((rand() % 100) / 100.0f) * 3.0f; // Bardziej w górę
        float vz = ((rand() % 100) - 50) / 100.0f * 2.0f;
        p.velocity = glm::vec3(vx, vy, vz);

        p.blockID = blockID;
        p.lifetime = 0.5f + ((rand() % 100) / 200.0f); // Żyje od 0.5 do 1.0 sekundy
        p.maxLifetime = p.lifetime;

        g_particles.push_back(p);
    }
}

// 2. Funkcja aktualizująca fizykę cząsteczek
void UpdateParticles(float dt) {
    for (auto it = g_particles.begin(); it != g_particles.end(); ) {
        // Grawitacja
        it->velocity.y += GRAVITY * dt;
        it->position += it->velocity * dt;

        // Zmniejszanie życia
        it->lifetime -= dt;

        // Kolizja z podłogą (prosta)
        if (it->position.y < 0) it->position.y = 0; // Zabezpieczenie przed voidem

        // Usuń martwe cząsteczki
        if (it->lifetime <= 0.0f) {
            it = g_particles.erase(it);
        } else {
            ++it;
        }
    }
}
void UpdateMining(float dt) {
    if (!g_isMining) return; // Jeśli nie trzymamy przycisku, nic nie rób

    // 1. Sprawdź, w co gracz celuje
    glm::vec3 eyePos = camera.Position + glm::vec3(0.0f, PLAYER_EYE_HEIGHT, 0.0f);
    glm::ivec3 hitBlock;
    glm::ivec3 prevBlock;

    bool hit = Raycast(eyePos, camera.Front, 5.0f, hitBlock, prevBlock);

    if (!hit) { g_miningTimer = 0.0f; return; }

    if (hitBlock != g_currentMiningPos) {
        g_currentMiningPos = hitBlock;
        g_miningTimer = 0.0f;
    }

    uint8_t blockID = GetBlockGlobal(hitBlock.x, hitBlock.y, hitBlock.z);

    // Jeśli w trybie latania (Creative), niszcz natychmiast
    uint8_t toolID = g_inventory[g_activeSlot].itemID;

    // Oblicz twardość uwzględniając narzędzie
    float hardness = GetBlockHardness(blockID, toolID);
    if (camera.flyingMode) hardness = 0.05f;

    if (hardness < 0) return; // Bedrock - niezniszczalny

    // 3. Zwiększ timer
    g_miningTimer += dt;

    // --- EFEKT WIZUALNY: Generuj trochę cząsteczek PODCZAS kopania ---
    // (np. co 0.2 sekundy)
    if (fmod(g_miningTimer, 0.2f) < dt) {
        // Stwórz 2-3 małe okruszki
        for(int i=0; i<3; i++) {
             Particle p;
             p.position = glm::vec3(hitBlock) + glm::vec3(0.5f) + glm::vec3((rand()%100-50)/100.0f * 0.6f);
             p.velocity = glm::vec3((rand()%100-50)/200.0f, 0.1f, (rand()%100-50)/200.0f);
             p.blockID = blockID;
             p.lifetime = 0.3f;
             p.maxLifetime = 0.3f;
             g_particles.push_back(p);
        }
    }

    // 4. Sprawdź, czy blok został zniszczony
    if (g_miningTimer >= hardness) {
        // === ZNISZCZ BLOK (Kod przeniesiony z mouse_callback) ===

        // 1. Usuń światło (jeśli to pochodnia)
        if (blockID == BLOCK_ID_TORCH) {
            g_torchPositions.remove(glm::vec3(hitBlock) + glm::vec3(0.0f, 0.2f, 0.0f));
        }
        if (blockID == BLOCK_ID_FURNACE) {
            // Wyrzuć zawartość na ziemię
            if (g_furnaces.count(hitBlock)) {
                FurnaceData& f = g_furnaces[hitBlock];
                SpawnDrop(f.input.itemID, glm::vec3(hitBlock) + 0.5f, glm::vec3(0,1,0));
                SpawnDrop(f.fuel.itemID, glm::vec3(hitBlock) + 0.5f, glm::vec3(0,1,0));
                SpawnDrop(f.result.itemID, glm::vec3(hitBlock) + 0.5f, glm::vec3(0,1,0));
                g_furnaces.erase(hitBlock); // Usuń dane
            }
        }

        // 2. Usuń blok
        SetBlock(hitBlock.x, hitBlock.y, hitBlock.z, BLOCK_ID_AIR);
        ma_engine_play_sound(&g_audioEngine, "break.mp3", NULL);
        // 3. Efekty (Wybuch + Drop)
        SpawnBlockParticles(blockID, glm::vec3(hitBlock)); // Duży wybuch

        glm::vec3 randVel = glm::vec3((rand()%10-5)*0.5f, 3.0f, (rand()%10-5)*0.5f);
        uint8_t dropID = blockID; // Domyślnie wypada to, co kopiemy (np. ziemia, drewno)

        if (blockID == BLOCK_ID_COAL_ORE) {
            dropID = ITEM_ID_COAL; // Ruda węgla -> Węgiel
        }
        // else if (blockID == BLOCK_ID_DIAMOND_ORE) {
        //     dropID = ITEM_ID_DIAMOND; // Ruda diamentu -> Diament
        // }
        else if (blockID == BLOCK_ID_STONE) {
            // Opcjonalnie: Kamień -> Bruk (Cobblestone), jeśli dodasz taki blok
            // dropID = BLOCK_ID_COBBLESTONE;
        }
        SpawnDrop(dropID, glm::vec3(hitBlock) + 0.5f, randVel);
        if (toolID == ITEM_ID_PICKAXE && !camera.flyingMode) {
            g_inventory[g_activeSlot].durability--;

            // Jeśli zniszczony -> usuń
            if (g_inventory[g_activeSlot].durability <= 0) {
                g_inventory[g_activeSlot] = {BLOCK_ID_AIR, 0, 0, 0};
                ma_engine_play_sound(&g_audioEngine, "break.mp3", NULL); // Dźwięk pęknięcia narzędzia
            }
        }
        // 4. Reset
        g_miningTimer = 0.0f;
    }
}
// --- ZAPISYWANIE EKWIPUNKU ---
void SaveInventory() {
    if (g_selectedWorldName.empty()) return;

    std::string path = "worlds/" + g_selectedWorldName + "/inventory.dat";
    std::ofstream outFile(path, std::ios::binary);

    if (outFile.is_open()) {
        // Zapisz cały wektor g_inventory naraz (to bezpieczne dla prostych struktur)
        outFile.write(reinterpret_cast<char*>(g_inventory.data()), g_inventory.size() * sizeof(ItemStack));
        outFile.close();
        std::cout << "Zapisano ekwipunek do: " << path << std::endl;
    } else {
        std::cout << "BLAD: Nie udalo sie zapisac ekwipunku!" << std::endl;
    }
}

// --- WCZYTYWANIE EKWIPUNKU ---
void LoadInventory() {
    // 1. Najpierw wyczyść ekwipunek (ustaw startowy zestaw)
    // Dzięki temu, jeśli to nowy świat, będziesz miał startowe itemy.
    // A jeśli wczytujesz stary, te itemy zostaną nadpisane z pliku.
    for (auto& slot : g_inventory) { slot = {BLOCK_ID_AIR, 0}; }

    // Domyślny zestaw startowy (taki sam jak w main)
    g_inventory[0] = {BLOCK_ID_GRASS, 64};
    g_inventory[1] = {BLOCK_ID_DIRT, 64};
    g_inventory[2] = {BLOCK_ID_STONE, 64};
    g_inventory[3] = {BLOCK_ID_TORCH, 64};
    g_inventory[4] = {BLOCK_ID_LOG, 64};
    g_inventory[5] = {BLOCK_ID_LEAVES, 64};
    g_inventory[6] = {BLOCK_ID_SAND, 64};
    g_inventory[7] = {BLOCK_ID_WATER, 64};
    g_inventory[8] = {BLOCK_ID_FURNACE, 64};

    // 2. Sprawdź, czy plik zapisu istnieje
    std::string path = "worlds/" + g_selectedWorldName + "/inventory.dat";
    if (std::filesystem::exists(path)) {
        std::ifstream inFile(path, std::ios::binary);
        if (inFile.is_open()) {
            // Wczytaj dane z pliku, nadpisując zestaw startowy
            inFile.read(reinterpret_cast<char*>(g_inventory.data()), g_inventory.size() * sizeof(ItemStack));
            inFile.close();
            std::cout << "Wczytano ekwipunek z pliku." << std::endl;
        }
    } else {
        std::cout << "Brak pliku ekwipunku - uzywanie zestawu startowego." << std::endl;
    }
}
void SaveFurnaces() {
    std::ofstream out("worlds/" + g_selectedWorldName + "/furnaces.dat", std::ios::binary);
    size_t count = g_furnaces.size();
    out.write((char*)&count, sizeof(count)); // Zapisz ilość pieców

    for (const auto& [pos, data] : g_furnaces) {
        out.write((char*)&pos, sizeof(pos));   // Pozycja
        out.write((char*)&data, sizeof(data)); // Dane (ItemStacki i czasy)
    }
}

void LoadFurnaces() {
    g_furnaces.clear();
    std::ifstream in("worlds/" + g_selectedWorldName + "/furnaces.dat", std::ios::binary);
    if (!in.is_open()) return;

    size_t count;
    in.read((char*)&count, sizeof(count));

    for (size_t i = 0; i < count; i++) {
        glm::ivec3 pos;
        FurnaceData data;
        in.read((char*)&pos, sizeof(pos));
        in.read((char*)&data, sizeof(data));
        g_furnaces[pos] = data;
    }
}
void SavePlayerPosition() {
    if (g_selectedWorldName.empty()) return;

    std::string path = "worlds/" + g_selectedWorldName + "/player.dat";
    std::ofstream outFile(path, std::ios::binary);

    if (outFile.is_open()) {
        // Zapisz wektor pozycji (X, Y, Z)
        outFile.write(reinterpret_cast<char*>(&camera.Position), sizeof(glm::vec3));

        // Opcjonalnie: Zapisz też kierunek patrzenia (Yaw, Pitch), jeśli chcesz
        outFile.write(reinterpret_cast<char*>(&camera.Yaw), sizeof(float));
        outFile.write(reinterpret_cast<char*>(&camera.Pitch), sizeof(float));

        outFile.close();
        std::cout << "Zapisano pozycje gracza." << std::endl;
    }
}

// --- WCZYTYWANIE POZYCJI GRACZA ---
void LoadPlayerPosition() {
    std::string path = "worlds/" + g_selectedWorldName + "/player.dat";

    if (std::filesystem::exists(path)) {
        std::ifstream inFile(path, std::ios::binary);
        if (inFile.is_open()) {
            // Wczytaj pozycję
            inFile.read(reinterpret_cast<char*>(&camera.Position), sizeof(glm::vec3));

            // Wczytaj kierunek patrzenia
            inFile.read(reinterpret_cast<char*>(&camera.Yaw), sizeof(float));
            inFile.read(reinterpret_cast<char*>(&camera.Pitch), sizeof(float));

            // Zaktualizuj wektory kamery (Front, Right, Up) na podstawie nowych Yaw/Pitch
            // (Zakładam, że masz metodę updateCameraVectors() albo ProcessMouseMovement wywołuje ją)
            camera.ProcessMouseMovement(0, 0, false); // Trik na odświeżenie wektorów bez ruchu

            inFile.close();
            std::cout << "Wczytano gracza na: " << camera.Position.x << ", " << camera.Position.y << ", " << camera.Position.z << std::endl;
        }
    } else {
        // Brak zapisu -> Domyślny spawn (wysoko, żeby nie wpaść pod ziemię)
        camera.Position = glm::vec3(8.0f, 50.0f, 8.0f);
        camera.Yaw = -90.0f;
        camera.Pitch = 0.0f;
        camera.ProcessMouseMovement(0, 0, false);
        std::cout << "Brak zapisu pozycji - spawn domyslny." << std::endl;
    }
}
void UpdateLiquids(float dt) {
    g_waterTimer += dt;
    if (g_waterTimer < 0.10f) return; // Szybciej (co 0.1s) dla płynności
    g_waterTimer = 0.0f;

    struct BlockChange { glm::ivec3 pos; uint8_t id; };
    std::vector<BlockChange> changes;

    int radius = 18;
    glm::ivec3 pPos = glm::ivec3(camera.Position);

    for (int x = pPos.x - radius; x <= pPos.x + radius; x++) {
        for (int z = pPos.z - radius; z <= pPos.z + radius; z++) {
            for (int y = 0; y < CHUNK_HEIGHT; y++) {

                uint8_t current = GetBlockGlobal(x, y, z);

                // Sprawdź czy to woda (Źródło lub Płynąca)
                bool isSource = (current == BLOCK_ID_WATER);
                bool isFlowing = (current >= BLOCK_ID_WATER_4 && current <= BLOCK_ID_WATER_1);

                if (!isSource && !isFlowing) continue;

                // --- 1. SPADANIE W DÓŁ (Wodospad) ---
                // Woda spadająca w dół ZAWSZE odzyskuje maksymalną siłę (WATER_4)
                uint8_t down = GetBlockGlobal(x, y - 1, z);
                if (down == BLOCK_ID_AIR) {
                    changes.push_back({ glm::ivec3(x, y - 1, z), BLOCK_ID_WATER_4 });
                    // Nie rozlewaj się na boki, jeśli spadasz (opcjonalne, ale wygląda lepiej)
                    continue;
                }
                else if (down == BLOCK_ID_WATER || (down >= 13 && down <= 16)) {
                    // Pod spodem jest woda - traktujemy to jak spadanie
                    continue;
                }

                // --- 2. ROZLEWANIE NA BOKI ---
                // Oblicz siłę nowej wody
                uint8_t nextID = BLOCK_ID_AIR;

                if (isSource) nextID = BLOCK_ID_WATER_4;      // Źródło -> 4
                else if (current == BLOCK_ID_WATER_4) nextID = BLOCK_ID_WATER_3;
                else if (current == BLOCK_ID_WATER_3) nextID = BLOCK_ID_WATER_2;
                else if (current == BLOCK_ID_WATER_2) nextID = BLOCK_ID_WATER_1;
                // WATER_1 nie rozlewa się dalej

                if (nextID != BLOCK_ID_AIR) {
                    int dx[] = { 1, -1, 0, 0 };
                    int dz[] = { 0, 0, 1, -1 };

                    for (int i = 0; i < 4; i++) {
                        glm::ivec3 targetPos(x + dx[i], y, z + dz[i]);
                        uint8_t targetBlock = GetBlockGlobal(targetPos.x, targetPos.y, targetPos.z);

                        // Jeśli obok jest pusto, wlej tam wodę
                        if (targetBlock == BLOCK_ID_AIR) {
                            changes.push_back({ targetPos, nextID });
                        }
                        // Jeśli obok jest SŁABSZA woda, nadpisz ją mocniejszą
                        // (np. WATER_4 nadpisuje WATER_2)
                        else if (targetBlock >= 13 && targetBlock <= 16) {
                            if (nextID < targetBlock) { // Pamiętaj: mniejsze ID to mniejsza liczba w enumie, ale tutaj ID są odwrotnie (4=13, 1=16)
                                // W naszym enumie: 4(13) < 3(14) < 2(15) < 1(16).
                                // Więc "Silniejsza" woda ma MNIEJSZE ID.
                                changes.push_back({ targetPos, nextID });
                            }
                        }
                    }
                }

                // --- 3. WYSYCHANIE (Cleanup) ---
                // Jeśli blok to Woda Płynąca, sprawdź czy ma "rodzica" (źródło zasilania)
                if (isFlowing) {
                    bool fed = false;

                    // Sprawdź górę (czy woda spada na mnie?)
                    uint8_t up = GetBlockGlobal(x, y + 1, z);
                    if (up == BLOCK_ID_WATER || (up >= 13 && up <= 16)) fed = true;

                    // Sprawdź boki (czy silniejsza woda wpływa we mnie?)
                    if (!fed) {
                        int dx[] = { 1, -1, 0, 0 };
                        int dz[] = { 0, 0, 1, -1 };
                        for (int i = 0; i < 4; i++) {
                            uint8_t side = GetBlockGlobal(x + dx[i], y, z + dz[i]);

                            // Źródło zawsze karmi
                            if (side == BLOCK_ID_WATER) { fed = true; break; }

                            // Silniejsza woda karmi słabszą
                            // (Silniejsza ma MNIEJSZE ID: 13 < 14)
                            if (side >= 13 && side <= 16) {
                                if (side < current) { fed = true; break; }
                            }
                        }
                    }

                    // Jeśli nikt nie karmi -> wyschnij
                    if (!fed) {
                         changes.push_back({ glm::ivec3(x, y, z), BLOCK_ID_AIR });
                    }
                }
            }
        }
    }
    g_lavaTimer += dt;
    if (g_lavaTimer < 0.50f) return; // Lawa płynie co 0.5 sekundy
    g_lavaTimer = 0.0f;

    // Kopiujemy logikę wody, ale zmieniamy ID


    for (int x = pPos.x - radius; x <= pPos.x + radius; x++) {
        for (int z = pPos.z - radius; z <= pPos.z + radius; z++) {
            for (int y = 0; y < CHUNK_HEIGHT; y++) {
                uint8_t current = GetBlockGlobal(x, y, z);

                if (!isAnyLava(current)) continue;

                // 1. Spadanie
                uint8_t down = GetBlockGlobal(x, y - 1, z);
                if (down == BLOCK_ID_AIR) {
                    changes.push_back({ glm::ivec3(x, y - 1, z), BLOCK_ID_LAVA_3 }); // Spada jako silna lawa
                    continue;
                }
                // Lawa niszczy rośliny/pochodnie pod sobą (opcjonalnie)

                // 2. Rozlewanie
                uint8_t nextID = BLOCK_ID_AIR;
                if (current == BLOCK_ID_LAVA)   nextID = BLOCK_ID_LAVA_3;
                else if (current == BLOCK_ID_LAVA_3) nextID = BLOCK_ID_LAVA_2;
                else if (current == BLOCK_ID_LAVA_2) nextID = BLOCK_ID_LAVA_1;
                // LAVA_1 nie płynie dalej (krótszy zasięg niż woda)

                if (nextID != BLOCK_ID_AIR) {
                    int dx[] = {1, -1, 0, 0}; int dz[] = {0, 0, 1, -1};
                    for(int i=0; i<4; i++) {
                        glm::ivec3 tPos(x+dx[i], y, z+dz[i]);
                        uint8_t target = GetBlockGlobal(tPos.x, tPos.y, tPos.z);

                        if (target == BLOCK_ID_AIR) {
                            changes.push_back({tPos, nextID});
                        }
                        // Kolizja z wodą -> Kamień (Cobblestone generator!)
                        else if (IsAnyWater(target)) {
                             changes.push_back({tPos, BLOCK_ID_STONE});
                             ma_engine_play_sound(&g_audioEngine, "fizz.mp3", NULL); // Dźwięk syczenia
                        }
                    }
                }

                // (Dodaj też logikę wysychania, analogicznie do wody)
            }
        }
    }
    // Aplikuj zmiany
    for (const auto& change : changes) {
        SetBlock(change.pos.x, change.pos.y, change.pos.z, change.id);
    }
}
void UpdateEntities(float dt, glm::vec3 playerPos) {
    for (auto it = g_entities.begin(); it != g_entities.end(); ) {

        // --- LOGIKA ZOMBIE ---
        if (it->type == ENTITY_ZOMBIE) {
            float dist = glm::distance(it->position, playerPos);

            // 1. Wykrywanie gracza (np. z 15 kratek)
            if (dist < 15.0f) {
                it->isWalking = true;

                // Oblicz wektor w stronę gracza
                glm::vec3 dir = glm::normalize(playerPos - it->position);

                // Obróć zombie w stronę gracza (atan2 zwraca kąt w radianach)
                it->rotationY = glm::degrees(atan2(dir.x, dir.z));

                // Ustaw prędkość (Zombie jest wolniejszy od gracza)
                float speed = 4.5f;
                it->velocity.x = dir.x * speed;
                it->velocity.z = dir.z * speed;

                // Auto-Jump (jeśli uderzy w ścianę)
                glm::vec3 frontPos = it->position + dir * 0.6f;
                uint8_t blockFront = GetBlockGlobal(round(frontPos.x), round(frontPos.y), round(frontPos.z));
                if (blockFront != BLOCK_ID_AIR && blockFront != BLOCK_ID_WATER) {
                     if (onGround) it->velocity.y = 6.0f;
                }

                // Atakowanie gracza
                if (dist < 1.0f) {
                    it->attackCooldown -= dt;
                    if (it->attackCooldown <= 0.0f) {
                        playerHealth -= 1.5f; // Zabierz 1.5 serca
                        it->attackCooldown = 1.0f; // Bij co sekundę
                        // ma_engine_play_sound(&g_audioEngine, "hurt.mp3", NULL);
                        std::cout << "Zombie ugryzl gracza!" << std::endl;
                    }
                }
            } else {
                // Jeśli gracz daleko - stój w miejscu (lub idź losowo, jak chcesz)
                it->isWalking = false;
                it->velocity.x = 0.0f;
                it->velocity.z = 0.0f;
            }
        }
        // --- LOGIKA ŚWINI (Stara) ---
        else if (it->type == ENTITY_PIG) {
            it->moveTimer -= dt;
            if (it->moveTimer <= 0.0f) {
                it->moveTimer = 2.0f + (rand() % 30) / 10.0f;
                it->isWalking = (rand() % 2 == 0);
                if (it->isWalking) it->rotationY = (float)(rand() % 360);
                else { it->velocity.x = 0.0f; it->velocity.z = 0.0f; }
            }
            if (it->isWalking) {
                float speed = 2.0f;
                float rad = glm::radians(it->rotationY);
                it->velocity.x = sin(rad) * speed;
                it->velocity.z = cos(rad) * speed;
                // (Tu można dodać auto-jump dla świni)
            }
        }

        // --- WSPÓLNA FIZYKA ---
        it->velocity.y += GRAVITY * dt;
        it->position += it->velocity * dt;

        // Kolizja z podłogą
        int blX = round(it->position.x);
        int blY_under = floor(it->position.y - 0.05f);
        int blZ = round(it->position.z);
        uint8_t blockUnder = GetBlockGlobal(blX, blY_under, blZ);
        bool isSolid = (blockUnder != BLOCK_ID_AIR && blockUnder != BLOCK_ID_WATER && blockUnder != BLOCK_ID_TORCH && blockUnder != BLOCK_ID_LEAVES);

        if (isSolid && it->position.y - (float)blY_under < 1.1f && it->velocity.y <= 0.0f) {
            it->position.y = (float)blY_under + 1.001f;
            it->velocity.y = 0.0f;
        }

        // Tarcie
        it->velocity.x *= 0.9f;
        it->velocity.z *= 0.9f;

        ++it;
    }
}

// 2. Spawnowanie (Twój kod)
void SpawnEntity(glm::vec3 pos,EntityType type) {
    Entity e;
    e.position = pos;
    e.type = type; // <<< Przypisz typ
    e.velocity = glm::vec3(0,0,0);
    e.rotationY = 0.0f;
    e.moveTimer = 0.0f;
    e.isWalking = false;
    e.health = 10.0f;
    e.color = glm::vec3(1.0f, 0.8f, 0.6f);
    g_entities.push_back(e);
}
void CheckCrafting() {
    // Wyczyść wynik na start
    g_craftingOutput = {BLOCK_ID_AIR, 0};

    // --- RECEPTURA 1: DREWNO -> 4 DESKI (używamy Sandstone jako desek) ---
    // Sprawdzamy każdy slot siatki. Jeśli którykolwiek zawiera LOG, dajemy wynik.
    // (To prosta receptura "bezkształtna")

    int logCount = 0;
    int otherCount = 0;

    for (int i = 0; i < 4; i++) {
        if (g_craftingGrid[i].itemID == BLOCK_ID_LOG) logCount++;
        else if (g_craftingGrid[i].itemID != BLOCK_ID_AIR) otherCount++;
    }

    // Jeśli mamy dokładnie 1 kłodę i nic więcej -> Daj 4 Deski
    if (logCount == 1 && otherCount == 0) {
        g_craftingOutput = {BLOCK_ID_SANDSTONE, 4};
    }

    int stoneCount = 0;
    for (int i = 0; i < 4; i++) {
        if (g_craftingGrid[i].itemID == BLOCK_ID_STONE) stoneCount++;
    }

    // Jeśli są 3 kamienie (i jedno puste miejsce, bo siatka ma 4 sloty)
    if (stoneCount == 3) {
        // Stwórz Kilof
        g_craftingOutput.itemID = ITEM_ID_PICKAXE;
        g_craftingOutput.count = 1;
        g_craftingOutput.durability = 50;    // Wytrzyma na 50 bloków
        g_craftingOutput.maxDurability = 50;
    }
}


// --- FUNKCJA RYSOWANIA ANIMOWANEJ ŚWINKI ---
void DrawPig(Shader& shader, const Entity& e, unsigned int vao) {
    // Tekstura: Piaskowiec (Różowawa)
    glm::vec2 uvOff = glm::vec2(UV_SANDSTONE[0], UV_SANDSTONE[1]);
    shader.setVec2("u_uvOffset", uvOff);
    shader.setVec2("u_uvOffsetTop", uvOff);
    shader.setVec2("u_uvOffsetBottom", uvOff);
    shader.setInt("u_multiTexture", 0);

    // Parametry Animacji
    float walkSpeed = 10.0f;
    float maxAngle = 45.0f; // Większy wymach (jak w MC)
    float swingAngle = 0.0f;

    if (e.isWalking) {
        swingAngle = sin(glfwGetTime() * walkSpeed) * maxAngle;
    } else {
        // Gdy stoi, nogi wracają do pionu (płynnie by było lepiej, ale 0.0f wystarczy)
        swingAngle = 0.0f;
    }

    // Bazowa macierz (Pozycja Moba)
    glm::mat4 baseModel = glm::mat4(1.0f);
    baseModel = glm::translate(baseModel, e.position);
    baseModel = glm::rotate(baseModel, glm::radians(e.rotationY ), glm::vec3(0, 1, 0));

    // --- 1. TUŁÓW ---
    {
        glm::mat4 model = baseModel;
        // Ciało musi być niżej, bo nogi są krótsze.
        // Wysokość nóg to 0.4. Ciało (wys 0.6) zaczyna się na 0.4, więc jego środek to 0.4 + 0.3 = 0.7.
        model = glm::translate(model, glm::vec3(0.0f, 0.7f, 0.0f));
        model = glm::scale(model, glm::vec3(0.9f, 0.6f, 1.3f)); // Długi tułów

        shader.setMat4("model", model);
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }

    // --- 2. GŁOWA ---
    {
        glm::mat4 model = baseModel;
        // Głowa na wysokości: 0.4 (nogi) + 0.6 (ciało) - trochę w dół = ~0.8/0.9
        model = glm::translate(model, glm::vec3(0.0f, 1.0f, 0.8f));
        model = glm::scale(model, glm::vec3(0.7f, 0.7f, 0.7f));

        shader.setMat4("model", model);
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }

    // --- 3. NOGI ---
    // Świnia ma krótkie nóżki!
    // Wysokość nogi: 0.4
    // Połowa wysokości (centrum sześcianu): 0.2

    glm::vec3 legScale(0.25f, 0.4f, 0.25f);

    // Pozycje "bioder" (punktów styku z ciałem)
    // Y = 0.4 (spód ciała)
    float hipY = 0.4f;

    glm::vec3 legOrigins[] = {
        glm::vec3(-0.3f, hipY,  0.4f), // Lewa Przednia
        glm::vec3( 0.3f, hipY,  0.4f), // Prawa Przednia
        glm::vec3(-0.3f, hipY, -0.4f), // Lewa Tylna
        glm::vec3( 0.3f, hipY, -0.4f)  // Prawa Tylna
    };

    for (int i = 0; i < 4; i++) {
        glm::mat4 model = baseModel;

        // 1. Idź do punktu biodra
        model = glm::translate(model, legOrigins[i]);

        // 2. Oblicz kąt (chodzenie po przekątnej)
        // LP (0) i PT (3) idą razem. PP (1) i LT (2) idą przeciwnie.
        float currentAngle = (i == 0 || i == 3) ? -swingAngle : swingAngle;

        // 3. Obróć w biodrze
        model = glm::rotate(model, glm::radians(currentAngle), glm::vec3(1, 0, 0));

        // 4. Przesuń w dół o połowę długości nogi
        // (bo sześcian rysuje się od środka, a my jesteśmy w biodrze)
        model = glm::translate(model, glm::vec3(0, -0.2f, 0)); // 0.2 to połowa z 0.4

        // 5. Skaluj
        model = glm::scale(model, legScale);

        shader.setMat4("model", model);
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }
}
void DrawDurabilityBar(Shader& uiShader, unsigned int vao, float x, float y, float w, float h, int cur, int max) {
    if (max <= 0) return; // Nie rysuj dla zwykłych bloków

    float percent = (float)cur / (float)max;

    // Tło paska (Czarne)
    float barH = 6.0f;
    float barY = y + 2.0f; // 2 piksele od dołu slotu

    // Wyłączamy tekstury (używamy czystego koloru z shadera?
    // Nie mamy shadera koloru, więc użyjmy tricku z teksturą 'slotTextureID' bardzo ciemną lub jasną)
    // Najprościej: Użyj 'texOverlay' (czarny) jako tła, i np. 'texSun' (jasny) jako paska, barwiąc go.
    // Ale Twój shader UI nie obsługuje kolorowania vertexów (u_color).

    // SZYBKI SPOSÓB: Użyjmy slotTextureID (szary) jako tła i texSun (biały/żółty) jako paska.
    // Możesz dodać 'uniform vec4 u_color' do ui.frag, żeby to zrobić profesjonalnie.
    // Na razie, narysujmy po prostu pasek.

    // TŁO (Czarne - używamy texOverlay)
    uiShader.setFloat("u_opacity", 1.0f);
    uiShader.setVec2("u_uvScale", 1.0f, 1.0f);
    uiShader.setVec2("u_uvOffset", 0.0f, 0.0f);
    DrawMenuButton(uiShader, vao, texOverlay, x + 4, barY, w - 8, barH); // Margines 4px

    // PASEK ŻYCIA (Zielony/Czerwony? Nie mamy kolorów, więc będzie biały z texSun lub szary)
    // Żeby zrobić to porządnie, musisz dodać `uniform vec4 u_color` do ui.frag.
    // Ale na szybko:
    float fillW = (w - 8) * percent;
    DrawMenuButton(uiShader, vao, texSun, x + 4, barY, fillW, barH);
}
void UpdateSurvival(float dt) {
    if (isDead) return;

    // 1. OBRAŻENIA OD UPADKU
    // Sprawdzamy nagłą zmianę prędkości z dużej ujemnej na 0 (uderzenie)
    if (onGround && lastYVelocity < -12.0f) {
        // Oblicz obrażenia (im szybciej spadał, tym mocniej boli)
        // -12 to bezpieczna prędkość. Każdy punkt powyżej to obrażenia.
        float damage = (abs(lastYVelocity) - 12.0f) * 0.5f;
        playerHealth -= damage;

        // Dźwięk uderzenia (jeśli masz plik hurt.mp3)
        // ma_engine_play_sound(&g_audioEngine, "hurt.mp3", NULL);
        std::cout << "Ala! Obrazenia od upadku: " << damage << std::endl;
    }
    // Zapisz prędkość dla następnej klatki
    lastYVelocity = playerVelocity.y;

    // 2. TOPIENIE SIĘ (Drowning)
    glm::vec3 headPos = camera.Position + glm::vec3(0.0f, PLAYER_EYE_HEIGHT, 0.0f);
    uint8_t headBlock = GetBlockGlobal(round(headPos.x), round(headPos.y), round(headPos.z));
    glm::vec3 feetPos = camera.Position; // Pozycja nóg
    uint8_t feetBlock = GetBlockGlobal(round(feetPos.x), round(feetPos.y), round(feetPos.z));

    bool headInWater = (headBlock == BLOCK_ID_WATER || (headBlock >= 13 && headBlock <= 16));

    if (headInWater && !camera.flyingMode) {
        airTimer -= dt;
        if (airTimer <= 0.0f) {
            playerHealth -= 1.0f; // Zadaj obrażenia co chwilę
            airTimer = 1.0f;      // Reset timer obrażeń (co 1 sekunda)
            // ma_engine_play_sound(&g_audioEngine, "hurt.mp3", NULL);
            std::cout << "Bul bul! Topie sie!" << std::endl;
        }
    } else {
        // Regeneruj powietrze, gdy głowa nad wodą
        airTimer += dt * 5.0f;
        if (airTimer > 10.0f) airTimer = 10.0f;
    }
    if (isAnyLava(headBlock) || isAnyLava(feetBlock)) { // Sprawdź głowę i nogi
        playerHealth -= 2.0f * dt * 5.0f; // Bardzo szybkie obrażenia
        // Spowolnienie
        // ...
    }

    // 3. ŚMIERĆ
    if (playerHealth <= 0.0f) {
        playerHealth = 0.0f;
        isDead = true;
        std::cout << "GAME OVER" << std::endl;
        // Tutaj można dodać wyrzucenie ekwipunku na ziemię!
    }
}
void InitClouds() {
    // 1. Generuj teksturę chmur szumem Perlina
    int width = 128;
    int height = 128;
    std::vector<unsigned char> data(width * height * 4); // RGBA

    float scale = 0.08f; // Rozmiar "kłębków"

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float noise = stb_perlin_noise3(x * scale, y * scale, 0.0f, 0, 0, 0);

            // Próg: jeśli szum > 0.2, to jest chmura. Inaczej pusto.
            unsigned char alpha = (noise > 0.2f) ? 255 : 0;

            int index = (y * width + x) * 4;
            data[index + 0] = 255; // R
            data[index + 1] = 255; // G
            data[index + 2] = 255; // B
            data[index + 3] = alpha; // A
        }
    }

    // 2. Stwórz Teksturę OpenGL
    glGenTextures(1, &cloudTexture);
    glBindTexture(GL_TEXTURE_2D, cloudTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data.data());

    // KLUCZOWE: GL_NEAREST sprawi, że chmury będą "klockowate", a nie rozmyte!
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT); // Powtarzaj teksturę w nieskończoność
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    // 3. Stwórz Wielki Prostokąt (Plane)
    // Rozmiar 512x512 powinien pokryć cały widok RENDER_DISTANCE
    float size = 512.0f;
    float heightY = 100.0f; // Wysokość chmur

    // Powtarzamy teksturę 4 razy na tym obszarze
    float uvRep = 4.0f;

    float vertices[] = {
        // Pos(XYZ)           // UV
        -size, heightY, -size,  0.0f, 0.0f,
         size, heightY, -size,  uvRep, 0.0f,
         size, heightY,  size,  uvRep, uvRep,
        -size, heightY, -size,  0.0f, 0.0f,
         size, heightY,  size,  uvRep, uvRep,
        -size, heightY,  size,  0.0f, uvRep
    };

    glGenVertexArrays(1, &cloudVAO);
    glGenBuffers(1, &cloudVBO);
    glBindVertexArray(cloudVAO);
    glBindBuffer(GL_ARRAY_BUFFER, cloudVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
}
// Funkcja działająca w tle
void ChunkWorkerThread() {
    while (g_isRunning) {
        glm::ivec2 coords;

        // 1. Czy jest coś do zrobienia?
        if (g_chunksToLoad.try_pop(coords)) {

            // 2. Wykonaj CIĘŻKĄ PRACĘ (Konstruktor Chunk liczy cały teren)
            // Uwaga: Używamy globalnych g_selectedWorldName i g_selectedWorldSeed
            Chunk* newChunk = new Chunk(
                glm::vec3(coords.x * CHUNK_WIDTH, 0.0f, coords.y * CHUNK_DEPTH),
                g_selectedWorldName,
                g_selectedWorldSeed
            );

            // 3. Wyślij gotowy obiekt do głównego wątku
            g_chunksReady.push(newChunk);
        }
        else {
            // Jeśli nie ma pracy, śpij chwilę, żeby nie grzać procesora
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}
void UnloadFarChunks() {
    // 1. Oblicz pozycję gracza w koordynatach chunków
    int pCX = static_cast<int>(floor(camera.Position.x / CHUNK_WIDTH));
    int pCZ = static_cast<int>(floor(camera.Position.z / CHUNK_DEPTH));

    // Margines bezpieczeństwa (np. 2 chunki dalej niż mgła)
    // Żeby nie usuwać chunków, które są na granicy widoczności (bo będą migać)
    int unloadDist = RENDER_DISTANCE + 3;

    // Lista współrzędnych do usunięcia (żeby nie psuć iteratora mapy podczas pętli)
    std::vector<glm::ivec2> toRemove;

    // 2. Przeglądamy wszystkie aktywne chunki
    for (auto const& [pos, chunkPtr] : g_WorldChunks) {
        // Oblicz dystans w osiach (Manhattan distance jest szybszy i pasuje do kwadratowej mapy)
        int distX = abs(pos.x - pCX);
        int distZ = abs(pos.y - pCZ); // W mapie użyliśmy .y jako Z

        // Jeśli chunk jest za daleko
        if (distX > unloadDist || distZ > unloadDist) {
            toRemove.push_back(pos);
        }
    }

    // 3. Usuwamy znalezione chunki
    for (auto pos : toRemove) {
        Chunk* chunkPtr = g_WorldChunks[pos];

        // A. ZAPISZ NA DYSK! (Kluczowe, żeby nie stracić zmian)
        chunkPtr->SaveToFile();

        // B. Usuń z listy 'chunk_storage' (to wywoła destruktor i zwolni VBO/VAO)
        // Musimy użyć remove_if, żeby znaleźć ten konkretny obiekt w liście
        chunk_storage.remove_if([chunkPtr](const Chunk& c){
            return &c == chunkPtr; // Porównaj adresy pamięci
        });

        // C. Usuń wskaźnik z mapy
        g_WorldChunks.erase(pos);
    }

    // Opcjonalne info (tylko jeśli coś usunięto)
    if (!toRemove.empty()) {
        // std::cout << "Usunieto " << toRemove.size() << " starych chunkow." << std::endl;
    }
}
void DrawZombie(Shader& shader, const Entity& e, unsigned int vao) {
    // Tekstura: Zielona (jak trawa)
    // Możesz też dodać nową teksturę UV_ZOMBIE do Chunk.h, jeśli narysujesz go w atlasie
    glm::vec2 uvOff = glm::vec2(UV_GRASS_TOP[0], UV_GRASS_TOP[1]);

    shader.setVec2("u_uvOffset", uvOff);
    shader.setVec2("u_uvOffsetTop", uvOff);
    shader.setVec2("u_uvOffsetBottom", uvOff);
    shader.setInt("u_multiTexture", 0);

    // Animacja chodzenia
    float limbAngle = 0.0f;
    if (e.isWalking) limbAngle = sin(glfwGetTime() * 10.0f) * 45.0f;

    // Animacja ataku (ręce w górze)
    float armsBaseAngle = 90.0f; // Zombie trzyma ręce przed sobą

    glm::mat4 baseModel = glm::mat4(1.0f);
    baseModel = glm::translate(baseModel, e.position);
    // Obrót: Zwróć uwagę, że dla humanoida rotationY musi być dopasowane
    baseModel = glm::rotate(baseModel, glm::radians(-e.rotationY), glm::vec3(0, 1, 0));

    // 1. TUŁÓW (Pionowy)
    {
        glm::mat4 model = baseModel;
        model = glm::translate(model, glm::vec3(0.0f, 1.0f, 0.0f)); // Środek na wys 1.0
        model = glm::scale(model, glm::vec3(0.5f, 0.9f, 0.25f));    // Chudy i wysoki
        shader.setMat4("model", model);
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }

    // 2. GŁOWA
    {
        glm::mat4 model = baseModel;
        model = glm::translate(model, glm::vec3(0.0f, 1.7f, 0.0f));
        model = glm::scale(model, glm::vec3(0.5f, 0.5f, 0.5f));
        shader.setMat4("model", model);
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }

    // 3. NOGI (x2)
    glm::vec3 legPos[] = { glm::vec3(-0.15f, 0.55f, 0.0f), glm::vec3(0.15f, 0.55f, 0.0f) };
    for (int i = 0; i < 2; i++) {
        glm::mat4 model = baseModel;
        model = glm::translate(model, legPos[i]);

        float angle = (i == 0) ? limbAngle : -limbAngle;
        model = glm::rotate(model, glm::radians(angle), glm::vec3(1, 0, 0));
        model = glm::translate(model, glm::vec3(0, -0.3f, 0)); // Przesuń środek nogi w dół

        model = glm::scale(model, glm::vec3(0.22f, 0.8f, 0.22f));
        shader.setMat4("model", model);
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }

    // 4. RĘCE (x2) - Wyciągnięte przed siebie
    glm::vec3 armPos[] = { glm::vec3(-0.35f, 1.3f, 0.0f), glm::vec3(0.35f, 1.3f, 0.0f) };
    for (int i = 0; i < 2; i++) {
        glm::mat4 model = baseModel;
        model = glm::translate(model, armPos[i]);

        // Machanie rękami (mniej niż nogami) + bazowe uniesienie
        float angle = (i == 0) ? -limbAngle * 0.5f : limbAngle * 0.5f;
        model = glm::rotate(model, glm::radians(-armsBaseAngle + angle), glm::vec3(1, 0, 0));
        model = glm::translate(model, glm::vec3(0, -0.3f, 0));

        model = glm::scale(model, glm::vec3(0.2f, 0.8f, 0.2f));
        shader.setMat4("model", model);
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }
}
void InitBloom() {
    // 1. Sprawdź wymiary
    std::cout << "DEBUG: InitBloom Start. Wymiary: " << SCR_WIDTH << "x" << SCR_HEIGHT << std::endl;

    if (SCR_WIDTH == 0 || SCR_HEIGHT == 0) {
        std::cout << "BLAD KRYTYCZNY: Probujemy utworzyc bufor o rozmiarze 0! Przerywam." << std::endl;
        return;
    }

    // 2. Czyszczenie starych (bez zmian)
    if (hdrFBO != 0) {
        glDeleteFramebuffers(1, &hdrFBO);
        glDeleteTextures(2, colorBuffers);
        glDeleteRenderbuffers(1, &rboDepth);
        glDeleteFramebuffers(2, pingpongFBO);
        glDeleteTextures(2, pingpongColorbuffers);
    }

    // 3. Tworzenie Głównego FBO
    glGenFramebuffers(1, &hdrFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);

    glGenTextures(2, colorBuffers);
    for (unsigned int i = 0; i < 2; i++) {
        glBindTexture(GL_TEXTURE_2D, colorBuffers[i]);

        // Próba utworzenia tekstury HDR (RGBA16F)
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, colorBuffers[i], 0);
    }

    unsigned int attachments[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    glDrawBuffers(2, attachments);

    // Renderbuffer (Głębia)
    glGenRenderbuffers(1, &rboDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, rboDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, SCR_WIDTH, SCR_HEIGHT);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rboDepth);

    // --- DIAGNOSTYKA BŁĘDU ---
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        std::cout << "BLAD KRYTYCZNY: HDR Framebuffer niekompletny! Kod: " << status << std::endl;
        if (status == GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT) std::cout << "Przyczyna: Problem z zalacznikiem (Tekstura/RBO)." << std::endl;
        if (status == GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT) std::cout << "Przyczyna: Brak zalacznikow." << std::endl;
        if (status == GL_FRAMEBUFFER_UNSUPPORTED) std::cout << "Przyczyna: Format GL_RGBA16F nieobslugiwany przez karte!" << std::endl;
    } else {
        std::cout << "SUKCES: HDR Framebuffer utworzony poprawnie." << std::endl;
    }
    // -------------------------

    // 4. Ping-Pong FBO (Blur)
    glGenFramebuffers(2, pingpongFBO);
    glGenTextures(2, pingpongColorbuffers);
    for (unsigned int i = 0; i < 2; i++) {
        glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[i]);
        glBindTexture(GL_TEXTURE_2D, pingpongColorbuffers[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pingpongColorbuffers[i], 0);

        // Sprawdzenie PingPong FBO
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            std::cout << "BLAD: PingPong FBO [" << i << "] niekompletny!" << std::endl;
    }

    // 5. Quad (tylko raz)
    if (quadVAO == 0) {
        float quadVertices[] = {
            -1.0f,  1.0f,  0.0f, 1.0f,
            -1.0f, -1.0f,  0.0f, 0.0f,
             1.0f, -1.0f,  1.0f, 0.0f,
            -1.0f,  1.0f,  0.0f, 1.0f,
             1.0f, -1.0f,  1.0f, 0.0f,
             1.0f,  1.0f,  1.0f, 1.0f
        };
        glGenVertexArrays(1, &quadVAO);
        glGenBuffers(1, &quadVBO);
        glBindVertexArray(quadVAO);
        glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
        glBindVertexArray(0);
    }

    // Powrót do domyślnego bufora
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// Funkcja rysująca Quad
void RenderQuad() {
    if (quadVAO == 0) InitBloom(); // Zabezpieczenie
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}
void UpdateFurnaces(float dt) {
    for (auto& [pos, furnace] : g_furnaces) {
        bool wasBurning = (furnace.burnTime > 0.0f);
        bool dirty = false; // Czy coś się zmieniło (np. zużyto węgiel)

        // 1. Zmniejszanie czasu palenia
        if (furnace.burnTime > 0.0f) {
            furnace.burnTime -= dt;
        }

        // 2. Sprawdź czy możemy coś przetopić
        // (Mamy wsad + wsad jest przetapialny + wynik pasuje)
        bool canSmelt = false;
        uint8_t resultID = BLOCK_ID_AIR;

        if (furnace.input.itemID == BLOCK_ID_IRON_ORE) resultID = ITEM_ID_IRON_INGOT;
        else if (furnace.input.itemID == BLOCK_ID_GOLD_ORE) resultID = BLOCK_ID_GOLD_ORE; // Tu daj sztabkę złota jak dodasz
        else if (furnace.input.itemID == BLOCK_ID_SAND) resultID = BLOCK_ID_ICE; // Piasek -> Szkło (Ice jako placeholder)
        else if (furnace.input.itemID == BLOCK_ID_LOG) resultID = ITEM_ID_COAL;

        if (resultID != BLOCK_ID_AIR) {
            if (furnace.result.itemID == BLOCK_ID_AIR ||
               (furnace.result.itemID == resultID && furnace.result.count < MAX_STACK_SIZE)) {
                canSmelt = true;
            }
        }

        // 3. Dorzuć do pieca jeśli trzeba
        if (canSmelt && furnace.burnTime <= 0.0f) {
            if (furnace.fuel.itemID == ITEM_ID_COAL || furnace.fuel.itemID == BLOCK_ID_LOG) { // Węgiel lub Drewno
                furnace.burnTime = 10.0f; // 10 sekund palenia
                furnace.maxBurnTime = 10.0f;
                furnace.fuel.count--;
                if (furnace.fuel.count <= 0) furnace.fuel = {BLOCK_ID_AIR, 0};
                dirty = true;
            }
        }

        // 4. Proces przetapiania (Cooking)
        if (canSmelt && furnace.burnTime > 0.0f) {
            furnace.cookTime += dt;
            if (furnace.cookTime >= 3.0f) { // 3 sekundy na przetopienie
                furnace.cookTime = 0.0f;
                furnace.input.count--;
                if (furnace.input.count <= 0) furnace.input = {BLOCK_ID_AIR, 0};

                furnace.result.itemID = resultID;
                furnace.result.count++;
            }
        } else {
            // Jeśli ogień zgasł albo zabrano wsad, postęp cofa się
            furnace.cookTime = 0.0f;
        }

        // Opcjonalnie: Jeśli piec się zapalił/zgasł, zaktualizuj oświetlenie chunka!
        // (Wymagałoby zmiany BLOCK_ID_FURNACE na BLOCK_ID_FURNACE_ON i ponownego obliczenia światła)
    }
}