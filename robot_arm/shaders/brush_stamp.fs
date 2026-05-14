#version 330 core

out vec4 FragColor;

in vec2 TexCoord;

uniform sampler2D brushTexture;
uniform float opacity;

void main()
{
    vec4 tex = texture(brushTexture, TexCoord);
    float alpha = tex.a * opacity;
    if (alpha < 0.01) {
        discard;
    }
    FragColor = vec4(0.0, 0.0, 0.0, alpha);
}
