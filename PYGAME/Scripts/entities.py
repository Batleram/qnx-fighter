import pygame
import math
import random

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
        self.velocity = [0,0]
        self.collisions = {'up': False, 'down': False, 'left': False, 'right': False}

        self.action = ''
        self.anim_offset = (-3, -3) #renders with an offset to pad the animation against the hitbox
        self.flip = False
        self.set_action('idle')

        self.last_movement = [0, 0]

    def rect(self):
        '''
        creates a rectangle at the entitiies current postion
        '''
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
        self.collisions = {'up': False, 'down': False, 'left': False, 'right': False} # this value will be reset every frame, used to stop constant increase of velocity

        frame_movement = (movement[0] + self.velocity[0], movement[1] + self.velocity[1])

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
        entity_rect = self.rect()  # Update entity rectangle for y-axis handling
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
            self.flip = False
        if movement[0] < 0:
            self.flip = True

        self.last_movement = movement # keeps track of movement

        # gravity aka terminal falling velocity "VERTICLE"
        self.velocity[1] = min(5, self.velocity[1] + 0.1) # (max velocity downwards, ) pos+ is downwards in pygame, from 5 to 0

        if self.collisions['down'] or self.collisions['up']: # if object hit, stop velocity
            self.velocity[1] = 0

        self.animation.update() # update animation


    def render(self, surf, offset={0,0}):
        '''
        renders entitiy asset
        '''
        surf.blit(pygame.transform.flip(self.animation.img(), self.flip, False), (self.pos[0] - offset[0] + self.anim_offset[0], self.pos[1] - offset[1] + self.anim_offset[1])) # fliping agasint horizontal axis



class Player(PhysicsEntity):
    def __init__(self, game, pos, size):
        '''
        instantiates plauer entity
        (game, position, size)
        '''
        super().__init__(game, 'player', pos, size)
        self.air_time = 0
        self.jumps = 1
        self.dead = 1

    def update(self, movement=(0,0)):
        '''
        updates players animations depending on movement
        '''
        super().update(movement=movement)

        self.air_time += 1

        if self.collisions['down']: # collide with the floor
            self.air_time = 0
            # restore the jumps
            self.jumps = 1

        self.wall_slide = False # reset every frame
        if (self.collisions['right'] or self.collisions['left']) and self.air_time > 4: # if player connects with a wall
            self.wall_slide = True
            self.velocity[1] = min(self.velocity[1], 0.5) # slow down falling
            if self.collisions['right']: # determine which animation to show
                self.flip = False
                self.set_action('wall_slide')
            else:
                self.flip = True
                self.set_action('wall_slide')

        if not self.wall_slide: 
            if self.air_time > 4: # in air for some time (highest priority)
                self.set_action('jump')
            elif movement[0] != 0: # if moving horizontally
                self.set_action('run')
            else:
                self.set_action('idle')
        
        
        # normalize horizontal vel "HORIZONTAL"
        if self.velocity[0] > 0:
            self.velocity[0] = max(self.velocity[0] - 0.1, 0) # right falling to left
        else:
            self.velocity[0] = min(self.velocity[0] + 0.1, 0) # left falling to to right

    def render(self, surf, offset={0,0}):
        '''
        partly overriding rendering for dashing
        '''
        if abs(self.dashing) <= 50: # not in first 10 frames of dash
            super().render(surf, offset=offset) # show player

    def jump(self):
        '''
        makes player jump
        -> bool if jump occurs
        '''
        if self.jumps: # if jump = 0, returns false
            self.velocity[1] = -3
            self.jumps -= 1 # count down jumps
            self.air_time = 5 # allows jump animation to start
            return True
        return False
    