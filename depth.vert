#version 330 core
layout (location = 0) in vec3 aPos;

// Uniformy, które otrzymujemy z C++
uniform mat4 lightSpaceMatrix;
uniform mat4 model; // <<< TO JEST KLUCZOWA, BRAKUJĄCA LINIA

void main()
{
    // Poprawna transformacja:
    // Pozycja wierzchołka jest najpierw mnożona przez 'model' (pozycja chunka),
    // a potem przez macierz światła.
    gl_Position = lightSpaceMatrix * model * vec4(aPos, 1.0);
}