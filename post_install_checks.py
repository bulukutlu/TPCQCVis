import os
import subprocess
import sys

GREEN = '\033[92m'
RED = '\033[91m'
RESET = '\033[0m'

def check_environment_variables():
    required_vars = ['TPCQCVIS_DIR', 'TPCQCVIS_DATA', 'TPCQCVIS_REPORT']
    all_vars_set = True
    for var in required_vars:
        if var not in os.environ:
            print(f"{RED}Error: Environment variable {var} is not set.{RESET}")
            all_vars_set = False
    return all_vars_set

def check_directories():
    required_dirs = [os.environ['TPCQCVIS_DIR'], os.environ['TPCQCVIS_DATA'], os.environ['TPCQCVIS_REPORT']]
    all_dirs_exist = True
    for d in required_dirs:
        if not os.path.isdir(d):
            print(f"{RED}Error: Directory {d} does not exist.{RESET}")
            all_dirs_exist = False
    return all_dirs_exist

def check_files():
    code_dir = os.environ['TPCQCVIS_DIR']
    required_files = ['token.json', 'credentials.json']
    all_files_exist = True
    for f in required_files:
        if not os.path.isfile(os.path.join(code_dir, f)):
            print(f"{RED}Error: File {f} does not exist in {code_dir}.{RESET}")
            all_files_exist = False
    return all_files_exist

def check_packages():
    required_packages = ['google.oauth2', 'google_auth_oauthlib', 'googleapiclient', 'requests', 'schedule', 'ROOT', 'concurrent.futures']
    all_packages_installed = True
    for pkg in required_packages:
        try:
            __import__(pkg)
        except ImportError:
            print(f"{RED}Error: Python package {pkg} is not installed.{RESET}")
            all_packages_installed = False
    return all_packages_installed

def check_alien_py():
    try:
        result = subprocess.run(["which", "alien.py"], capture_output=True, text=True)
        if result.returncode != 0:
            print(f"{RED}Error: alien.py is not available in the PATH.{RESET}")
            return False
    except Exception as e:
        print(f"{RED}Error: Unable to check for alien.py. Exception: {e}{RESET}")
        return False
    return True

def check_gpg():
    try:
        result = subprocess.run(["which", "gpg"], capture_output=True, text=True)
        if result.returncode != 0:
            print(f"{RED}Error: gpg is not available in the PATH.{RESET}")
            return False
    except Exception as e:
        print(f"{RED}Error: Unable to check for gpg. Exception: {e}{RESET}")
        return False
    return True

def check_sshpass():
    try:
        result = subprocess.run(["which", "sshpass"], capture_output=True, text=True)
        if result.returncode != 0:
            print(f"{RED}Error: sshpass is not available in the PATH.{RESET}")
            return False
    except Exception as e:
        print(f"{RED}Error: Unable to check for sshpass. Exception: {e}{RESET}")
        return False
    return True

def check_jupyter():
    try:
        result = subprocess.run(["which", "jupyter"], capture_output=True, text=True)
        if result.returncode != 0:
            print(f"{RED}Error: jupyter is not available in the PATH.{RESET}")
            return False
    except Exception as e:
        print(f"{RED}Error: Unable to check for jupyter. Exception: {e}{RESET}")
        return False
    return True

def main():
    print("Checking environment setup...")
    checks = [
        check_environment_variables,
        check_directories,
        check_files,
        check_packages,
        check_alien_py,
        check_gpg,
        check_sshpass,
        check_jupyter
    ]
    all_checks_passed = True
    for check in checks:
        if not check():
            all_checks_passed = False
    if all_checks_passed:
        print(f"{GREEN}All checks passed. Environment is correctly set up.{RESET}")
    else:
        print(f"{RED}Some checks failed. Please fix the issues above and try again.{RESET}")

if __name__ == "__main__":
    main()
