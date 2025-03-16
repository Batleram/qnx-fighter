import pygame
class Background:
    def __init__(self, image, speed, display):
        self.image = image
        self.speed = speed
        self.display = display
        self.scale = 5

    def draw(self):
        self.image.update()
        newSurf = pygame.transform.scale(self.image.img(), self.display.get_size()) # scale the image
        self.display.blit(newSurf, (0,0)) # no outline