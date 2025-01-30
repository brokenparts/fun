#version 330 core

in vec2 texcoord;

out vec4 color;

uniform sampler2D u_sampler;
uniform vec2      u_resolution;

float k_identity[9] = float[9](
  0.0f, 0.0f, 0.0f,
  0.0f, 1.0f, 0.0f,
  0.0f, 0.0f, 0.0f
);

float k_sharpen[9] = float[9](
   0.0f, -1.0f,  0.0f,
  -1.0f,  5.0f, -1.0f,
   0.0f, -1.0f,  0.0f
);

float k_edge[9] = float[9](
  -1.0f, -1.0f, -1.0f,
  -1.0f,  8.0f, -1.0f,
  -1.0f, -1.0f, -1.0f
);

float k_gaussian[9] = float[9](
  1.0f / 16.0f, 2.0f / 16.0f, 1.0f / 16.0f,
  2.0f / 16.0f, 4.0f / 16.0f, 2.0f / 16.0f,
  1.0f / 16.0f, 2.0f / 16.0f, 1.0f / 16.0f
);

void main() {
  vec2 dir_step = vec2(1.0f) / u_resolution;
  vec2 k_directions[9] = vec2[9](
    vec2(-dir_step.x, -dir_step.y), vec2(0.0f, -dir_step.y), vec2(dir_step.x, -dir_step.y),
    vec2(-dir_step.x,  0.0f),       vec2(0.0f,  0.0f),       vec2(dir_step.x,  0.0f),
    vec2(-dir_step.x,  dir_step.y), vec2(0.0f,  dir_step.y), vec2(dir_step.x,  dir_step.y)
  );

  float[9] kernel = k_gaussian;

  vec3 sum = vec3(0.0f);
  for (int i = 0; i < 9; ++i) {
    sum += texture(u_sampler, texcoord + k_directions[i]).rgb * kernel[i];
  }

  //sum = texture(u_sampler, texcoord + k_directions[4]).rgb * kernel[4];

  color = vec4(sum, 1.0f);
}