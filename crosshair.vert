#version 330 core
layout (location = 0) in vec2 aPos;

// NOWOŚĆ: Używamy macierzy ortograficznej zamiast perspektywy
uniform mat4 projection;

void main()
{
    // Rysuj w przestrzeni 2D
    gl_Position = projection * vec4(aPos.x, aPos.y, 0.0, 1.0);
}