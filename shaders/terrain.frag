#version 330 core
out vec4 FragColor;

in VS_OUT {
    vec3 FragPos;
    vec2 TexCoord;
    mat3 TBN;
    float ErosionDelta;
} fs_in;

// Grass
uniform sampler2D grassAlbedo;
uniform sampler2D grassNormal;
uniform sampler2D grassRoughness;
uniform sampler2D grassAO;
// Rock
uniform sampler2D rockAlbedo;
uniform sampler2D rockNormal;
uniform sampler2D rockRoughness;
uniform sampler2D rockAO;
// Snow
uniform sampler2D snowAlbedo;
uniform sampler2D snowNormal;
uniform sampler2D snowRoughness;

// Light + camera
uniform vec3 lightDir;      // нормализованный
uniform vec3 lightColor;    // интенсивность
uniform float ambientFactor;
uniform float specularFactor;
uniform vec3 viewPos;

uniform float grassToRockStart;
uniform float grassToRockEnd;
uniform float rockToSnowStart;
uniform float rockToSnowEnd;
uniform bool showErosionHeatmap;
uniform float heatmapScale;

const float PI = 3.14159265359;

// Schlick Fresnel приближение
vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

// GGX нормальное распределение (NDF)
float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a      = roughness*roughness;
    float a2     = a*a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;

    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / denom;
}

// Schlick-GGX аппрокс. геометрического затухания
float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

// Smith метод для Geometry
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2  = GeometrySchlickGGX(NdotV, roughness);
    float ggx1  = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

void main() {
    
    // высота в мировых координатах
    float h = fs_in.FragPos.y;

    // задаём зоны перехода
    float g2r_min = grassToRockStart;
    float g2r_max = max(grassToRockEnd, grassToRockStart + 0.01);
    float r2s_min = rockToSnowStart;
    float r2s_max = max(rockToSnowEnd, rockToSnowStart + 0.01);

    // вычисляем веса
    float rockBlend = smoothstep(g2r_min, g2r_max, h);
    float snowBlend = smoothstep(r2s_min, r2s_max, h);
    float wSnow  = snowBlend;
    float wGrass = (1.0 - rockBlend) * (1.0 - wSnow);
    float wRock  = rockBlend * (1.0 - wSnow);

    // ----------------- sample -----------------
    // Grass
    vec3 albG = texture(grassAlbedo,    fs_in.TexCoord).rgb;
    float rouG = texture(grassRoughness, fs_in.TexCoord).r;
    float aoG  = texture(grassAO,        fs_in.TexCoord).r;
    vec3 nrmG  = normalize(fs_in.TBN * (texture(grassNormal, fs_in.TexCoord).xyz*2.0-1.0));

    // Rock
    vec3 albR = texture(rockAlbedo,    fs_in.TexCoord).rgb;
    float rouR = texture(rockRoughness, fs_in.TexCoord).r;
    float aoR  = texture(rockAO,        fs_in.TexCoord).r;
    vec3 nrmR  = normalize(fs_in.TBN * (texture(rockNormal, fs_in.TexCoord).xyz*2.0-1.0));

    // Snow
    vec3 albS = texture(snowAlbedo,    fs_in.TexCoord).rgb;
    float rouS = texture(snowRoughness, fs_in.TexCoord).r;
    float aoS  = 1.0;
    vec3 nrmS  = normalize(fs_in.TBN * (texture(snowNormal, fs_in.TexCoord).xyz*2.0-1.0));

    // смешиваем параметры
    vec3  albedo    = wGrass*albG + wRock*albR + wSnow*albS;
    float roughness = wGrass*rouG + wRock*rouR + wSnow*rouS;
    float ao        = wGrass*aoG  + wRock*aoR  + wSnow*aoS;
    vec3  N         = normalize(wGrass*nrmG + wRock*nrmR + wSnow*nrmS);

    // PBR расчёт (Cook‐Torrance) — как раньше
    vec3 V = normalize(viewPos - fs_in.FragPos);
    vec3 L = normalize(-lightDir);
    vec3 H = normalize(V + L);

    // F0
    vec3 F0 = vec3(0.04);
    vec3 F  = fresnelSchlick(max(dot(H,V),0.0), F0);

    float NDF = DistributionGGX(N,H,roughness);
    float G   = GeometrySmith(N,V,L,roughness);
    vec3  spec = (NDF*G*F) / (4.0*max(dot(N,V),0.0)*max(dot(N,L),0.0)+0.0001);
    vec3  kD   = (vec3(1.0)-F);

    float NdotL = max(dot(N,L),0.0);
    vec3 Lo = (kD * albedo/PI + spec * specularFactor) * lightColor * NdotL;
    vec3 ambient = ambientFactor * albedo * ao;
    vec3 color = ambient + Lo;

    if (showErosionHeatmap) {
        float eroded = clamp(-fs_in.ErosionDelta * heatmapScale, 0.0, 1.0);
        float deposited = clamp(fs_in.ErosionDelta * heatmapScale, 0.0, 1.0);
        vec3 neutral = vec3(0.16);
        vec3 erosionRed = vec3(1.0, 0.08, 0.03);
        vec3 depositBlue = vec3(0.05, 0.32, 1.0);
        color = mix(neutral, erosionRed, eroded);
        color = mix(color, depositBlue, deposited);
    }

    // тонемап + гамма
    color = color/(color+vec3(1.0));
    color = pow(color, vec3(1.0/2.2));

    FragColor = vec4(color,1.0);
}
