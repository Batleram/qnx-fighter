import pygame
import math

from scripts.bullet import Bullet

class PhysicsEntity:
    def __init__(self, game, e_type, pos, size):
        '''
        initializes entities
        (game, entitiy type, position, size)
        '''
        self.game = game
        self.type = e_type
        self.pos = list(pos) #make sure each entitiy has it's own list, (x,y)
        self.size = size
        self.speed = 10
        self.velocity = [0,0,0, 0]
        self.collisions = {'up': False, 'down': False, 'left': False, 'right': False}
        self.scale = 0
        self.flip = False

        self.action = ''
        self.anim_offset = (-50, -100) #renders with an offset to pad the animation against the hitbox
        self.set_action('idle')

        self.last_movement = [0, 0]

    def rect(self):
        '''
        creates a rectangle at the entitiies current postion
        '''
        # draw a rectangle around the entity
        if self.type == "player":
            if self.flip:
                pygame.draw.rect(self.game.display, (255, 0, 0), (self.pos[0], self.pos[1], self.size[0], self.size[1]), 2)
                return pygame.Rect(self.pos[0], self.pos[1], self.size[0], self.size[1])
            else:
                pygame.draw.rect(self.game.display, (255, 0, 0), (self.pos[0]-100, self.pos[1], self.size[0], self.size[1]), 2)
                return pygame.Rect(self.pos[0]-100, self.pos[1], self.size[0], self.size[1])
        else:
            if self.flip:
                pygame.draw.rect(self.game.display, (255, 0, 0), (self.pos[0]-100, self.pos[1], self.size[0], self.size[1]), 2)
                return pygame.Rect(self.pos[0]-100, self.pos[1], self.size[0], self.size[1])
            else:
                pygame.draw.rect(self.game.display, (255, 0, 0), (self.pos[0], self.pos[1], self.size[0], self.size[1]), 2)
                return pygame.Rect(self.pos[0], self.pos[1], self.size[0], self.size[1])
    
    def set_action(self, action):
        '''
        sets a new action to change animation
        (string of animation name) -> animation
        '''
        if action != self.action: # if action has changed
            self.action = action
            self.animation = self.game.assets[self.type + '/' + self.action].copy()


    
    def update(self, tilemap, movement=(0,0)):
        '''
        updates frames and entitiy position 
        '''
        #Normalizing movement vector is in player and enemy
        

        self.collisions = {'up': False, 'down': False, 'left': False, 'right': False} # this value will be reset every frame, used to stop constant increase of velocity

        frame_movement = ((movement[0] + self.velocity[0]) * self.game.game_speed * self.speed, (movement[1] + self.velocity[1]) * self.game.game_speed * self.speed) # movement based on velocity

        self.pos[0] += frame_movement[0]
        entity_rect = self.rect() # getting the entities rectange

        # move tile based on collision on y axis
        for rect in tilemap.physics_rects_around(self.pos):
            if entity_rect.colliderect(rect):
                if frame_movement[0] > 0: # if moving right and you collide with tile
                    entity_rect.right = rect.left
                    self.collisions['right'] = True
                if frame_movement[0] < 0: # if moving left
                    entity_rect.left = rect.right
                    self.collisions['left'] = True
                self.pos[0] = entity_rect.x
        
        # Note: Y-axis collision handling comes after X-axis handling
        self.pos[1] += frame_movement[1]
       
        # move tile based on collision on y axis
        for rect in tilemap.physics_rects_around(self.pos):
            if entity_rect.colliderect(rect):
                if frame_movement[1] > 0: # if moving right and you collide with tile
                    entity_rect.bottom = rect.top
                    self.collisions['down'] = True
                if frame_movement[1] < 0: # if moving left
                    entity_rect.top = rect.bottom
                    self.collisions['up'] = True
                self.pos[1] = entity_rect.y

        # find when to flip img for animation
        if movement[0] > 0:
            if self.type == "player2":
                self.flip = False
            else:
                self.flip = True
        if movement[0] < 0:
            if self.type == "player2":
                self.flip = True
            else:
                self.flip = False
                
        self.last_movement = movement # keeps track of movement

        # gravity aka terminal falling velocity "VERTICLE"
        self.velocity[1] = min(5, self.velocity[1] + 0.1) # (max velocity downwards, ) pos+ is downwards in pygame, from 5 to 0

        if self.collisions['down'] or self.collisions['up'] or self.pos[1] > (self.game.screen_size[1] - 50): # if object hit, stop velocity
            # - 50 for the floor
            self.velocity[1] = 0

        self.animation.update() # update animation


    def render(self, surf, offset={0,0}):
        '''
        renders entitiy asset
        '''
        newSurf = pygame.transform.scale(self.animation.img(), (self.animation.img().get_width() * self.scale, self.animation.img().get_height() * self.scale)) # scale the image
        surf.blit(pygame.transform.flip(newSurf, self.flip, False), (self.pos[0] - offset[0] + self.anim_offset[0], self.pos[1] - offset[1] + self.anim_offset[1])) # fliping agasint horizontal axis

