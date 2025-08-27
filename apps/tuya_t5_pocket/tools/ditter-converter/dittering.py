#!/usr/bin/env python3
"""
Video Dithering Converter
=========================

A tool to convert videos to monochrome dithered GIFs or PNGs at 168x384 resolution.
Supports multiple dithering algorithms and configurable FPS.

Usage:
    python dittering.py input.mp4 output.gif --algorithm floyd-steinberg --fps 10
    python dittering.py input.gif output.gif --algorithm ordered --fps 15
    python dittering.py input.png output.gif --algorithm atkinson --fps 1
    python dittering.py input.png output.png --algorithm floyd-steinberg
"""

import argparse
import cv2
import numpy as np
from PIL import Image
import imageio
import os
import sys
from typing import Tuple, Optional


class DitheringConverter:
    """Video dithering converter with multiple algorithms."""
    
    def __init__(self, target_width: int = 168, target_height: int = 384):
        self.target_width = target_width
        self.target_height = target_height
        
    def load_video(self, input_path: str) -> list:
        """Load video frames from MP4, GIF, or PNG file."""
        if input_path.lower().endswith('.gif'):
            return self._load_gif(input_path)
        elif input_path.lower().endswith('.png'):
            return self._load_png(input_path)
        else:
            return self._load_mp4(input_path)
    
    def _load_gif(self, gif_path: str) -> list:
        """Load frames from GIF file."""
        try:
            gif = imageio.mimread(gif_path)
            frames = []
            for frame in gif:
                # Convert to RGB if needed
                if len(frame.shape) == 3 and frame.shape[2] == 4:
                    frame = frame[:, :, :3]  # Remove alpha channel
                frames.append(frame)
            return frames
        except Exception as e:
            print(f"Error loading GIF: {e}")
            return []
    
    def _load_mp4(self, mp4_path: str) -> list:
        """Load frames from MP4 file."""
        try:
            cap = cv2.VideoCapture(mp4_path)
            frames = []
            while True:
                ret, frame = cap.read()
                if not ret:
                    break
                # Convert BGR to RGB
                frame = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
                frames.append(frame)
            cap.release()
            return frames
        except Exception as e:
            print(f"Error loading MP4: {e}")
            return []
    
    def _load_png(self, png_path: str) -> list:
        """Load frames from PNG file (single frame)."""
        try:
            # Load PNG using PIL
            image = Image.open(png_path)
            
            # Convert to RGB if needed
            if image.mode in ('RGBA', 'LA', 'P'):
                image = image.convert('RGB')
            
            # Convert PIL image to numpy array
            frame = np.array(image)
            
            return [frame]
        except Exception as e:
            print(f"Error loading PNG: {e}")
            return []
    
    def resize_frame(self, frame: np.ndarray) -> np.ndarray:
        """Resize frame to target dimensions while maintaining aspect ratio."""
        h, w = frame.shape[:2]
        
        # Calculate aspect ratios
        target_ratio = self.target_width / self.target_height
        current_ratio = w / h
        
        if current_ratio > target_ratio:
            # Image is wider than target, crop width
            new_w = int(h * target_ratio)
            start_x = (w - new_w) // 2
            frame = frame[:, start_x:start_x + new_w]
        else:
            # Image is taller than target, crop height
            new_h = int(w / target_ratio)
            start_y = (h - new_h) // 2
            frame = frame[start_y:start_y + new_h, :]
        
        # Resize to target dimensions
        frame = cv2.resize(frame, (self.target_width, self.target_height))
        return frame
    
    def to_grayscale(self, frame: np.ndarray) -> np.ndarray:
        """Convert frame to grayscale."""
        if len(frame.shape) == 3:
            gray = cv2.cvtColor(frame, cv2.COLOR_RGB2GRAY)
        else:
            gray = frame
        return gray
    
    def invert_frame(self, frame: np.ndarray, keep_black: bool = False) -> np.ndarray:
        """Invert the frame (negative effect).
        
        Args:
            frame: Grayscale frame (0-255)
            keep_black: If True, keep black areas (0) as black, only invert non-black areas
        """
        if keep_black:
            # Keep black areas as black, invert only non-black areas
            result = frame.copy()
            # Invert only pixels that are not black (value > 0)
            mask = frame > 0
            result[mask] = 255 - frame[mask]
            return result
        else:
            # Standard inversion
            return 255 - frame
    
    def adjust_exposure(self, frame: np.ndarray, exposure: float) -> np.ndarray:
        """Adjust exposure/brightness of the frame.
        
        Args:
            frame: Grayscale frame (0-255)
            exposure: Exposure adjustment (-2.0 to 2.0)
                      - Negative values darken
                      - Positive values brighten
                      - 0.0 = no change
        """
        # Apply exposure adjustment
        adjusted = frame * (2.0 ** exposure)
        
        # Clip to valid range (0-255)
        adjusted = np.clip(adjusted, 0, 255)
        
        return adjusted.astype(np.uint8)
    
    def filter_color_range(self, frame: np.ndarray, min_value: int = 0, max_value: int = 255) -> np.ndarray:
        """Filter color range to improve dithering.
        
        Args:
            frame: Grayscale frame (0-255)
            min_value: Minimum value to keep (0-255)
            max_value: Maximum value to keep (0-255)
        """
        # Normalize the range to 0-255
        if min_value >= max_value:
            return frame
        
        # Scale the frame to the specified range
        range_size = max_value - min_value
        normalized = ((frame - min_value) / range_size) * 255
        
        # Clip to valid range
        filtered = np.clip(normalized, 0, 255)
        
        return filtered.astype(np.uint8)
    
    def floyd_steinberg_dither(self, image: np.ndarray) -> np.ndarray:
        """Floyd-Steinberg dithering algorithm."""
        h, w = image.shape
        result = image.copy().astype(float)
        
        for y in range(h):
            for x in range(w):
                old_pixel = result[y, x]
                new_pixel = 255 if old_pixel > 127 else 0
                result[y, x] = new_pixel
                
                error = old_pixel - new_pixel
                
                # Distribute error to neighboring pixels
                if x + 1 < w:
                    result[y, x + 1] += error * 7 / 16
                if x - 1 >= 0 and y + 1 < h:
                    result[y + 1, x - 1] += error * 3 / 16
                if y + 1 < h:
                    result[y + 1, x] += error * 5 / 16
                if x + 1 < w and y + 1 < h:
                    result[y + 1, x + 1] += error * 1 / 16
        
        return result.astype(np.uint8)
    
    def ordered_dither(self, image: np.ndarray) -> np.ndarray:
        """Ordered dithering using Bayer matrix."""
        # 8x8 Bayer matrix
        bayer_matrix = np.array([
            [0, 48, 12, 60, 3, 51, 15, 63],
            [32, 16, 44, 28, 35, 19, 47, 31],
            [8, 56, 4, 52, 11, 59, 7, 55],
            [40, 24, 36, 20, 43, 27, 39, 23],
            [2, 50, 14, 62, 1, 49, 13, 61],
            [34, 18, 46, 30, 33, 17, 45, 29],
            [10, 58, 6, 54, 9, 57, 5, 53],
            [42, 26, 38, 22, 41, 25, 37, 21]
        ]) / 64.0
        
        h, w = image.shape
        result = np.zeros_like(image)
        
        for y in range(h):
            for x in range(w):
                threshold = bayer_matrix[y % 8, x % 8]
                result[y, x] = 255 if image[y, x] / 255.0 > threshold else 0
        
        return result
    
    def atkinson_dither(self, image: np.ndarray) -> np.ndarray:
        """Atkinson dithering algorithm (used in early Macintosh)."""
        h, w = image.shape
        result = image.copy().astype(float)
        
        for y in range(h):
            for x in range(w):
                old_pixel = result[y, x]
                new_pixel = 255 if old_pixel > 127 else 0
                result[y, x] = new_pixel
                
                error = (old_pixel - new_pixel) / 8  # Atkinson uses 1/8 distribution
                
                # Distribute error to neighboring pixels (Atkinson pattern)
                neighbors = [
                    (0, 1), (1, 0), (1, 1),  # Right, Down, Down-Right
                    (0, 2), (2, 0)             # Two pixels away
                ]
                
                for dy, dx in neighbors:
                    ny, nx = y + dy, x + dx
                    if 0 <= ny < h and 0 <= nx < w:
                        result[ny, nx] += error
        
        return result.astype(np.uint8)
    
    def sierra_dither(self, image: np.ndarray) -> np.ndarray:
        """Sierra dithering algorithm (Sierra-2-4A)."""
        h, w = image.shape
        result = image.copy().astype(float)
        
        for y in range(h):
            for x in range(w):
                old_pixel = result[y, x]
                new_pixel = 255 if old_pixel > 127 else 0
                result[y, x] = new_pixel
                
                error = old_pixel - new_pixel
                
                # Sierra-2-4A pattern
                if x + 1 < w:
                    result[y, x + 1] += error * 2 / 4
                if x + 2 < w:
                    result[y, x + 2] += error * 1 / 4
                if y + 1 < h:
                    if x - 1 >= 0:
                        result[y + 1, x - 1] += error * 1 / 4
                    result[y + 1, x] += error * 2 / 4
                    if x + 1 < w:
                        result[y + 1, x + 1] += error * 1 / 4
        
        return result.astype(np.uint8)
    
    def burkes_dither(self, image: np.ndarray) -> np.ndarray:
        """Burkes dithering algorithm."""
        h, w = image.shape
        result = image.copy().astype(float)
        
        for y in range(h):
            for x in range(w):
                old_pixel = result[y, x]
                new_pixel = 255 if old_pixel > 127 else 0
                result[y, x] = new_pixel
                
                error = old_pixel - new_pixel
                
                # Burkes pattern
                if x + 1 < w:
                    result[y, x + 1] += error * 8 / 32
                if x + 2 < w:
                    result[y, x + 2] += error * 4 / 32
                if y + 1 < h:
                    if x - 2 >= 0:
                        result[y + 1, x - 2] += error * 2 / 32
                    if x - 1 >= 0:
                        result[y + 1, x - 1] += error * 4 / 32
                    result[y + 1, x] += error * 8 / 32
                    if x + 1 < w:
                        result[y + 1, x + 1] += error * 4 / 32
                    if x + 2 < w:
                        result[y + 1, x + 2] += error * 2 / 32
        
        return result.astype(np.uint8)
    
    def stucki_dither(self, image: np.ndarray) -> np.ndarray:
        """Stucki dithering algorithm."""
        h, w = image.shape
        result = image.copy().astype(float)
        
        for y in range(h):
            for x in range(w):
                old_pixel = result[y, x]
                new_pixel = 255 if old_pixel > 127 else 0
                result[y, x] = new_pixel
                
                error = old_pixel - new_pixel
                
                # Stucki pattern
                if x + 1 < w:
                    result[y, x + 1] += error * 8 / 42
                if x + 2 < w:
                    result[y, x + 2] += error * 4 / 42
                if y + 1 < h:
                    if x - 2 >= 0:
                        result[y + 1, x - 2] += error * 2 / 42
                    if x - 1 >= 0:
                        result[y + 1, x - 1] += error * 4 / 42
                    result[y + 1, x] += error * 8 / 42
                    if x + 1 < w:
                        result[y + 1, x + 1] += error * 4 / 42
                    if x + 2 < w:
                        result[y + 1, x + 2] += error * 2 / 42
                if y + 2 < h:
                    if x - 1 >= 0:
                        result[y + 2, x - 1] += error * 1 / 42
                    result[y + 2, x] += error * 2 / 42
                    if x + 1 < w:
                        result[y + 2, x + 1] += error * 1 / 42
        
        return result.astype(np.uint8)
    
    def threshold_dither(self, image: np.ndarray) -> np.ndarray:
        """Simple threshold dithering (no error diffusion)."""
        return np.where(image > 127, 255, 0).astype(np.uint8)
    
    def random_dither(self, image: np.ndarray) -> np.ndarray:
        """Random dithering with noise."""
        # Add random noise to break up patterns
        noise = np.random.randint(0, 64, image.shape, dtype=np.uint8)
        noisy_image = np.clip(image.astype(int) + noise, 0, 255).astype(np.uint8)
        return np.where(noisy_image > 127, 255, 0).astype(np.uint8)
    
    def _print_gif_specs(self, output_path: str, frame_count: int, fps: int) -> None:
        """Print final GIF specifications."""
        try:
            # Get file size
            file_size = os.path.getsize(output_path)
            file_size_mb = file_size / (1024 * 1024)
            
            # Get image dimensions
            with Image.open(output_path) as img:
                width, height = img.size
                
            # Calculate duration
            duration_sec = frame_count / fps
            
            print("\n" + "="*50)
            print("📊 FINAL GIF SPECIFICATIONS")
            print("="*50)
            print(f"📁 File: {output_path}")
            print(f"📏 Dimensions: {width}x{height} pixels")
            print(f"🎬 Frames: {frame_count}")
            print(f"⚡ FPS: {fps}")
            print(f"⏱️  Duration: {duration_sec:.2f} seconds")
            print(f"💾 File Size: {file_size_mb:.2f} MB ({file_size:,} bytes)")
            print(f"🎨 Format: Monochrome (1-bit)")
            print(f"🔄 Loop: Infinite")
            print("="*50)
            
        except Exception as e:
            print(f"Warning: Could not get GIF specifications: {e}")
    
    def _print_png_specs(self, output_path: str) -> None:
        """Print final PNG specifications."""
        try:
            # Get file size
            file_size = os.path.getsize(output_path)
            file_size_kb = file_size / 1024
            
            # Get image dimensions
            with Image.open(output_path) as img:
                width, height = img.size
                
            print("\n" + "="*50)
            print("📊 FINAL PNG SPECIFICATIONS")
            print("="*50)
            print(f"📁 File: {output_path}")
            print(f"📏 Dimensions: {width}x{height} pixels")
            print(f"💾 File Size: {file_size_kb:.2f} KB ({file_size:,} bytes)")
            print(f"🎨 Format: Monochrome (1-bit)")
            print(f"🖼️  Type: Single frame")
            print("="*50)
            
        except Exception as e:
            print(f"Warning: Could not get PNG specifications: {e}")
    
    def _reduce_frames(self, frames: list, max_frames: int) -> list:
        """Reduce number of frames by sampling evenly."""
        if len(frames) <= max_frames:
            return frames
        
        # Calculate step size to sample frames evenly
        step = len(frames) / max_frames
        reduced_frames = []
        
        for i in range(max_frames):
            frame_index = int(i * step)
            if frame_index < len(frames):
                reduced_frames.append(frames[frame_index])
        
        return reduced_frames
    
    def _cut_duration(self, frames: list, start_time: Optional[float], 
                      end_time: Optional[float], fps: float) -> list:
        """Cut frames based on start and end time."""
        if start_time is None and end_time is None:
            return frames
        
        total_frames = len(frames)
        start_frame = 0
        end_frame = total_frames
        
        if start_time is not None:
            start_frame = int(start_time * fps)
            start_frame = max(0, min(start_frame, total_frames - 1))
        
        if end_time is not None:
            end_frame = int(end_time * fps)
            end_frame = max(start_frame + 1, min(end_frame, total_frames))
        
        print(f"Cutting frames: {start_frame} to {end_frame} (time: {start_time or 0:.2f}s to {end_time or total_frames/fps:.2f}s)")
        return frames[start_frame:end_frame]
    
    def dither_frame(self, frame: np.ndarray, algorithm: str) -> np.ndarray:
        """Apply dithering algorithm to frame."""
        if algorithm == "floyd-steinberg":
            return self.floyd_steinberg_dither(frame)
        elif algorithm == "ordered":
            return self.ordered_dither(frame)
        elif algorithm == "atkinson":
            return self.atkinson_dither(frame)
        elif algorithm == "sierra":
            return self.sierra_dither(frame)
        elif algorithm == "burkes":
            return self.burkes_dither(frame)
        elif algorithm == "stucki":
            return self.stucki_dither(frame)
        elif algorithm == "threshold":
            return self.threshold_dither(frame)
        elif algorithm == "random":
            return self.random_dither(frame)
        else:
            raise ValueError(f"Unknown algorithm: {algorithm}")
    
    def process_video(self, input_path: str, output_path: str, 
                     algorithm: str = "floyd-steinberg", fps: int = 10,
                     max_frames: Optional[int] = None, 
                     start_time: Optional[float] = None,
                     end_time: Optional[float] = None,
                     invert: bool = False,
                     exposure: float = 0.0,
                     keep_black: bool = False,
                     min_value: Optional[int] = None,
                     max_value: Optional[int] = None) -> bool:
        """Process video with specified dithering algorithm."""
        print(f"Loading video: {input_path}")
        frames = self.load_video(input_path)
        
        if not frames:
            print("No frames loaded!")
            return False
        
        # Apply duration cutting if specified
        if start_time is not None or end_time is not None:
            frames = self._cut_duration(frames, start_time, end_time, fps)
            if not frames:
                print("No frames remaining after duration cut!")
                return False
        
        # Apply frame reduction if specified
        if max_frames is not None and len(frames) > max_frames:
            frames = self._reduce_frames(frames, max_frames)
            print(f"Reduced to {len(frames)} frames")
        
        print(f"Processing {len(frames)} frames...")
        processed_frames = []
        
        for i, frame in enumerate(frames):
            if i % 10 == 0:
                print(f"Processing frame {i+1}/{len(frames)}")
            
            # Resize frame
            resized = self.resize_frame(frame)
            
            # Convert to grayscale
            gray = self.to_grayscale(resized)
            
            # Filter color range if specified
            if min_value is not None or max_value is not None:
                min_val = min_value if min_value is not None else 0
                max_val = max_value if max_value is not None else 255
                gray = self.filter_color_range(gray, min_val, max_val)
            
            # Adjust exposure if specified
            if exposure != 0.0:
                gray = self.adjust_exposure(gray, exposure)
            
            # Invert if requested
            if invert:
                gray = self.invert_frame(gray, keep_black)
            
            # Apply dithering
            dithered = self.dither_frame(gray, algorithm)
            
            # Convert to PIL Image for GIF
            pil_image = Image.fromarray(dithered)
            processed_frames.append(pil_image)
        
        # Determine output format
        is_png_output = output_path.lower().endswith('.png')
        
        if is_png_output:
            print(f"Saving PNG: {output_path}")
            try:
                # Save as single PNG frame
                processed_frames[0].save(output_path)
                
                # Print final PNG specifications
                self._print_png_specs(output_path)
                
                print("Conversion completed successfully!")
                return True
            except Exception as e:
                print(f"Error saving PNG: {e}")
                return False
        else:
            print(f"Saving GIF: {output_path}")
            try:
                processed_frames[0].save(
                    output_path,
                    save_all=True,
                    append_images=processed_frames[1:],
                    duration=1000//fps,  # Duration in milliseconds
                    loop=0
                )
                
                # Print final GIF specifications
                self._print_gif_specs(output_path, len(processed_frames), fps)
                
                print("Conversion completed successfully!")
                return True
            except Exception as e:
                print(f"Error saving GIF: {e}")
                return False


