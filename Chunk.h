#ifndef CHUNK_H
#define CHUNK_H

#include <glad/glad.h>
#include <vector>
#include <map>
#include <glm/glm.hpp>
#include <glm/gtc/integer.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <fstream>
#include <string>
#include <sstream>
#include <filesystem>
#include "Shader.h"
// STB_PERLIN jest dołączany z main.cpp

// --- Definicje (bez zmian) ---
const int CHUNK_WIDTH = 16;
const int CHUNK_HEIGHT = 64;
const int CHUNK_DEPTH = 16;
const int SEA_LEVEL = 32;

// --- ID BLOKÓW (bez zmian) ---
#define BLOCK_ID_AIR 0
#define BLOCK_ID_GRASS 1
#define BLOCK_ID_DIRT 2
#define BLOCK_ID_STONE 3
#define BLOCK_ID_WATER 4
#define BLOCK_ID_BEDROCK 5
#define BLOCK_ID_SAND 6
#define BLOCK_ID_SNOW 7
#define BLOCK_ID_ICE 8
#define BLOCK_ID_SANDSTONE 9
#define BLOCK_ID_TORCH 10
#define BLOCK_ID_LOG 11     // <<< NOWOŚĆ
#define BLOCK_ID_LEAVES 12  // <<< NOWOŚĆ
#define BLOCK_ID_WATER_4 13 // Najmocniejszy nurt (zaraz przy źródle)
#define BLOCK_ID_WATER_3 14
#define BLOCK_ID_WATER_2 15
#define BLOCK_ID_WATER_1 16 // Najsłabszy nurt (koniec strumienia)
#define BLOCK_ID_COAL_ORE    17
#define BLOCK_ID_IRON_ORE    18
#define BLOCK_ID_GOLD_ORE    19
#define BLOCK_ID_DIAMOND_ORE 21
#define BLOCK_ID_LAVA    22 // Źródło
#define BLOCK_ID_LAVA_3  23
#define BLOCK_ID_LAVA_2  24
#define BLOCK_ID_LAVA_1  25

// --- Definicje Atlasu (bez zmian) ---
const float ATLAS_COLS = 8.0f;
const float ATLAS_ROWS = 3.0f;
const float UV_STEP_U = 1.0f / ATLAS_COLS;
const float UV_STEP_V = 1.0f / ATLAS_ROWS;

const float UV_GRASS_TOP[2] = { 0.0f * UV_STEP_U, 2.0f * UV_STEP_V };
const float UV_GRASS_SIDE[2] = { 1.0f * UV_STEP_U, 2.0f * UV_STEP_V };
const float UV_DIRT[2] = { 0.0f * UV_STEP_U, 1.0f * UV_STEP_V };
const float UV_STONE[2] = { 1.0f * UV_STEP_U, 1.0f * UV_STEP_V };
const float UV_WATER[2] = { 0.0f * UV_STEP_U, 0.0f * UV_STEP_V };
const float UV_BEDROCK[2] = { 1.0f * UV_STEP_U, 0.0f * UV_STEP_V };
const float UV_SAND[2] = { 2.0f * UV_STEP_U, 2.0f * UV_STEP_V };
const float UV_SNOW[2] = { 2.0f * UV_STEP_U, 1.0f * UV_STEP_V };
const float UV_ICE[2] = { 2.0f * UV_STEP_U, 0.0f * UV_STEP_V };
const float UV_SANDSTONE[2] = { 3.0f * UV_STEP_U, 2.0f * UV_STEP_V };
const float UV_TORCH[2] = { 3.0f * UV_STEP_U, 1.0f * UV_STEP_V }; // <<< DODAJ (dostosuj to do swojego atlas.png!
const float UV_LOG_SIDE[2] = { 4.0f * UV_STEP_U, 2.0f * UV_STEP_V };
const float UV_LOG_TOP[2]  = { 4.0f * UV_STEP_U, 1.0f * UV_STEP_V };
const float UV_LEAVES[2]   = { 3.0f * UV_STEP_U, 0.0f * UV_STEP_V };
const float UV_PICKAXE[2] = { 4.0f * UV_STEP_U, 0.0f * UV_STEP_V };
const float UV_COAL_ORE[2]    = { 5.0f * UV_STEP_U, 2.0f * UV_STEP_V };
const float UV_IRON_ORE[2]    = { 5.0f * UV_STEP_U, 1.0f * UV_STEP_V };
const float UV_GOLD_ORE[2]    = { 5.0f * UV_STEP_U, 0.0f * UV_STEP_V };
const float UV_DIAMOND_ORE[2] = { 6.0f * UV_STEP_U, 2.0f * UV_STEP_V };
const float UV_LAVA[2] = { 6.0f * UV_STEP_U, 1.0f * UV_STEP_V }; // Dostosuj do swojego atlasu!

// --- Dane Geometrii (bez zmian) ---
const float baseFaces[][18] = {
    // T1(V1,V2,V3), T2(V1,V3,V4)
    // Góra (+Y)
    {-0.5f, 0.5f,  0.5f,  0.5f, 0.5f,  0.5f,  0.5f, 0.5f, -0.5f, -0.5f, 0.5f,  0.5f,  0.5f, 0.5f, -0.5f, -0.5f, 0.5f, -0.5f},
    // Dół (-Y)
    {-0.5f,-0.5f, -0.5f,  0.5f,-0.5f, -0.5f,  0.5f,-0.5f,  0.5f, -0.5f,-0.5f, -0.5f,  0.5f,-0.5f,  0.5f, -0.5f,-0.5f,  0.5f},
    // Przód (+Z)
    {-0.5f, -0.5f, 0.5f,  0.5f, -0.5f, 0.5f,  0.5f,  0.5f, 0.5f, -0.5f, -0.5f, 0.5f,  0.5f,  0.5f, 0.5f, -0.5f,  0.5f, 0.5f},
    // Tył (-Z)
    { 0.5f, -0.5f,-0.5f, -0.5f, -0.5f,-0.5f, -0.5f,  0.5f,-0.5f,  0.5f, -0.5f,-0.5f, -0.5f,  0.5f,-0.5f,  0.5f,  0.5f,-0.5f},
    // Prawo (+X)
    { 0.5f, -0.5f, 0.5f,  0.5f, -0.5f,-0.5f,  0.5f,  0.5f,-0.5f,  0.5f, -0.5f, 0.5f,  0.5f,  0.5f,-0.5f,  0.5f,  0.5f, 0.5f},
    // Lewo (-X)
    {-0.5f, -0.5f,-0.5f, -0.5f, -0.5f, 0.5f, -0.5f,  0.5f, 0.5f, -0.5f, -0.5f,-0.5f, -0.5f,  0.5f, 0.5f, -0.5f,  0.5f,-0.5f}
};
const float baseUVs[][12] = {
    { 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f },
    { 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f },
    { 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f },
    { 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f },
    { 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f },
    { 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f }
};