class Player(PhysicsEntity):
    def __init__(self, game, e_type, pos, size):
        '''
        instantiates player entity
        (game, position, size)
        '''
        super().__init__(game, e_type, pos, size)
        self.jumps = 1
        self.crouch = False
        self.timerAction = 0
        self.health = 5
        self.isBlocking = False
        self.isJumping = False
        self.isAttacking = False

    def jump(self):
        '''
        player jump
        '''
        if self.jumps > 0:
            self.velocity[1] = -2
            self.jumps -= 1
            self.isJumping = True

    def attack(self):
        '''
        player attack
        '''
        if self.isBlocking or self.isJumping:
            return
        
        self.isAttacking = True
        self.timerAction = 25
        

    def update(self, tilemap, movement=(0,0)):
        '''
        updates players animations depending on movement
        '''

        if self.timerAction > 0 and self.isAttacking:
            self.timerAction -= 1
            self.isAttacking = True
        
        if self.timerAction > 0 and self.isJumping:
            self.timerAction -= 1
            self.isJumping = True

        if self.isAttacking:
            self.set_action('attack')
            self.isAttacking = False
        elif self.crouch:
            self.set_action('kick')
            # restrict movement when crouching
            movement = (0, 0)
            self.isBlocking = False
        elif self.jumps == 0 and self.isJumping:
            self.set_action('jump')
            self.isBlocking = False
            self.isJumping = False
        elif self.isBlocking:
            self.set_action('block')
        elif movement[0] != 0: # if moving horizontally
            self.set_action('run')
            self.isBlocking = False
        else:
            self.set_action('idle')
        player_movement = movement

        # bound the character to within the screen
        if self.pos[0] < 0:
            self.pos[0] = 0
        if self.pos[0] > (self.game.display.get_width()):
            self.pos[0] = self.game.display.get_width()
        if self.pos[1] > (self.game.display.get_height()):
            self.pos[1] = self.game.display.get_height()
            self.jumps = 1
        
        super().update(tilemap, movement=player_movement)
        movement_magnitude = math.sqrt((movement[0] * movement[0] + movement[1] * movement[1]))
        if movement_magnitude > 0:
            player_movement = (movement[0] / movement_magnitude, movement[1] / movement_magnitude)

        # normalize horizontal vel "HORIZONTAL"
        if self.velocity[0] > 0:
            self.velocity[0] = max(self.velocity[0] - 0.1, 0) # right falling to left
        else:
            self.velocity[0] = min(self.velocity[0] + 0.1, 0) # left falling to to right


    def render(self, surf, offset={0,0}):
        '''
        partly overriding rendering for dashing
        '''
        super().render(surf, offset=offset) # show player
