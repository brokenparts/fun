#version 330 core

in vec2 texcoord;

out vec4 color;

uniform sampler2D tex;

void main() {
  float bias = 5.0f / 1280.0f;
  color.r = texture(tex, texcoord + vec2(bias, 0.0f)).r;
  color.g = texture(tex, texcoord + vec2(0.0f, 0.0f)).g;
  color.b = texture(tex, texcoord - vec2(bias, 0.0f)).b;
  color.a = 1.0f;
}