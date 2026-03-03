#version 460 core

in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D displayTex;
uniform sampler2D physicsTex;
uniform sampler2D normalTex;
uniform vec4 uvBounds;
uniform int viewMode; // 0=normal, 1=normals remap, 2=lightmap, 3=force field

void main() {
    // Remap TexCoord
    vec2 uv = mix(uvBounds.xy, uvBounds.zw, TexCoord);

    if (viewMode == 1) {
        // Normal map
        vec4 n = texture(displayTex, uv);
        FragColor = vec4(n.xyz * 0.5 + 0.5, 1.0);
    }
    else if (viewMode == 2) {
        // Lightmap
        vec4 light = texture(displayTex, uv);
        vec3 mapped = light.rgb / (light.rgb + vec3(1.0));
        FragColor = vec4(mapped, 1.0);
    }
    else if (viewMode == 3) {
        // Force field
        vec4 force = texture(displayTex, uv);
        float magnitude = length(force.rg);
        vec2 dir = magnitude > 0.001 ? normalize(force.rg) : vec2(0.0);
        float angle = atan(dir.y, dir.x) / 3.14159 * 0.5 + 0.5; // 0..1

        vec3 color;
        float h = angle * 6.0;
        float f = fract(h);
        if (h < 1.0)      color = vec3(1.0, f, 0.0);
        else if (h < 2.0) color = vec3(1.0 - f, 1.0, 0.0);
        else if (h < 3.0) color = vec3(0.0, 1.0, f);
        else if (h < 4.0) color = vec3(0.0, 1.0 - f, 1.0);
        else if (h < 5.0) color = vec3(f, 0.0, 1.0);
        else              color = vec3(1.0, 0.0, 1.0 - f);

        float brightness = clamp(magnitude * 2.0, 0.0, 1.0);
        FragColor = vec4(color * brightness, 1.0);
    }
    else {
        // Default
        FragColor = texture(displayTex, uv);
    }
}