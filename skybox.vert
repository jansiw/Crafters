#version 330 core
layout (location = 0) in vec3 aPos;

uniform mat4 view;
uniform mat4 projection;

out vec3 TexCoords; // Współrzędne tekstury (również wektor kierunku)

void main()
{
    TexCoords = aPos;
    // Ważne: usuń tłumaczenie (translation) z macierzy widoku (view)
    // Dzięki temu sześcian skyboxa podąża za kamerą, ale nie przesuwa się
    // Pozycja z = 1.0 (minimalna głębia) gwarantuje, że jest renderowany za wszystkim
    vec4 pos = projection * mat4(mat3(view)) * vec4(aPos, 1.0);
    gl_Position = pos.xyww; // Użycie pos.xyww ustawia Z na W, gwarantując głębię 1.0
}