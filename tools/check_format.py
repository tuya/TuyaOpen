#!/usr/bin/env python3
"""
Code format checker script
Used to check if modified C/C++ files in PR comply with clang-format standards,
detect Chinese characters in code, and validate file headers

Support modes:
- PR mode (default): Check files modified in PR relative to base branch
- Debug mode: Check specified local files or directories for development
"""

import os
import sys
import subprocess
import tempfile
import argparse
import re
from pathlib import Path
from datetime import datetime
import glob


class FormatChecker:
    def __init__(self, base_ref="master", debug_mode=False):
        self.base_ref = base_ref
        self.debug_mode = debug_mode
        self.project_root = self._find_project_root()
        self.clang_format_ignore = self._load_ignore_patterns()
        self.current_year = datetime.now().year
        
    def _find_project_root(self):
        """Find project root directory"""
        current = Path.cwd()
        while current != current.parent:
            if (current / ".clang-format").exists():
                return current
            current = current.parent
        return Path.cwd()
    
    def _load_ignore_patterns(self):
        """Load ignore patterns from .clang-format-ignore file"""
        ignore_file = self.project_root / ".clang-format-ignore"
        patterns = []
        if ignore_file.exists():
            with open(ignore_file, 'r') as f:
                for line in f:
                    line = line.strip()
                    if line and not line.startswith('#'):
                        patterns.append(line)
        return patterns
    
    def _should_ignore_file(self, file_path):
        """Check if file should be ignored"""
        rel_path = str(file_path)
        for pattern in self.clang_format_ignore:
            if rel_path.startswith(pattern):
                return True
        return False
    
    def _get_changed_files(self):
        """Get modified C/C++ files relative to base branch"""
        try:
            # Get modified files
            result = subprocess.run([
                'git', 'diff', '--name-only', '--diff-filter=AM', 
                f'{self.base_ref}...HEAD'
            ], capture_output=True, text=True, cwd=self.project_root)
            
            if result.returncode != 0:
                print(f"Error: Unable to get git diff: {result.stderr}")
                return []
            
            files = []
            for line in result.stdout.strip().split('\n'):
                if not line:
                    continue
                    
                # Only process C/C++ files
                if line.endswith(('.c', '.cpp', '.h', '.hpp', '.cc', '.cxx')):
                    file_path = self.project_root / line
                    if file_path.exists() and not self._should_ignore_file(line):
                        files.append(line)
            
            return files
            
        except subprocess.CalledProcessError as e:
            print(f"Error: Failed to execute git command: {e}")
            return []
    
    def _get_local_files(self, file_patterns=None, directories=None):
        """Get local C/C++ files for debug mode"""
        files = []
        c_cpp_extensions = ('.c', '.cpp', '.h', '.hpp', '.cc', '.cxx')
        
        # Process specified files
        if file_patterns:
            for pattern in file_patterns:
                # Support glob patterns
                matched_files = glob.glob(pattern, recursive=True)
                for file_path in matched_files:
                    path_obj = Path(file_path)
                    if path_obj.is_file() and path_obj.suffix in c_cpp_extensions:
                        rel_path = path_obj.relative_to(self.project_root) if path_obj.is_absolute() else path_obj
                        if not self._should_ignore_file(str(rel_path)):
                            files.append(str(rel_path))
        
        # Process specified directories
        if directories:
            for directory in directories:
                dir_path = Path(directory)
                if not dir_path.exists():
                    print(f"Warning: Directory does not exist: {directory}")
                    continue
                
                # Find all C/C++ files in directory recursively
                for ext in c_cpp_extensions:
                    pattern = f"**/*{ext}"
                    for file_path in dir_path.glob(pattern):
                        rel_path = file_path.relative_to(self.project_root) if file_path.is_absolute() else file_path
                        if not self._should_ignore_file(str(rel_path)):
                            files.append(str(rel_path))
        
        # If no files or directories specified, scan current directory
        if not file_patterns and not directories:
            current_dir = Path.cwd()
            for ext in c_cpp_extensions:
                pattern = f"**/*{ext}"
                for file_path in current_dir.glob(pattern):
                    rel_path = file_path.relative_to(self.project_root) if file_path.is_absolute() else file_path
                    if not self._should_ignore_file(str(rel_path)):
                        files.append(str(rel_path))
        
        # Remove duplicates and return sorted list
        return sorted(list(set(files)))
    
    def _check_clang_format_available(self):
        """Check if clang-format is available"""
        try:
            result = subprocess.run(['clang-format', '--version'], 
                                  capture_output=True, text=True)
            return result.returncode == 0
        except FileNotFoundError:
            return False
    
    def _format_file_content(self, file_path):
        """Format file content using clang-format"""
        try:
            with open(file_path, 'r', encoding='utf-8') as f:
                original_content = f.read()
            
            result = subprocess.run([
                'clang-format', '-style=file', str(file_path)
            ], capture_output=True, text=True, cwd=self.project_root)
            
            if result.returncode != 0:
                print(f"Warning: clang-format failed to process file {file_path}: {result.stderr}")
                return original_content, original_content
            
            return original_content, result.stdout
            
        except Exception as e:
            print(f"Error: Failed to read file {file_path}: {e}")
            return "", ""
    
    def _show_diff(self, file_path, original, formatted):
        """Show formatting differences for file"""
        import difflib
        
        original_lines = original.splitlines(keepends=True)
        formatted_lines = formatted.splitlines(keepends=True)
        
        diff = difflib.unified_diff(
            original_lines, formatted_lines,
            fromfile=f"a/{file_path}", tofile=f"b/{file_path}",
            lineterm=''
        )
        
        diff_content = ''.join(diff)
        if diff_content:
            print(f"❌ File {file_path} does not conform to format standards")
            return True
        return False
    
    def _check_chinese_characters(self, file_path):
        """Check if file contains Chinese characters"""
        try:
            with open(file_path, 'r', encoding='utf-8') as f:
                content = f.read()
            
            # Chinese character pattern (covers most common Chinese characters)
            chinese_pattern = re.compile(r'[\u4e00-\u9fff\u3400-\u4dbf\uf900-\ufaff\u3000-\u303f]')
            
            chinese_errors = []
            lines = content.splitlines()
            
            for line_num, line in enumerate(lines, 1):
                matches = chinese_pattern.finditer(line)
                for match in matches:
                    chinese_errors.append({
                        'line': line_num,
                        'column': match.start() + 1,
                        'character': match.group(),
                        'context': line.strip()
                    })
            
            return chinese_errors
            
        except Exception as e:
            print(f"Error: Failed to read file {file_path} for Chinese check: {e}")
            return []
    
    def _show_chinese_errors(self, file_path, chinese_errors):
        """Show Chinese character errors for file"""
        if chinese_errors:
            print(f"❌ File {file_path} contains Chinese characters")
            return True
        return False
    
    def _check_file_header(self, file_path):
        """Check if file has proper header format"""
        try:
            with open(file_path, 'r', encoding='utf-8') as f:
                content = f.read()
            
            lines = content.splitlines()
            header_errors = []
            header_warnings = []
            
            # Check if file starts with /** comment block
            if not lines or not lines[0].strip().startswith('/**'):
                header_errors.append("File must start with /** comment block")
                return header_errors, header_warnings
            
            # Find the end of the header comment block
            header_end = -1
            for i, line in enumerate(lines):
                if line.strip().endswith('*/'):
                    header_end = i
                    break
            
            if header_end == -1:
                header_errors.append("Header comment block must end with */")
                return header_errors, header_warnings
            
            header_lines = lines[:header_end + 1]
            header_content = '\n'.join(header_lines)
            
            # Check for required @file tag
            if not re.search(r'@file\s+\S+', header_content):
                header_errors.append("Missing @file tag in header")
            
            # Check for required @brief tag
            if not re.search(r'@brief\s+.+', header_content):
                header_errors.append("Missing @brief tag in header")
            
            # Check for @copyright tag with current year
            copyright_pattern = r'@copyright\s+Copyright\s*\([^)]*\)\s*(\d{4})-?(\d{4})?\s+.*All Rights Reserved'
            copyright_match = re.search(copyright_pattern, header_content)
            
            if not copyright_match:
                header_errors.append("Missing or invalid @copyright tag format")
            else:
                start_year = int(copyright_match.group(1))
                end_year = copyright_match.group(2)
                
                if end_year:
                    end_year = int(end_year)
                    if end_year != self.current_year:
                        header_errors.append(f"Copyright end year should be {self.current_year}, found {end_year}")
                else:
                    if start_year != self.current_year:
                        header_errors.append(f"Copyright year should include current year {self.current_year}")
            

            
            return header_errors, header_warnings
            
        except Exception as e:
            print(f"Error: Failed to read file {file_path} for header check: {e}")
            return [f"Failed to read file for header check: {e}"], []
    
    def _show_header_errors(self, file_path, header_errors):
        """Show file header errors"""
        if header_errors:
            print(f"❌ File {file_path} has invalid header format")
            return True
        return False
    
    def _show_header_warnings(self, file_path, header_warnings):
        """Show file header warnings"""
        if header_warnings:
            print(f"⚠️  File {file_path} has header suggestions")
            return True
        return False
    
    def check_format(self, file_patterns=None, directories=None):
        """Check code format, Chinese characters, and file headers"""
        if not self._check_clang_format_available():
            print("Error: clang-format is not installed or not in PATH")
            print("Please install clang-format (recommended version 14)")
            return False
        
        if self.debug_mode:
            print("🔧 Debug mode: Checking local files...")
            changed_files = self._get_local_files(file_patterns, directories)
            if not changed_files:
                print("❌ No C/C++ files found matching the criteria")
                return True
        else:
            print(f"📋 PR mode: Checking files modified relative to {self.base_ref} branch...")
            changed_files = self._get_changed_files()
            if not changed_files:
                print("✅ No C/C++ files need to be checked")
                return True
        
        print(f"📁 Found {len(changed_files)} C/C++ file(s) to check:")
        for file_path in changed_files:
            print(f"  - {file_path}")
        
        format_errors = []
        chinese_errors = []
        header_errors = []
        header_warnings = []
        
        for file_path in changed_files:
            full_path = self.project_root / file_path
            original, formatted = self._format_file_content(full_path)
            
            if original != formatted:
                if self._show_diff(file_path, original, formatted):
                    format_errors.append(file_path)
        
        for file_path in changed_files:
            full_path = self.project_root / file_path
            chinese_issues = self._check_chinese_characters(full_path)
            
            if chinese_issues:
                if self._show_chinese_errors(file_path, chinese_issues):
                    chinese_errors.append(file_path)
        
        for file_path in changed_files:
            full_path = self.project_root / file_path
            errors, warnings = self._check_file_header(full_path)
            
            if errors:
                if self._show_header_errors(file_path, errors):
                    header_errors.append(file_path)
            elif warnings:
                if self._show_header_warnings(file_path, warnings):
                    header_warnings.append(file_path)
        
        # Summary - only errors cause failure, warnings are just suggestions
        has_errors = format_errors or chinese_errors or header_errors
        
        if format_errors:
            print(f"\n❌ Found {len(format_errors)} file(s) that do not conform to format:")
            for file_path in format_errors:
                print(f"  - {file_path}")
        
        if chinese_errors:
            print(f"\n❌ Found {len(chinese_errors)} file(s) containing Chinese characters:")
            for file_path in chinese_errors:
                print(f"  - {file_path}")
        
        if header_errors:
            print(f"\n❌ Found {len(header_errors)} file(s) with header issues:")
            for file_path in header_errors:
                print(f"  - {file_path}")
        
        if header_warnings:
            print(f"\n⚠️  Found {len(header_warnings)} file(s) with header suggestions:")
            for file_path in header_warnings:
                print(f"  - {file_path}")
        
        if not has_errors and not header_warnings:
            print("\n✅ All files conform to format standards, contain no Chinese characters, and have proper headers!")
        elif not has_errors:
            print("\n✅ All files pass required checks! (Some suggestions above)")
        
        return not has_errors


