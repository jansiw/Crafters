#version 330 core
out vec4 FragColor;

in vec3 TexCoords;

// Cubemap to specjalny typ tekstury 6-ściennej
uniform samplerCube skybox;

void main()
{
    // Sampling z cubemapy używa TexCoords jako wektora kierunku
    FragColor = texture(skybox, TexCoords);
}