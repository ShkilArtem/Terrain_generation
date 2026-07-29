#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec2 aTexCoord;
layout(location=3) in vec3 aTangent;
layout(location=4) in vec3 aBitangent;
layout(location=5) in float aErosionDelta;
layout(location=6) in float aHardness;

out VS_OUT {
    vec3 FragPos;
    vec2 TexCoord;
    mat3 TBN;
    float ErosionDelta;
    float Hardness;
} vs_out;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    vs_out.FragPos  = vec3(model * vec4(aPos, 1.0));
    vs_out.TexCoord = aTexCoord;
    vs_out.ErosionDelta = aErosionDelta;
    vs_out.Hardness = aHardness;

    // TBN transforms sampled normal maps from tangent space into world space.
    vec3 T = normalize(mat3(model) * aTangent);
    vec3 B = normalize(mat3(model) * aBitangent);
    vec3 N = normalize(mat3(model) * aNormal);
    vs_out.TBN = mat3(T, B, N);

    gl_Position = projection * view * model * vec4(aPos, 1.0);
}
