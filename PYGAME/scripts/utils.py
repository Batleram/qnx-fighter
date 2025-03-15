import os

import pygame

BASE_IMG_PATH = 'data/images/'

def load_image(path):
    '''
    short cut to load a single image, removes the background and increasing preformance
    (file path) -> (img)
    '''
    img = pygame.image.load(BASE_IMG_PATH + path).convert() # helps preformance when rendering
    img.set_colorkey((0, 0, 0)) # removes the background in png images
    return img

def load_images(path):
    '''
    loads all images within a collection of images
    (file path) -> (List of images within file)
    '''
    images = []
    for img_name in sorted(os.listdir(BASE_IMG_PATH + path)):  # gives all files in the path, sorted makes it consistent with other operating systems (starting at 0->8)
        images.append(load_image(path + '/' + img_name))
    return images


class Animation:
    def __init__(self, images, img_dur=5, loop=True):
        '''
        initializing animation
        '''
        self.images = images
        self.loop = loop
        self.img_duration = img_dur
        self.done = False
        self.frame = 0

    def copy(self):
        '''
        enables object to make a copy of it's animation
        it's reference points to the same set of images
        changing one image will change all copy's animation

        () -> (Animation)
        '''
        return Animation(self.images, self.img_duration, self.loop)
    
    def update(self):
        '''
        increments the frame in the animation loop
        '''
        if self.loop:
            self.frame = (self.frame + 1) % (self.img_duration * len(self.images)) # forces frame to loop around when it reaches the end
        else:
            self.frame = min(self.frame + 1, self.img_duration * len(self.images) - 1) # dont want it to go past the end of the animation
            if self.frame >= self.img_duration * len(self.images) - 1:
                self.done = True # when animation is done, just in case


    def img(self):
        '''
        returns the current image of the animation
        '''
        return self.images[int(self.frame / self.img_duration)] # divides frame by how long each image shows for