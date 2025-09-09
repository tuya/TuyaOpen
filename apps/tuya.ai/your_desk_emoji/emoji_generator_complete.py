#!/usr/bin/env python3
"""
Complete Emoji Generator for Desk-Emoji Robot
Generates all emoji animations (basic + fun expressions) for 160x80 display
"""

from PIL import Image, ImageDraw
import os
import math

# Screen dimensions (matching your project)
SCREEN_WIDTH = 160
SCREEN_HEIGHT = 80

# Eye parameters (keep original size, just crop the canvas)
REF_EYE_HEIGHT = 60
REF_EYE_WIDTH = 60
REF_SPACE_BETWEEN_EYE = 15
REF_CORNER_RADIUS = 15

class CompleteEmojiGenerator:
    def __init__(self, width=SCREEN_WIDTH, height=SCREEN_HEIGHT):
        self.width = width
        self.height = height
        self.center_x = width // 2
        self.center_y = height // 2
        
        # Eye positions (centered)
        self.left_eye_x = self.center_x - REF_EYE_WIDTH // 2 - REF_SPACE_BETWEEN_EYE // 2
        self.left_eye_y = self.center_y
        self.right_eye_x = self.center_x + REF_EYE_WIDTH // 2 + REF_SPACE_BETWEEN_EYE // 2
        self.right_eye_y = self.center_y
        
    def draw_eyes(self, left_width, left_height, right_width, right_height, 
                  left_x=None, left_y=None, right_x=None, right_y=None,
                  corner_radius=REF_CORNER_RADIUS, color='white'):
        """Draw two eyes with specified dimensions and positions"""
        if left_x is None:
            left_x = self.left_eye_x
        if left_y is None:
            left_y = self.left_eye_y
        if right_x is None:
            right_x = self.right_eye_x
        if right_y is None:
            right_y = self.right_eye_y
            
        # Create image
        img = Image.new('RGB', (self.width, self.height), 'black')
        draw = ImageDraw.Draw(img)
        
        # Draw left eye
        left_rect = [
            left_x - left_width // 2,
            left_y - left_height // 2,
            left_x + left_width // 2,
            left_y + left_height // 2
        ]
        draw.rounded_rectangle(left_rect, radius=corner_radius, fill=color)
        
        # Draw right eye
        right_rect = [
            right_x - right_width // 2,
            right_y - right_height // 2,
            right_x + right_width // 2,
            right_y + right_height // 2
        ]
        draw.rounded_rectangle(right_rect, radius=corner_radius, fill=color)
        
        return img, draw
    
    # ==================== BASIC EMOTIONS ====================
    
    def generate_blink_animation(self, frames=12):
        """Generate fast blinking animation"""
        images = []
        
        # Normal state (shorter)
        for _ in range(2):
            img, draw = self.draw_eyes(REF_EYE_WIDTH, REF_EYE_HEIGHT, REF_EYE_WIDTH, REF_EYE_HEIGHT)
            images.append(img)
        
        # Closing eyes (faster)
        for i in range(frames // 2):
            height = REF_EYE_HEIGHT - (REF_EYE_HEIGHT * i / (frames // 2))
            img, draw = self.draw_eyes(REF_EYE_WIDTH, max(height, 2), REF_EYE_WIDTH, max(height, 2))
            images.append(img)
        
        # Opening eyes (faster)
        for i in range(frames // 2):
            height = (REF_EYE_HEIGHT * i / (frames // 2))
            img, draw = self.draw_eyes(REF_EYE_WIDTH, height, REF_EYE_WIDTH, height)
            images.append(img)
        
        # Normal state (shorter)
        for _ in range(2):
            img, draw = self.draw_eyes(REF_EYE_WIDTH, REF_EYE_HEIGHT, REF_EYE_WIDTH, REF_EYE_HEIGHT)
            images.append(img)
        
        return images
    
    def generate_happy_animation(self, frames=20):
        """Generate happy expression with squinting eyes and bouncing effect"""
        images = []
        
        # Start with normal eyes
        img, draw = self.draw_eyes(REF_EYE_WIDTH, REF_EYE_HEIGHT, REF_EYE_WIDTH, REF_EYE_HEIGHT)
        images.append(img)
        
        # Happy squinting animation with bounce
        for i in range(frames):
            # Calculate squint intensity (0.0 to 1.0)
            squint_progress = abs(math.sin(i * math.pi / 4))  # Oscillating squint
            
            # Calculate bounce offset
            bounce_offset = 3 * math.sin(i * math.pi / 3)  # Gentle bounce
            
            # Squint the eyes (reduce height)
            squint_height = REF_EYE_HEIGHT - (REF_EYE_HEIGHT * 0.4 * squint_progress)
            
            # Apply bounce to Y position
            left_y = self.left_eye_y + bounce_offset
            right_y = self.right_eye_y + bounce_offset
            
            img, draw = self.draw_eyes(REF_EYE_WIDTH, squint_height, REF_EYE_WIDTH, squint_height,
                                     left_y=left_y, right_y=right_y)
            
            # Add sparkle effect (small white dots)
            if i % 3 == 0:  # Every 3rd frame
                draw = ImageDraw.Draw(img)
                # Left sparkle
                sparkle_x = self.left_eye_x + 25
                sparkle_y = self.left_eye_y - 20
                draw.ellipse([sparkle_x-2, sparkle_y-2, sparkle_x+2, sparkle_y+2], fill='white')
                
                # Right sparkle
                sparkle_x = self.right_eye_x - 25
                sparkle_y = self.right_eye_y - 20
                draw.ellipse([sparkle_x-2, sparkle_y-2, sparkle_x+2, sparkle_y+2], fill='white')
            
            images.append(img)
        
        return images
    
    def generate_sad_animation(self, frames=15):
        """Generate sad expression with upward triangle overlay"""
        images = []
        
        # Start with normal eyes
        img, draw = self.draw_eyes(REF_EYE_WIDTH, REF_EYE_HEIGHT, REF_EYE_WIDTH, REF_EYE_HEIGHT)
        images.append(img)
        
        # Add upward triangle overlay animation
        for i in range(frames):
            img, draw = self.draw_eyes(REF_EYE_WIDTH, REF_EYE_HEIGHT, REF_EYE_WIDTH, REF_EYE_HEIGHT)
            
            # Draw upward triangles (black overlay to create sad effect)
            offset = REF_EYE_HEIGHT // 2 - (i * 2)
            triangle_size = 10 + i
            
            # Left eye triangle
            left_triangle = [
                self.left_eye_x - REF_EYE_WIDTH // 2 - 5,
                self.left_eye_y - offset + 5,
                self.left_eye_x + REF_EYE_WIDTH // 2 + 5,
                self.left_eye_y - 5 - offset,
                self.left_eye_x - REF_EYE_WIDTH // 2 - 5,
                self.left_eye_y - REF_EYE_HEIGHT - offset
            ]
            draw.polygon(left_triangle, fill='black')
            
            # Right eye triangle
            right_triangle = [
                self.right_eye_x + REF_EYE_WIDTH // 2 + 5,
                self.right_eye_y - offset + 5,
                self.right_eye_x - REF_EYE_WIDTH // 2 - 5,
                self.right_eye_y - 5 - offset,
                self.right_eye_x + REF_EYE_WIDTH // 2 + 5,
                self.right_eye_y - REF_EYE_HEIGHT - offset
            ]
            draw.polygon(right_triangle, fill='black')
            
            images.append(img)
        
        return images
    
    def generate_anger_animation(self, frames=15):
        """Generate anger expression with diagonal triangle overlay"""
        images = []
        
        # Start with normal eyes
        img, draw = self.draw_eyes(REF_EYE_WIDTH, REF_EYE_HEIGHT, REF_EYE_WIDTH, REF_EYE_HEIGHT)
        images.append(img)
        
        # Add diagonal triangle overlay animation
        for i in range(frames):
            img, draw = self.draw_eyes(REF_EYE_WIDTH, REF_EYE_HEIGHT, REF_EYE_WIDTH, REF_EYE_HEIGHT)
            
            # Draw diagonal triangles (black overlay to create anger effect)
            offset = REF_EYE_HEIGHT // 2 - (i * 2)
            triangle_size = 10 + i
            
            # Left eye triangle (slanted right)
            left_triangle = [
                self.left_eye_x + REF_EYE_WIDTH // 2 + 5,
                self.left_eye_y - offset + 5,
                self.left_eye_x - REF_EYE_WIDTH // 2 - 5,
                self.left_eye_y - 5 - offset,
                self.left_eye_x + REF_EYE_WIDTH // 2 + 5,
                self.left_eye_y - REF_EYE_HEIGHT - offset
            ]
            draw.polygon(left_triangle, fill='black')
            
            # Right eye triangle (slanted left)
            right_triangle = [
                self.right_eye_x - REF_EYE_WIDTH // 2 - 5,
                self.right_eye_y - offset + 5,
                self.right_eye_x + REF_EYE_WIDTH // 2 + 5,
                self.right_eye_y - 5 - offset,
                self.right_eye_x - REF_EYE_WIDTH // 2 - 5,
                self.right_eye_y - REF_EYE_HEIGHT - offset
            ]
            draw.polygon(right_triangle, fill='black')
            
            images.append(img)
        
        return images
    
    def generate_surprise_animation(self, frames=15):
        """Generate surprise expression with shrinking animation"""
        images = []
        
        # Start with normal eyes
        img, draw = self.draw_eyes(REF_EYE_WIDTH, REF_EYE_HEIGHT, REF_EYE_WIDTH, REF_EYE_HEIGHT)
        images.append(img)
        
        # Shrinking animation
        initial_size = REF_EYE_WIDTH
        min_size = 10
        
        for i in range(frames):
            size = initial_size - (initial_size - min_size) * i / frames
            corner_radius = max(REF_CORNER_RADIUS - i, 1)
            
            img, draw = self.draw_eyes(size, size, size, size, corner_radius=corner_radius)
            images.append(img)
        
        return images
    
    def generate_sleep_animation(self, frames=15):
        """Generate sleep expression with Z's floating above"""
        images = []
        
        # Start with normal eyes
        img, draw = self.draw_eyes(REF_EYE_WIDTH, REF_EYE_HEIGHT, REF_EYE_WIDTH, REF_EYE_HEIGHT)
        images.append(img)
        
        # Gradually close eyes and add floating Z's
        for i in range(frames):
            # Calculate eye closure (from normal to thin line)
            if i < frames // 2:
                eye_height = REF_EYE_HEIGHT - (REF_EYE_HEIGHT * i / (frames // 2))
            else:
                eye_height = 2  # Keep closed
            
            img, draw = self.draw_eyes(REF_EYE_WIDTH, max(eye_height, 2), REF_EYE_WIDTH, max(eye_height, 2))
            
            # Add floating Z's when eyes are closed
            if eye_height <= 2:
                draw = ImageDraw.Draw(img)
                # Calculate Z positions (floating upward)
                z_offset = (i - frames // 2) * 8
                
                # Draw multiple Z's
                for z_num in range(3):
                    z_x = self.center_x - 20 + z_num * 20
                    z_y = self.center_y - 40 - z_offset + z_num * 15
                    
                    # Draw Z shape (simplified)
                    if z_y > 20:  # Only draw if Z is still on screen
                        # Top horizontal line
                        draw.line([z_x-8, z_y-5, z_x+8, z_y-5], fill='white', width=2)
                        # Diagonal line
                        draw.line([z_x+8, z_y-5, z_x-8, z_y+5], fill='white', width=2)
                        # Bottom horizontal line
                        draw.line([z_x-8, z_y+5, z_x+8, z_y+5], fill='white', width=2)
            
            images.append(img)
        
        return images
    
    def generate_wakeup_animation(self, frames=12):
        """Generate wakeup animation (from thin lines to normal)"""
        images = []
        
        # Start with sleep state
        img, draw = self.draw_eyes(REF_EYE_WIDTH, 2, REF_EYE_WIDTH, 2)
        images.append(img)
        
        # Gradually increase height
        for i in range(frames):
            height = 2 + (REF_EYE_HEIGHT - 2) * i / frames
            img, draw = self.draw_eyes(REF_EYE_WIDTH, height, REF_EYE_WIDTH, height)
            images.append(img)
        
        return images
    
    def generate_left_animation(self, frames=12):
        """Generate left movement animation"""
        images = []
        
        # Start centered
        img, draw = self.draw_eyes(REF_EYE_WIDTH, REF_EYE_HEIGHT, REF_EYE_WIDTH, REF_EYE_HEIGHT)
        images.append(img)
        
        # Move left
        for i in range(frames // 2):
            offset = i * 2
            left_x = self.left_eye_x - offset
            right_x = self.right_eye_x - offset
            img, draw = self.draw_eyes(REF_EYE_WIDTH, REF_EYE_HEIGHT, REF_EYE_WIDTH, REF_EYE_HEIGHT,
                                     left_x=left_x, right_x=right_x)
            images.append(img)
        
        # Move back to center
        for i in range(frames // 2):
            offset = (frames // 2 - i) * 2
            left_x = self.left_eye_x - offset
            right_x = self.right_eye_x - offset
            img, draw = self.draw_eyes(REF_EYE_WIDTH, REF_EYE_HEIGHT, REF_EYE_WIDTH, REF_EYE_HEIGHT,
                                     left_x=left_x, right_x=right_x)
            images.append(img)
        
        return images
    
    def generate_right_animation(self, frames=12):
        """Generate right movement animation"""
        images = []
        
        # Start centered
        img, draw = self.draw_eyes(REF_EYE_WIDTH, REF_EYE_HEIGHT, REF_EYE_WIDTH, REF_EYE_HEIGHT)
        images.append(img)
        
        # Move right
        for i in range(frames // 2):
            offset = i * 2
            left_x = self.left_eye_x + offset
            right_x = self.right_eye_x + offset
            img, draw = self.draw_eyes(REF_EYE_WIDTH, REF_EYE_HEIGHT, REF_EYE_WIDTH, REF_EYE_HEIGHT,
                                     left_x=left_x, right_x=right_x)
            images.append(img)
        
        # Move back to center
        for i in range(frames // 2):
            offset = (frames // 2 - i) * 2
            left_x = self.left_eye_x + offset
            right_x = self.right_eye_x + offset
            img, draw = self.draw_eyes(REF_EYE_WIDTH, REF_EYE_HEIGHT, REF_EYE_WIDTH, REF_EYE_HEIGHT,
                                     left_x=left_x, right_x=right_x)
            images.append(img)
        
        return images
    
    def generate_center_animation(self, frames=6):
        """Generate center state (static)"""
        images = []
        
        for _ in range(frames):
            img, draw = self.draw_eyes(REF_EYE_WIDTH, REF_EYE_HEIGHT, REF_EYE_WIDTH, REF_EYE_HEIGHT)
            images.append(img)
        
        return images
    
    # ==================== FUN EXPRESSIONS ====================
    
    def generate_wink_animation(self, frames=12):
        """Generate winking animation - one eye closes while other stays open"""
        images = []
        
        # Both eyes open
        for _ in range(2):
            img, draw = self.draw_eyes(REF_EYE_WIDTH, REF_EYE_HEIGHT, REF_EYE_WIDTH, REF_EYE_HEIGHT)
            images.append(img)
        
        # Left eye closes, right eye stays open
        for i in range(frames // 2):
            left_height = REF_EYE_HEIGHT - (REF_EYE_HEIGHT * i / (frames // 2))
            img, draw = self.draw_eyes(REF_EYE_WIDTH, max(left_height, 2), REF_EYE_WIDTH, REF_EYE_HEIGHT)
            images.append(img)
        
        # Left eye opens, right eye stays open
        for i in range(frames // 2):
            left_height = (REF_EYE_HEIGHT * i / (frames // 2))
            img, draw = self.draw_eyes(REF_EYE_WIDTH, left_height, REF_EYE_WIDTH, REF_EYE_HEIGHT)
            images.append(img)
        
        # Both eyes open
        for _ in range(2):
            img, draw = self.draw_eyes(REF_EYE_WIDTH, REF_EYE_HEIGHT, REF_EYE_WIDTH, REF_EYE_HEIGHT)
            images.append(img)
        
        return images
    
    def generate_heart_eyes_animation(self, frames=15):
        """Generate heart eyes animation - eyes turn into hearts"""
        images = []
        
        # Start with normal eyes
        img, draw = self.draw_eyes(REF_EYE_WIDTH, REF_EYE_HEIGHT, REF_EYE_WIDTH, REF_EYE_HEIGHT)
        images.append(img)
        
        # Transform eyes into hearts
        for i in range(frames):
            img = Image.new('RGB', (self.width, self.height), 'black')
            draw = ImageDraw.Draw(img)
            
            # Calculate heart size based on frame
            heart_size = 20 + (i * 2)
            
            # Draw heart-shaped eyes
            # Left heart eye
            left_heart_x = self.left_eye_x
            left_heart_y = self.left_eye_y
            
            # Heart shape (simplified)
            heart_points = [
                (left_heart_x - heart_size//2, left_heart_y),
                (left_heart_x, left_heart_y - heart_size//2),
                (left_heart_x + heart_size//2, left_heart_y),
                (left_heart_x, left_heart_y + heart_size//2),
                (left_heart_x - heart_size//4, left_heart_y + heart_size//4),
                (left_heart_x + heart_size//4, left_heart_y + heart_size//4),
            ]
            draw.polygon(heart_points, fill='white')
            
            # Right heart eye
            right_heart_x = self.right_eye_x
            right_heart_y = self.right_eye_y
            
            heart_points = [
                (right_heart_x - heart_size//2, right_heart_y),
                (right_heart_x, right_heart_y - heart_size//2),
                (right_heart_x + heart_size//2, right_heart_y),
                (right_heart_x, right_heart_y + heart_size//2),
                (right_heart_x - heart_size//4, right_heart_y + heart_size//4),
                (right_heart_x + heart_size//4, right_heart_y + heart_size//4),
            ]
            draw.polygon(heart_points, fill='white')
            
            images.append(img)
        
        return images
    
    def generate_rolling_eyes_animation(self, frames=20):
        """Generate rolling eyes animation - eyes roll in circles"""
        images = []
        
        for i in range(frames):
            img = Image.new('RGB', (self.width, self.height), 'black')
            draw = ImageDraw.Draw(img)
            
            # Calculate rotation angle
            angle = (i * 360 / frames) * math.pi / 180
            
            # Calculate eye positions in circle
            radius = 15
            left_offset_x = radius * math.cos(angle)
            left_offset_y = radius * math.sin(angle)
            right_offset_x = radius * math.cos(angle + math.pi)  # Opposite direction
            right_offset_y = radius * math.sin(angle + math.pi)
            
            # Draw eyes at rotated positions
            left_x = self.left_eye_x + left_offset_x
            left_y = self.left_eye_y + left_offset_y
            right_x = self.right_eye_x + right_offset_x
            right_y = self.right_eye_y + right_offset_y
            
            # Draw left eye
            left_rect = [
                left_x - REF_EYE_WIDTH // 2,
                left_y - REF_EYE_HEIGHT // 2,
                left_x + REF_EYE_WIDTH // 2,
                left_y + REF_EYE_HEIGHT // 2
            ]
            draw.rounded_rectangle(left_rect, radius=REF_CORNER_RADIUS, fill='white')
            
            # Draw right eye
            right_rect = [
                right_x - REF_EYE_WIDTH // 2,
                right_y - REF_EYE_HEIGHT // 2,
                right_x + REF_EYE_WIDTH // 2,
                right_y + REF_EYE_HEIGHT // 2
            ]
            draw.rounded_rectangle(right_rect, radius=REF_CORNER_RADIUS, fill='white')
            
            images.append(img)
        
        return images
    
    def generate_zigzag_animation(self, frames=16):
        """Generate zigzag animation - eyes move in zigzag pattern"""
        images = []
        
        for i in range(frames):
            img = Image.new('RGB', (self.width, self.height), 'black')
            draw = ImageDraw.Draw(img)
            
            # Calculate zigzag movement
            zigzag_amplitude = 20
            zigzag_frequency = 3
            offset_y = zigzag_amplitude * math.sin(i * zigzag_frequency * math.pi / frames)
            
            # Apply offset to both eyes
            left_y = self.left_eye_y + offset_y
            right_y = self.right_eye_y + offset_y
            
            # Draw eyes
            left_rect = [
                self.left_eye_x - REF_EYE_WIDTH // 2,
                left_y - REF_EYE_HEIGHT // 2,
                self.left_eye_x + REF_EYE_WIDTH // 2,
                left_y + REF_EYE_HEIGHT // 2
            ]
            draw.rounded_rectangle(left_rect, radius=REF_CORNER_RADIUS, fill='white')
            
            right_rect = [
                self.right_eye_x - REF_EYE_WIDTH // 2,
                right_y - REF_EYE_HEIGHT // 2,
                self.right_eye_x + REF_EYE_WIDTH // 2,
                right_y + REF_EYE_HEIGHT // 2
            ]
            draw.rounded_rectangle(right_rect, radius=REF_CORNER_RADIUS, fill='white')
            
            images.append(img)
        
        return images
    
    def generate_rainbow_animation(self, frames=18):
        """Generate rainbow animation - eyes change colors in rainbow sequence"""
        images = []
        
        # Rainbow colors
        colors = [
            (255, 0, 0),    # Red
            (255, 127, 0),  # Orange
            (255, 255, 0),  # Yellow
            (0, 255, 0),    # Green
            (0, 0, 255),    # Blue
            (75, 0, 130),   # Indigo
            (148, 0, 211),  # Violet
        ]
        
        for i in range(frames):
            # Cycle through colors
            color_index = i % len(colors)
            color = colors[color_index]
            
            img, draw = self.draw_eyes(REF_EYE_WIDTH, REF_EYE_HEIGHT, REF_EYE_WIDTH, REF_EYE_HEIGHT, color=color)
            images.append(img)
        
        return images
    
    def save_gif(self, images, filename, duration=50):
        """Save images as GIF animation"""
        if images:
            images[0].save(
                filename,
                save_all=True,
                append_images=images[1:],
                duration=duration,
                loop=0
            )
            print(f"Generated {filename} with {len(images)} frames")

def main():
    """Generate all emoji animations"""
    generator = CompleteEmojiGenerator()
    
    # Create output directory
    output_dir = "/home/luoben/TuyaOpen/apps/tuya.ai/your_desk_emoji/emoji_animations"
    os.makedirs(output_dir, exist_ok=True)
    
    # Generate all animations
    animations = {
        # Basic emotions
        'happy': generator.generate_happy_animation,
        'sad': generator.generate_sad_animation,
        'anger': generator.generate_anger_animation,
        'surprise': generator.generate_surprise_animation,
        'sleep': generator.generate_sleep_animation,
        'wakeup': generator.generate_wakeup_animation,
        'left': generator.generate_left_animation,
        'right': generator.generate_right_animation,
        'center': generator.generate_center_animation,
        # Fun expressions
        'wink': generator.generate_wink_animation,
        'heart_eyes': generator.generate_heart_eyes_animation,
        'rolling': generator.generate_rolling_eyes_animation,
        'zigzag': generator.generate_zigzag_animation,
        'rainbow': generator.generate_rainbow_animation,
    }
    
    for name, func in animations.items():
        print(f"Generating {name}...")
        images = func()
        filename = os.path.join(output_dir, f"{name}.gif")
        generator.save_gif(images, filename)
    
    print("All animations generated successfully!")
    print(f"Total: {len(animations)} emoji animations")
    print(f"Screen size: {SCREEN_WIDTH}x{SCREEN_HEIGHT}")
    print(f"Eye size: {REF_EYE_WIDTH}x{REF_EYE_HEIGHT}")
    print(f"Output directory: {output_dir}")

if __name__ == "__main__":
    main()
