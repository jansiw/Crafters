#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 TexCoord;

uniform mat4 projection;
uniform mat4 view;
uniform vec3 viewPos; // Pozycja gracza
uniform vec2 u_cloudOffset; // Do przesuwania chmur

void main()
{
    // Chmury są zawsze nad graczem, ale przesuwają się w X i Z razem z nim
    // (Żeby chmury nie "uciekały" jak skybox, ale też żeby nie kończyły się horyzontem)
    vec3 pos = aPos;
    pos.x += viewPos.x;
    pos.z += viewPos.z;

    TexCoord = aTexCoord + u_cloudOffset; // Przesuwanie wiatrem
    gl_Position = projection * view * vec4(pos, 1.0);
}