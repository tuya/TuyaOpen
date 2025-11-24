#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Pixel Art GIF/PNG to C Header Tool
Convert GIF/PNG images to C header files with RGB pixel data structures.
Resamples images to default 32x32 pixels (configurable).
"""

import os
import sys
from PIL import Image, ImageOps, ImageEnhance
import argparse


def sanitize_name(name):
    """Convert filename to valid C identifier"""
    # Replace invalid characters with underscore
    name = ''.join(c if c.isalnum() or c == '_' else '_' for c in name)
    # Ensure it doesn't start with a number
    if name and name[0].isdigit():
        name = '_' + name
    return name


def improved_resize(frame, target_width, target_height, resample_method='auto'):
    """
    Improved resize function with better edge handling and color preservation
    
    Args:
        frame: PIL Image to resize
        target_width: Target width
        target_height: Target height
        resample_method: Resampling method ('auto', 'box', 'lanczos', 'nearest', 'bicubic')
    
    Returns:
        Resized PIL Image
    """
    orig_width, orig_height = frame.size
    
    # Determine if we're upscaling or downscaling
    scale_x = target_width / orig_width
    scale_y = target_height / orig_height
    is_downscale = scale_x < 1.0 or scale_y < 1.0
    
    # Select resampling method
    if resample_method == 'auto':
        if is_downscale:
            # For downscaling, use BOX (area averaging) which reduces color shifts
            # BOX averages all pixels in the source area, better for pixel art
            resample = Image.Resampling.BOX
        else:
            # For upscaling, use LANCZOS for smooth results
            resample = Image.Resampling.LANCZOS
    elif resample_method == 'box':
        resample = Image.Resampling.BOX
    elif resample_method == 'lanczos':
        resample = Image.Resampling.LANCZOS
    elif resample_method == 'nearest':
        resample = Image.Resampling.NEAREST
    elif resample_method == 'bicubic':
        resample = Image.Resampling.BICUBIC
    else:
        resample = Image.Resampling.BOX
    
    # For very small downscales, use a two-pass approach to reduce artifacts
    # This helps prevent color bleeding at edges by doing a gradual downscale
    if is_downscale and (scale_x < 0.5 or scale_y < 0.5):
        # First pass: resize to intermediate size (2x target) to reduce color bleeding
        intermediate_w = max(target_width * 2, orig_width // 2)
        intermediate_h = max(target_height * 2, orig_height // 2)
        intermediate = frame.resize((intermediate_w, intermediate_h), Image.Resampling.BOX)
        # Second pass: final resize to target
        result = intermediate.resize((target_width, target_height), Image.Resampling.BOX)
    else:
        # Single pass resize
        result = frame.resize((target_width, target_height), resample)
    
    return result


def generate_c_header(image_path, output_header_path, output_image_path, width=32, height=32, resample_method='auto', invert=False, contrast=1.0, frame_reduction=0.0):
    """
    Convert image to C header file with RGB pixel data structures
    
    Args:
        image_path: Input image path (GIF or PNG)
        output_header_path: Output C header file path
        output_image_path: Output resampled image path
        width: Target width (default 32)
        height: Target height (default 32)
        resample_method: Resampling method ('auto', 'box', 'lanczos', 'nearest', 'bicubic')
        invert: Whether to invert colors before conversion (default False)
        contrast: Contrast factor (0.0 = no contrast, 1.0 = normal, 2.0 = high contrast, default 1.0)
        frame_reduction: Frame reduction percentage for animated GIFs (0.0 = keep all, 0.5 = keep 50%, 0.75 = keep 25%, default 0.0)
    """
    try:
        # Open image
        img = Image.open(image_path)
        
        # Get base name for C identifiers
        base_name = os.path.splitext(os.path.basename(image_path))[0]
        var_name = sanitize_name(base_name)
        
        # Check if it's an animated GIF
        is_animated = getattr(img, 'is_animated', False)
        frames = []
        original_frame_count = 0
        
        if is_animated:
            # Process frames (with optional reduction)
            frame_count = img.n_frames
            original_frame_count = frame_count
            
            # Calculate which frames to keep based on reduction percentage
            if frame_reduction > 0.0 and frame_reduction < 1.0:
                keep_percentage = 1.0 - frame_reduction
                frames_to_keep = max(1, int(frame_count * keep_percentage))
                # Evenly sample frames across the animation
                if frames_to_keep < frame_count:
                    step = frame_count / frames_to_keep
                    frame_indices = [int(i * step) for i in range(frames_to_keep)]
                    # Ensure we always include the last frame
                    if frame_indices[-1] != frame_count - 1:
                        frame_indices[-1] = frame_count - 1
                else:
                    frame_indices = list(range(frame_count))
            else:
                frame_indices = list(range(frame_count))
            
            for frame_idx in frame_indices:
                img.seek(frame_idx)
                # Convert to RGB (handles transparency properly)
                if img.mode in ('RGBA', 'LA', 'P'):
                    # Handle transparency: convert to RGB with white background
                    background = Image.new('RGB', img.size, (255, 255, 255))
                    if img.mode == 'P':
                        frame = img.convert('RGBA')
                    else:
                        frame = img
                    if frame.mode == 'RGBA':
                        background.paste(frame, mask=frame.split()[-1])  # Use alpha channel as mask
                        frame = background
                    else:
                        frame = frame.convert('RGB')
                else:
                    frame = img.convert('RGB')
                # Invert colors if requested (before contrast/resizing)
                if invert:
                    frame = ImageOps.invert(frame.convert('RGB'))
                # Apply contrast adjustment if not 1.0 (before resizing)
                if contrast != 1.0:
                    enhancer = ImageEnhance.Contrast(frame)
                    frame = enhancer.enhance(contrast)
                # Use improved resize with better edge handling
                frame = improved_resize(frame, width, height, resample_method)
                frames.append(frame)
        else:
            # Single frame
            # Convert to RGB (handles transparency properly)
            if img.mode in ('RGBA', 'LA', 'P'):
                # Handle transparency: convert to RGB with white background
                background = Image.new('RGB', img.size, (255, 255, 255))
                if img.mode == 'P':
                    frame = img.convert('RGBA')
                else:
                    frame = img
                if frame.mode == 'RGBA':
                    background.paste(frame, mask=frame.split()[-1])  # Use alpha channel as mask
                    frame = background
                else:
                    frame = frame.convert('RGB')
            else:
                frame = img.convert('RGB')
            # Invert colors if requested (before contrast/resizing)
            if invert:
                frame = ImageOps.invert(frame.convert('RGB'))
            # Apply contrast adjustment if not 1.0 (before resizing)
            if contrast != 1.0:
                enhancer = ImageEnhance.Contrast(frame)
                frame = enhancer.enhance(contrast)
            # Use improved resize with better edge handling
            frame = improved_resize(frame, width, height, resample_method)
            frames.append(frame)
        
        # Save resampled image(s)
        if is_animated:
            # Save as animated GIF
            frames[0].save(
                output_image_path,
                save_all=True,
                append_images=frames[1:],
                duration=img.info.get('duration', 100),
                loop=img.info.get('loop', 0)
            )
        else:
            # Save as PNG
            output_image_path = output_image_path.replace('.gif', '.png')
            frames[0].save(output_image_path)
        
        # Generate C header file
        header_guard = f"__{var_name.upper()}_H__"
        
        c_code = f"""/**
 * @file {os.path.basename(output_header_path)}
 * @brief Pixel art data for {base_name}
 * @note Generated from {os.path.basename(image_path)}
 * @note Image size: {width}x{height} pixels
 */

