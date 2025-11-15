#ifndef CAMERA_H
#define CAMERA_H

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vector>

// Domyślne wartości kamery
const float YAW         = -90.0f;
const float PITCH       =  0.0f;
const float SPEED       =  8.0f;
const float SENSITIVITY =  0.1f;
const float ZOOM        =  45.0f;

// --- NOWOŚĆ: Stałe gracza ---
const float PLAYER_EYE_HEIGHT = 1.6f; // Oczy są 1.6 bloku nad stopami
const float PLAYER_WIDTH = 0.6f;      // Gracz ma 0.6 bloku szerokości
// --- KONIEC NOWOŚCI ---

class Camera
{
public:
    // Atrybuty kamery
    glm::vec3 Position; // UWAGA: To jest teraz pozycja STÓP
    glm::vec3 Front;
    glm::vec3 Up;
    glm::vec3 Right;
    glm::vec3 WorldUp;
    // Kąty Eulera
    float Yaw;
    float Pitch;
    // Opcje kamery
    float MovementSpeed;
    float MouseSensitivity;
    float Zoom;

    // --- NOWOŚĆ: Stan Latania ---
    bool flyingMode;
    // --- KONIEC NOWOŚCI ---

    // Konstruktor
    Camera(glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f), float yaw = YAW, float pitch = PITCH) : Front(glm::vec3(0.0f, 0.0f, -1.0f)), MovementSpeed(SPEED), MouseSensitivity(SENSITIVITY), Zoom(ZOOM)
    {
        Position = position; // To jest pozycja STÓP
        WorldUp = up;
        Yaw = yaw;
        Pitch = pitch;
        flyingMode = true; // Zaczynamy w trybie latania
        updateCameraVectors();
    }

    // --- ZMODYFIKOWANE: GetViewMatrix ---
    // Zwraca macierz widoku z pozycji OCZU
    glm::mat4 GetViewMatrix()
    {
        glm::vec3 eyePosition = Position + glm::vec3(0.0f, PLAYER_EYE_HEIGHT, 0.0f);
        return glm::lookAt(eyePosition, eyePosition + Front, Up);
    }

    // --- NOWOŚĆ: Przełącznik trybu Latanie/Chodzenie ---
    void ToggleFlyingMode() {
        flyingMode = !flyingMode;
    }

    // Przetwarza input z myszy (bez zmian)
    void ProcessMouseMovement(float xoffset, float yoffset, GLboolean constrainPitch = true)
    {
        xoffset *= MouseSensitivity;
        yoffset *= MouseSensitivity;
        Yaw   += xoffset;
        Pitch += yoffset;
        if (constrainPitch)
        {
            if (Pitch > 89.0f) Pitch = 89.0f;
            if (Pitch < -89.0f) Pitch = -89.0f;
        }
        updateCameraVectors();
    }

    // Funkcja do aktualizacji wektorów (bez zmian)
private:
    void updateCameraVectors()
    {
        glm::vec3 front;
        front.x = cos(glm::radians(Yaw)) * cos(glm::radians(Pitch));
        front.y = sin(glm::radians(Pitch));
        front.z = sin(glm::radians(Yaw)) * cos(glm::radians(Pitch));
        Front = glm::normalize(front);
        Right = glm::normalize(glm::cross(Front, WorldUp));
        Up    = glm::normalize(glm::cross(Right, Front));
    }
};
#endif