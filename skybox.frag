#version 330 core
out vec4 FragColor;

in vec3 TexCoords;

uniform vec3 u_sunDir;
uniform float u_time;

// Funkcja losowa 3D (deterministyczna)
float random3D(vec3 p) {
    return fract(sin(dot(p, vec3(12.9898, 78.233, 128.852))) * 43758.5453);
}

// Rysowanie kwadratu (Słońce/Księżyc)
// Używamy projekcji na płaszczyznę prostopadłą do obiektu, żeby zachować kształt
float drawSquare(vec3 viewDir, vec3 lightDir, float size, float blur) {
    vec3 right = normalize(cross(vec3(0.0, 1.0, 0.0), lightDir));
    vec3 up = normalize(cross(lightDir, right));
    float x = dot(viewDir, right);
    float y = dot(viewDir, up);

    // Ukryj tył
    if (dot(viewDir, lightDir) < 0.0) return 0.0;

    float maskX = smoothstep(size + blur, size, abs(x));
    float maskY = smoothstep(size + blur, size, abs(y));
    return maskX * maskY;
}

void main()
{
    vec3 viewDir = normalize(TexCoords);
    float sunHeight = u_sunDir.y;

    // 1. KOLORY NIEBA (Gradient Sferyczny)
    // Używamy viewDir.y bezpośrednio, co gwarantuje idealny gradient góra-dół bez kantów
    vec3 dayTop = vec3(0.2, 0.6, 1.0);
    vec3 dayBottom = vec3(0.7, 0.8, 1.0);
    vec3 dayColor = mix(dayBottom, dayTop, clamp(viewDir.y * 0.5 + 0.5, 0.0, 1.0));

    vec3 sunsetTop = vec3(0.2, 0.1, 0.4);
    vec3 sunsetBottom = vec3(1.0, 0.6, 0.3);
    vec3 sunsetColor = mix(sunsetBottom, sunsetTop, clamp(viewDir.y * 0.5 + 0.5, 0.0, 1.0));

    vec3 nightTop = vec3(0.0, 0.0, 0.02);
    vec3 nightBottom = vec3(0.0, 0.0, 0.1);
    vec3 nightColor = mix(nightBottom, nightTop, clamp(viewDir.y * 0.5 + 0.5, 0.0, 1.0));

    vec3 finalSky;

    // Płynne mieszanie pór dnia
    if (sunHeight > 0.0) {
        float t = smoothstep(0.0, 0.5, sunHeight);
        finalSky = mix(sunsetColor, dayColor, t);
    } else {
        float t = smoothstep(0.0, -0.3, sunHeight);
        finalSky = mix(sunsetColor, nightColor, t);
    }

    // 2. SŁOŃCE (Kwadratowe)
    float sunSize = 0.04;
    float sunMask = drawSquare(viewDir, normalize(u_sunDir), sunSize, 0.002);
    if (sunMask > 0.0) finalSky = mix(finalSky, vec3(1.0, 1.0, 0.8), sunMask);

    float sunGlow = drawSquare(viewDir, normalize(u_sunDir), sunSize * 2.5, 0.2);
    finalSky += vec3(1.0, 0.6, 0.1) * sunGlow * 0.3;

    // 3. KSIĘŻYC (Kwadratowy)
    vec3 moonDir = -normalize(u_sunDir);
    float moonSize = 0.035;
    float moonMask = drawSquare(viewDir, moonDir, moonSize, 0.002);
    if (moonMask > 0.0) finalSky = mix(finalSky, vec3(0.9, 0.9, 1.0), moonMask);

    float moonGlow = drawSquare(viewDir, moonDir, moonSize * 2.0, 0.15);
    finalSky += vec3(0.3, 0.4, 0.6) * moonGlow * 0.2;

    // --- 4. GWIAZDY (NAPRAWIONE - SFERYCZNE PIKSELE) ---
    if (sunHeight < 0.2) {
        // Zamiast rzutować na ściany sześcianu, używamy wektora sferycznego.
        // Skalujemy go bardzo mocno (300.0) i zaokrąglamy (floor).
        // To tworzy efekt "wokseli na powierzchni kuli".

        vec3 starCoord = floor(viewDir * 300.0);
        float starVal = random3D(starCoord);

        // Rysuj gwiazdę, jeśli wylosowana wartość jest wysoka
        if (starVal > 0.997) { // Zwiększ próg dla mniejszej ilości gwiazd
                               float twinkle = sin(u_time * 2.0 + starVal * 100.0) * 0.5 + 0.5;

                               // Gwiazdy znikają przy horyzoncie (żeby nie wchodziły na góry)
                               float horizonFade = smoothstep(0.0, 0.2, viewDir.y);

                               // Gwiazdy znikają rano
                               float dayFade = smoothstep(0.2, -0.3, sunHeight);

                               float sphereRadius = 0.5;

                               // viewDir.y to wysokość (1.0 to zenit/czubek głowy)
                               // Rysujemy tylko tam, gdzie wysokość jest większa niż nasz promień
                               float sphereMask = smoothstep(sphereRadius, sphereRadius + 0.05, viewDir.y);

                               // -------------------------------------

                               finalSky += vec3(1.0) * dayFade * sphereMask;
        }
    }

    FragColor = vec4(finalSky, 1.0);
}