import pygame
import math

class Image:
    def __init__(self, img, pos, speed, offset=0):
        '''
        initializing the heart
        (image, position=[x,y], speed)
        '''
        self.img = img
        self.pos = pos
        self.speed = speed
        self.posy = pos[1]
        self.offset = offset

    def update(self):
        '''
        update fn, calculates new position on y axis
        '''
        # make the image smoothly move up and down
        self.pos[1] = self.posy + 10 * math.sin(pygame.time.get_ticks() / 1000 * self.speed * self.offset)

    def render(self, surf):
        '''
        renders img on screen
        '''
        surf.blit(self.img, self.pos)

    def scale(self, scale):
        '''
        scales the image
        '''
        self.img = pygame.transform.scale(self.img, (self.img.get_width() * scale, self.img.get_height() * scale))

class Text:
    def __init__(self, level, pos=[0,0]):
        '''
        initializing the level counter
        (current level, position=[x,y])
        '''
        self.level = level
        self.pos = pos
    

    def render(self, surf, fontsize, color=(255,255, 0), text=None):
        '''
        renders img on screen
        (surface, font size)
        '''
        if not text == None:
            self.level = text
        self.fontsize = fontsize
        current_level = pygame.font.SysFont('ignore warning this resets to default', fontsize).render(f"{self.level}", False, color)
        surf.blit(current_level, self.pos)