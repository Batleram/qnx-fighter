class Particle:
    def __init__(self, game, p_type, pos, velocity=[0, 0], frame=0):
        '''
        instantiates a partcle
        (game, particle type, position: tuple, velocity=list, frame tp start on)
        '''
        self.game = game
        self.type = p_type
        self.pos = list(pos)
        self.velocity = list(velocity) # making a copy
        self.animation = self.game.assets['particle/' + p_type].copy() # get a copy of animation 
        self.frame = frame

    
    def update(self):
        '''
        updates particle affect, returns true once animation is finished playing
        -> bool
        '''
        kill = False
        if self.animation.done:
            kill = True
        self.pos[0] += self.velocity[0]
        self.pos[1] += self.velocity[1]

        self.animation.update()

        return kill
    
    def render(self, surf, offset=(0,0)):
        img = self.animation.img()
        surf.blit(img, (self.pos[0] - offset[0] - img.get_width() // 2, self.pos[1] - offset[1] - img.get_height() // 2))




