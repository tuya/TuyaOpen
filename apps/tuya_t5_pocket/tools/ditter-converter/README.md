# Video Dithering Converter

A Python tool to convert videos to monochrome dithered GIFs or PNGs at 168x384 resolution. Supports multiple dithering algorithms and configurable FPS.

## Features

- **Input Formats**: MP4, GIF, and PNG files
- **Output Format**: Monochrome GIF or PNG at 168x384 resolution
- **Multiple Dithering Algorithms**: Floyd-Steinberg, Ordered, and Atkinson
- **Configurable FPS**: Adjustable output frame rate
- **Aspect Ratio Preservation**: Maintains aspect ratio during resizing
- **Progress Tracking**: Shows processing progress
- **Frame Reduction**: Limit frames for smaller file sizes
- **Duration Cutting**: Cut video to specific time ranges
- **Color Inversion**: Invert colors for negative/white-on-black effects
- **Keep Black**: Preserve black areas when inverting (white-on-black effect)
- **Exposure Adjustment**: Brighten or darken video before dithering

## Installation

1. Install Python dependencies:
```bash
pip install -r requirements.txt
```

## Usage

### Basic Usage

```bash
# Convert MP4 to dithered GIF
python dittering.py input.mp4 output.gif

# Convert GIF to dithered GIF
python dittering.py input.gif output.gif

# Convert PNG to dithered GIF
python dittering.py input.png output.gif

# Convert PNG to dithered PNG
python dittering.py input.png output.png

# Convert video to dithered PNG (first frame only)
python dittering.py input.mp4 output.png
```

### Advanced Usage

```bash
# Use Floyd-Steinberg dithering with 15 FPS
python dittering.py input.mp4 output.gif --algorithm floyd-steinberg --fps 15

# Use Ordered dithering with 10 FPS
python dittering.py input.mp4 output.gif --algorithm ordered --fps 10

# Use Atkinson dithering with 12 FPS
python dittering.py input.mp4 output.gif --algorithm atkinson --fps 12

# Custom dimensions (default is 168x384)
python dittering.py input.mp4 output.gif --width 200 --height 400

# Limit to 50 frames for smaller file size
python dittering.py input.mp4 output.gif --max-frames 50

# Cut video from 5 seconds to 15 seconds
python dittering.py input.mp4 output.gif --start-time 5 --end-time 15

# Combine frame reduction and duration cutting
python dittering.py input.mp4 output.gif --max-frames 30 --start-time 2 --end-time 8

# Create inverted (negative) effect
python dittering.py input.mp4 output.gif --invert

# Combine inversion with other options
python dittering.py input.mp4 output.gif --invert --algorithm atkinson --fps 15

# Brighten video before dithering
python dittering.py input.mp4 output.gif --exposure 0.5

# Darken video before dithering
python dittering.py input.mp4 output.gif --exposure -0.5

# Combine exposure with other effects
python dittering.py input.mp4 output.gif --exposure 1.0 --invert --algorithm floyd-steinberg

# Invert but keep black areas as black (white-on-black effect)
python dittering.py input.mp4 output.gif --invert --keep-black

# Combine keep-black with other options
python dittering.py input.mp4 output.gif --invert --keep-black --exposure 0.5

# Use different dithering algorithms
python dittering.py input.mp4 output.gif --algorithm sierra
python dittering.py input.mp4 output.gif --algorithm burkes
python dittering.py input.mp4 output.gif --algorithm stucki
python dittering.py input.mp4 output.gif --algorithm threshold
python dittering.py input.mp4 output.gif --algorithm random

# Filter color range to improve dithering
python dittering.py input.mp4 output.gif --min-value 50 --max-value 200

# Combine color filtering with other effects
python dittering.py input.mp4 output.gif --min-value 30 --max-value 180 --exposure 0.5 --algorithm sierra
```

### Command Line Options

- `input`: Input video file (MP4 or GIF)
- `output`: Output GIF file
- `--algorithm, -a`: Dithering algorithm to use
  - `floyd-steinberg` (default): Best quality, slower processing
  - `ordered`: Fast processing, good for patterns
  - `atkinson`: Classic Macintosh-style dithering
- `--fps, -f`: Output GIF FPS (default: 10)
- `--width, -w`: Target width (default: 168)
- `--height, -H`: Target height (default: 384)
- `--max-frames, -m`: Maximum number of frames to include in GIF
- `--start-time, -s`: Start time in seconds (cut beginning of video)
- `--end-time, -e`: End time in seconds (cut end of video)
- `--invert, -i`: Invert video colors before dithering (negative effect)
- `--keep-black, -k`: When inverting, keep black areas as black (only invert non-black areas)
- `--exposure, -x`: Exposure adjustment (-2.0 to 2.0, negative=darker, positive=brighter)
- `--min-value, -l`: Minimum color value to keep (0-255, for range filtering)
- `--max-value, -u`: Maximum color value to keep (0-255, for range filtering)