#ifndef {header_guard}
#define {header_guard}

#include "../pixel_art_types.h"

#ifdef __cplusplus
extern "C" {{
#endif

"""
        
        # Generate pixel data for each frame
        if is_animated:
            c_code += f"#define {var_name.upper()}_FRAME_COUNT {len(frames)}\n\n"
            
            # Generate data for each frame
            for frame_idx, frame in enumerate(frames):
                pixels = list(frame.getdata())
                array_name = f"{var_name}_frame_{frame_idx}_data"
                
                c_code += f"// Frame {frame_idx} pixel data\n"
                c_code += f"static const pixel_rgb_t {array_name}[{width * height}] = {{\n"
                
                # Format pixels: 5 pixels per line (matching reference format)
                for i in range(0, len(pixels), 5):
                    line_pixels = pixels[i:i+5]
                    pixel_strs = [f"{{0x{r:02X}, 0x{g:02X}, 0x{b:02X}}}" for r, g, b in line_pixels]
                    c_code += "    " + ", ".join(pixel_strs)
                    if i + 5 < len(pixels):
                        c_code += ","
                    c_code += "\n"
                
                c_code += "};\n\n"
            
            # Generate frame array
            c_code += f"// Frame array\n"
            c_code += f"static const pixel_frame_t {var_name}_frames[{var_name.upper()}_FRAME_COUNT] = {{\n"
            for frame_idx in range(len(frames)):
                c_code += f"    {{{var_name}_frame_{frame_idx}_data, {width}, {height}}},\n"
            c_code += "};\n\n"
            
            # Generate convenience structure
            c_code += f"""/**
 * @brief Pixel art data for {base_name}
 */
const pixel_art_t {var_name} = {{
    .frames = {var_name}_frames, .frame_count = {var_name.upper()}_FRAME_COUNT, .width = {width}, .height = {height}}};

"""
        else:
            # Single frame
            pixels = list(frames[0].getdata())
            array_name = f"{var_name}_pixels"
            
            c_code += f"// Pixel data array\n"
            c_code += f"static const pixel_rgb_t {array_name}[{width * height}] = {{\n"
            
            # Format pixels: 5 pixels per line (matching reference format)
            for i in range(0, len(pixels), 5):
                line_pixels = pixels[i:i+5]
                pixel_strs = [f"{{0x{r:02X}, 0x{g:02X}, 0x{b:02X}}}" for r, g, b in line_pixels]
                c_code += "    " + ", ".join(pixel_strs)
                if i + 5 < len(pixels):
                    c_code += ","
                c_code += "\n"
            
            c_code += "};\n\n"
            
            # Generate convenience structure
            c_code += f"""/**
 * @brief Pixel art frame for {base_name}
 */
const pixel_frame_t {var_name} = {{
    .pixels = {array_name},
    .width = {width},
    .height = {height}
}};

"""
        
        c_code += """#ifdef __cplusplus
}
#endif

#endif /* """ + header_guard + """ */
"""
        
        # Write header file
        with open(output_header_path, 'w', encoding='utf-8') as f:
            f.write(c_code)
        
        print(f"✓ Converted: {os.path.basename(image_path)}")
        print(f"  → Header: {os.path.basename(output_header_path)}")
        print(f"  → Image: {os.path.basename(output_image_path)}")
        print(f"  → Size: {width}x{height} pixels")
        if is_animated:
            print(f"  → Frames: {len(frames)} (original: {original_frame_count})")
            if frame_reduction > 0.0:
                reduction_pct = frame_reduction * 100
                print(f"  → Frame reduction: {reduction_pct:.1f}%")
        
        return True
        
    except Exception as e:
        print(f"✗ Error converting {image_path}: {e}")
        import traceback
        traceback.print_exc()
        return False


def main():
    parser = argparse.ArgumentParser(
        description='Pixel Art GIF/PNG to C Header Tool',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Convert all images in images/ folder with default 32x32 size
  python tool_pixel_art_gen.py

  # Convert with custom size
  python tool_pixel_art_gen.py --width 64 --height 64

  # Convert specific image
  python tool_pixel_art_gen.py --input images/1.gif --output output/

  # Use specific resampling method (box is best for downscaling pixel art)
  python tool_pixel_art_gen.py --resample box

  # Invert colors (useful for images with clear background and black strokes)
  python tool_pixel_art_gen.py --invert

  # Apply contrast adjustment (0.1 = low contrast/mono mapping, 1.0 = normal, 2.0 = high)
  python tool_pixel_art_gen.py --contrast 0.1

  # Reduce GIF frames by 50% (keeps 50% of frames, evenly sampled)
  python tool_pixel_art_gen.py --frame-reduction 0.5

Note: The tool uses improved resampling algorithms to reduce color shifts:
  - Auto mode: BOX (area averaging) for downscaling, LANCZOS for upscaling
  - BOX resampling averages all pixels in source area, reducing edge color shifts
  - Two-pass resampling for very small downscales to minimize artifacts
  - Transparent backgrounds are converted to white (not black)
  - Use --invert to invert colors before conversion
        """
    )
    
    parser.add_argument(
        '--input', '-i',
        help='Input image file or directory (default: images/)',
        default='images'
    )
    
    parser.add_argument(
        '--output', '-o',
        help='Output directory (default: output/)',
        default='output'
    )
    
    parser.add_argument(
        '--width', '-w',
        type=int,
        help='Target width in pixels (default: 32)',
        default=32
    )
    
    parser.add_argument(
        '--height', '-H',
        type=int,
        help='Target height in pixels (default: 32)',
        default=32
    )
    
    parser.add_argument(
        '--resample', '-r',
        choices=['auto', 'box', 'lanczos', 'nearest', 'bicubic'],
        default='auto',
        help='Resampling method: auto (smart selection), box (area averaging, best for downscaling), lanczos (smooth upscaling), nearest (pixel-perfect), bicubic (smooth) (default: auto)'
    )
    
    parser.add_argument(
        '--invert', '-I',
        action='store_true',
        help='Invert colors before conversion (useful for images with clear background and black strokes)'
    )
    
    parser.add_argument(
        '--contrast', '-c',
        type=float,
        default=0.1,
        help='Contrast factor: 0.0 = no contrast (grayscale midpoint), 0.1 = low contrast/mono mapping, 1.0 = normal, 2.0 = high contrast (default: 0.1)'
    )
    
    parser.add_argument(
        '--frame-reduction', '-f',
        type=float,
        default=0.0,
        help='Frame reduction percentage for animated GIFs: 0.0 = keep all frames, 0.5 = keep 50%% of frames (reduce by 50%%), 0.75 = keep 25%% of frames (reduce by 75%%). Frames are evenly sampled across the animation (default: 0.0)'
    )
    
    args = parser.parse_args()
    
    # Validate frame reduction parameter
    if args.frame_reduction < 0.0 or args.frame_reduction >= 1.0:
        print(f"Error: Frame reduction must be between 0.0 and 1.0 (exclusive), got {args.frame_reduction}")
        return 1
    
    # Ensure output directory exists
    os.makedirs(args.output, exist_ok=True)
    
    # Determine input files
    input_files = []
    
    if os.path.isfile(args.input):
        # Single file
        input_files.append(args.input)
    elif os.path.isdir(args.input):
        # Directory - find all GIF and PNG files
        for ext in ['*.gif', '*.png', '*.GIF', '*.PNG']:
            import glob
            input_files.extend(glob.glob(os.path.join(args.input, ext)))
    else:
        print(f"Error: Input path '{args.input}' does not exist")
        return 1
    
    if not input_files:
        print(f"No GIF or PNG files found in '{args.input}'")
        return 1
    
    print(f"Found {len(input_files)} image file(s)")
    print(f"Target size: {args.width}x{args.height} pixels\n")
    
    success_count = 0
    for image_path in sorted(input_files):
        # Generate output paths
        base_name = os.path.splitext(os.path.basename(image_path))[0]
        header_name = f"{base_name}.h"
        output_header_path = os.path.join(args.output, header_name)
        
        # Determine output image extension
        img_ext = '.gif' if image_path.lower().endswith('.gif') else '.png'
        output_image_path = os.path.join(args.output, f"{base_name}_resampled{img_ext}")
        
        if generate_c_header(image_path, output_header_path, output_image_path, args.width, args.height, args.resample, args.invert, args.contrast, args.frame_reduction):
            success_count += 1
    
    print(f"\n✓ Successfully converted {success_count}/{len(input_files)} file(s)")
    return 0 if success_count == len(input_files) else 1


if __name__ == "__main__":
    sys.exit(main())

