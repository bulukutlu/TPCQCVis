import os
import shutil
import fnmatch
import argparse

def find_and_move_files(source_dir, dest_dir, pattern):
    # Create the destination directory if it doesn't exist
    if not os.path.exists(dest_dir):
        os.makedirs(dest_dir)

    # Walk through the source directory
    for root, _, files in os.walk(source_dir):
        for filename in fnmatch.filter(files, pattern):
            source_file_path = os.path.join(root, filename)

            # Calculate the relative path of the file within the source directory
            relative_path = os.path.relpath(source_file_path, source_dir)

            # Construct the corresponding destination path
            dest_file_path = os.path.join(dest_dir, relative_path)

            # Create directories if they don't exist in the destination path
            os.makedirs(os.path.dirname(dest_file_path), exist_ok=True)

            # Move the file to the destination directory
            shutil.move(source_file_path, dest_file_path)

if __name__ == "__main__":
    # Create an argument parser
    parser = argparse.ArgumentParser(description="Find and move files matching a pattern while preserving directory structure.")
    
    # Add command-line arguments
    parser.add_argument("-i", "--input", required=True, help="Source directory path")
    parser.add_argument("-o", "--output", required=True, help="Destination directory path")
    parser.add_argument("-p", "--pattern", required=True, help="File pattern (e.g., *.txt)")

    # Parse the command-line arguments
    args = parser.parse_args()

    # Check if the source directory exists
    if not os.path.exists(args.input):
        print(f"Source directory '{args.input}' does not exist.")
        exit(1)

    # Call the function to find and move files
    find_and_move_files(args.input, args.output, args.pattern)
    print(f"Files matching '{args.pattern}' moved from '{args.input}' to '{args.output}'.")
