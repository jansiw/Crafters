#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;

uniform mat4 projection;
uniform mat4 model;
uniform vec2 u_uvOffset; // NOWOŚĆ: Przesunięcie UV
uniform vec2 u_uvScale;  // NOWOŚĆ: Skala UV

out vec2 TexCoord;

void main()
{
    gl_Position = projection * model * vec4(aPos, 0.0, 1.0);

    // Przeskaluj i przesuń UV, aby wybrać poprawną ikonę z atlasu
    TexCoord = (aTexCoord * u_uvScale) + u_uvOffset;
}