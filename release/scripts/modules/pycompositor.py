import numpy as np
from PIL import Image, ImageOps


def mix_arrays(a, b, factor):
    """Basic linear interpolation, factor from 0.0 to 1.0"""
    return a + max(min(factor, 1.0), 0.0) * (b - a)


def get_array_rgb(rgba):
    """Return RGB values of an RGBA 3D-array."""
    return rgba[...,0:3]


def get_array_alpha(rgba):
    """Return alpha values of an RGBA 3D-array."""
    return rgba[...,3]


def array_to_pil(rgba):
    """Convert a 3D floating point numpy array into a PIL image. Returns the image and
        a meta object that should be passed to pil_to_array() once the image is
        converted back into an array."""
    maxRgb = max(get_array_rgb(rgba).max(), 1.0)
    maxAlpha = max(get_array_alpha(rgba).max(), 1.0)

    #Normalize values to 8-bit range (0-255) that Pillow can handle
    norm = rgba / maxRgb * 255.0 #RGB values
    norm[...,3] = rgba[...,3] / maxAlpha * 255.0 #Alpha value
    im = ImageOps.flip(Image.fromarray(np.uint8(norm), "RGBA"))
    return im, (maxRgb, maxAlpha)


def pil_to_array(im, meta):
    """Convert a PIL image into a 3D floating point numpy array."""
    #Restore values from 0-255 range to original floating point values
    rgba = np.array(ImageOps.flip(im)) / 255.0
    rgba[...,0:3] *= meta[0]
    rgba[...,3] *= meta[1]
    return rgba