def main():
    """Main function with command line interface."""
    parser = argparse.ArgumentParser(
        description="Convert videos to monochrome dithered GIFs",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python dittering.py input.mp4 output.gif --algorithm floyd-steinberg --fps 10
  python dittering.py input.gif output.gif --algorithm ordered --fps 15
  python dittering.py video.mp4 result.gif --algorithm atkinson --fps 12
        """
    )
    
    parser.add_argument("input", help="Input video file (MP4, GIF, or PNG)")
    parser.add_argument("output", help="Output file (GIF or PNG)")
    parser.add_argument(
        "--algorithm", "-a",
        choices=["floyd-steinberg", "ordered", "atkinson", "sierra", "burkes", "stucki", "threshold", "random"],
        default="floyd-steinberg",
        help="Dithering algorithm to use (default: floyd-steinberg)"
    )
    parser.add_argument(
        "--fps", "-f",
        type=int,
        default=10,
        help="Output GIF FPS (default: 10)"
    )
    parser.add_argument(
        "--width", "-w",
        type=int,
        default=384,
        help="Target width (default: 168)"
    )
    parser.add_argument(
        "--height", "-H",
        type=int,
        default=168,
        help="Target height (default: 384)"
    )
    parser.add_argument(
        "--max-frames", "-m",
        type=int,
        help="Maximum number of frames to include in GIF"
    )
    parser.add_argument(
        "--start-time", "-s",
        type=float,
        help="Start time in seconds (cut beginning of video)"
    )
    parser.add_argument(
        "--end-time", "-e",
        type=float,
        help="End time in seconds (cut end of video)"
    )
    parser.add_argument(
        "--invert", "-i",
        action="store_true",
        help="Invert video colors before dithering (negative effect)"
    )
    parser.add_argument(
        "--keep-black", "-k",
        action="store_true",
        help="When inverting, keep black areas as black (only invert non-black areas)"
    )
    parser.add_argument(
        "--exposure", "-x",
        type=float,
        default=0.0,
        help="Exposure adjustment (-2.0 to 2.0, negative=darker, positive=brighter, default=0.0)"
    )
    parser.add_argument(
        "--min-value", "-l",
        type=int,
        help="Minimum color value to keep (0-255, for range filtering)"
    )
    parser.add_argument(
        "--max-value", "-u",
        type=int,
        help="Maximum color value to keep (0-255, for range filtering)"
    )
    
    args = parser.parse_args()
    
    # Validate input file
    if not os.path.exists(args.input):
        print(f"Error: Input file '{args.input}' not found!")
        sys.exit(1)
    
    # Create converter
    converter = DitheringConverter(args.width, args.height)
    
    # Process video
    success = converter.process_video(
        args.input,
        args.output,
        args.algorithm,
        args.fps,
        args.max_frames,
        args.start_time,
        args.end_time,
        args.invert,
        args.exposure,
        args.keep_black,
        args.min_value,
        args.max_value
    )
    
    if not success:
        sys.exit(1)


if __name__ == "__main__":
    main()