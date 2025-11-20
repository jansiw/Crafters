#version 330 core
out vec4 FragColor;

in vec2 TexCoord;

uniform sampler2D cloudTexture;

void main()
{
    vec4 texColor = texture(cloudTexture, TexCoord);

    // Jeśli piksel jest czarny (lub przezroczysty), nie rysuj go
    if(texColor.a < 0.1)
    discard;

    // Białe chmury z lekką przezroczystością
    FragColor = vec4(1.0, 1.0, 1.0, 0.8);
}