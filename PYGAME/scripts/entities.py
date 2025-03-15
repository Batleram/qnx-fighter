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
        self.speed = 5
        self.velocity = [0,0,0, 0]
        self.collisions = {'up': False, 'down': False, 'left': False, 'right': False}

        self.action = ''
        self.anim_offset = (0, 0) #renders with an offset to pad the animation against the hitbox
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


        self.last_movement = movement # keeps track of movement


        if self.collisions['down'] or self.collisions['up']: # if object hit, stop velocity
            self.velocity[1] = 0

        self.animation.update() # update animation


    def render(self, surf, offset={0,0}):
        '''
        renders entitiy asset
        '''
        surf.blit(pygame.transform.flip(self.animation.img(), False, False), (self.pos[0] - offset[0] + self.anim_offset[0], self.pos[1] - offset[1] + self.anim_offset[1])) # fliping agasint horizontal axis

class Player(PhysicsEntity):
    def __init__(self, game, pos, size):
        '''
        instantiates player entity
        (game, position, size)
        '''
        super().__init__(game, 'player', pos, size)
        self.air_time = 0
        self.jumps = 1
        self.wall_slide = False
        self.dashing = 0

    def update(self, tilemap, movement=(0,0)):
        '''
        updates players animations depending on movement
        '''
        player_movement = movement
        movement_magnitude = math.sqrt((movement[0] * movement[0] + movement[1] * movement[1]))
        if movement_magnitude > 0:
            player_movement = (movement[0] / movement_magnitude, movement[1] / movement_magnitude)
        
        super().update(tilemap, movement=player_movement)


    def render(self, surf, offset={0,0}):
        '''
        partly overriding rendering for dashing
        '''
        super().render(surf, offset=offset) # show player

class Enemy(PhysicsEntity):
    def __init__(self, game, pos, size):
        '''
        instantiates the enemies
        (game, position: tuple, size)
        '''
        super().__init__(game, 'enemy', pos, size)
        self.set_action('idle')
        self.shoot_speed = 700
        self.shoot_wait = self.shoot_speed
    
    def update(self, tilemap, movement=(0,0)):
        self.shoot_wait -= self.game.deltatime * (1 - (self.game.slowdown * (self.game.slowdown_timer_change-1)/self.game.slowdown_timer_change))
        if self.shoot_wait < 0:
            dx = self.game.player.rect().centerx - self.rect().centerx
            dy = self.game.player.rect().centery - self.rect().centery
            bullet_angle = math.atan2(dx, -dy) - (math.pi/2)
            new_bullet = Bullet(self.game, self.rect().center, 7, bullet_angle, size=(18, 18), type='enemy')
            self.game.bullets.append(new_bullet)
            self.shoot_wait = self.shoot_speed
        enemy_movement = movement
        movement_magnitude = math.sqrt((movement[0] * movement[0] + movement[1] * movement[1]))
        if movement_magnitude > 0:
            enemy_movement = (movement[0] / movement_magnitude, movement[1] / movement_magnitude)
        enemy_movement = [enemy_movement[0] * 0.75, enemy_movement[1] * 0.75]

        super().update(tilemap, movement=enemy_movement)
        if self.rect().colliderect(self.game.player.rect()):
            if not self.game.dead:
                self.game.sfx['player_death'].play(0)
            self.game.dead += 1
            

    def render(self, surf, offset=(0, 0)):
        super().render(surf, offset=offset)