import numpy as np
from PIL import Image

# Generates a signed distance field image and writes it out as a PNG.
#
# Usage:
# python sdf_norm_image_gen.py

WIDTH      = 256
HEIGHT     = 256
HALF_SIZE  = 0.6
OUT_FILE   = "sdf.png"


def main():
    y, x = np.indices( ( HEIGHT, WIDTH ), dtype=np.float32 )

    # Map to [-1, 1]
    u = ( x / WIDTH ) * 2.0 - 1.0
    v = ( y / HEIGHT ) * 2.0 - 1.0

    dx = np.abs( u ) - HALF_SIZE
    dy = np.abs( v ) - HALF_SIZE
    outside = np.sqrt( np.clip( dx, 0, None ) ** 2 + np.clip( dy, 0, None ) ** 2 )
    inside = np.minimum( np.maximum( dx, dy ), 0.0 )
    sdf = outside + inside

    normalized = ( sdf - sdf.min() ) / ( sdf.max() - sdf.min() )
    image = Image.fromarray( ( normalized * 255.0 ).astype( np.uint8 ), mode="L" )
    image.save( OUT_FILE )


if __name__ == "__main__":
    main()
