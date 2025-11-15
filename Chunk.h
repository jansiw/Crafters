#ifndef CHUNK_H
#define CHUNK_H

#include <glad/glad.h>
#include <vector>
#include <map> // Dołączamy <map>
#include <glm/glm.hpp>
#include <glm/gtc/integer.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath> // <<< NOWOŚĆ
#include <fstream>  // <<< NOWOŚĆ (Do zapisu/odczytu plików)
#include <string>   // <<< NOWOŚĆ
#include <sstream>  // <<< NOWOŚĆ (Do budowania nazw plików)
#include <filesystem> // <<< NOWOŚĆ (Do sprawdzania, czy plik istnieje)
#include "Shader.h"
// STB_PERLIN jest dołączany z main.cpp

// --- Definicje (bez zmian) ---
const int CHUNK_WIDTH = 16;
const int CHUNK_HEIGHT = 64;
const int CHUNK_DEPTH = 16;
const int SEA_LEVEL = 32;
#define BLOCK_ID_AIR 0
#define BLOCK_ID_GRASS 1
#define BLOCK_ID_DIRT 2
#define BLOCK_ID_STONE 3
#define BLOCK_ID_WATER 4
#define BLOCK_ID_BEDROCK 5 // <<< NOWY BLOK

// --- Definicje Atlasu (bez zmian) ---
const float ATLAS_COLS = 2.0f;
const float ATLAS_ROWS = 3.0f;
const float UV_STEP_U = 1.0f / ATLAS_COLS;
const float UV_STEP_V = 1.0f / ATLAS_ROWS;
const float UV_GRASS_TOP[2] = { 0.0f * UV_STEP_U, 2.0f * UV_STEP_V };
const float UV_GRASS_SIDE[2] = { 1.0f * UV_STEP_U, 2.0f * UV_STEP_V };
const float UV_DIRT[2] = { 0.0f * UV_STEP_U, 1.0f * UV_STEP_V };
const float UV_STONE[2] = { 1.0f * UV_STEP_U, 1.0f * UV_STEP_V };
const float UV_WATER[2] = { 0.0f * UV_STEP_U, 0.0f * UV_STEP_V };
const float UV_BEDROCK[2] = { 1.0f * UV_STEP_U, 0.0f * UV_STEP_V };    // <<< NOWOŚĆ: Dół-Prawo

