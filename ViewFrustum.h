#ifndef VIEWFRUSTUM_H
#define VIEWFRUSTUM_H

#include <glm/glm.hpp>
#include <array>

class ViewFrustum {
public:
    struct Plane {
        glm::vec3 normal;
        float distance;

        // Normalizacja płaszczyzny (żeby odległości były poprawne)
        void normalize() {
            float mag = glm::length(normal);
            normal /= mag;
            distance /= mag;
        }
    };

    std::array<Plane, 6> planes; // 6 ścian piramidy widzenia (Lewa, Prawa, Dół, Góra, Bliska, Daleka)

    // Aktualizacja frustum na podstawie macierzy (Projection * View)
    void Update(const glm::mat4& mat) {
        // Lewa
        planes[0].normal.x = mat[0][3] + mat[0][0];
        planes[0].normal.y = mat[1][3] + mat[1][0];
        planes[0].normal.z = mat[2][3] + mat[2][0];
        planes[0].distance = mat[3][3] + mat[3][0];

        // Prawa
        planes[1].normal.x = mat[0][3] - mat[0][0];
        planes[1].normal.y = mat[1][3] - mat[1][0];
        planes[1].normal.z = mat[2][3] - mat[2][0];
        planes[1].distance = mat[3][3] - mat[3][0];

        // Dół
        planes[2].normal.x = mat[0][3] + mat[0][1];
        planes[2].normal.y = mat[1][3] + mat[1][1];
        planes[2].normal.z = mat[2][3] + mat[2][1];
        planes[2].distance = mat[3][3] + mat[3][1];

        // Góra
        planes[3].normal.x = mat[0][3] - mat[0][1];
        planes[3].normal.y = mat[1][3] - mat[1][1];
        planes[3].normal.z = mat[2][3] - mat[2][1];
        planes[3].distance = mat[3][3] - mat[3][1];

        // Bliska
        planes[4].normal.x = mat[0][3] + mat[0][2];
        planes[4].normal.y = mat[1][3] + mat[1][2];
        planes[4].normal.z = mat[2][3] + mat[2][2];
        planes[4].distance = mat[3][3] + mat[3][2];

        // Daleka
        planes[5].normal.x = mat[0][3] - mat[0][2];
        planes[5].normal.y = mat[1][3] - mat[1][2];
        planes[5].normal.z = mat[2][3] - mat[2][2];
        planes[5].distance = mat[3][3] - mat[3][2];

        for (auto& plane : planes) plane.normalize();
    }

    // Sprawdź, czy pudełko (Chunk) jest w polu widzenia
    bool IsBoxVisible(const glm::vec3& min, const glm::vec3& max) {
        for (const auto& plane : planes) {
            // Znajdź punkt pudełka najbardziej "wchodzący" w ujemną stronę płaszczyzny
            glm::vec3 p;
            p.x = (plane.normal.x > 0) ? max.x : min.x;
            p.y = (plane.normal.y > 0) ? max.y : min.y;
            p.z = (plane.normal.z > 0) ? max.z : min.z;

            // Jeśli ten punkt jest za płaszczyzną, całe pudełko jest niewidoczne
            if (glm::dot(plane.normal, p) + plane.distance < 0) return false;
        }
        return true;
    }
};

#endif