## Exposure Adjustment

The `--exposure` parameter allows you to brighten or darken the video before dithering:

- **Range**: -2.0 to 2.0
- **0.0**: No change (default)
- **Negative values**: Darken the video
- **Positive values**: Brighten the video

### Exposure Examples:
- `--exposure -1.0`: Darken significantly (good for bright videos)
- `--exposure -0.5`: Darken moderately
- `--exposure 0.0`: No adjustment
- `--exposure 0.5`: Brighten moderately
- `--exposure 1.0`: Brighten significantly (good for dark videos)

### Processing Order:
1. Convert to grayscale
2. Filter color range (if specified)
3. Adjust exposure (if specified)
4. Invert colors (if specified)
5. Apply dithering

## Color Range Filtering

The `--min-value` and `--max-value` parameters allow you to filter the color range to improve dithering:

- **Range**: 0-255 for both parameters
- **Purpose**: Focus dithering on specific brightness ranges
- **Use cases**: Remove very dark or very bright areas that don't dither well

### Color Range Examples:
- `--min-value 50 --max-value 200`: Remove very dark and very bright areas
- `--min-value 100 --max-value 255`: Focus on bright areas only
- `--min-value 0 --max-value 150`: Focus on darker areas only
- `--min-value 80 --max-value 180`: Focus on mid-tones only

## Dithering Algorithms

### 1. Floyd-Steinberg Dithering
- **Best quality** for most images
- **Error diffusion** algorithm
- Distributes quantization error to neighboring pixels
- **Slower processing** but highest quality results

### 2. Ordered Dithering
- **Fastest processing**
- Uses **Bayer matrix** pattern
- Good for **geometric patterns** and textures
- **Predictable** dithering pattern

### 3. Atkinson Dithering
- **Classic Macintosh** style
- **Reduced error diffusion** (1/8 instead of full error)
- Creates **sharper** but more **contrasty** results
- Good for **retro aesthetic**

### 4. Sierra Dithering
- **Sierra-2-4A** algorithm
- **Balanced** error diffusion
- Good for **natural images** and photographs
- **Medium speed** processing

### 5. Burkes Dithering
- **Extended error diffusion** pattern
- **Smooth gradients** and transitions
- Good for **detailed images**
- **Slower** than basic algorithms

### 6. Stucki Dithering
- **Wide error diffusion** pattern
- **Excellent** for **text and line art**
- **High quality** results
- **Slower** processing

### 7. Threshold Dithering
- **No error diffusion**
- **Fastest** processing
- **High contrast** results
- Good for **simple graphics**

### 8. Random Dithering
- **Noise-based** dithering
- **Breaks up patterns**
- Good for **texture reduction**
- **Medium speed** processing

## Examples

### Example 1: Basic Conversion
```bash
python dittering.py video.mp4 result.gif
```
Converts `video.mp4` to `result.gif` using Floyd-Steinberg dithering at 10 FPS.

### Example 2: High FPS Animation
```bash
python dittering.py animation.mp4 smooth.gif --fps 20 --algorithm ordered
```
Creates a smooth 20 FPS animation using ordered dithering.

### Example 3: Retro Style
```bash
python dittering.py modern.mp4 retro.gif --algorithm atkinson --fps 8
```
Creates a retro-style GIF with Atkinson dithering at 8 FPS.

## Technical Details

### Resolution
- **Default**: 168x384 pixels
- **Aspect Ratio**: Maintained during resizing
- **Cropping**: Centers the image when aspect ratio doesn't match

### Processing Pipeline
1. **Load** video frames (MP4/GIF)
2. **Resize** to target dimensions
3. **Convert** to grayscale
4. **Apply** dithering algorithm
5. **Save** as GIF with specified FPS

### Performance
- **Floyd-Steinberg**: ~2-3x slower, best quality
- **Ordered**: Fastest, good for patterns
- **Atkinson**: Medium speed, retro aesthetic

## Troubleshooting

### Common Issues

1. **"No frames loaded"**
   - Check if input file exists and is valid
   - Ensure file is MP4 or GIF format

2. **"Error loading MP4"**
   - Install OpenCV: `pip install opencv-python`
   - Check if file is corrupted

3. **"Error loading GIF"**
   - Install imageio: `pip install imageio`
   - Check if GIF is valid

4. **Large file sizes**
   - Reduce FPS: `--fps 5`
   - Use ordered dithering for smaller files

### Dependencies
- `opencv-python`: Video processing
- `numpy`: Array operations
- `Pillow`: Image processing
- `imageio`: GIF handling

## License

This tool is part of the Tuya T5 Pocket project. 