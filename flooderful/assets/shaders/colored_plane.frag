#version 150 core

out vec4 outColor;

uniform vec4 color;
uniform sampler2D texture;

in vec2 _texCoord;

void main()
{
    outColor = color * texture2D(texture, _texCoord);
}