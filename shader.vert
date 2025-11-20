#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in vec3 aNormal;
layout (location = 3) in float aAO; // <<< NOWOŚĆ: Ambient Occlusion (0.0 do 1.0)
layout (location = 4) in float aLight; // <<< NOWOŚĆ

out vec2 TexCoord;
out vec3 Normal;
out vec3 FragPos;
out vec4 FragPosLightSpace;
out float AO; // <<< Przekazujemy do fragment shadera
out float LightLevel; // <<< Przekazujemy dalej

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 lightSpaceMatrix;

// --- Obsługa Atlasu ---
uniform int u_useUVTransform;
uniform vec2 u_uvScale;
uniform vec2 u_uvOffset;
uniform vec2 u_uvOffsetTop;
uniform vec2 u_uvOffsetBottom;
uniform int u_multiTexture;

// --- Obsługa Wody ---
uniform int u_isWater;
uniform float u_time;
uniform vec3 viewPos;


void main()
{
    vec4 worldPosition = model * vec4(aPos, 1.0);

    // Efekt Falowania (Zachowany z poprzednich kroków)
    bool isReallyWater = (u_isWater == 1);
    if (u_useUVTransform == 0 && isReallyWater) {
        if (aTexCoord.x > 0.15) isReallyWater = false;
    }
    if (isReallyWater) {
        float dist = length(viewPos - worldPosition.xyz);
        if (dist < 40.0) {
            float wave = sin(worldPosition.x * 1.5 + worldPosition.z * 1.0 + u_time * 3.0);
            worldPosition.y += wave * 0.1 * (1.0 - (dist / 40.0));
        }
    }

    FragPos = vec3(worldPosition);
    Normal = mat3(transpose(inverse(model))) * aNormal;
    AO = aAO;
    LightLevel = aLight; // <<< Przypisz

    // Obsługa UV
    if (u_useUVTransform == 1) {
        vec2 finalOffset = u_uvOffset;
        if (u_multiTexture == 1) {
            if (aNormal.y > 0.5) finalOffset = u_uvOffsetTop;
            else if (aNormal.y < -0.5) finalOffset = u_uvOffsetBottom;
        }
        TexCoord = aTexCoord * u_uvScale + finalOffset;
    } else {
        TexCoord = aTexCoord;
    }

    FragPosLightSpace = lightSpaceMatrix * worldPosition;
    gl_Position = projection * view * worldPosition;
}