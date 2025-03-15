import math
import pygame
import random

from scripts.spark import Spark

class Bullet():
    def __init__(self, game, pos, velocity, angle, size=(18, 18), type='player'):
        self.game = game
        self.pos = list(pos)
        self.velocity = velocity
        self.angle = angle
        self.size = size
        self.type = type
    
    def render(self, surf, offset=(0, 0)):
        surf.blit(self.game.assets[self.type + 'bullet'], (self.pos[0] - offset[0], self.pos[1] - offset[1]))

    def update(self, tilemap):
        self.pos[0] += math.cos(self.angle) * self.velocity * self.game.game_speed
        self.pos[1] += math.sin(self.angle) * self.velocity * self.game.game_speed
        if tilemap.solid_check(self.pos):
            self.spark(self.angle - math.pi)
            return True
        else:
            if self.type == 'player':
                for enemy in self.game.enemies.copy():
                    rect = enemy.rect()
                    if rect.collidepoint(self.pos):
                        self.spark(self.angle)
                        self.game.sfx['enemy_death'].play(0)
                        self.game.enemies.remove(enemy)
                        self.game.screenshake = max(15, self.game.screenshake)
                        if not self.game.dead:
                            self.game.game_timer += 5000
                        return True
            elif self.type == 'enemy':
                rect = self.game.player.rect()
                if rect.collidepoint(self.pos) and not self.game.dead:
                    self.spark(self.angle)
                    self.game.sfx['player_death'].play(0)
                    self.game.screenshake = max(25, self.game.screenshake)
                    self.game.dead += 1
                    return True
    
    def spark(self, angle):
        angle_range = math.pi/3
        for i in range(10):
            new_spark = Spark(self.pos, (angle - angle_range/2 + (random.random() * angle_range)), random.random() * 8 + 1)
            self.game.sparks.append(new_spark)