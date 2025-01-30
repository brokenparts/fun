#version 330 core

in vec2 texcoord;

out vec4 color;

uniform sampler2D tex;

void main() {
  color = vec4(texture(tex, vec2(texcoord.x, texcoord.y + sin(texcoord.x * 10) / 10.0f)).rgb, 1.0f);
}