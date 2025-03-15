import pygame

from scripts.UI import Image, Text

class GameOver():
    def __init__(self, game, score):
        ''' create the gameover UI'''

        self.game = game
        self.score = score

        self.death_panel = pygame.Surface((1920, 1080), pygame.SRCALPHA)

        self.text = Text('Game Over', [830, 100])
        self.text2 = Text('Score: '+str(score),  [845, 335])
        self.text4 = Text('You Suck :(', [870, 540])
        self.text3 = Text('Press Enter to play again, Escape to return to main menu', [350, 720])



    def update(self):
        ''' update the menu'''

    def render(self):
        ''' render the menu'''
        self.death_panel.fill((255,255,255, 100))

        self.text.render(self.death_panel, 70, (0,0,0))
        self.text2.render(self.death_panel, 70, (0,0,0))
        self.text3.render(self.death_panel, 70, (0,0,0))

        if self.score[0] == "-":
            self.text4.render(self.death_panel, 50, (0,0,0))
        
        self.game.display.blit(self.death_panel, (0,0))



