#version 330 core
out vec4 FragColor;

in VS_OUT {
    vec3 FragPos;
    vec2 TexCoord;
    mat3 TBN;
    float ErosionDelta;
} fs_in;

uniform vec3 lightDir;
uniform vec3 lightColor;
uniform float ambientFactor;
uniform float specularFactor;
uniform vec3 viewPos;

uniform vec3 terrainColor;
uniform bool showErosionHeatmap;
uniform float heatmapScale;

void main() {
    vec3 N = normalize(fs_in.TBN[2]);
    vec3 V = normalize(viewPos - fs_in.FragPos);
    vec3 L = normalize(-lightDir);
    vec3 H = normalize(V + L);

    float NdotL = max(dot(N, L), 0.0);
    float halfLambert = NdotL * 0.5 + 0.5;
    float spec = pow(max(dot(N, H), 0.0), 48.0) * specularFactor;

    vec3 base = terrainColor;
    vec3 ambient = ambientFactor * base;
    vec3 diffuse = base * lightColor * halfLambert;
    vec3 color = ambient + diffuse + spec * lightColor;

    if (showErosionHeatmap) {
        float eroded = clamp(-fs_in.ErosionDelta * heatmapScale, 0.0, 1.0);
        float deposited = clamp(fs_in.ErosionDelta * heatmapScale, 0.0, 1.0);
        vec3 neutral = vec3(0.16, 0.16, 0.16);
        vec3 erosionRed = vec3(1.0, 0.08, 0.03);
        vec3 depositBlue = vec3(0.05, 0.32, 1.0);

        color = neutral;
        color = mix(color, erosionRed, eroded);
        color = mix(color, depositBlue, deposited);
    }

    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));

    FragColor = vec4(color, 1.0);
}