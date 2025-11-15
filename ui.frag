#version 330 core
out vec4 FragColor;

in vec2 TexCoord;

uniform sampler2D uiTexture;

void main()
{
    // Rysuj na podstawie tekstury, która ma przezroczystość (alpha)
    FragColor = texture(uiTexture, TexCoord);
    // Karta graficzna użyje alpha do blendowania (mieszania)
}