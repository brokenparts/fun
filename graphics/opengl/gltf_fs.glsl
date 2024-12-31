// SPDX-License-Identifier: MIT

#version 330 core

in vec2 v_t;

uniform sampler2D u_texture;

out vec4 color;

void main() {
    color = texture(u_texture, v_t);
}
