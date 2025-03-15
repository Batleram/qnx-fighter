import sys
import os
import math
import random
import pygame

from Scripts.utils import load_image, load_images, Animation
from Scripts.entities import PhysicsEntity, Player

class Game:
    def __init__(self):
        '''
        initializes Game
        '''
        pygame.init()

        # change the window caption
        pygame.display.set_caption("Street Fighter")
        # create window
        self.screen = pygame.display.set_mode((640,480))

        self.display = pygame.Surface((320, 240), pygame.SRCALPHA) # render on smaller resolution then scale it up to bigger screen

        self.display_2 = pygame.Surface((320, 240))

        self.clock = pygame.time.Clock()
        
        self.movement = [False, False]

        self.assets = {
            'player': Animation(load_images('PlayerBlue/Run'), img_dur=1),
            'player/idle': Animation(load_images('PlayerBlue/Run'), img_dur=1),
            'player/run': Animation(load_images('PlayerBlue/Run'), img_dur=4),
            'player/jump': Animation(load_images('PlayerBlue/kick')),
            'player/block': Animation(load_images('PlayerBlue/block')),
            'player/crouch': Animation(load_images('PlayerBlue/block')),
        }

        # initalizing player
        self.player_blue = Player(self, (100, 100), (24, 24))
        self.player_red = Player(self, (200, 100), (24, 24))

        # tracking level
        self.level = 0
        self.load_level(self.level)

        # screen shake
        self.screenshake = 0


    def load_level(self, map_id):
        self.player_blue.dead = 0
        self.player_red.dead = 0


    def run(self):
        '''
        runs the Game
        '''

        # creating an infinite game loop
        while True:
            self.display.fill((0, 0, 0, 0))    # outlines
            # clear the screen for new image generation in loop
            self.display_2.blit(self.assets['background'], (0,0)) # no outline

            self.screenshake = max(0, self.screenshake-1) # resets screenshake value


            if self.player_blue.dead: # get hit once
                self.dead += 1
                self.load_level(self.level)

            if self.player_red.dead: # get hit once
                self.dead += 1

            if not self.player_red.dead:
                # update player movement
                self.player_red.update((self.movement[1] - self.movement[0], 0))
                self.player_red.render(self.display)
            
            if not self.player_blue.dead:
                # update player movement
                self.player_blue.update((self.movement[1] - self.movement[0], 0))
                self.player_blue.render(self.display)

            # ouline based on display
            display_mask = pygame.mask.from_surface(self.display)
            display_sillhouette = display_mask.to_surface(setcolor=(0, 0, 0, 180), unsetcolor=(0, 0, 0, 0)) # 180 opaque, 0 transparent
            self.display_2.blit(display_sillhouette, (0, 0))
            for offset in [(-1, 0), (1, 0), (0, -1), (0, 1)]:
                self.display_2.blit(display_sillhouette, offset) # putting what we drew onframe back into display

            for event in pygame.event.get():
                if event.type == pygame.QUIT: # have to code the window closing
                    pygame.quit()
                    sys.exit()
                if event.type == pygame.KEYDOWN:
                    if event.key == pygame.K_LEFT: # referencing right and left arrow keys
                        self.movement[0] = True
                    if event.key == pygame.K_RIGHT: 
                        self.movement[1] = True
                    if event.key == pygame.K_UP: # jump!, dont care about it's release as I dont want a constant jump on hold
                        if self.player.jump():  # velocity pointing upwards, gravity will pull player back down over time
                            pass
                    if event.key == pygame.K_x:
                        self.player.dash()
                if event.type == pygame.KEYUP: # when key is released
                    if event.key == pygame.K_LEFT: 
                        self.movement[0] = False
                    if event.key == pygame.K_RIGHT: 
                        self.movement[1] = False

            self.display_2.blit(self.display, (0, 0)) # cast display 2 on display

            screenshake_offset = (random.random() * self.screenshake - self.screenshake / 2, random.random() * self.screenshake - self.screenshake / 2)
            self.screen.blit(pygame.transform.scale(self.display_2, self.screen.get_size()), screenshake_offset) # render (now scaled) display image on big screen
            pygame.display.update()
            self.clock.tick(60) # run at 60 fps, like a sleep

# returns the game then runs it
Game().run()