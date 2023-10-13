#!/bin/bash

# Set the source directory path
source_dir="/mnt/cave/alice/data/2023/"

# Set the destination directory paths for .root and .html files
root_dest_dir="/path/to/remote/root/files/directory"
html_dest_dir="/path/to/remote/html/files/directory"

# Set the remote server details
remote_server="username@remote-server"

# Transfer .root files
scp -r $source_dir/**/*.root $remote_server:$root_dest_dir

# Transfer .html files
scp -r $source_dir/**/*.html $remote_server:$html_dest_dir
