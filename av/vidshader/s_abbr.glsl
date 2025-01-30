#version 330 core

in vec2 texcoord;

out vec4 color;

uniform sampler2D u_sampler;
uniform vec2      u_resolution;

void main() {
  float bias = 5.0f / u_resolution.x;
  color.r = texture(u_sampler, texcoord + vec2(bias, 0.0f)).r;
  color.g = texture(u_sampler, texcoord + vec2(0.0f, 0.0f)).g;
  color.b = texture(u_sampler, texcoord - vec2(bias, 0.0f)).b;
  color.a = 1.0f;
}