// --- NOWOŚĆ: WEKTORY NORMALNE DLA KAŻDEJ ŚCIANY ---
const float baseNormals[][18] = {
    // Góra (+Y)
    { 0.0f,  1.0f,  0.0f,  0.0f,  1.0f,  0.0f,  0.0f,  1.0f,  0.0f,  0.0f,  1.0f,  0.0f,  0.0f,  1.0f,  0.0f,  0.0f,  1.0f,  0.0f },
    // Dół (-Y)
    { 0.0f, -1.0f,  0.0f,  0.0f, -1.0f,  0.0f,  0.0f, -1.0f,  0.0f,  0.0f, -1.0f,  0.0f,  0.0f, -1.0f,  0.0f,  0.0f, -1.0f,  0.0f },
    // Przód (+Z)
    { 0.0f,  0.0f,  1.0f,  0.0f,  0.0f,  1.0f,  0.0f,  0.0f,  1.0f,  0.0f,  0.0f,  1.0f,  0.0f,  0.0f,  1.0f,  0.0f,  0.0f,  1.0f },
    // Tył (-Z)
    { 0.0f,  0.0f, -1.0f,  0.0f,  0.0f, -1.0f,  0.0f,  0.0f, -1.0f,  0.0f,  0.0f, -1.0f,  0.0f,  0.0f, -1.0f,  0.0f,  0.0f, -1.0f },
    // Prawo (+X)
    { 1.0f,  0.0f,  0.0f,  1.0f,  0.0f,  0.0f,  1.0f,  0.0f,  0.0f,  1.0f,  0.0f,  0.0f,  1.0f,  0.0f,  0.0f,  1.0f,  0.0f,  0.0f },
    // Lewo (-X)
    {-1.0f,  0.0f,  0.0f, -1.0f,  0.0f,  0.0f, -1.0f,  0.0f,  0.0f, -1.0f,  0.0f,  0.0f, -1.0f,  0.0f,  0.0f, -1.0f,  0.0f,  0.0f }
};
const float torchVertices[] = {
    // Pierwsza płaszczyzna (X-Z)
    -0.25f, -0.5f,  0.0f,   0.25f, -0.5f,  0.0f,   0.25f,  0.5f,  0.0f,
    -0.25f, -0.5f,  0.0f,   0.25f,  0.5f,  0.0f,  -0.25f,  0.5f,  0.0f,
    // Druga płaszczyzna (Z-X)
     0.0f, -0.5f, -0.25f,   0.0f, -0.5f,  0.25f,   0.0f,  0.5f,  0.25f,
     0.0f, -0.5f, -0.25f,   0.0f,  0.5f,  0.25f,   0.0f,  0.5f, -0.25f
};
// Współrzędne UV dla pochodni
const float torchUVs[] = {
    0.0f, 0.0f,   1.0f, 0.0f,   1.0f, 1.0f,
    0.0f, 0.0f,   1.0f, 1.0f,   0.0f, 1.0f,
    0.0f, 0.0f,   1.0f, 0.0f,   1.0f, 1.0f,
    0.0f, 0.0f,   1.0f, 1.0f,   0.0f, 1.0f
};
// Wektory normalne (aby oświetlenie działało)
const float torchNormals[] = {
    0.0f, 0.0f, 1.0f,   0.0f, 0.0f, 1.0f,   0.0f, 0.0f, 1.0f,
    0.0f, 0.0f, 1.0f,   0.0f, 0.0f, 1.0f,   0.0f, 0.0f, 1.0f,
    1.0f, 0.0f, 0.0f,   1.0f, 0.0f, 0.0f,   1.0f, 0.0f, 0.0f,
    1.0f, 0.0f, 0.0f,   1.0f, 0.0f, 0.0f,   1.0f, 0.0f, 0.0f
};

// Deklaracja globalnej mapy
class Chunk;
struct CompareIveco2 {
    bool operator()(const glm::ivec2& a, const glm::ivec2& b) const {
        if (a.x != b.x) return a.x < b.x;
        return a.y < b.y;
    }
};
extern std::map<glm::ivec2, Chunk*, CompareIveco2> g_WorldChunks;


class Chunk
{
public:
    unsigned int VAO_solid, VBO_solid;
    int vertexCount_solid;
    unsigned int VAO_transparent, VBO_transparent;
    int vertexCount_transparent;
    unsigned int VAO_foliage, VBO_foliage; // <<< NOWOŚĆ
    int vertexCount_foliage;
    std::string worldName;
    int worldSeed;
    glm::vec3 position;
    uint8_t blocks[CHUNK_WIDTH][CHUNK_HEIGHT][CHUNK_DEPTH]{};
    uint8_t lightMap[CHUNK_WIDTH][CHUNK_HEIGHT][CHUNK_DEPTH];

    // --- Funkcje Zapisu/Odczytu (bez zmian) ---
    std::string GetChunkFileName() {
        std::stringstream ss;
        ss << "worlds/" << worldName << "/chunk_" << static_cast<int>(position.x) << "_" << static_cast<int>(position.z) << ".dat";
        return ss.str();
    }
    void SaveToFile() {
        std::string fileName = GetChunkFileName();
        std::ofstream outFile(fileName, std::ios::binary);
        if (outFile.is_open()) {
            outFile.write(reinterpret_cast<char*>(blocks), sizeof(blocks));
            outFile.close();
        }
    }
    float vertexAO(bool side1, bool side2, bool corner) {
        if (side1 && side2) return 0.0f; // Róg całkowicie zasłonięty -> Najciemniej
        return 3.0f - ((side1 ? 1 : 0) + (side2 ? 1 : 0) + (corner ? 1 : 0));
    }
    float vertexLight(int x, int y, int z, bool s1, bool s2, bool c,
                      int nx, int ny, int nz,
                      int dx, int dy, int dz)
    {
        // Pobieramy światło z 4 bloków otaczających ten róg
        // 1. Blok bezpośrednio przy ścianie (nx, ny, nz)
        int l_face = 0;
        if (nx >= 0 && nx < CHUNK_WIDTH && ny >= 0 && ny < CHUNK_HEIGHT && nz >= 0 && nz < CHUNK_DEPTH)
            l_face = lightMap[nx][ny][nz];
        else l_face = 15; // Krawędź świata = słońce

        // 2. Side 1
        int l_s1 = 0;
        if (nx+dx >= 0 && nx+dx < CHUNK_WIDTH && ny+dy >= 0 && ny+dy < CHUNK_HEIGHT && nz+dz >= 0 && nz+dz < CHUNK_DEPTH)
            l_s1 = lightMap[nx+dx][ny+dy][nz+dz]; // Uwaga: dx/dy/dz dodane do N
        else l_s1 = 15;

        // 3. Side 2 (To było skomplikowane w AO, tutaj upraszczamy - bierzemy z lightMap)
        // W AO mieliśmy bool s1, s2. Tutaj musimy pobrać wartości z mapy.
        // Dla uproszczenia: Zrobimy średnią z samego bloku przed twarzą i jego sąsiadów.

        return (float)l_face / 15.0f; // NA RAZIE ZOSTAWMY PROSTĄ WERSJĘ, ALE ZARAZ JĄ ULEPSZYMY W PĘTLI
    }
    bool LoadFromFile() {
        std::string fileName = GetChunkFileName();
        if (!std::filesystem::exists(fileName)) {
            return false;
        }
        std::ifstream inFile(fileName, std::ios::binary);
        if (inFile.is_open()) {
            inFile.read(reinterpret_cast<char*>(blocks), sizeof(blocks));
            inFile.close();
            return true;
        }
        return false;
    }
    void SetBlockSafe(int x, int y, int z, uint8_t id) {
        if (x >= 0 && x < CHUNK_WIDTH && y >= 0 && y < CHUNK_HEIGHT && z >= 0 && z < CHUNK_DEPTH) {
            // Nie nadpisuj pnia liśćmi!
            if (blocks[x][y][z] == BLOCK_ID_LOG && id == BLOCK_ID_LEAVES) return;

            // Stawiaj tylko w powietrzu (chyba że to pień, on może przebijać liście)
            if (blocks[x][y][z] == BLOCK_ID_AIR || (id == BLOCK_ID_LOG)) {
                blocks[x][y][z] = id;
            }
        }
    }

    // 2. Generowanie kuli liści (Korona)
    void CreateLeafCluster(int x, int y, int z, int radius) {
        for (int dx = -radius; dx <= radius; dx++) {
            for (int dy = -radius; dy <= radius; dy++) {
                for (int dz = -radius; dz <= radius; dz++) {
                    // Równanie sfery: x^2 + y^2 + z^2 <= r^2
                    // Dodajemy trochę losowości (0.7), żeby nie była idealnie okrągła
                    if ((dx*dx + dy*dy + dz*dz) <= (radius * radius) + (rand() % 2)) {
                        // Losowa szansa na brak liścia (żeby korona była "poszarpana")
                        if (rand() % 10 != 0) {
                            SetBlockSafe(x + dx, y + dy, z + dz, BLOCK_ID_LEAVES);
                        }
                    }
                }
            }
        }
    }

    // 3. Proceduralne Drzewo (Z gałęziami)
    void GenerateProceduralTree(int x, int y, int z) {
        int height = 5 + (rand() % 3); // Wysokość pnia: 5-7 bloków

        // A. Budujemy Główny Pień
        for (int i = 0; i < height; i++) {
            SetBlockSafe(x, y + i, z, BLOCK_ID_LOG);
        }

        // B. Główna korona na szczycie
        CreateLeafCluster(x, y + height, z, 2);

        // C. Gałęzie boczne (Branches)
        // Próbujemy stworzyć 3-4 gałęzie
        int numBranches = 3 + (rand() % 2);

        for (int i = 0; i < numBranches; i++) {
            // Gałęzie zaczynają się w połowie wysokości drzewa
            int branchStartH = 3 + (rand() % (height - 3));

            // Losowy kierunek (-1, 0, 1)
            int dirX = (rand() % 3) - 1;
            int dirZ = (rand() % 3) - 1;

            // Unikaj gałęzi, która nie idzie w żadną stronę (0,0)
            if (dirX == 0 && dirZ == 0) dirX = 1;

            // Długość gałęzi
            int branchLength = 2 + (rand() % 2);

            // Budujemy gałąź (ukośnie w górę)
            int bx = x;
            int by = y + branchStartH;
            int bz = z;

            for (int b = 0; b < branchLength; b++) {
                bx += dirX;
                by += 1; // Gałąź idzie zawsze w górę
                bz += dirZ;
                SetBlockSafe(bx, by, bz, BLOCK_ID_LOG);
            }

            // D. Mała kępka liści na końcu gałęzi
            CreateLeafCluster(bx, by, bz, 1); // Promień 1 (mała kępka)
        }
    }

