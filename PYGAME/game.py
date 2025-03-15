import sys
import os
import math
import random
import pygame, os

from scripts.utils import load_image, load_images, Animation
from scripts.entities import Player, Enemy
from scripts.bullet import Bullet
from scripts.tilemap import Tilemap
from scripts.UI import Text
from scripts.menu import Menu
from scripts.gameover import GameOver


class Game:
    def __init__(self):
        '''
        initializes Game
        '''
        pygame.init()

        # change the window caption
        pygame.display.set_caption("Timeframe")

        # create window
        self.screen = pygame.display.set_mode((0,0), pygame.FULLSCREEN)
        self.screen_size = pygame.display.get_surface().get_size()
        self.display = pygame.Surface((1920, 1080))

        self.clock = pygame.time.Clock()
        
        self.movement = [False, False, False, False]  # left, right, up, down
        self.slowdown = 0 # slow down the game
        self.game_speed = 1

        self.assets = {
            'ground': load_images('tiles/ground'),
            'obstacles': load_images('tiles/obstacles'),
            'player': load_image('entities/player.png'),
            'player/idle': Animation(load_images('entities/player/idle'), img_dur=6),
            'enemy/idle': Animation(load_images('entities/enemy/idle'), img_dur=6),
            'target': load_image('entities/target_round_a.png'),
            'playerbullet': load_image('entities/PlayerBullet.png'),
            'enemybullet': load_image('entities/enemy_bullet.png'),
            'W': load_image('UI/W.png'),
            'A': load_image('UI/A.png'),
            'S': load_image('UI/S.png'),
            'D': load_image('UI/D.png'),
            'ESC': load_image('UI/ESC.png'),
            'click': load_image('UI/click.png'),
            'button': load_image('UI/button.png'),
            'button': load_image('UI/button.png'),
        }

        # adding sound
        self.sfx = {
            'player_death': pygame.mixer.Sound('data/sfx/player_death.wav'),
            'enemy_death': pygame.mixer.Sound('data/sfx/enemy_kill.wav'),
            'select': pygame.mixer.Sound('data/sfx/select.wav'),
            'shoot': pygame.mixer.Sound('data/sfx/shoot.wav'),
        }
        self.ost = {
            'introstart': pygame.mixer.Sound('data/Music/FixedMusic/IntroMusic_Start.wav'),
            'introloop': pygame.mixer.Sound('data/Music/FixedMusic/IntroMusic_Loop.wav'),
            'introtrans': pygame.mixer.Sound('data/Music/FixedMusic/IntroMusic_Transition.wav'),
            'battleloop': pygame.mixer.Sound('data/Music/FixedMusic/BattleMusic_Loop.wav'),
            'battletrans': pygame.mixer.Sound('data/Music/FixedMusic/BattleMusic_Transition.wav'),
            'deathloop': pygame.mixer.Sound('data/Music/FixedMusic/SlowMusicStyle_Loop.wav'),
            'deathtrans': pygame.mixer.Sound('data/Music/FixedMusic/SlowMusicStyle_Transition.wav'),
        }

        self.sfx['shoot'].set_volume(0.8)
        self.sfx['select'].set_volume(0.3)
        self.sfx['player_death'].set_volume(0.7)
        self.sfx['enemy_death'].set_volume(0.8)

        #self.ost['introstart'].set_volume(0.6)
        self.ost['introloop'].set_volume(0.8)
        self.ost['battleloop'].set_volume(0.6)
        self.ost['deathloop'].set_volume(0.8)


        # initalizing player
        self.player = Player(self, (self.display.get_width()/2, self.display.get_height()/2), (42, 42))

        # initalizing tilemap
        self.tilemap = Tilemap(self, tile_size=64)
        self.tilemap.load('data/maps/map.json')
        self.ground = Tilemap(self, tile_size=64)
        self.ground.load('data/maps/ground.json')

        self.formated_timer = '0:00'

        # screen shake
        self.screenshake = 0

        self.dead = 0

        self.scroll = [0, 0]

        self.playmus = True
        self.playmenumus = True

        pygame.mouse.set_visible(False)
    def main_menu(self):
        self.ost['battleloop'].stop()
        self.ost['deathloop'].stop()
        if self.playmenumus:
            self.ost['introloop'].play(-1)
            self.playmenumus = False
        self.menu_ground = Tilemap(self, tile_size=64)
        self.menu_ground.load('data/maps/main_menu.json')
        while True:
            self.display.fill((255, 255, 255))
            self.menu_ground.render(self.display, (0, 0))

            self.title = Text('Timeframe', [750, 200])
            self.title.render(self.display, 120, (0,0,0))

            self.display.blit(pygame.transform.scale(self.assets['button'], (self.assets['button'].get_width() * 1.75, self.assets['button'].get_height() * 1.75)), (850, 485))
            start_text = Text('Start', (920, 509))
            start_text.render(self.display, 50, color=(0, 0, 0))
            start_rect = pygame.Rect(850, 485, self.assets['button'].get_width() * 1.75, self.assets['button'].get_height() * 1.75)

            self.display.blit(pygame.transform.scale(self.assets['button'], (self.assets['button'].get_width() * 1.75, self.assets['button'].get_height() * 1.75)), (850, 635))
            controls_text = Text('Tutorial', (900, 659))
            controls_text.render(self.display, 50, color=(0, 0, 0))
            controls_rect = pygame.Rect(850, 635, self.assets['button'].get_width() * 1.75, self.assets['button'].get_height() * 1.75)

            self.display.blit(pygame.transform.scale(self.assets['button'], (self.assets['button'].get_width() * 1.75, self.assets['button'].get_height() * 1.75)), (850, 785))
            quit_text = Text('Quit', (920, 809))
            quit_text.render(self.display, 50, color=(0, 0, 0))
            quit_rect = pygame.Rect(850, 785, self.assets['button'].get_width() * 1.75, self.assets['button'].get_height() * 1.75)

            mpos = pygame.mouse.get_pos() # gets mouse positon
            mpos = (mpos[0] / (self.screen_size[0]/self.display.get_width()), mpos[1] / (self.screen_size[1]/self.display.get_height())) # since screen sometimes scales
            self.display.blit(pygame.transform.scale(self.assets['target'], (32, 32)), (mpos[0], mpos[1]))

            for event in pygame.event.get():
                if event.type == pygame.QUIT: # have to code the window closing
                    pygame.quit()
                    sys.exit()
                if event.type == pygame.MOUSEBUTTONDOWN:
                    if event.button == 1:
                        if start_rect.collidepoint(mpos):
                            self.sfx['select'].play(0)
                            self.run()
                        if controls_rect.collidepoint(mpos):
                            self.sfx['select'].play(0)
                            self.controls()
                        if quit_rect.collidepoint(mpos):
                            pygame.quit()
                            sys.exit()
                if event.type == pygame.KEYDOWN:
                    if event.key == pygame.K_ESCAPE:
                        pygame.quit()
                        sys.exit()
                    if event.key == pygame.K_RETURN:
                        self.sfx['select'].play(0)
                        self.ost['introstart'].stop()
                        self.run()
            
            self.screen.blit(pygame.transform.scale(self.display, self.screen_size), [0,0])
            pygame.display.update()
            self.deltatime = self.clock.tick(60) # run at 60 fps, like a sleep
            

    def controls(self):
        while True:
            self.display.fill((255, 255, 255))

            self.display.blit(pygame.transform.scale(self.assets['button'], (self.assets['button'].get_width() * 1.75, self.assets['button'].get_height() * 1.75)), (850, 785))
            back_text = Text('Back', (920, 809))
            back_text.render(self.display, 50, color=(0, 0, 0))
            back_rect = pygame.Rect(850, 785, self.assets['button'].get_width() * 1.75, self.assets['button'].get_height() * 1.75)

            mpos = pygame.mouse.get_pos() # gets mouse positon
            mpos = (mpos[0] / (self.screen_size[0]/self.display.get_width()), mpos[1] / (self.screen_size[1]/self.display.get_height())) # since screen sometimes scales
            self.display.blit(pygame.transform.scale(self.assets['target'], (32, 32)), (mpos[0], mpos[1]))

            for event in pygame.event.get():
                if event.type == pygame.QUIT: # have to code the window closing
                    pygame.quit()
                    sys.exit()
                if event.type == pygame.MOUSEBUTTONDOWN:
                    if event.button == 1:
                        if back_rect.collidepoint(mpos):
                            self.sfx['select'].play(0)
                            self.main_menu()
                if event.type == pygame.KEYDOWN:
                    if event.key == pygame.K_ESCAPE:
                        self.sfx['select'].play(0)
                        self.main_menu()

            # render the main menu
            menu = Menu(self)
            menu.update()
            menu.render()

            

            
            
            self.screen.blit(pygame.transform.scale(self.display, self.screen_size), [0,0])
            pygame.display.update()
            self.deltatime = self.clock.tick(60) # run at 60 fps, like a sleep

    def game_over(self, score):
        # render the game over screen
        if self.playmus:
            self.ost['battleloop'].stop()
            self.ost['introloop'].stop()
            self.ost['deathloop'].play(-1)
            self.playmus = False
        
        game_over = GameOver(self, score)
        game_over.update()
        game_over.render()



    def run(self):
        '''
        runs the Game
        '''
        self.screenshake = 0

        self.playmus = True
        self.playmenumus = True

        self.dead = 0

        self.player.pos = [self.display.get_width()/2, self.display.get_height()/2]

        self.movement = [False, False, False, False]  # left, right, up, down
        self.slowdown = 0 # slow down the game
        self.game_speed = 1

        #Enemy spawn wait (milliseconds)
        self.enemy_timer_reset = 1600
        self.enemy_timer = self.enemy_timer_reset

        self.enemies = []
        self.bullets = []
        self.sparks = []

        self.deltatime = 0

        self.game_timer = 30000

        self.slowdown_timer_change = 3

        self.has_moved = False

        level_bar = Text("Time Left: " + str(self.game_timer), pos=(self.display.get_width() // 2 -30, 13))
        self.ost['deathloop'].stop()
        self.ost['introloop'].stop()
        self.ost['battleloop'].stop()
        self.ost['battleloop'].play(-1)
        


        # creating an infinite game loop
        while True:
            self.display.fill((255, 255, 255))
            # clear the screen for new image generation in loop

            self.screenshake = max(0, self.screenshake-1) # resets screenshake value

            #Count game timer down if has moved
            if self.has_moved and not self.dead:
                self.game_timer -= self.deltatime * (1 + self.slowdown * (self.slowdown_timer_change -1))

            
            #Count down if has moved, if time elapsed spawn enemy
            if self.has_moved:
                self.enemy_timer -= self.deltatime * (1 - (self.slowdown * (self.slowdown_timer_change-1)/self.slowdown_timer_change))
            if self.enemy_timer < 0:
                enemy_pos = [100, 100]
                if random.randint(0, 1) == 0:
                    enemy_pos[0] = 0 + ((self.display.get_width() + 42) * random.randint(0, 1)) - 42
                    enemy_pos[1] = random.randint(0, self.display.get_height() + 42) - 42
                else:
                    enemy_pos[1] = 0 + ((self.display.get_height() + 42) * random.randint(0, 1)) - 42
                    enemy_pos[0] = random.randint(0, self.display.get_width() + 42) - 42
                new_enemy = Enemy(self, enemy_pos, (42, 42))
                self.enemies.append(new_enemy)
                #next wait (milliseconds)
                self.enemy_timer_reset -= 100
                if self.enemy_timer_reset < 700:
                    self.enemy_timer_reset = 700
                self.enemy_timer = self.enemy_timer_reset

            render_scroll = (0, 0)

            self.ground.render(self.display, offset=render_scroll)
            self.tilemap.render(self.display, offset=render_scroll)

            for bullet in self.bullets.copy():
                collided = bullet.update(self.tilemap)
                if collided:
                    self.bullets.remove(bullet)
                else:
                    if bullet.pos[0] > self.display.get_width():
                        self.bullets.remove(bullet)
                    if self.player.pos[1] > self.display.get_height():
                        self.bullets.remove(bullet)
                    if self.player.pos[0] < 0 - bullet.size[0]:
                        self.bullets.remove(bullet)
                    if self.player.pos[1] < 0 - bullet.size[1]:
                        self.player.pos[1] = 0 
                    bullet.render(self.display, offset=render_scroll)

            # handle changes in game speed
            if self.slowdown:
                self.game_speed = 0.4
            else:
                self.game_speed = 1
            
            #update enemies
            for enemy in self.enemies:
                enemy.update(self.tilemap, (self.player.pos[0] - enemy.pos[0], self.player.pos[1] - enemy.pos[1]))
                enemy.render(self.display)

            if not self.dead:
                # update player movement
                self.player.update(self.tilemap, (self.movement[1] - self.movement[0], self.movement[3] - self.movement[2]))
                if self.player.pos[0] > self.display.get_width() - 42:
                    self.player.pos[0] = self.display.get_width() - 42
                if self.player.pos[1] > self.display.get_height() - 42:
                    self.player.pos[1] = self.display.get_height() - 42
                if self.player.pos[0] < 0:
                    self.player.pos[0] = 0 
                if self.player.pos[1] < 0:
                    self.player.pos[1] = 0 
                self.player.render(self.display, offset=render_scroll)

            for spark in self.sparks:
                spark.update()
                spark.render(self.display, offset=render_scroll)

            #Show time left
            min = math.floor((self.game_timer/1000/60))
            sec = math.floor((self.game_timer/1000) % 60)
            ms = self.game_timer - math.floor(self.game_timer/1000) * 1000
            if sec == 0:
                sec = '00'
            elif sec < 10:
                sec = '0' + str(sec)
            if min:
                formatted_timer = str(min) + ':' + str(sec)
            else:
                formatted_timer = str(sec)
                if int(sec) <= 10:
                    formatted_timer = formatted_timer + '.' + str(ms)
            self.formated_timer = formatted_timer 
            level_bar.render(self.display, 50, color=(0, 0, 0), text=formatted_timer)

            if self.dead: # get hit once
                self.dead += 1
            if self.dead > 80:
                self.game_over(self.formated_timer)

            # player cursor display bulleye
            mpos = pygame.mouse.get_pos() # gets mouse positon
            mpos = (mpos[0] / (self.screen_size[0]/self.display.get_width()), mpos[1] / (self.screen_size[1]/self.display.get_height())) # since screen sometimes scales
            self.display.blit(pygame.transform.scale(self.assets['target'], (32, 32)), (mpos[0], mpos[1]))


            for event in pygame.event.get():
                if event.type == pygame.QUIT: # have to code the window closing
                    pygame.quit()
                    sys.exit()
                if event.type == pygame.MOUSEBUTTONDOWN:
                    if event.button == 1 and not self.dead:
                        dx = mpos[0] - self.player.rect().centerx
                        dy = mpos[1] - self.player.rect().centery
                        bullet_angle = math.atan2(dx, -dy) - (math.pi/2)
                        new_bullet = Bullet(self, self.player.rect().center, 7, bullet_angle, (18, 18))
                        self.sfx['shoot'].play(0)
                        self.bullets.append(new_bullet)
                        self.game_timer -= 1000
                        self.screenshake = max(10, self.screenshake)
                if event.type == pygame.KEYDOWN:
                    if event.key == pygame.K_ESCAPE:
                        self.sfx['select'].play(0)
                        self.sfx['select'].play(0)
                        self.main_menu()
                    if self.dead and event.key == pygame.K_RETURN:
                        self.sfx['select'].play(0)
                        self.sfx['select'].play(0)
                        self.run()
                    if event.key == pygame.K_a: # referencing right and left arrow keys
                        self.movement[0] = True
                    elif event.key == pygame.K_d: 
                        self.movement[1] = True
                    elif event.key == pygame.K_w:
                        self.movement[2] = True
                    elif event.key == pygame.K_s:
                        self.movement[3] = True
                    self.has_moved = True
                if event.type == pygame.KEYUP: # when key is released
                    if event.key == pygame.K_a: 
                        self.movement[0] = False
                    elif event.key == pygame.K_d: 
                        self.movement[1] = False
                    elif event.key == pygame.K_w:
                        self.movement[2] = False
                    elif event.key == pygame.K_s:
                        self.movement[3] = False
                
            if self.movement[1] - self.movement[0] == 0 and self.movement[3] - self.movement[2] == 0 or self.dead:
                self.slowdown = True
            else:
                self.slowdown = False
            

            screenshake_offset = (random.random() * self.screenshake - self.screenshake / 2, random.random() * self.screenshake - self.screenshake / 2)
            self.screen.blit(pygame.transform.scale(self.display, self.screen_size), screenshake_offset)
            pygame.display.update()
            self.deltatime = self.clock.tick(60) # run at 60 fps, like a sleep

# returns the game then runs it
Game().main_menu()