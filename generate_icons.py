import os
from PIL import Image, ImageDraw

def create_matrix_icon(size, path):
    # Create a new image with transparent background
    img = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)

    # Matrix Green color
    color = (0, 0, 0) # Black is standard for protocol icons in the status area usually, but let's do the brand color?
    # Actually, Pidgin protocol icons are often colored. Matrix is Black or dark grey with White 'M' or the other way around.
    # The official logo is black 'M' (or shapes) often, or white on black.
    # Let's make a simple "M" shape or just a filled square with an M.
    
    # Official brand color is #000000 (Black) or White on Black.
    # Let's do a black square with a white 'm' for visibility, or just the 'm' shape in black.
    # Let's try to draw the 'M' shape which is [ \ / ]
    
    # Let's keep it simple: A black circle/square with a white 'm' text or shape.
    
    # Draw a circle background (optional, but good for icons)
    # draw.ellipse((0, 0, size-1, size-1), fill=(0, 0, 0, 255))
    
    # Let's just draw a stylized 'm' in black.
    # [ | \ / | ]
    
    padding = size // 8
    width = size - 2 * padding
    height = size - 2 * padding
    
    x_start = padding
    y_start = padding
    x_end = size - padding
    y_end = size - padding
    
    # Points for M
    points = [
        (x_start, y_end),          # Bottom Left
        (x_start, y_start),        # Top Left
        (x_start + width/2, y_start + height/2), # Middle Bottom
        (x_end, y_start),          # Top Right
        (x_end, y_end)             # Bottom Right
    ]
    
    # Draw lines
    draw.line(points, fill=(0, 0, 0, 255), width=max(1, size // 10))
    
    # Save
    img.save(path)
    print(f"Generated {path}")

def main():
    sizes = [16, 22, 48]
    base_dir = "icons"
    
    for size in sizes:
        path = os.path.join(base_dir, str(size), "matrix.png")
        create_matrix_icon(size, path)

if __name__ == "__main__":
    main()
