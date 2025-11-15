#ifndef CHUNK_H
#define CHUNK_H

#include <glad/glad.h>
#include <vector>
#include <map> // Dołączamy <map>
#include <glm/glm.hpp>
#include <glm/gtc/integer.hpp>
#include <glm/gtc/matrix_transform.hpp>
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

    glm::vec3 position;
    uint8_t blocks[CHUNK_WIDTH][CHUNK_HEIGHT][CHUNK_DEPTH]{};

    // Inicjalizuj wszystkie nowe zmienne
    explicit Chunk(glm::vec3 pos) :
        VAO_solid(0), VBO_solid(0), vertexCount_solid(0),
        VAO_transparent(0), VBO_transparent(0), vertexCount_transparent(0),
        position(pos)
    {
        // Generowanie terenu (bez zmian)
        for (int x = 0; x < CHUNK_WIDTH; x++) {
            for (int z = 0; z < CHUNK_DEPTH; z++) {
                float globalX = (float)x + position.x;
                float globalZ = (float)z + position.z;
                float noise = stb_perlin_noise3(globalX * 0.02f, 0.0f, globalZ * 0.02f, 0, 0, 0);
                int terrainHeight = (int)(((noise + 1.0f) / 2.0f) * 32.0f) + 16;
                for (int y = 0; y < CHUNK_HEIGHT; y++) {
                    if (y > terrainHeight) {
                        if (y <= SEA_LEVEL) blocks[x][y][z] = BLOCK_ID_WATER;
                        else blocks[x][y][z] = BLOCK_ID_AIR;
                    } else if (y == terrainHeight) {
                        if (y < SEA_LEVEL - 1) blocks[x][y][z] = BLOCK_ID_DIRT;
                        else blocks[x][y][z] = BLOCK_ID_GRASS;
                    } else if (y > terrainHeight - 4) {
                        blocks[x][y][z] = BLOCK_ID_DIRT;
                    } else {
                        blocks[x][y][z] = BLOCK_ID_STONE;
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
        else if (blockID == BLOCK_ID_DIRT) uvBase = UV_DIRT;
        else if (blockID == BLOCK_ID_STONE) uvBase = UV_STONE;
        else if (blockID == BLOCK_ID_WATER) uvBase = UV_WATER;
        else return;

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