// --- Dane Geometrii (Poprawna wersja, bez zmian) ---
const float baseFaces[][18] = {
    // T1(V1,V2,V3), T2(V1,V3,V4)
    // Góra (+Y) - NOWA, POPRAWNA WERSJA (CCW)
    {-0.5f, 0.5f,  0.5f,  0.5f, 0.5f,  0.5f,  0.5f, 0.5f, -0.5f, -0.5f, 0.5f,  0.5f,  0.5f, 0.5f, -0.5f, -0.5f, 0.5f, -0.5f},
    // Dół (-Y) - NOWA, POPRAWNA WERSJA (CCW)
    {-0.5f,-0.5f, -0.5f,  0.5f,-0.5f, -0.5f,  0.5f,-0.5f,  0.5f, -0.5f,-0.5f, -0.5f,  0.5f,-0.5f,  0.5f, -0.5f,-0.5f,  0.5f},
    // --- TE 4 ŚCIANY SĄ Z POPRZEDNIEJ WERSJI (KTÓRA DZIAŁAŁA DLA BOKÓW) ---
    // Przód (+Z) - Poprawny
    {-0.5f, -0.5f, 0.5f,  0.5f, -0.5f, 0.5f,  0.5f,  0.5f, 0.5f, -0.5f, -0.5f, 0.5f,  0.5f,  0.5f, 0.5f, -0.5f,  0.5f, 0.5f},
    // Tył (-Z) - Poprawny
    { 0.5f, -0.5f,-0.5f, -0.5f, -0.5f,-0.5f, -0.5f,  0.5f,-0.5f,  0.5f, -0.5f,-0.5f, -0.5f,  0.5f,-0.5f,  0.5f,  0.5f,-0.5f},
    // Prawo (+X) - Poprawne
    { 0.5f, -0.5f, 0.5f,  0.5f, -0.5f,-0.5f,  0.5f,  0.5f,-0.5f,  0.5f, -0.5f, 0.5f,  0.5f,  0.5f,-0.5f,  0.5f,  0.5f, 0.5f},
    // Lewo (-X) - Poprawne
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

// Deklaracja globalnej mapy
class Chunk;
// POPRAWKA: Pełna definicja komparatora
struct CompareIveco2 {
    bool operator()(const glm::ivec2& a, const glm::ivec2& b) const {
        if (a.x != b.x) {
            return a.x < b.x;
        }
        return a.y < b.y;
    }
};
extern std::map<glm::ivec2, Chunk*, CompareIveco2> g_WorldChunks;


class Chunk
{
public:
    // Dwa zestawy VAO/VBO
    unsigned int VAO_solid, VBO_solid;
    int vertexCount_solid;
    unsigned int VAO_transparent, VBO_transparent;
    int vertexCount_transparent;
    std::string worldName;
    int worldSeed;
    glm::vec3 position;
    uint8_t blocks[CHUNK_WIDTH][CHUNK_HEIGHT][CHUNK_DEPTH]{};

    // --- NOWOŚĆ: Funkcje Zapisu/Odczytu ---

    // Zwraca nazwę pliku dla tego chunka, np. "world/chunk_0_16.dat"
    std::string GetChunkFileName() {
        std::stringstream ss;
        ss << "worlds/" << worldName << "/chunk_" << static_cast<int>(position.x) << "_" << static_cast<int>(position.z) << ".dat";
        return ss.str();
    }

    // Zapisuje tablicę 'blocks' do pliku binarnego
    void SaveToFile() {
        std::string fileName = GetChunkFileName();
        std::ofstream outFile(fileName, std::ios::binary);
        if (outFile.is_open()) {
            // Zapisz 16384 bajtów (16*64*16) bezpośrednio z pamięci do pliku
            outFile.write(reinterpret_cast<char*>(blocks), sizeof(blocks));
            outFile.close();
        }
    }

    // Wczytuje dane z pliku binarnego do tablicy 'blocks'
    bool LoadFromFile() {
        std::string fileName = GetChunkFileName();
        if (!std::filesystem::exists(fileName)) {
            return false; // Plik nie istnieje, musimy go wygenerować
        }

        std::ifstream inFile(fileName, std::ios::binary);
        if (inFile.is_open()) {
            // Wczytaj 16384 bajtów z pliku prosto do pamięci
            inFile.read(reinterpret_cast<char*>(blocks), sizeof(blocks));
            inFile.close();
            return true; // Wczytano pomyślnie
        }
        return false;
    }
    // ZASTĄP STARY KONSTRUKTOR TYM NOWYM:
    explicit Chunk(glm::vec3 pos, std::string worldName, int worldSeed) :
        VAO_solid(0), VBO_solid(0), vertexCount_solid(0),
        VAO_transparent(0), VBO_transparent(0), vertexCount_transparent(0),
        position(pos), worldName(worldName), worldSeed(worldSeed) // <<< ZAPISZ NOWE ZMIENNE
    {
        // Spróbuj wczytać z pliku (ta logika jest już poprawna)
        if (LoadFromFile()) {
            return;
        }

        // --- Ustawienia Generatora (Duże Komory 5x6) ---
        float terrainNoiseZoom = 0.02f;
        float detailNoiseZoom = 0.08f;
        float terrainAmplitude = 16.0f;
        float baseHeight = 32.0f;
        float cavernNoiseZoom = 0.03f;
        float cavernThreshold = 0.5f;
        const int caveStartDepth = 5;
        const int CAVE_WATER_LEVEL = 10;
        float aquiferNoiseZoom = 0.04f;
        float aquiferThreshold = 0.90f;
        float bedrockNoiseZoom = 0.15f;
        float bedrockThreshold = 0.3f;
        int heightMap[CHUNK_WIDTH][CHUNK_DEPTH];

        // --- ETAP 1: Generowanie Mapy Wysokości (2D) i Lądu ---
        for (int x = 0; x < CHUNK_WIDTH; x++) {
            for (int z = 0; z < CHUNK_DEPTH; z++) {

                float globalX = (float)x + position.x;
                float globalZ = (float)z + position.z;

                // --- POPRAWKA: Używamy seeda jako offsetu Y ---
                float seed_offset = (float)worldSeed;

                // (Stare wywołanie: ..., 0.0f, ..., 0, 0, worldSeed)
                float baseNoise = stb_perlin_noise3(globalX * terrainNoiseZoom, seed_offset, globalZ * terrainNoiseZoom, 0, 0, 0);

                // (Stare wywołanie: ..., 0.0f, ..., 0, 0, worldSeed + 1)
                float detailNoise = stb_perlin_noise3(globalX * detailNoiseZoom, seed_offset + 1000.0f, globalZ * detailNoiseZoom, 0, 0, 0);

                float combinedNoise = baseNoise + (detailNoise * 0.25f);
                int terrainHeight = (int)(baseHeight + (combinedNoise * terrainAmplitude));
                heightMap[x][z] = terrainHeight;

                // (Stare wywołanie: ..., 0.0f, ..., 0, 0, worldSeed + 2)
                float bedrockNoise = stb_perlin_noise3(globalX * bedrockNoiseZoom, seed_offset + 2000.0f, globalZ * bedrockNoiseZoom, 0, 0, 0);

                for (int y = 0; y < CHUNK_HEIGHT; y++) {
                    // ... (reszta pętli 'y' bez zmian) ...
                    uint8_t blockID = BLOCK_ID_AIR; // Domyślnie

                    if (y == 0) {
                        blockID = BLOCK_ID_BEDROCK;
                    }
                    else if (y == 1) {
                        if (bedrockNoise > bedrockThreshold) blockID = BLOCK_ID_BEDROCK;
                        else blockID = BLOCK_ID_STONE;
                    }
                    else if (y > terrainHeight) blockID = BLOCK_ID_AIR;
                    else if (y == terrainHeight) {
                        if (y < SEA_LEVEL - 1) blockID = BLOCK_ID_DIRT;
                        else blockID = BLOCK_ID_GRASS;
                    } else if (y > terrainHeight - 4) blockID = BLOCK_ID_DIRT;
                    else blockID = BLOCK_ID_STONE;
                    blocks[x][y][z] = blockID;
                }
            }
        }

        // --- ETAP 2: Rzeźbienie Jaskiń (Duże Komory) ---
        for (int x = 0; x < CHUNK_WIDTH; x++) {
            for (int z = 0; z < CHUNK_DEPTH; z++) {
                for (int y = 2; y < heightMap[x][z] - caveStartDepth; y++) {
                    if (blocks[x][y][z] == BLOCK_ID_STONE || blocks[x][y][z] == BLOCK_ID_DIRT) {
                        float globalX = (float)x + position.x;
                        float globalZ = (float)z + position.z;
                        float globalY = (float)y;

                        // --- POPRAWKA: worldSeed jest już w globalX/Z, ale dodajemy go też do Y ---
                        float seed_offset = (float)worldSeed;

                        // (Stare wywołanie: ..., 0, 0, worldSeed + 3)
                        float cavernValue = stb_perlin_noise3(globalX * cavernNoiseZoom, (globalY * cavernNoiseZoom) + seed_offset + 3000.0f, globalZ * cavernNoiseZoom, 0, 0, 0);
                        if (cavernValue > cavernThreshold) blocks[x][y][z] = BLOCK_ID_AIR;
                    }
                }
            }
        }

        // --- ETAP 3: Generowanie Wody (z Rzadkimi Akwiferami) ---
        for (int x = 0; x < CHUNK_WIDTH; x++) {
            for (int z = 0; z < CHUNK_DEPTH; z++) {
                int terrainHeight = heightMap[x][z];
                for (int y = 2; y < CHUNK_HEIGHT; y++) {
                    if (blocks[x][y][z] == BLOCK_ID_AIR) {
                        float globalX = (float)x + position.x;
                        float globalZ = (float)z + position.z;
                        float globalY = (float)y;

                        // --- POPRAWKA: Używamy seeda jako offsetu Y ---
                        float seed_offset = (float)worldSeed;

                        if (y > terrainHeight && terrainHeight < SEA_LEVEL && y <= SEA_LEVEL) {
                            blocks[x][y][z] = BLOCK_ID_WATER;
                        }
                        else if (y <= terrainHeight && y <= CAVE_WATER_LEVEL) {
                            // (Stare wywołanie: ..., 0, 0, worldSeed + 4)
                            float aquiferNoise = stb_perlin_noise3(globalX * aquiferNoiseZoom, (globalY * 0.1f) + seed_offset + 4000.0f, globalZ * aquiferNoiseZoom, 0, 0, 0);
                            if (aquiferNoise > aquiferThreshold) blocks[x][y][z] = BLOCK_ID_WATER;
                        }
                    }
                }
            }
        }
    }
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


    void buildMesh()
    {
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

        for (int x = 0; x < CHUNK_WIDTH; x++) {
            for (int y = 0; y < CHUNK_HEIGHT; y++) {
                for (int z = 0; z < CHUNK_DEPTH; z++) {
                    uint8_t currentBlockID = blocks[x][y][z];
                    if (currentBlockID == BLOCK_ID_AIR) continue;

                    bool isTransparent = (currentBlockID == BLOCK_ID_WATER);

                    uint8_t neighbors[6];
                    neighbors[0] = getBlock(x, y + 1, z); // Góra
                    neighbors[1] = getBlock(x, y - 1, z); // Dół
                    neighbors[2] = getBlock(x, y, z + 1); // Przód
                    neighbors[3] = getBlock(x, y, z - 1); // Tył
                    neighbors[4] = getBlock(x + 1, y, z); // Prawo
                    neighbors[5] = getBlock(x - 1, y, z); // Lewo

                    for(int i = 0; i < 6; i++) {
                        uint8_t neighborID = neighbors[i];

                        if (isTransparent) {
                            if (neighborID != BLOCK_ID_WATER) {
                                addFaceToMesh(transparentMesh, i, x, y, z, currentBlockID);
                            }
                        } else {
                            if (neighborID == BLOCK_ID_AIR || neighborID == BLOCK_ID_WATER) {
                                addFaceToMesh(solidMesh, i, x, y, z, currentBlockID);
                            }
                        }
                    }
                }
            }
        }

        if (!solidMesh.empty()) {
            vertexCount_solid = static_cast<int>(solidMesh.size() / 5);
            glGenVertexArrays(1, &VAO_solid);
            glGenBuffers(1, &VBO_solid);
            glBindVertexArray(VAO_solid);
            glBindBuffer(GL_ARRAY_BUFFER, VBO_solid);
            glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(solidMesh.size() * sizeof(float)), solidMesh.data(), GL_STATIC_DRAW);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
            glEnableVertexAttribArray(1);
        }

        if (!transparentMesh.empty()) {
            vertexCount_transparent = static_cast<int>(transparentMesh.size() / 5);
            glGenVertexArrays(1, &VAO_transparent);
            glGenBuffers(1, &VBO_transparent);
            glBindVertexArray(VAO_transparent);
            glBindBuffer(GL_ARRAY_BUFFER, VBO_transparent);
            glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(transparentMesh.size() * sizeof(float)), transparentMesh.data(), GL_STATIC_DRAW);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
            glEnableVertexAttribArray(1);
        }

        glBindVertexArray(0);
    }

    // addFaceToMesh (bez zmian)
    void addFaceToMesh(std::vector<float>& mesh, int faceIndex, int x, int y, int z, uint8_t blockID) {

        const float* uvBase;

        if (blockID == BLOCK_ID_GRASS) {
            if (faceIndex == 0) uvBase = UV_GRASS_TOP;
            else if (faceIndex == 1) uvBase = UV_DIRT;
            else uvBase = UV_GRASS_SIDE;
        }
        else if (blockID == BLOCK_ID_DIRT) {
            uvBase = UV_DIRT;
        }
        else if (blockID == BLOCK_ID_STONE) {
            uvBase = UV_STONE;
        }
        else if (blockID == BLOCK_ID_WATER) {
            uvBase = UV_WATER;
        }
        else if (blockID == BLOCK_ID_BEDROCK) { // <<< NOWOŚĆ
            uvBase = UV_BEDROCK;
        }
        else {
            return; // Nieznany blok
        }

        float u_start = uvBase[0];
        float v_start = uvBase[1];

        for (int i = 0; i < 6; i++) {
            mesh.push_back(baseFaces[faceIndex][i * 3 + 0] + static_cast<float>(x));
            mesh.push_back(baseFaces[faceIndex][i * 3 + 1] + static_cast<float>(y));
            mesh.push_back(baseFaces[faceIndex][i * 3 + 2] + static_cast<float>(z));
            float u = u_start + (baseUVs[faceIndex][i * 2 + 0] * UV_STEP_U);
            float v = v_start + (baseUVs[faceIndex][i * 2 + 1] * UV_STEP_V);
            mesh.push_back(u);
            mesh.push_back(v);
        }
    }

    // Dwie funkcje rysujące
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

    // Destruktor
    ~Chunk() {
        if (vertexCount_solid > 0) {
            glDeleteVertexArrays(1, &VAO_solid);
            glDeleteBuffers(1, &VBO_solid);
        }
        if (vertexCount_transparent > 0) {
            glDeleteVertexArrays(1, &VAO_transparent);
            glDeleteBuffers(1, &VBO_transparent);
        }
    }
};

#endif