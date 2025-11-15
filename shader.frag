#version 330 core
out vec4 FragColor;

in vec2 TexCoord;
// NOWOŚĆ: Pobierz odległość z vertex shadera
in float v_fogFactor;

uniform sampler2D ourTexture;

// --- NOWOŚĆ: Zmienne Mgły ---
uniform vec3 u_fogColor;    // Kolor mgły (taki sam jak kolor nieba)
uniform float u_fogStart; // Jak blisko mgła się zaczyna
uniform float u_fogEnd;   // Jak daleko mgła jest w 100% gęsta
// --- KONIEC NOWOŚCI ---

void main()
{
    // Pobierz kolor z tekstury
    vec4 textureColor = texture(ourTexture, TexCoord);

    // --- NOWOŚĆ: Obliczenia Mgły ---

    // Oblicz współczynnik mgły (0.0 = brak mgły, 1.0 = pełna mgła)
    // smoothstep() tworzy ładne, gładkie przejście
    float fogAmount = smoothstep(u_fogStart, u_fogEnd, v_fogFactor);

    // Zmieszaj kolor tekstury z kolorem mgły
    FragColor = mix(textureColor, vec4(u_fogColor, 1.0), fogAmount);
}