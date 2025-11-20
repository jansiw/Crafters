#version 330 core
out vec4 FragColor;

// --- DANE WEJŚCIOWE ---
in vec2 TexCoord;
in vec3 Normal;
in vec3 FragPos;
in vec4 FragPosLightSpace;
in float AO;
in float LightLevel; // <<< Odbierz

// --- MATERIAŁ I ŚWIAT ---
uniform sampler2D ourTexture;
uniform vec3 viewPos;
uniform vec3 u_fogColor;
uniform float u_fogStart;
uniform float u_fogEnd;
uniform sampler2D shadowMap;

// --- DEFINICJA ŚWIATŁA KIERUNKOWEGO (SŁOŃCE) ---
struct DirLight {
    vec3 direction;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};
uniform DirLight u_dirLight;

// --- DEFINICJA ŚWIATŁA PUNKTOWEGO (POCHODNIA) ---
struct PointLight {
    vec3 position;
    float constant;
    float linear;
    float quadratic;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};
#define MAX_POINT_LIGHTS 8
uniform PointLight u_pointLights[MAX_POINT_LIGHTS];
uniform int u_pointLightCount;

// --- FUNKCJA CIENIA (PCF 5x5) ---
// (Ta funkcja jest poprawna, bez zmian)
float calculateShadow(vec4 fragPosLightSpace, sampler2D shadowMap)
{
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    if (projCoords.z > 1.0) {
        return 1.0;
    }
    projCoords = projCoords * 0.5 + 0.5;
    float currentDepth = projCoords.z;
    float shadowFactor = 0.0;
    float bias = 0.005;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    int pcfRange = 2;
    float totalSamples = 0.0;
    for(int x = -pcfRange; x <= pcfRange; ++x)
    {
        for(int y = -pcfRange; y <= pcfRange; ++y)
        {
            float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
            if (currentDepth - bias <= pcfDepth) {
                shadowFactor += 1.0;
            }
            totalSamples += 1.0;
        }
    }
    shadowFactor /= totalSamples;
    return mix(0.3, 1.0, shadowFactor);
}

// --- FUNKCJA: Obliczanie światła kierunkowego ---
// POPRAWKA: Dodaliśmy argument 'bool isReflective'
vec3 CalculateDirLight(DirLight light, vec3 norm, vec3 viewDir, vec4 fragPosLightSpace, sampler2D shadowMap, bool isReflective)
{
    // Domyślnie zakładamy pełne światło (brak cienia)
    float shadow = 1.0;

    // Obliczaj cień TYLKO dla bloków matowych (ziemia, drewno itp.)
    // Jeśli to woda/lód (isReflective), pomijamy cień, żeby fale nie migotały
    if (!isReflective) {
        shadow = calculateShadow(fragPosLightSpace, shadowMap);
    }

    vec3 lightDir = normalize(-light.direction);

    // Diffuse (zawsze)
    float diff = max(dot(norm, lightDir), 0.0);

    // Ambient (zawsze)
    vec3 ambient  = light.ambient;
    vec3 diffuse  = light.diffuse * diff * shadow; // Cień wpływa na diffuse

    // Specular (Tylko jeśli 'isReflective')
    vec3 specular = vec3(0.0);
    if(isReflective)
    {
        vec3 reflectDir = reflect(-lightDir, norm);
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);

        // Woda powinna się błyszczeć nawet w cieniu (wygląda to ładniej),
        // ale możesz tu dodać '* shadow' jeśli wolisz
        specular = light.specular * spec;
    }

    return (ambient + diffuse + specular);
}

// --- FUNKCJA: Obliczanie światła punktowego ---
// POPRAWKA: Dodaliśmy argument 'bool isReflective'
vec3 CalculatePointLight(PointLight light, vec3 norm, vec3 fragPos, vec3 viewDir, bool isReflective)
{
    vec3 lightDir = normalize(light.position - fragPos);
    float diff = max(dot(norm, lightDir), 0.0);

    float dist = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * dist + light.quadratic * (dist * dist));

    vec3 ambient  = light.ambient * attenuation;
    vec3 diffuse  = light.diffuse * diff * attenuation;

    // Specular (Tylko jeśli 'isReflective')
    vec3 specular = vec3(0.0); // Domyślnie brak połysku
    if(isReflective)
    {
        vec3 reflectDir = reflect(-lightDir, norm);
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
        specular = light.specular * spec * attenuation;
    }

    return (ambient + diffuse + specular);
}

