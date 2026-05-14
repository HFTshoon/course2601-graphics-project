#version 330 core

out vec4 FragColor;

in vec2 TexCoord;

uniform sampler2D brushTexture;
uniform float opacity;
uniform vec3 strokeColor;

void main()
{
    vec4 tex = texture(brushTexture, TexCoord);
    float alpha = tex.a * opacity;
    if (alpha < 0.01) {
        discard;
    }
    FragColor = vec4(strokeColor, alpha);
}