    // --- ENUM BIOMÓW (bez zmian) ---
    enum BiomeType {
        BIOME_PLAINS,
        BIOME_DESERT,
        BIOME_SNOWY,
        BIOME_FOREST,    // <<< NOWOŚĆ
        BIOME_MOUNTAINS  // <<< NOWOŚĆ
    };

    // --- KONSTRUKTOR Z BIOMAMI (bez zmian) ---
    explicit Chunk(glm::vec3 pos, std::string worldName, int worldSeed) :
        VAO_solid(0), VBO_solid(0), vertexCount_solid(0),
        VAO_transparent(0), VBO_transparent(0), vertexCount_transparent(0),VBO_foliage(0), vertexCount_foliage(0),
        position(pos), worldName(worldName), worldSeed(worldSeed)
    {
        if (LoadFromFile()) {
            return;
        }

        // --- Ustawienia Generatora ---
        float terrainNoiseZoom = 0.02f;
        float detailNoiseZoom = 0.08f;
        float terrainAmplitude = 16.0f;
        float baseHeight = 32.0f;

        float biomeNoiseZoom = 0.005f;
        float biomeTemperatureThreshold = 0.5f;
        float biomeSnowThreshold = -0.3f;

        float cavernNoiseZoom = 0.03f;
        float cavernThreshold = 0.5f;
        const int caveStartDepth = 5;
        const int CAVE_WATER_LEVEL = 10;
        float aquiferNoiseZoom = 0.04f;
        float aquiferThreshold = 0.90f;
        float bedrockNoiseZoom = 0.15f;
        float bedrockThreshold = 0.3f;
        int heightMap[CHUNK_WIDTH][CHUNK_DEPTH];

        // --- ETAP 1: Mapa Wysokości, Biomów i Ląd ---
        for (int x = 0; x < CHUNK_WIDTH; x++) {
            for (int z = 0; z < CHUNK_DEPTH; z++) {

                float globalX = (float)x + position.x;
                float globalZ = (float)z + position.z;
                float seed_offset = (float)worldSeed;

                // 1. Oblicz Biomy (Szum temperatury/wilgotności)
                // Zmieniamy zakresy, żeby zmieścić 5 biomów
                float biomeNoise = stb_perlin_noise3(globalX * biomeNoiseZoom, seed_offset + 5000.0f, globalZ * biomeNoiseZoom, 0, 0, 0);

                BiomeType currentBiome;

                if (biomeNoise > 0.6f)       currentBiome = BIOME_DESERT;    // Bardzo gorąco
                else if (biomeNoise > 0.2f)  currentBiome = BIOME_PLAINS;    // Umiarkowanie
                else if (biomeNoise > -0.3f) currentBiome = BIOME_FOREST;    // Wilgotno (Las)
                else if (biomeNoise > -0.7f) currentBiome = BIOME_MOUNTAINS; // Zimno/Wysoko
                else                         currentBiome = BIOME_SNOWY;     // Bardzo zimno

                // 2. Oblicz wysokość terenu
                // Góry mają mnożnik wysokości, żeby były wyższe
                float heightMultiplier = 1.0f;
                if (currentBiome == BIOME_MOUNTAINS) heightMultiplier = 2.5f; // Góry są 2.5x wyższe

                float baseNoise = stb_perlin_noise3(globalX * terrainNoiseZoom, seed_offset, globalZ * terrainNoiseZoom, 0, 0, 0);
                float detailNoise = stb_perlin_noise3(globalX * detailNoiseZoom, seed_offset + 1000.0f, globalZ * detailNoiseZoom, 0, 0, 0);

                float combinedNoise = baseNoise + (detailNoise * 0.25f);

                // Aplikujemy mnożnik dla gór
                int terrainHeight = (int)(baseHeight + (combinedNoise * terrainAmplitude * heightMultiplier));

                // Zabezpieczenie, żeby nie wyjść poza chunk
                if (terrainHeight >= CHUNK_HEIGHT - 5) terrainHeight = CHUNK_HEIGHT - 5;
                if (terrainHeight < 2) terrainHeight = 2;

                heightMap[x][z] = terrainHeight;

                float bedrockNoise = stb_perlin_noise3(globalX * bedrockNoiseZoom, seed_offset + 2000.0f, globalZ * bedrockNoiseZoom, 0, 0, 0);

                // 3. Ustaw bloki na podstawie biomu
                for (int y = 0; y < CHUNK_HEIGHT; y++)
                {
                    uint8_t blockID = BLOCK_ID_AIR;

                    if (y == 0) blockID = BLOCK_ID_BEDROCK;
                    else if (y == 1) {
                        if (bedrockNoise > bedrockThreshold) blockID = BLOCK_ID_BEDROCK;
                        else blockID = BLOCK_ID_STONE;
                    }
                    else if (y > terrainHeight) {
                        blockID = BLOCK_ID_AIR;
                    }
                    else // Jesteśmy w terenie
                    {
                        if (currentBiome == BIOME_DESERT) {
                            if (y == terrainHeight) blockID = BLOCK_ID_SAND;
                            else if (y > terrainHeight - 4) blockID = BLOCK_ID_SAND;
                            else if (y > terrainHeight - 8) blockID = BLOCK_ID_SANDSTONE;
                            else blockID = BLOCK_ID_STONE;
                        }
                        else if (currentBiome == BIOME_PLAINS) {
                            if (y == terrainHeight) {
                                if (y < SEA_LEVEL - 1) blockID = BLOCK_ID_DIRT;
                                else blockID = BLOCK_ID_GRASS;
                            } else if (y > terrainHeight - 4) blockID = BLOCK_ID_DIRT;
                            else blockID = BLOCK_ID_STONE;
                        }
                        else if (currentBiome == BIOME_FOREST) { // Las wygląda jak Plains, ale drzewa dodamy później
                            if (y == terrainHeight) {
                                if (y < SEA_LEVEL - 1) blockID = BLOCK_ID_DIRT;
                                else blockID = BLOCK_ID_GRASS;
                            } else if (y > terrainHeight - 4) blockID = BLOCK_ID_DIRT;
                            else blockID = BLOCK_ID_STONE;
                        }
                        else if (currentBiome == BIOME_MOUNTAINS) {
                            // W górach na wierzchu jest kamień, a na szczytach śnieg
                            if (y == terrainHeight) {
                                if (y > 55) blockID = BLOCK_ID_SNOW; // Szczyty
                                else if (y > 40) blockID = BLOCK_ID_STONE; // Zbocza
                                else blockID = BLOCK_ID_GRASS; // Podnóża
                            }
                            else blockID = BLOCK_ID_STONE;
                        }
                        else if (currentBiome == BIOME_SNOWY) {
                            if (y == terrainHeight) {
                                if (y < SEA_LEVEL - 1) blockID = BLOCK_ID_ICE; // Zamarznięta plaża
                                else blockID = BLOCK_ID_SNOW;
                            } else if (y > terrainHeight - 4) blockID = BLOCK_ID_DIRT;
                            else blockID = BLOCK_ID_STONE;
                        }
                    }
                    blocks[x][y][z] = blockID;
                }
            }
        }

        // --- ETAP 2: Rzeźbienie Jaskiń (bez zmian) ---


        // --- ETAP 2: Jaskinie 3D ("Cheese Caves") ---

        // --- PARAMETRY DO ZABAWY ---
        // caveScale: Mniejsza liczba = większe, dłuższe tunele (0.04 - 0.08 jest ok)
        float caveScale = 0.05f;

        // caveThreshold: Im mniejsza liczba, tym więcej dziur.
        // 0.40 - 0.50 to zazwyczaj dobre wartości dla stb_perlin.
        // Jeśli ustawisz za mało (np. 0.1), świat będzie pusty. Jeśli za dużo (0.8), brak jaskiń.
        float caveThreshold = 0.40f;

        for (int x = 0; x < CHUNK_WIDTH; x++) {
            for (int z = 0; z < CHUNK_DEPTH; z++) {

                // Pobieramy wysokość terenu w tym miejscu (żeby wiedzieć gdzie jest powierzchnia)
                int terrainHeight = heightMap[x][z];

                // Kopiemy od dołu (nad bedrockiem) AŻ DO SAMEJ POWIERZCHNI
                // Dzięki temu jaskinie mogą utworzyć wejścia w ziemi
                for (int y = 2; y <= terrainHeight; y++) {

                    // 1. Pobierz aktualny blok
                    uint8_t currentBlock = blocks[x][y][z];

                    // 2. Sprawdź, czy można tu kopać
                    bool canCarve = (currentBlock == BLOCK_ID_STONE ||
                                     currentBlock == BLOCK_ID_DIRT ||
                                     currentBlock == BLOCK_ID_GRASS ||
                                     currentBlock == BLOCK_ID_SANDSTONE ||
                                     currentBlock == BLOCK_ID_SAND);

                    if (canCarve)
                    {
                        float globalX = (float)x + position.x;
                        float globalY = (float)y;
                        float globalZ = (float)z + position.z;
                        float seed_offset = (float)worldSeed;

                        // 3. Generuj Szum 3D
                        float noise3D = stb_perlin_noise3(
                            globalX * caveScale,
                            (globalY * caveScale * 1.5f) + seed_offset + 3000.0f,
                            globalZ * caveScale,
                            0, 0, 0
                        );

                        // --- NOWOŚĆ: OCHRONA POWIERZCHNI ---
                        // Obliczamy głębokość (jak daleko jesteśmy od powierzchni)
                        float depth = (float)(terrainHeight - y);

                        // Ustawiamy bazowy próg
                        float dynamicThreshold = caveThreshold; // np. 0.40

                        // Jeśli jesteśmy w górnych 10 blokach (blisko trawy)
                        if (depth < 10.0f) {
                            // Zwiększamy próg liniowo, im bliżej powierzchni.
                            // Na samej powierzchni (depth=0) próg wzrośnie o +0.35
                            // To sprawia, że szum musi być OGROMNY, żeby przebić powierzchnię.
                            float protection = 1.0f - (depth / 10.0f);
                            dynamicThreshold += protection * 0.35f;
                        }
                        // -----------------------------------

                        // 4. Decyzja: Dziura czy Skała?
                        if (noise3D > dynamicThreshold) {
                            if (y < 3) {
                                blocks[x][y][z] = BLOCK_ID_LAVA;
                            }
                            else {
                                // Wyżej jest normalna, pusta jaskinia
                                blocks[x][y][z] = BLOCK_ID_AIR;
                            }
                        }

                    }

                }
            }
        }
        float oreNoiseZoom = 0.15f;

        for (int x = 0; x < CHUNK_WIDTH; x++) {
            for (int z = 0; z < CHUNK_DEPTH; z++) {
                for (int y = 1; y < heightMap[x][z]; y++) {

                    // Rudy generujemy TYLKO w kamieniu
                    if (blocks[x][y][z] == BLOCK_ID_STONE) {

                        float globalX = (float)x + position.x;
                        float globalY = (float)y;
                        float globalZ = (float)z + position.z;
                        float seed = (float)worldSeed;

                        // --- 1. WĘGIEL (Coal) ---
                        // Występuje wszędzie, duże złoża
                        float coalNoise = stb_perlin_noise3(globalX * oreNoiseZoom, globalY * oreNoiseZoom, globalZ * oreNoiseZoom, 0, 0, 0);
                        if (coalNoise > 0.65f) {
                            blocks[x][y][z] = BLOCK_ID_COAL_ORE;
                            continue; // Jeśli to węgiel, nie sprawdzaj innych rud
                        }

                        // --- 2. ŻELAZO (Iron) ---
                        // Występuje poniżej poziomu 50, średnie złoża
                        if (y < 50) {
                            // Przesuwamy seed (+1000), żeby żelazo nie pojawiało się w tym samym miejscu co węgiel
                            float ironNoise = stb_perlin_noise3(globalX * oreNoiseZoom, globalY * oreNoiseZoom, globalZ * oreNoiseZoom + seed + 1000.0f, 0, 0, 0);
                            if (ironNoise > 0.70f) {
                                blocks[x][y][z] = BLOCK_ID_IRON_ORE;
                                continue;
                            }
                        }

                        // --- 3. ZŁOTO (Gold) ---
                        // Występuje głęboko (poniżej 30), rzadkie
                        if (y < 30) {
                            float goldNoise = stb_perlin_noise3(globalX * oreNoiseZoom, globalY * oreNoiseZoom, globalZ * oreNoiseZoom + seed + 2000.0f, 0, 0, 0);
                            if (goldNoise > 0.78f) { // Wyższy próg = rzadsze
                                blocks[x][y][z] = BLOCK_ID_GOLD_ORE;
                                continue;
                            }
                        }

                        // --- 4. DIAMENT (Diamond) ---
                        // Bardzo głęboko (poniżej 16), bardzo rzadkie, małe złoża
                        if (y < 16) {
                            // Używamy większego zooma dla diamentów, żeby żyły były mniejsze
                            float diamondNoise = stb_perlin_noise3(globalX * 0.2f, globalY * 0.2f, globalZ * 0.2f + seed + 3000.0f, 0, 0, 0);
                            if (diamondNoise > 0.82f) { // Bardzo wysoki próg
                                blocks[x][y][z] = BLOCK_ID_DIAMOND_ORE;
                                continue;
                            }
                        }
                    }
                }
            }
        }
        // --- ETAP 3: Generowanie Wody (z uwzględnieniem biomów) ---
        for (int x = 0; x < CHUNK_WIDTH; x++) {
            for (int z = 0; z < CHUNK_DEPTH; z++) {

                float globalX = (float)x + position.x;
                float globalZ = (float)z + position.z;
                float seed_offset = (float)worldSeed;

                // Ponownie sprawdź biom
                float biomeNoise = stb_perlin_noise3(globalX * biomeNoiseZoom, seed_offset + 5000.0f, globalZ * biomeNoiseZoom, 0, 0, 0);
                BiomeType currentBiome = BIOME_PLAINS;
                if (biomeNoise > biomeTemperatureThreshold) currentBiome = BIOME_DESERT;
                else if (biomeNoise < biomeSnowThreshold) currentBiome = BIOME_SNOWY;

                int terrainHeight = heightMap[x][z];

                for (int y = 2; y < CHUNK_HEIGHT; y++) {
                    if (blocks[x][y][z] == BLOCK_ID_AIR) {
                        float globalY = (float)y;

                        // Woda oceaniczna (POPRAWKA - JEST WODA NA PUSTYNI)
                        if (y > terrainHeight && terrainHeight < SEA_LEVEL && y <= SEA_LEVEL) {
                             if (currentBiome == BIOME_SNOWY) {
                                if (y == SEA_LEVEL) blocks[x][y][z] = BLOCK_ID_ICE; // Lód na powierzchni
                                else blocks[x][y][z] = BLOCK_ID_WATER; // Woda pod lodem
                             } else {
                                 blocks[x][y][z] = BLOCK_ID_WATER;
                             }
                        }
                        // Woda w jaskiniach (Akwifery) (POPRAWKA - JEST WODA NA PUSTYNI)
                        else if (y <= terrainHeight && y <= CAVE_WATER_LEVEL) {
                            float aquiferNoise = stb_perlin_noise3(globalX * aquiferNoiseZoom, (globalY * 0.1f) + seed_offset + 4000.0f, globalZ * aquiferNoiseZoom, 0, 0, 0);
                            if (aquiferNoise > aquiferThreshold) {
                                blocks[x][y][z] = BLOCK_ID_WATER;
                            }
                        }
                    }
                }
            }
        }
        float treeNoiseZoom = 0.8f; // Bardziej "zaszumione" rozmieszczenie (żeby nie było regularne)

        int treeMargin = 5; // Sprawdzamy 5 kratek w każdą stronę poza chunk

        for (int x = -treeMargin; x < CHUNK_WIDTH + treeMargin; x++) {
            for (int z = -treeMargin; z < CHUNK_DEPTH + treeMargin; z++) {

                // 1. Obliczamy globalne pozycje (nawet dla sąsiadów)
                float globalX = (float)x + position.x;
                float globalZ = (float)z + position.z;
                float seed_offset = (float)worldSeed;

                // 2. Musimy ponownie obliczyć biom i wysokość w tym punkcie
                // (Nie możemy użyć tablicy heightMap, bo wychodzimy poza zakres 0-15!)

                float biomeNoise = stb_perlin_noise3(globalX * biomeNoiseZoom, seed_offset + 5000.0f, globalZ * biomeNoiseZoom, 0, 0, 0);

                BiomeType currentBiome;
                if (biomeNoise > 0.6f)       currentBiome = BIOME_DESERT;
                else if (biomeNoise > 0.2f)  currentBiome = BIOME_PLAINS;
                else if (biomeNoise > -0.3f) currentBiome = BIOME_FOREST;
                else if (biomeNoise > -0.7f) currentBiome = BIOME_MOUNTAINS;
                else                         currentBiome = BIOME_SNOWY;

                // Obliczanie wysokości (kopia logiki z Etapu 1)
                float heightMultiplier = 1.0f;
                if (currentBiome == BIOME_MOUNTAINS) heightMultiplier = 2.5f;

                float baseNoise = stb_perlin_noise3(globalX * terrainNoiseZoom, seed_offset, globalZ * terrainNoiseZoom, 0, 0, 0);
                float detailNoise = stb_perlin_noise3(globalX * detailNoiseZoom, seed_offset + 1000.0f, globalZ * detailNoiseZoom, 0, 0, 0);
                float combinedNoise = baseNoise + (detailNoise * 0.25f);

                int terrainHeight = (int)(baseHeight + (combinedNoise * terrainAmplitude * heightMultiplier));
                if (terrainHeight >= CHUNK_HEIGHT - 5) terrainHeight = CHUNK_HEIGHT - 5;
                if (terrainHeight < 2) terrainHeight = 2;

                // 3. Sprawdzamy blok na powierzchni (tylko jeśli jesteśmy wewnątrz chunka możemy sprawdzić blocks[][])
                // Ale my jesteśmy w pętli rozszerzonej.
                // Dla uproszczenia: Drzewa rosną na podstawie biomu i wysokości.

                // Ustalamy gęstość drzew
                float localTreeThreshold = 1.0f;
                if (currentBiome == BIOME_DESERT)      localTreeThreshold = 2.0f;
                else if (currentBiome == BIOME_PLAINS) localTreeThreshold = 0.98f;
                else if (currentBiome == BIOME_FOREST) localTreeThreshold = 0.75f;
                else if (currentBiome == BIOME_MOUNTAINS) localTreeThreshold = 0.95f;
                else if (currentBiome == BIOME_SNOWY)  localTreeThreshold = 0.90f;

                if (terrainHeight > SEA_LEVEL)
                {
                    float treeNoise = stb_perlin_noise3(globalX * 0.8f, seed_offset + 6000.0f, globalZ * 0.8f, 0, 0, 0);
                    float normalizedTreeNoise = (treeNoise + 1.0f) * 0.5f;

                    if (normalizedTreeNoise > localTreeThreshold)
                    {
                        // --- SADZIMY DRZEWO ---
                        if (terrainHeight + 10 < CHUNK_HEIGHT) {

                            // Ponieważ SetBlockSafe ma wbudowane sprawdzanie granic (0-15),
                            // możemy próbować stawiać bloki gdziekolwiek.
                            // Jeśli drzewo jest u sąsiada (np. x=-2), ale liść wejdzie do nas (x=0),
                            // to SetBlockSafe go postawi. Jeśli liść będzie poza (x=-1), SetBlockSafe go zignoruje.

                            if (currentBiome == BIOME_SNOWY || currentBiome == BIOME_MOUNTAINS) {
                                // Choinka
                                int trunkH = 6 + rand()%3; // Używamy rand, ale dla spójności lepiej byłoby użyć hasha z pozycji
                                // Dla uproszczenia rand() zadziała, ale drzewa na granicach mogą "migać" przy przeładowaniu.
                                // Na tym etapie to wystarczy.

                                for(int i=1; i<=trunkH; i++) SetBlockSafe(x, terrainHeight+i, z, BLOCK_ID_LOG);

                                for(int ly = trunkH; ly >= 2; ly--) {
                                    int radius = (trunkH - ly) / 2 + 1;
                                    for(int lx=-radius; lx<=radius; lx++) {
                                        for(int lz=-radius; lz<=radius; lz++) {
                                            if (abs(lx)+abs(lz) <= radius)
                                                SetBlockSafe(x+lx, terrainHeight+ly, z+lz, BLOCK_ID_LEAVES);
                                        }
                                    }
                                }
                                SetBlockSafe(x, terrainHeight+trunkH+1, z, BLOCK_ID_LEAVES);
                            }
                            else {
                                // Dąb Proceduralny
                                GenerateProceduralTree(x, terrainHeight + 1, z);
                            }
                        }
                    }
                }
            }
        } // --- KONIEC ETAPU 4 ---
        std::memset(lightMap, 0, sizeof(lightMap));
    } // Koniec konstruktora

