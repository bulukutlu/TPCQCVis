import os
import shutil
import argparse

def walklevel(some_dir, level=1):
    some_dir = some_dir.rstrip(os.path.sep)
    assert os.path.isdir(some_dir)
    num_sep = some_dir.count(os.path.sep)
    for root, dirs, files in os.walk(some_dir):
        yield root, dirs, files
        num_sep_this = root.count(os.path.sep)
        if num_sep + level <= num_sep_this:
            del dirs[:]

def latest_modified_file(directory, file_name):
    file_path = os.path.join(directory, file_name)
    if not os.path.exists(file_path):
        return None  # File doesn't exist in the directory 
    latest_file = file_name
    latest_mtime = os.path.getmtime(file_path) 
    for filename in os.listdir(directory):
        if filename != "index.html": # Exclude index.html as irrelevant
            file_path = os.path.join(directory, filename)
            if os.path.isfile(file_path):
                mtime = os.path.getmtime(file_path)
                if mtime > latest_mtime:
                    latest_file = filename
                    latest_mtime = mtime

    if latest_file==file_name:
        return True
    else:
        print("New file "+filename+" found at "+directory)
        return False

def generate_file_list(directory, rerun = False, debug= False):
    if debug: print("Rerun:"+str(rerun))
    # Check if there is anything new since last update of file_list.txt
    latest = latest_modified_file(directory,"file_list.txt")
    if debug: print("Latest:"+str(latest))
    if rerun or not latest:
        print("Generating file list for "+directory)
        with open(directory+'/file_list.txt', 'w') as file:
            for root, dirs, files in walklevel(directory, level=0):
                for dir_name in dirs:
                    file.write(dir_name + '/\n')
                    generate_file_list(os.path.join(root, dir_name),rerun,debug)
                for file_name in files:
                    if file_name.endswith('.html') and file_name != "index.html":
                        file.write(file_name + '\n')
    else:
        for root, dirs, files in walklevel(directory, level=0):
            for dir_name in dirs:
                generate_file_list(os.path.join(root, dir_name))

def copy_index_html(root_directory, rerun = False, debug= False):
    if debug: print("Rerun:"+str(rerun))
    # Get the path of the index.html file
    index_html_path = os.path.join('/eos/project/a/alice-tpc-qc/www_resources/index.html')

    # Check if the index.html file exists
    if not os.path.isfile(index_html_path):
        print("index.html file not found in the root directory.")
        return

    # Get all subdirectories
    subdirectories = [x[0] for x in os.walk(root_directory)]

    # Iterate over subdirectories
    for subdirectory in subdirectories:
        # Get the destination path for the index.html file in the subdirectory
        destination_path = os.path.join(subdirectory, 'index.html')

        # Check if the index.html file already exists in destination
        if rerun or not os.path.isfile(destination_path):
            # Copy the index.html file to the subdirectory
            shutil.copy2(index_html_path, destination_path)

            print("Copied index.html to "+str(destination_path))

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Update the sources for tpc-qc webpage.")
    parser.add_argument("-r", "--rerun", action="store_true", help="Update everything.")
    parser.add_argument("-d", "--debug", action="store_true", help="Print debug output.")
    args = parser.parse_args()
    print("Updating server, with args:"+str(args))
    # Update server
    root_directory = '/eos/project/a/alice-tpc-qc/www/' 
    copy_index_html(root_directory, args.rerun, args.debug)
    generate_file_list(root_directory,  args.rerun, args.debug)