void main()
{
    vec4 textureColor = texture(ourTexture, TexCoord);

    // --- ALFA TEST (DISCARD) ---
    float UV_STEP_U = 1.0 / 8.0;
    float UV_STEP_V = 1.0 / 3.0;
    bool isWaterPixel = (TexCoord.s >= 0.0 && TexCoord.s < UV_STEP_U) &&
    (TexCoord.t >= 0.0 && TexCoord.t < UV_STEP_V);

    if (isWaterPixel)
    {
        // Oblicz kąt patrzenia (1.0 = patrzysz prosto w dół, 0.0 = patrzysz na horyzont)
        vec3 viewDir = normalize(viewPos - FragPos);
        vec3 norm = normalize(Normal);
        float angle = dot(viewDir, norm);

        // Fresnel: Bardziej przezroczysta gdy patrzysz w dół (0.6),
        // bardziej lita gdy patrzysz pod kątem (0.95)
        float alpha = mix(0.95, 0.6, angle);

        textureColor.a = alpha;
        textureColor.rgb *= vec3(0.5, 0.7, 1.0); // Lekki tint
    }
    bool isLeaves = (TexCoord.s >= 3.0 * UV_STEP_U && TexCoord.s < 4.0 * UV_STEP_U) &&
    (TexCoord.t >= 0.0 * UV_STEP_V && TexCoord.t < 1.0 * UV_STEP_V);

    bool isTorch = (TexCoord.s >= 3.0 * UV_STEP_U && TexCoord.s < 4.0 * UV_STEP_U) &&
    (TexCoord.t >= 1.0 * UV_STEP_V && TexCoord.t < 2.0 * UV_STEP_V);

    if (isLeaves || isTorch)
    {
        if (textureColor.a < 0.5)
        discard;
    }

    vec3 norm = normalize(Normal);
    vec3 viewDir = normalize(viewPos - FragPos);

    // --- Sprawdzenie Odbicia ---
    bool isWater = (TexCoord.s >= 0.0 * UV_STEP_U && TexCoord.s < 1.0 * UV_STEP_U) &&
    (TexCoord.t >= 0.0 * UV_STEP_V && TexCoord.t < 1.0 * UV_STEP_V);
    bool isIce = (TexCoord.s >= 2.0 * UV_STEP_U && TexCoord.s < 3.0 * UV_STEP_U) &&
    (TexCoord.t >= 0.0 * UV_STEP_V && TexCoord.t < 1.0 * UV_STEP_V);

    bool isReflective = isWater || isIce;

    // --- Obliczanie świateł ---
    vec3 dirLight = CalculateDirLight(u_dirLight, norm, viewDir, FragPosLightSpace, shadowMap, isReflective);

    // 2. Oblicz światła punktowe (Pochodnie - opcjonalne, jeśli używamy wokseli, ale zostawmy dla efektów)
    vec3 pointLights = vec3(0.0);
    for(int i = 0; i < u_pointLightCount; i++)
    {
        pointLights += CalculatePointLight(u_pointLights[i], norm, FragPos, viewDir, isReflective);
    }

    // --- ŁĄCZENIE OŚWIETLENIA (POPRAWIONE) ---

    // A. Światło Wokselowe (LightLevel) wpływa na siłę światła otoczenia i słońca
    // (Jeśli jesteśmy w jaskini, LightLevel jest niski, więc słońce słabiej świeci)
    // Używamy max(LightLevel, 0.1), żeby w całkowitej ciemności było minimalne światło
    float voxelLight = max(LightLevel, 0.05);

    // B. Sumujemy światła
    // Mnożymy słońce przez voxelLight, żeby w jaskiniach nie było słońca
    vec3 finalLighting = (dirLight * voxelLight) + pointLights;

    // C. Aplikujemy Ambient Occlusion (AO) - przyciemniamy rogi
    finalLighting = finalLighting * AO;

    // D. Mnożymy przez kolor tekstury
    vec3 result = finalLighting * textureColor.rgb;
    // --- Mgła ---
    float dist = length(viewPos - FragPos);
    float fogAmount = clamp((dist - u_fogStart) / (u_fogEnd - u_fogStart), 0.0, 1.0);
    result = mix(result, u_fogColor, fogAmount);
    float gamma = 1.8;
    result = pow(result, vec3(1.0 / gamma));
    FragColor = vec4(result, textureColor.a);
}