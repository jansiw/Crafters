#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;

uniform mat4 projection;
uniform mat4 model; // <<< DODAJ TĘ LINIĘ

out vec2 TexCoord;

void main()
{
    // Mnożymy przez macierz model (przesunięcie/skalowanie)
    gl_Position = projection * model * vec4(aPos.x, aPos.y, 0.0, 1.0);
    TexCoord = aTexCoord;
}