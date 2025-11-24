#version 330 core
out vec4 FragColor;
in vec2 TexCoords;

uniform sampler2D scene;
uniform sampler2D bloomBlur;
uniform float exposure;

void main() {
    // TEST: Pokaż surową teksturę sceny.
    // Jeśli nadal czarno -> Błąd jest w pętli w main.cpp (nie rysujesz do FBO).
    // Jeśli widzisz grę (nawet brzydką/ciemną) -> Błąd jest w logice Blooma/ToneMappingu.
    FragColor = texture(scene, TexCoords);
}