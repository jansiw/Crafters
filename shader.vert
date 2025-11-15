#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec2 TexCoord;
// NOWOŚĆ: Przekaż odległość do fragment shadera
out float v_fogFactor;

void main()
{
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    TexCoord = aTexCoord;

    // Oblicz odległość od kamery (długość wektora Z w przestrzeni widoku)
    // To jest nasza głębia.
    v_fogFactor = abs((view * model * vec4(aPos, 1.0)).z);
}