    // "Mądry" getBlock (bez zmian)
    uint8_t getBlock(int x, int y, int z) {
        if (y < 0 || y >= CHUNK_HEIGHT) return BLOCK_ID_AIR;
        if (x >= 0 && x < CHUNK_WIDTH && z >= 0 && z < CHUNK_DEPTH) return blocks[x][y][z];

        int globalX = static_cast<int>(position.x) + x;
        int globalZ = static_cast<int>(position.z) + z;
        int chunkX = static_cast<int>(floor(static_cast<float>(globalX) / CHUNK_WIDTH));
        int chunkZ = static_cast<int>(floor(static_cast<float>(globalZ) / CHUNK_DEPTH));
        auto it = g_WorldChunks.find(glm::ivec2(chunkX, chunkZ));
        if (it == g_WorldChunks.end()) return BLOCK_ID_AIR;
        Chunk* neighborChunk = it->second;
        int localX = globalX - (chunkX * CHUNK_WIDTH);
        int localZ = globalZ - (chunkZ * CHUNK_DEPTH);
        if (localX < 0 || localX >= CHUNK_WIDTH || localZ < 0 || localZ >= CHUNK_DEPTH) return BLOCK_ID_AIR;
        return neighborChunk->blocks[localX][y][localZ];
    }
    // Funkcja pobierająca światło (bezpieczna dla granic chunka)
    uint8_t getLight(int x, int y, int z) {
        // 1. Jeśli wewnątrz chunka, zwróć wartość
        if (y >= 0 && y < CHUNK_HEIGHT && x >= 0 && x < CHUNK_WIDTH && z >= 0 && z < CHUNK_DEPTH) {
            return lightMap[x][y][z];
        }

        // 2. Jeśli poza wysokością świata
        if (y < 0) return 0; // Pod dnem jest ciemno
        if (y >= CHUNK_HEIGHT) return 15; // Nad niebem jest jasno

        // 3. Jeśli u sąsiada
        int globalX = static_cast<int>(position.x) + x;
        int globalZ = static_cast<int>(position.z) + z;
        int chunkX = static_cast<int>(floor(static_cast<float>(globalX) / CHUNK_WIDTH));
        int chunkZ = static_cast<int>(floor(static_cast<float>(globalZ) / CHUNK_DEPTH));

        // Szukamy sąsiada w globalnej mapie
        auto it = g_WorldChunks.find(glm::ivec2(chunkX, chunkZ));
        if (it == g_WorldChunks.end()) {
            // WAŻNE: Jeśli chunk nie jest załadowany, zwróć 0 (CIEMNOŚĆ),
            // a nie 15. To naprawia świecące podziemia.
            return 0;
        }

        Chunk* neighborChunk = it->second;
        int localX = globalX - (chunkX * CHUNK_WIDTH);
        int localZ = globalZ - (chunkZ * CHUNK_DEPTH);

        // Zwróć światło sąsiada
        return neighborChunk->lightMap[localX][y][localZ];
    }
    bool isAnyWater(uint8_t id) {
        return id == BLOCK_ID_WATER || (id >= 13 && id <= 16);
    }
    bool isAnyLava(uint8_t id) {
        return id == BLOCK_ID_LAVA || (id >= 23 && id <= 25);
    }

