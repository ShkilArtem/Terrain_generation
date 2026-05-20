#version 330 core
out vec4 FragColor;

in VS_OUT {
    vec3 FragPos;
    vec2 TexCoord;
    mat3 TBN;
} fs_in;

uniform vec3 lightDir;
uniform vec3 lightColor;
uniform float ambientFactor;
uniform float specularFactor;
uniform vec3 viewPos;

uniform float grassToRockStart;
uniform float grassToRockEnd;
uniform float rockToSnowStart;
uniform float rockToSnowEnd;

uniform vec3 grassColor;
uniform vec3 rockColor;
uniform vec3 snowColor;

void main() {
    float h = fs_in.FragPos.y;

    float g2r_min = grassToRockStart;
    float g2r_max = max(grassToRockEnd, grassToRockStart + 0.01);
    float r2s_min = rockToSnowStart;
    float r2s_max = max(rockToSnowEnd, rockToSnowStart + 0.01);

    float rockBlend = smoothstep(g2r_min, g2r_max, h);
    float snowBlend = smoothstep(r2s_min, r2s_max, h);
    float wSnow  = snowBlend;
    float wGrass = (1.0 - rockBlend) * (1.0 - wSnow);
    float wRock  = rockBlend * (1.0 - wSnow);

    vec3 N = normalize(fs_in.TBN[2]);
    vec3 V = normalize(viewPos - fs_in.FragPos);
    vec3 L = normalize(-lightDir);
    vec3 H = normalize(V + L);

    vec3 albedo = wGrass * grassColor + wRock * rockColor + wSnow * snowColor;

    float slope = 1.0 - max(N.y, 0.0);
    vec3 slopeAccent = mix(vec3(1.0), vec3(0.72, 0.78, 0.86), smoothstep(0.18, 0.75, slope));
    albedo *= slopeAccent;

    float NdotL = max(dot(N, L), 0.0);
    float spec = pow(max(dot(N, H), 0.0), 48.0) * specularFactor;

    vec3 ambient = ambientFactor * albedo;
    vec3 diffuse = albedo * lightColor * NdotL;
    vec3 color = ambient + diffuse + spec * lightColor;

    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));

    FragColor = vec4(color, 1.0);
}