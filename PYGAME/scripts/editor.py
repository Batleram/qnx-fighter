import sys
import pygame

from utils import load_images
from tilemap import Tilemap

RENDER_SCALE = 1.0

class Editor:
    def __init__(self):
        '''
        initializes Editor
        '''
        pygame.init()

        # change the window caption
        pygame.display.set_caption("editor")

        # create window
        self.screen = pygame.display.set_mode((0,0), pygame.FULLSCREEN)
        self.screen_size = pygame.display.get_surface().get_size()

        self.display = pygame.Surface((1920, 1080)) # render on different resolution then scale it up to screen if needed

        self.clock = pygame.time.Clock()
        
        self.assets = {
            'ground': load_images('tiles/ground'),
            'obstacles': load_images('tiles/obstacles'),
        }
        
        self.movement = [False, False, False, False] # camera movement in all 4 directions

        #initalizing tilemap
        self.tilemap = Tilemap(self, tile_size=64)

        self.curr_level = 'data/maps/main_menu'

        try: # only load the map if it exists
            self.tilemap.load(self.curr_level + '.json')
        except FileNotFoundError:
            pass

        # creating 'camera'  scroll for it's movement
        self.scroll = [0, 0]


        self.tile_list = list(self.assets) # list of dictionary keys
        self.tile_group = 0 # which group
        self.tile_variant = 0  # which tile within group

        self.clicking = False # for clicking mechanic
        self.right_clicking = False
        self.shift = False
        self.ongrid = True

    def run(self):
        '''
        runs the Editor
        '''
        # creating an infinite game loop
        while True:
            # clear the screen for new image generation in loop
            self.display.fill((0,0,0))

            self.scroll[0] += (self.movement[1] - self.movement[0]) * 2 # camera x axis
            self.scroll[1] += (self.movement[3] - self.movement[2]) * 2 # camera y axis
            render_scroll = (int(self.scroll[0]), int(self.scroll[1]))

            # render the tile map
            self.tilemap.render(self.display, offset=render_scroll)

            current_tile_img = self.assets[self.tile_list[self.tile_group]][self.tile_variant] # select the tile
            #current_tile_img.set_alpha(150) # partially transparent, 0 -> full, 255 -> none

            mpos = pygame.mouse.get_pos() # gets mouse positon
            mpos = (mpos[0] / (self.screen_size[0]/self.display.get_width()), mpos[1] / (self.screen_size[1]/self.display.get_height())) # since screen sometimes scales
            tile_pos = (int((mpos[0] + self.scroll[0]) // self.tilemap.tile_size), int((mpos[1] + self.scroll[1]) // self.tilemap.tile_size)) #coord of mouse in refernce to tile map, snaps img to grid

            # indicate where tile will be placed
            if self.ongrid:
                self.display.blit(current_tile_img, (tile_pos[0] * self.tilemap.tile_size - self.scroll[0], tile_pos[1] * self.tilemap.tile_size - self.scroll[1]))
            else: 
                self.display.blit(current_tile_img, mpos)

            if self.clicking and self.ongrid: # assing positon on tile map to that asset
                self.tilemap.tilemap[str(tile_pos[0]) + ';' + str(tile_pos[1])] = {'type': self.tile_list[self.tile_group], 'variant': self.tile_variant, 'pos': tile_pos}
            if self.right_clicking:
                tile_loc = str(tile_pos[0]) + ';' + str(tile_pos[1])
                if tile_loc in self.tilemap.tilemap:
                    # if location exists
                    del self.tilemap.tilemap[tile_loc]
                for tile in self.tilemap.offgrid_tiles.copy(): # take a copy of refernce so we dont mess up the actual iteration
                    tile_img = self.assets[tile['type']][tile['variant']]
                    tile_r = pygame.Rect(tile['pos'][0] - self.scroll[0], tile['pos'][1] - self.scroll[1], tile_img.get_width(), tile_img.get_height())
                    if tile_r.collidepoint(mpos): # if this tile is colliding with mouse
                        self.tilemap.offgrid_tiles.remove(tile)

            self.display.blit(current_tile_img, (32, 32))

            for event in pygame.event.get():
                if event.type == pygame.QUIT: # have to code the window closing
                    pygame.quit()
                    sys.exit()
                if event.type == pygame.MOUSEBUTTONDOWN:
                    if event.button == 1: # left click, places
                        self.clicking = True
                        if not self.ongrid:
                            self.tilemap.offgrid_tiles.append({'type': self.tile_list[self.tile_group], 'variant': self.tile_variant, 'pos': (mpos[0] + self.scroll[0], mpos[1] + self.scroll[1])})
                    if event.button == 3: # right click
                        self.right_clicking = True
                if event.type == pygame.MOUSEBUTTONUP:
                    if event.button == 1: # left click
                        self.clicking = False
                    if event.button == 3: # right click
                        self.right_clicking = False
                    
                    if self.shift:
                        # scroll between variants
                        if event.button == 4: # scroll wheel up, % so no null pointer
                            self.tile_variant = (self.tile_variant - 1) % len(self.assets[self.tile_list[self.tile_group]])
                        if event.button == 5: # scroll wheel down, % trick so no null pointer
                            self.tile_variant = (self.tile_variant + 1) % len(self.assets[self.tile_list[self.tile_group]])
                    else:
                        # scroll between groups
                        if event.button == 4: # scroll wheel up, % so no null pointer, len(title_list) = how many groups
                            self.tile_group = (self.tile_group - 1) % len(self.tile_list)
                            self.tile_variant = 0 # no index errors
                        if event.button == 5: # scroll wheel down, % trick so no null pointer
                            self.tile_group = (self.tile_group + 1) % len(self.tile_list)
                            self.tile_variant = 0 
                  

                if event.type == pygame.KEYDOWN:
                    if event.key == pygame.K_a: # referencing WASD
                        self.movement[0] = True
                    if event.key == pygame.K_d:
                        self.movement[1] = True
                    if event.key == pygame.K_w:
                        self.movement[2] = True
                    if event.key == pygame.K_s:
                        self.movement[3] = True
                    if event.key == pygame.K_LSHIFT:
                        self.shift = True
                    if event.key == pygame.K_g: # switch drawing on/offgrid 
                        self.ongrid = not self.ongrid
                    if event.key == pygame.K_o: # same tilemap
                        self.tilemap.save(self.curr_level + '.json') # path we are saving it to
                    if event.key == pygame.K_t:
                        self.tilemap.autotile()
                if event.type == pygame.KEYUP: # when key is released
                    if event.key == pygame.K_a: 
                        self.movement[0] = False
                    if event.key == pygame.K_d: 
                        self.movement[1] = False
                    if event.key == pygame.K_w:
                        self.movement[2] = False
                    if event.key == pygame.K_s:
                        self.movement[3] = False
                    if event.key == pygame.K_LSHIFT:
                        self.shift = False
            
            self.screen.blit(pygame.transform.scale(self.display, self.screen.get_size()), (0,0)) # render (now scaled) display image on big screen
            pygame.display.update()
            self.clock.tick(60) # run at 60 fps, like a sleep

# returns the editor then runs it
Editor().run()