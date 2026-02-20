#version 460 core

in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D displayTex;
uniform sampler2D physicsTex;
uniform sampler2D normalTex;
uniform vec4 uvBounds;

void main() {
    // Remap TexCoord
    vec2 uv = mix(uvBounds.xy, uvBounds.zw, TexCoord);
    FragColor = texture(displayTex, uv);
}