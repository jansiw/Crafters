#version 330 core
out vec4 FragColor;

in vec2 TexCoord;

uniform sampler2D uiTexture;
uniform float u_opacity; // <<< NOWOŚĆ: Globalna przezroczystość

void main()
{
    FragColor = texture(uiTexture, TexCoord);

    // Odrzuć piksele, które są w pełni przezroczyste
    if(FragColor.a < 0.1)
        discard;

    // Zastosuj globalne wygaszanie
    FragColor.a *= u_opacity;
}