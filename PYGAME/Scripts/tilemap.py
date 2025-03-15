import json
import pygame

# depends on order location that we are rendering the tiles, tuple(sorted() solves this, + we can't use list as a key therefore tuple
AUTOTILE_MAP = {
    tuple(sorted([(1, 0), (0, 1)])): 0,
    tuple(sorted([(1, 0), (0, 1), (-1, 0)])): 1,
    tuple(sorted([(-1, 0), (0, 1)])): 2, 
    tuple(sorted([(-1, 0), (0, -1), (0, 1)])): 3,
    tuple(sorted([(-1, 0), (0, -1)])): 4,
    tuple(sorted([(-1, 0), (0, -1), (1, 0)])): 5,
    tuple(sorted([(1, 0), (0, -1)])): 6,
    tuple(sorted([(1, 0), (0, -1), (0, 1)])): 7,
    tuple(sorted([(1, 0), (-1, 0), (0, 1), (0, -1)])): 8,
}
NEIGHBOR_OFFSET = [(-1, 0), (-1, -1), (0, -1), (1, -1), (1, 0), (0, 0), (-1, 1), (0, 1), (1, 1)]
PHYSICS_TILES = {'obstacles'}
AUTOTILE_TYPES = {}

class Tilemap:
    def __init__(self, game, tile_size=64):
        '''
        initializing tilemap
        (game, tile size 64x64 px)
        '''
        self.game = game
        self.tile_size = tile_size
        self.tilemap = {} # map tile based on location, using a dictionary for conveince (dont have to fill in all the space like lists) 
        self.offgrid_tiles = []

    def extract(self, id_pairs, keep=False):
        '''
        takes the ids of a tile list, and checks where the tile is in the list
        (List of tile ids: List, want to keep tile: bool) -> (list of matches)
        '''
        matches = []
        # offgrid
        for tile in self.offgrid_tiles.copy():
            if (tile['type'], tile['variant']) in id_pairs: # look for match
                matches.append(tile.copy())
                if not keep:
                    self.offgrid_tiles.remove(tile)
        
        for loc in self.tilemap.copy():
            tile = self.tilemap[loc]
            if (tile['type'], tile['variant']) in id_pairs:
                matches.append(tile.copy())
                # change position for the tile we are referncing bc we want it in pixels, 
                # copy so we dont modify the actual tile in the tilemap
                matches[-1]['pos'] = matches[-1]['pos'].copy() 
                matches[-1]['pos'][0] *= self.tile_size # x axis
                matches[-1]['pos'][1] *= self.tile_size # y axis
                if not keep:
                    del self.tilemap[loc]
        return matches

    def tiles_around(self, pos):
        '''
        returns all the tiles around a chosen position
        (position) --> (list of tiles)
        '''
        tiles = []
        # convert pixel position into grid position
        tile_loc = (int(pos[0] // self.tile_size), int(pos[1] // self.tile_size)) # remove the .0 w int()
        for offset in NEIGHBOR_OFFSET:
            check_loc = str(tile_loc[0] + offset[0]) + ';' + str(tile_loc[1] + offset[1])
            if check_loc in self.tilemap: # checks if tile is there and not just empty space
                tiles.append(self.tilemap[check_loc])
        
        return tiles
    
    def save(self, path):
        '''
        saves the tile map
        (file path to save to)
        '''
        f = open(path, 'w') # open file
        json.dump({'tilemap': self.tilemap, 'tile_size': self.tile_size, 'offgrid': self.offgrid_tiles}, f) # dump object into file
        f.close()
    
    def load(self, path):
        '''
        load the tilemap using the path of the json file
        (file path to access tilemap from)
        '''
        f = open(path, 'r')
        map_data = json.load(f)
        f.close()

        self.tilemap = map_data['tilemap']
        self.tile_size = map_data['tile_size']
        self.offgrid_tiles = map_data['offgrid']

    def solid_check(self, pos):
        '''
        checks the position and returns the location of any solide tiles next to it
        (pos: tuple) -> (str)
        '''
        tile_loc = str(int(pos[0] // self.tile_size)) + ';' + str(int(pos[1] // self.tile_size)) # gives tile location
        if tile_loc in self.tilemap:
            if self.tilemap[tile_loc]['type'] in PHYSICS_TILES:
                return self.tilemap[tile_loc]
    
    def autotile(self):
        '''
        auto tiles depending on it's neightbors
        '''
        for loc in self.tilemap:
            tile = self.tilemap[loc]
            neighbors = set()
            for shift in [(1, 0), (-1, 0), (0, -1), (0, 1)]:
                check_loc = str(tile['pos'][0] + shift[0]) + ';' + str(tile['pos'][1] + shift[1])
                if check_loc in self.tilemap:
                    if self.tilemap[check_loc]['type'] == tile['type']: # check if neighbors are same type/group
                        neighbors.add(shift)
            neighbors = tuple(sorted(neighbors)) #tuple(sorted() solves this, + we can't use list as a key therefore tuple
            if (tile['type'] in AUTOTILE_TYPES) and (neighbors in AUTOTILE_MAP):
                tile['variant'] = AUTOTILE_MAP[neighbors]

    
    def physics_rects_around(self, pos):
        '''
        filters nearby tiles to check if they have physics
        (position) -> (list of rectangles) 
        '''
        rects = []
        for tile in self.tiles_around(pos):
            if tile['type'] in PHYSICS_TILES:
                rects.append(pygame.Rect(tile['pos'][0] * self.tile_size, tile['pos'][1] * self.tile_size, self.tile_size, self.tile_size))
        return rects

    def render(self, surf, offset=(0, 0)):
        '''
        renders tilemap on surface
        (screen surface)
        '''
        # rendering offgrid tiles, decor gets rendered first (behind the actual tiles)
        for tile in self.offgrid_tiles:
            surf.blit(self.game.assets[tile['type']][tile['variant']], (tile['pos'][0] - offset[0], tile['pos'][1] - offset[1]))

        # for x in range(top left tile x position [tile coord], to top  right edge of screen [tile coord])
        for x in range(offset[0] // self.tile_size, (offset[0] + surf.get_width()) // self.tile_size + 1):
            for y in range(offset[1] // self.tile_size, (offset[1] + surf.get_height()) // self.tile_size + 1):
                loc = str(x) + ';' + str(y)
                if loc in self.tilemap:
                    tile = self.tilemap[loc]
                    # pos * tile size bc it's in terms of grid within tilemap currently, we want position in terms of pixels
                    # (tile in assets, rendering pos)
                    surf.blit(self.game.assets[tile['type']][tile['variant']], (tile['pos'][0] * self.tile_size - offset[0], tile['pos'][1] * self.tile_size - offset[1]))