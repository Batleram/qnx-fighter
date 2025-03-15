from scripts.UI import Image, Text

class Menu():
    def __init__(self, game):
        ''' create the menu UI'''
        self.game = game

        self.explain2 = Text('Good Luck and Have Fun!', [760, 660])


        # WASD
        # (self, img, pos, speed)
        self.W = Image(self.game.assets['W'].copy(), [500,250], 10, .5)
        self.W.scale(4) 
        self.A = Image(self.game.assets['A'].copy(), [445,305], 10, .5)
        self.A.scale(4)
        self.S = Image(self.game.assets['S'].copy(), [500,360], 10, .5)
        self.S.scale(4)
        self.D = Image(self.game.assets['D'].copy(), [555,305], 10, .5)
        self.D.scale(4)

        self.Move = Text(' Up - Jump \n Down - Block \n Left\Right - Move', [430, 450])

        self.ESC = Image(self.game.assets['ESC'].copy(), [1300,330], 10, .3)
        self.ESC.scale(4)

        self.Leave = Text('Exit', [1300, 450])

        self.click = Image(self.game.assets['click'].copy(), [940, 335], 10, .4)
        self.click.scale(4)

        self.Click = Text('Shoot', [920, 450])



    def update(self):
        ''' update the menu'''
        self.W.update()
        self.A.update()
        self.S.update()
        self.D.update()
        self.ESC.update()
        self.click.update()

    def render(self):
        ''' render the menu'''
        self.W.render(self.game.display)
        self.A.render(self.game.display)
        self.S.render(self.game.display)
        self.D.render(self.game.display)
        self.ESC.render(self.game.display)
        self.click.render(self.game.display)
        self.Move.render(self.game.display, 50, (0,0,0))
        self.Leave.render(self.game.display, 50, (0,0,0))
        self.Click.render(self.game.display, 50, (0,0,0))
        self.explain2.render(self.game.display, 50, (0,0,0))


