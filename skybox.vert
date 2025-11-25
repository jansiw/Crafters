#version 330 core
layout (location = 0) in vec3 aPos;

out vec3 TexCoords; // To będzie nasz kierunek patrzenia

uniform mat4 projection;
uniform mat4 view;

void main()
{
    TexCoords = aPos;
    vec4 pos = projection * view * vec4(aPos, 1.0);

    // Trik: Ustawiamy Z na W, żeby skybox zawsze był "na końcu świata" (głębokość 1.0)
    gl_Position = pos.xyww;
}