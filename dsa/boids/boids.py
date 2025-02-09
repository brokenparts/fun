#!/usr/bin/env python3

# SPDX-License-Identifier: MIT

import math
import random

import pygame as g
from pygame.locals import *

SCR_WIDTH 	= 1280
SCR_HEIGHT	= 720

ARENA 		= 300

BOID_VISION = 50
BOID_AVOID	= 15

CENTER = g.Vector2(SCR_WIDTH / 2, SCR_HEIGHT / 2)

WHITE = (255, 255, 255)

class Bot():
    def __init__(self, pos, ang):
        ang = math.radians(ang)
        self.pos = g.Vector2(pos)
        self.vel = g.Vector2(math.cos(ang), math.sin(ang))

def delta_angle(ang_from, ang_to):
    if (ang_to - ang_from) > -180:
        return ang_to - ang_from
    else:
        return -1 * (ang_to - ang_from)

def signof(x):
    if x > 0:
        return 1
    else:
        return -1

def average_vec2(arr):
    assert len(arr) > 0
    return g.Vector2(
        sum([v.x for v in arr]) / len(arr),
        sum([v.y for v in arr]) / len(arr)
    )

def scale_vec(v, s):
    return g.Vector2(v.x * s, v.y * s)

if __name__ == '__main__':
    g.init()
    surf = g.display.set_mode((SCR_WIDTH, SCR_HEIGHT))
    g.display.set_caption('Boids')

    bots = [ ]

    while True:
        # Process inputs
        for evt in g.event.get():
            if evt.type == QUIT:
                exit(1)
            elif evt.type == MOUSEBUTTONDOWN:
                # spawn a bot
                if (evt.pos - CENTER).length() <= ARENA:
                    bots.append(Bot(evt.pos, random.uniform(0, 365)))

        # Update bots
        for i, bot in enumerate(bots):
            # Collect neighbors
            neighbors = []
            for j, neighbor in enumerate(bots):
                if i != j and (bot.pos - neighbor.pos).length() < BOID_VISION:
                    neighbors.append(neighbor)

            # Calculate average position / angle of neighbors
            avg_pos = bot.pos
            avg_vel = bot.vel
            # avg_ang = bot.ang
            if len(neighbors) > 0:
                avg_pos = average_vec2([n.pos for n in neighbors])
                avg_vel = average_vec2([n.vel for n in neighbors])

            # Separation
            vel_s = g.Vector2(0, 0)
            for n in neighbors:
                if (n.pos - bot.pos).length() < BOID_AVOID:
                    vel_s = vel_s + (bot.pos - n.pos)
            vel_s = scale_vec(vel_s, 0.005)

            # Alignment
            vel_a = scale_vec(avg_vel - bot.vel, 0.005)

            # Cohesion
            vel_c = scale_vec(avg_pos - bot.pos, 0.003)

            # Track towards center
            vel_t = scale_vec(CENTER - bot.pos, 0.0005)

            # Avoid barrier
            # vel_b = scale_vec(g.Vector2())

            bot.vel = bot.vel + vel_s + vel_a + vel_c + vel_t

            # step = g.Vector2(bot.vel.x * 0.0025, bot.vel.y * 0.0025)
            bot.pos = bot.pos + scale_vec(bot.vel, 0.005)

        # Draw arena
        surf.fill((0, 33, 33))
        g.draw.circle(surf, (100, 33, 33), CENTER, ARENA)
        g.draw.circle(surf, WHITE, CENTER, ARENA, width=2)

        # Draw bot vision visualizers
        # for bot in bots:
        # 	g.draw.circle(surf, (100, 50, 50), bot.pos, BOID_VISION)

        # Draw bots
        for bot in bots:
            g.draw.rect(surf, (33, 200, 33), [ bot.pos[0] - 5, bot.pos[1] - 5, 10, 10 ])
            velocity_vec = bot.vel
            velocity_vec.scale_to_length(30)
            g.draw.line(surf, (200, 200, 33), bot.pos, bot.pos + velocity_vec, 1)

        g.display.update()
