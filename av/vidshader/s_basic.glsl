#version 330 core

in vec2 texcoord;

out vec4 color;

uniform sampler2D tex;

void main() {
  color = vec4(texture(tex, texcoord).rgb, 1.0f);
}