def main():
    parser = argparse.ArgumentParser(
        description="Check if code format complies with clang-format standards, detect Chinese characters, and validate file headers",
        formatter_class=argparse.RawTextHelpFormatter
    )
    
    # Mode selection
    parser.add_argument("--debug", "--local", action="store_true",
                       help="Enable debug mode to check local specified files instead of PR files")
    
    # PR mode options
    parser.add_argument("--base", default="master", 
                       help="Base branch name (default: master)")
    
    # Debug mode options
    parser.add_argument("--files", nargs="+", metavar="FILE",
                       help="Debug mode: Specify files to check (supports wildcards)")
    parser.add_argument("--dir", "--directories", nargs="+", metavar="DIR",
                       help="Debug mode: Specify directories to check (recursive)")
    
    # General options
    parser.add_argument("--verbose", "-v", action="store_true",
                       help="Show verbose information")
    
    args = parser.parse_args()
    
    # Validation
    if args.debug and (args.files or args.dir):
        pass  # Valid: debug mode with specified files/dirs
    elif args.debug:
        pass  # Valid: debug mode scanning current directory
    elif not args.debug and (args.files or args.dir):
        print("Error: --files and --dir arguments can only be used in debug mode (--debug)")
        sys.exit(1)
    
    if args.verbose:
        print(f"Project root: {Path.cwd()}")
        if args.debug:
            print("Run mode: Debug mode")
            if args.files:
                print(f"Specified files: {args.files}")
            if args.dir:
                print(f"Specified directories: {args.dir}")
        else:
            print("Run mode: PR check mode")
            print(f"Base branch: {args.base}")
        print(f"Current year: {datetime.now().year}")
    
    checker = FormatChecker(base_ref=args.base, debug_mode=args.debug)
    
    if args.debug:
        success = checker.check_format(file_patterns=args.files, directories=args.dir)
    else:
        success = checker.check_format()
    
    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main() 