    bool isAnyLiquid(uint8_t id) {
        return isAnyWater(id) || isAnyLava(id);
    }
    // Zwraca wysokość "sufitu" wody (identyczne wartości jak w addFaceToMesh)
    float getWaterHeight(uint8_t id) {
        if (id == BLOCK_ID_WATER)        return 0.40f;
        if (id == BLOCK_ID_WATER_4)      return 0.30f;
        if (id == BLOCK_ID_WATER_3)      return 0.10f;
        if (id == BLOCK_ID_WATER_2)      return -0.10f;
        if (id == BLOCK_ID_WATER_1)      return -0.35f;
        if (id == BLOCK_ID_LAVA)    return 0.40f;
        if (id == BLOCK_ID_LAVA_3)  return 0.20f;
        if (id == BLOCK_ID_LAVA_2)  return -0.10f;
        if (id == BLOCK_ID_LAVA_1)  return -0.35f;
        return -1.0f; // Nie woda
    }
    void CalculateLighting() {
        // 1. Resetuj mapę światła
        std::memset(lightMap, 0, sizeof(lightMap));

        struct LightNode { int x, y, z, level; };
        std::vector<LightNode> queue;

        // --- 1. SŁOŃCE (Światło z nieba) ---
        // Dla każdej kolumny (x, z) idziemy od góry do dołu.
        // Dopóki jest powietrze/woda -> ustawiamy MAX światła.
        // Jak trafimy na blok stały -> przerywamy (cień).
        for (int x = 0; x < CHUNK_WIDTH; x++) {
            for (int z = 0; z < CHUNK_DEPTH; z++) {
                for (int y = CHUNK_HEIGHT - 1; y >= 0; y--) {
                    uint8_t block = blocks[x][y][z];

                    // Co przepuszcza światło pionowo?
                    bool transparent = (block == BLOCK_ID_AIR ||
                                        block == BLOCK_ID_WATER ||
                                        isAnyWater(block) ||
                                        block == BLOCK_ID_LEAVES ||
                                        block == BLOCK_ID_TORCH ||
                                        block == BLOCK_ID_ICE ||
                                        block == BLOCK_ID_SNOW); // Śnieg (półblok?) - uznajmy że przepuszcza lub nie, zależy od Ciebie.

                    if (transparent) {
                        lightMap[x][y][z] = 15; // Pełne słońce
                        queue.push_back({x, y, z, 15}); // Dodaj do kolejki, żeby rozlało się na boki (do jaskiń)
                    } else {
                        break; // Trafiliśmy na dach/ziemię - poniżej jest ciemno
                    }
                }
            }
        }

        // --- 2. POCHODNIE (Dodatkowe źródła) ---
        for (int x = 0; x < CHUNK_WIDTH; x++) {
            for (int z = 0; z < CHUNK_DEPTH; z++) {
                for (int y = 0; y < CHUNK_HEIGHT; y++) {
                    uint8_t block = blocks[x][y][z];

                    // Pochodnia LUB Lawa (każda, nawet płynąca) dają światło
                    if (block == BLOCK_ID_TORCH || isAnyLava(block)) {
                        lightMap[x][y][z] = 15;
                        queue.push_back({x, y, z, 15});
                    }
                }
            }
        }

        // --- 3. PROPAGACJA (Rozlewanie światła - BFS) ---
        int head = 0;
        while(head < queue.size()) {
            LightNode node = queue[head++];

            // Jeśli światło jest już słabe, nie rozlewaj dalej
            if (node.level <= 0) continue;

            // Sprawdź 6 sąsiadów
            int dx[] = {1, -1, 0, 0, 0, 0};
            int dy[] = {0, 0, 1, -1, 0, 0};
            int dz[] = {0, 0, 0, 0, 1, -1};

            for(int i=0; i<6; i++) {
                int nx = node.x + dx[i];
                int ny = node.y + dy[i];
                int nz = node.z + dz[i];

                // Sprawdź granice chunka
                if(nx >= 0 && nx < CHUNK_WIDTH &&
                   ny >= 0 && ny < CHUNK_HEIGHT &&
                   nz >= 0 && nz < CHUNK_DEPTH)
                {
                    uint8_t neighborBlock = blocks[nx][ny][nz];

                    // Czy blok przepuszcza światło?
                    bool isTrans = (neighborBlock == BLOCK_ID_AIR ||
                                    neighborBlock == BLOCK_ID_WATER ||
                                    neighborBlock == BLOCK_ID_LEAVES ||
                                    neighborBlock == BLOCK_ID_TORCH ||
                                    isAnyWater(neighborBlock) ||
                                    neighborBlock == BLOCK_ID_ICE);

                    if (isTrans) {
                        // Jeśli sąsiad jest ciemniejszy niż (my - 1), to go rozświetl
                        if (lightMap[nx][ny][nz] < node.level - 1) {
                            lightMap[nx][ny][nz] = node.level - 1;
                            queue.push_back({nx, ny, nz, node.level - 1});
                        }
                    }
                }
            }
        }
    }
    // --- ZMODYFIKOWANA FUNKCJA buildMesh ---
    void buildMesh()
    {
        if (vertexCount_foliage > 0) { // <<< NOWOŚĆ
            glDeleteVertexArrays(1, &VAO_foliage);
            glDeleteBuffers(1, &VBO_foliage);
            VAO_foliage = 0; VBO_foliage = 0; vertexCount_foliage = 0;
        }
        // Sprzątanie starych meshy
        if (vertexCount_solid > 0) {
            glDeleteVertexArrays(1, &VAO_solid);
            glDeleteBuffers(1, &VBO_solid);
            VAO_solid = 0; VBO_solid = 0; vertexCount_solid = 0;
        }
        if (vertexCount_transparent > 0) {
            glDeleteVertexArrays(1, &VAO_transparent);
            glDeleteBuffers(1, &VBO_transparent);
            VAO_transparent = 0; VBO_transparent = 0; vertexCount_transparent = 0;
        }

        std::vector<float> solidMesh;
        std::vector<float> transparentMesh;
        std::vector<float> foliageMesh;     // <<< NOWOŚĆ: Dla Liści/Pochodni
        for (int x = 0; x < CHUNK_WIDTH; x++) {
            for (int y = 0; y < CHUNK_HEIGHT; y++) {
                for (int z = 0; z < CHUNK_DEPTH; z++) {
                    uint8_t currentBlockID = blocks[x][y][z];
                    if (currentBlockID == BLOCK_ID_AIR) continue;
                    if (currentBlockID == BLOCK_ID_TORCH) {
                        // Pochodnie są "przezroczyste", dodaj je do transparentMesh
                        addTorchToMesh(foliageMesh, x, y, z);
                        continue; // Przejdź do następnego bloku
                    }
                    // -
                    bool isTransparent = (isAnyWater(currentBlockID) || currentBlockID == BLOCK_ID_ICE);

                    bool isFoliage = (currentBlockID == BLOCK_ID_LEAVES);

                    uint8_t neighbors[6];
                    neighbors[0] = getBlock(x, y + 1, z); // Góra
                    neighbors[1] = getBlock(x, y - 1, z); // Dół
                    neighbors[2] = getBlock(x, y, z + 1); // Przód
                    neighbors[3] = getBlock(x, y, z - 1); // Tył
                    neighbors[4] = getBlock(x + 1, y, z); // Prawo
                    neighbors[5] = getBlock(x - 1, y, z); // Lewo

                    for(int i = 0; i < 6; i++) {
                        uint8_t neighborID = neighbors[i];

                        // --- POPRAWIONA LOGIKA CULLINGU (z poprzedniego kroku) ---
                        if (isTransparent)
                        {
                            bool shouldDraw = false;

                            if (isAnyWater(currentBlockID)) {
                                // Jeśli sąsiad to woda, rysuj tylko gdy jest niżej
                                if (isAnyWater(neighborID)) {
                                    if (getWaterHeight(currentBlockID) > getWaterHeight(neighborID)) shouldDraw = true;
                                }
                                // Rysuj jeśli sąsiad to powietrze, liście, pochodnia itp. (ale nie lód)
                                else if (neighborID != BLOCK_ID_ICE) {
                                    shouldDraw = true;
                                }
                            }
                            else { // Lód
                                // Lód rysuje się, jeśli sąsiad nie jest lodem i nie jest solidny (np. powietrze)
                                if (neighborID != BLOCK_ID_ICE &&
                                    (neighborID == BLOCK_ID_AIR || isAnyWater(neighborID) || neighborID == BLOCK_ID_LEAVES || neighborID == BLOCK_ID_TORCH))
                                {
                                    shouldDraw = true;
                                }
                            }

                            if (shouldDraw) {
                                addFaceToMesh(transparentMesh, i, x, y, z, currentBlockID);
                            }
                        }
                        // --- PRZYPADEK 2: LIŚCIE (Foliage) ---
                        else if (isFoliage)
                        {
                            // Liście rysują się wszędzie, chyba że stykają się z innymi liśćmi
                            if (neighborID != BLOCK_ID_LEAVES) {
                                addFaceToMesh(foliageMesh, i, x, y, z, currentBlockID);
                            }
                        }
                        // --- PRZYPADEK 3: BLOKI STAŁE (Solid) ---
                        // TEGO BRAKOWAŁO! To tutaj rysujemy ziemię, kamień, drewno.
                        else
                        {
                            // Logika dla LAWY (żeby łączyła się ze sobą)
                            if (isAnyLava(currentBlockID)) {
                                bool shouldDraw = true;
                                if (isAnyLava(neighborID)) {
                                    // Nie rysuj ściany między lawą, chyba że sąsiad jest niższy
                                    if (getWaterHeight(currentBlockID) <= getWaterHeight(neighborID)) {
                                        shouldDraw = false;
                                    }
                                }
                                else if (!isAnyWater(neighborID) && neighborID != BLOCK_ID_AIR && neighborID != BLOCK_ID_LEAVES && neighborID != BLOCK_ID_TORCH) {
                                    // Sąsiad jest solidny (np. kamień) -> nie rysuj
                                    shouldDraw = false;
                                }

                                if (shouldDraw) addFaceToMesh(solidMesh, i, x, y, z, currentBlockID);
                            }
                            // Logika dla reszty bloków stałych
                            else {
                                // Rysuj, jeśli sąsiad jest "przezroczysty" (powietrze, woda, lawa, liście...)
                                if (neighborID == BLOCK_ID_AIR ||
                                    isAnyWater(neighborID) ||
                                    isAnyLava(neighborID) ||  // <<< Lawa odsłania kamień
                                    neighborID == BLOCK_ID_ICE ||
                                    neighborID == BLOCK_ID_LEAVES ||
                                    neighborID == BLOCK_ID_TORCH)
                                {
                                    addFaceToMesh(solidMesh, i, x, y, z, currentBlockID);
                                }
                            }
                        }
                    }
                }
            }
        }

        // --- ZMIANA: Stride jest teraz 8 (XYZ + UV + NX NY NZ) ---
        if (!solidMesh.empty()) {
            vertexCount_solid = static_cast<int>(solidMesh.size() / 9); // DZIELIMY PRZEZ 8
            glGenVertexArrays(1, &VAO_solid);
            glGenBuffers(1, &VBO_solid);
            glBindVertexArray(VAO_solid);
            glBindBuffer(GL_ARRAY_BUFFER, VBO_solid);
            glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(solidMesh.size() * sizeof(float)), solidMesh.data(), GL_STATIC_DRAW);

            GLsizei stride = 10 * sizeof(float); // 8 floatów na wierzchołek
            // Pozycja (X,Y,Z)
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
            glEnableVertexAttribArray(0);
            // Współrzędne Tekstury (U,V)
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
            glEnableVertexAttribArray(1);
            // Wektory Normalne (NX,NY,NZ)
            glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, (void*)(5 * sizeof(float)));
            glEnableVertexAttribArray(2);
            glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, stride, (void*)(8 * sizeof(float)));
            glEnableVertexAttribArray(3);
            glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, stride, (void*)(9 * sizeof(float)));
            glEnableVertexAttribArray(4);
        }

        if (!transparentMesh.empty()) {
            vertexCount_transparent = static_cast<int>(transparentMesh.size() / 9); // DZIELIMY PRZEZ 8
            glGenVertexArrays(1, &VAO_transparent);
            glGenBuffers(1, &VBO_transparent);
            glBindVertexArray(VAO_transparent);
            glBindBuffer(GL_ARRAY_BUFFER, VBO_transparent);
            glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(transparentMesh.size() * sizeof(float)), transparentMesh.data(), GL_STATIC_DRAW);

            GLsizei stride = 10 * sizeof(float); // 8 floatów na wierzchołek
            // Pozycja (X,Y,Z)
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
            glEnableVertexAttribArray(0);
            // Współrzędne Tekstury (U,V)
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
            glEnableVertexAttribArray(1);
            // Wektory Normalne (NX,NY,NZ)
            glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, (void*)(5 * sizeof(float)));
            glEnableVertexAttribArray(2);
            glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, stride, (void*)(8 * sizeof(float)));
            glEnableVertexAttribArray(3);
            glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, stride, (void*)(9 * sizeof(float)));
            glEnableVertexAttribArray(4);
        }
        if (!foliageMesh.empty()) { // <<< CAŁY TEN BLOK JEST NOWY
            vertexCount_foliage = static_cast<int>(foliageMesh.size() / 9);
            glGenVertexArrays(1, &VAO_foliage);
            glGenBuffers(1, &VBO_foliage);
            glBindVertexArray(VAO_foliage);
            glBindBuffer(GL_ARRAY_BUFFER, VBO_foliage);
            glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(foliageMesh.size() * sizeof(float)), foliageMesh.data(), GL_STATIC_DRAW);

            GLsizei stride = 10 * sizeof(float);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, (void*)(5 * sizeof(float)));
            glEnableVertexAttribArray(2);
            glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, stride, (void*)(8 * sizeof(float)));
            glEnableVertexAttribArray(3);
            glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, stride, (void*)(9 * sizeof(float)));
            glEnableVertexAttribArray(4);
        }

        glBindVertexArray(0);
    }

    // --- ZMODYFIKOWANA FUNKCJA addFaceToMesh ---
    void addFaceToMesh(std::vector<float>& mesh, int faceIndex, int x, int y, int z, uint8_t blockID) {
        const float* uvBase;
        // ... (TWÓJ KOD WYBORU TEKSTUR - SKOPIUJ GO STĄD LUB ZOJAW SWÓJ) ...
        if (blockID == BLOCK_ID_GRASS) {
            if (faceIndex == 0) uvBase = UV_GRASS_TOP;
            else if (faceIndex == 1) uvBase = UV_DIRT;
            else uvBase = UV_GRASS_SIDE;
        }
        else if (blockID == BLOCK_ID_DIRT) uvBase = UV_DIRT;
        else if (blockID == BLOCK_ID_LOG) {
             if (faceIndex == 0 || faceIndex == 1) uvBase = UV_LOG_TOP; else uvBase = UV_LOG_SIDE;
        }
        else if (blockID == BLOCK_ID_LEAVES) uvBase = UV_LEAVES;
        else if (blockID == BLOCK_ID_STONE) uvBase = UV_STONE;
        else if (isAnyWater(blockID)) uvBase = UV_WATER;
        else if (blockID == BLOCK_ID_BEDROCK) uvBase = UV_BEDROCK;
        else if (blockID == BLOCK_ID_SAND) uvBase = UV_SAND;
        else if (blockID == BLOCK_ID_SANDSTONE) uvBase = UV_SANDSTONE;
        else if (blockID == BLOCK_ID_SNOW) uvBase = UV_SNOW;
        else if (blockID == BLOCK_ID_ICE) uvBase = UV_ICE;
        else if (blockID == BLOCK_ID_TORCH) uvBase = UV_TORCH;
        else if (blockID == BLOCK_ID_COAL_ORE)    uvBase = UV_COAL_ORE;
        else if (blockID == BLOCK_ID_IRON_ORE)    uvBase = UV_IRON_ORE;
        else if (blockID == BLOCK_ID_GOLD_ORE)    uvBase = UV_GOLD_ORE;
        else if (blockID == BLOCK_ID_DIAMOND_ORE) uvBase = UV_DIAMOND_ORE;
        else if (isAnyLava(blockID)) uvBase = UV_LAVA;
        else return;
        // -----------------------------------------------------------

        float u_start = uvBase[0];
        float v_start = uvBase[1];

        // Oblicz wysokość dla wody
        float topHeight = 0.5f; // Domyślna wysokość (pełny blok)

        if (isAnyLiquid(blockID)) {
            uint8_t blockAbove = getBlock(x, y + 1, z);
            // Jeśli nad nami jest TA SAMA ciecz -> pełny blok
            if ((isAnyWater(blockID) && isAnyWater(blockAbove)) ||
                (isAnyLava(blockID) && isAnyLava(blockAbove))) {
                topHeight = 0.5f;
                } else {
                    topHeight = getWaterHeight(blockID);
                }
        }

        // --- OBLICZANIE AO ---
        // Sprawdzamy 8 bloków dookoła ściany, żeby obliczyć cienie dla 4 rogów
        // (To jest uproszczona wersja, która używa getBlock)

        // Wektory bazowe dla tej ściany (gdzie jest góra/prawo na tej ścianie)
        // Kolejność faceIndex: 0:Up, 1:Down, 2:Front, 3:Back, 4:Right, 5:Left
        int ax = 0, ay = 0, az = 0; // Kierunek normalnej
        if (faceIndex == 0) ay = 1;
        else if (faceIndex == 1) ay = -1;
        else if (faceIndex == 2) az = 1;
        else if (faceIndex == 3) az = -1;
        else if (faceIndex == 4) ax = 1;
        else if (faceIndex == 5) ax = -1;

        // Dla każdego z 4 wierzchołków (00, 10, 11, 01 w przestrzeni UV) obliczamy AO
        // Ale pętla leci przez 6 wierzchołków (2 trójkąty). Obliczymy AO w locie.

        for (int i = 0; i < 6; i++) {
            float vx = baseFaces[faceIndex][i * 3 + 0];
            float vy = baseFaces[faceIndex][i * 3 + 1];
            float vz = baseFaces[faceIndex][i * 3 + 2];

            if (isAnyLiquid(blockID) && vy > 0.0f) vy = topHeight;

            // 1. Kierunki dla tego wierzchołka
            int dx = (vx > 0) ? 1 : -1;
            int dy = (vy > 0) ? 1 : -1;
            int dz = (vz > 0) ? 1 : -1;

            if (ax != 0) dx = 0;
            if (ay != 0) dy = 0;
            if (az != 0) dz = 0;

            // 2. Współrzędne bloku "przed" ścianą
            int nx = x + ax;
            int ny = y + ay;
            int nz = z + az;

            // --- OBLICZANIE AO (Bez zmian) ---
            // (Tutaj używamy getBlock jak wcześniej)
            bool s1_solid = false, s2_solid = false, c_solid = false;

            auto getSolid = [&](int _x, int _y, int _z) {
                uint8_t id = getBlock(_x, _y, _z); // Używamy getBlock
                return (id != BLOCK_ID_AIR && !isAnyWater(id) && id != BLOCK_ID_TORCH && id != BLOCK_ID_LEAVES);
            };

            if (faceIndex == 0 || faceIndex == 1) { // Y
                s1_solid = getSolid(nx + dx, ny, nz);
                s2_solid = getSolid(nx, ny, nz + dz);
                c_solid  = getSolid(nx + dx, ny, nz + dz);
            }
            else if (faceIndex == 2 || faceIndex == 3) { // Z
                s1_solid = getSolid(nx + dx, ny, nz);
                s2_solid = getSolid(nx, ny + dy, nz);
                c_solid  = getSolid(nx + dx, ny + dy, nz);
            }
            else { // X
                s1_solid = getSolid(nx, ny, nz + dz);
                s2_solid = getSolid(nx, ny + dy, nz);
                c_solid  = getSolid(nx, ny + dy, nz + dz);
            }

            float aoLevel = vertexAO(s1_solid, s2_solid, c_solid);
            float aoFactor = 0.5f + (aoLevel / 3.0f) * 0.5f;


            // --- OBLICZANIE ŚWIATŁA (POPRAWIONE) ---
            // Używamy nowej funkcji getLight() zamiast tablicy lightMap[]

            int l_face = getLight(nx, ny, nz);
            int l_s1 = 0, l_s2 = 0, l_c = 0;

            if (faceIndex == 0 || faceIndex == 1) { // Y
                l_s1 = getLight(nx + dx, ny, nz);
                l_s2 = getLight(nx, ny, nz + dz);
                l_c  = getLight(nx + dx, ny, nz + dz);
            }
            else if (faceIndex == 2 || faceIndex == 3) { // Z
                l_s1 = getLight(nx + dx, ny, nz);
                l_s2 = getLight(nx, ny + dy, nz);
                l_c  = getLight(nx + dx, ny + dy, nz);
            }
            else { // X
                l_s1 = getLight(nx, ny, nz + dz);
                l_s2 = getLight(nx, ny + dy, nz);
                l_c  = getLight(nx, ny + dy, nz + dz);
            }

            // Ulepszone uśrednianie:
            // Jeśli blok sąsiedni jest lity (solid), to on "blokuje" światło.
            // Wtedy nie powinniśmy brać jego ciemności (0) do średniej,
            // bo to tworzy brzydkie cienie w rogach.
            // Zamiast tego, bierzemy światło z l_face.

            if (s1_solid) l_s1 = l_face;
            if (s2_solid) l_s2 = l_face;
            if (c_solid)  l_c  = l_face;

            int totalLight = l_face + l_s1 + l_s2 + l_c;
            float smoothLight = (float)totalLight / 4.0f;
            float lightFactor = smoothLight / 15.0f;

            if (lightFactor < 0.05f) lightFactor = 0.05f; // Minimum

            // --- WYPEŁNIANIE MESHA ---
            mesh.push_back(vx + x);
            mesh.push_back(vy + y);
            mesh.push_back(vz + z);

            float u = u_start + (baseUVs[faceIndex][i * 2 + 0] * UV_STEP_U);
            float v = v_start + (baseUVs[faceIndex][i * 2 + 1] * UV_STEP_V);
            mesh.push_back(u);
            mesh.push_back(v);

            mesh.push_back(baseNormals[faceIndex][i * 3 + 0]);
            mesh.push_back(baseNormals[faceIndex][i * 3 + 1]);
            mesh.push_back(baseNormals[faceIndex][i * 3 + 2]);
            if (isAnyLava(blockID)) lightFactor = 1.0f;
            mesh.push_back(aoFactor);    // Atrybut 3
            mesh.push_back(lightFactor); // Atrybut 4
        }
    }
    void addTorchToMesh(std::vector<float>& mesh, int x, int y, int z) {
        // Użyj tekstury UV_TORCH zdefiniowanej wyżej
        const float* uvBase = UV_TORCH;
        float u_start = uvBase[0];
        float v_start = uvBase[1];

        // Dodajemy 12 wierzchołków
        for (int i = 0; i < 12; i++) {
            // Pozycja (X,Y,Z)
            mesh.push_back(torchVertices[i * 3 + 0] + static_cast<float>(x));
            mesh.push_back(torchVertices[i * 3 + 1] + static_cast<float>(y));
            mesh.push_back(torchVertices[i * 3 + 2] + static_cast<float>(z));

            // Tekstura (U,V)
            // Używamy 1/16 szerokości tekstury, bo pochodnia jest wąska
            float u = u_start + (torchUVs[i * 2 + 0] * UV_STEP_U);
            float v = v_start + (torchUVs[i * 2 + 1] * UV_STEP_V);
            mesh.push_back(u);
            mesh.push_back(v);

            // Normalne (NX,NY,NZ)
            mesh.push_back(torchNormals[i * 3 + 0]);
            mesh.push_back(torchNormals[i * 3 + 1]);
            mesh.push_back(torchNormals[i * 3 + 2]);
            mesh.push_back(1.0f); // AO
            mesh.push_back(1.0f); // Light Level (MAX)
        }
    }
    // Dwie funkcje rysujące (bez zmian)
    void DrawSolid(Shader &shader, unsigned int textureID)
    {
        if (vertexCount_solid == 0) return;
        glm::mat4 model = glm::translate(glm::mat4(1.0f), position);
        shader.setMat4("model", model);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textureID);
        glBindVertexArray(VAO_solid);
        glDrawArrays(GL_TRIANGLES, 0, vertexCount_solid);
    }
    void DrawFoliage(Shader &shader, unsigned int textureID) // <<< NOWA FUNKCJA
{
        if (vertexCount_foliage == 0) return;
        glm::mat4 model = glm::translate(glm::mat4(1.0f), position);
        shader.setMat4("model", model);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textureID);
        glBindVertexArray(VAO_foliage);
        glDrawArrays(GL_TRIANGLES, 0, vertexCount_foliage);
}
    void DrawTransparent(Shader &shader, unsigned int textureID)
    {
        if (vertexCount_transparent == 0) return;
        glm::mat4 model = glm::translate(glm::mat4(1.0f), position);
        shader.setMat4("model", model);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textureID);
        glBindVertexArray(VAO_transparent);
        glDrawArrays(GL_TRIANGLES, 0, vertexCount_transparent);
    }

    // Destruktor (bez zmian)
    ~Chunk() {
        if (vertexCount_solid > 0) {
            glDeleteVertexArrays(1, &VAO_solid);
            glDeleteBuffers(1, &VBO_solid);
        }
        if (vertexCount_transparent > 0) {
            glDeleteVertexArrays(1, &VAO_transparent);
            glDeleteBuffers(1, &VBO_transparent);
        }
        if (vertexCount_foliage > 0) { // <<< NOWOŚĆ
            glDeleteVertexArrays(1, &VAO_foliage);
            glDeleteBuffers(1, &VBO_foliage);
        }
    }
};

#endif