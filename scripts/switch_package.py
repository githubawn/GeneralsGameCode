# scripts/switch_package.py
# Automates the packaging of Command & Conquer Generals Zero Hour for Nintendo Switch.
# Extracts files from Steam installation, builds RomFS, and runs elf2nro.

import os
import sys
import shutil
import subprocess
import configparser

# Local machine paths (Steam install, devkitPro root) are read from an ini file
# that is kept out of git so this script can be committed without leaking personal
# folder layouts. Copy scripts/switch_package.ini.example -> scripts/switch_package.ini
# and edit it. Environment variables (STEAM_ZH_DIR, DEVKITPRO) override the ini.
INI_NAME = "switch_package.ini"
EXAMPLE_NAME = "switch_package.ini.example"

def load_config():
    ini_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), INI_NAME)
    cfg = configparser.ConfigParser()
    if os.path.exists(ini_path):
        cfg.read(ini_path)
    paths = cfg["Paths"] if cfg.has_section("Paths") else {}

    def resolve(key):
        # ini value takes precedence, then environment variable of the same name.
        raw = paths.get(key, "").strip() if hasattr(paths, "get") else ""
        if not raw:
            raw = os.environ.get(key, "").strip()
        return os.path.expandvars(os.path.expanduser(raw)) if raw else None

    return {
        "steam_dir": resolve("STEAM_ZH_DIR"),
        "devkitpro": resolve("DEVKITPRO"),
        "ini_path": ini_path,
    }

def main():
    # Paths configuration
    workspace_root = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
    build_dir = os.path.join(workspace_root, "build", "switch-generalsmd-sdl3-bgfx")
    elf_path = os.path.join(build_dir, "GeneralsMD", "Release", "generalszh")

    config = load_config()
    steam_dir = config["steam_dir"]
    romfs_dir = os.path.join(build_dir, "romfs")
    output_nro = os.path.join(build_dir, "generalszh.nro")

    print("--- Nintendo Switch Homebrew Packaging Script ---")

    # 0. Verify local config is present
    if not steam_dir:
        example_path = os.path.join(os.path.dirname(config["ini_path"]), EXAMPLE_NAME)
        print(f"Error: STEAM_ZH_DIR not configured.")
        print(f"Copy {example_path} to {config['ini_path']} and set your paths,")
        print("or set the STEAM_ZH_DIR environment variable.")
        sys.exit(1)

    # 1. Verify z_generals.elf exists
    if not os.path.exists(elf_path):
        print(f"Error: Target ELF not found at {elf_path}")
        print("Please build the switch-generalsmd-sdl3-bgfx preset first.")
        sys.exit(1)

    # 2. Verify Steam directory exists
    if not os.path.exists(steam_dir):
        print(f"Error: Steam installation directory not found at {steam_dir}")
        print(f"Please ensure the game is installed or update STEAM_ZH_DIR in {config['ini_path']}.")
        sys.exit(1)
        
    # 3. Create or clear RomFS directory
    print(f"Preparing RomFS directory: {romfs_dir}")
    if os.path.exists(romfs_dir):
        shutil.rmtree(romfs_dir)
    os.makedirs(romfs_dir, exist_ok=True)
    
    # 4. Copy game files (excluding exes and dlls)
    print(f"Copying game assets from {steam_dir}...")
    ignore_extensions = ['.exe', '.dll', '.msi', '.cab', '.bat', '.pdb']
    
    copied_count = 0
    for root, dirs, files in os.walk(steam_dir):
        # Calculate relative path
        rel_path = os.path.relpath(root, steam_dir)
        target_root = romfs_dir if rel_path == "." else os.path.join(romfs_dir, rel_path)
        
        # Ensure target subdirectories exist
        os.makedirs(target_root, exist_ok=True)
        
        for file in files:
            ext = os.path.splitext(file)[1].lower()
            if ext in ignore_extensions:
                continue
                
            src_file = os.path.join(root, file)
            dst_file = os.path.join(target_root, file)
            
            # Copy file
            shutil.copy2(src_file, dst_file)
            copied_count += 1
            
    print(f"Successfully copied {copied_count} asset files to RomFS.")
    
    # 5. Locate build_romfs and elf2nro
    devkitpro_path = config["devkitpro"] or ""
    build_romfs_path = os.path.join(devkitpro_path, "tools", "bin", "build_romfs.exe")
    elf2nro_path = os.path.join(devkitpro_path, "tools", "bin", "elf2nro.exe")
    
    if not os.path.exists(build_romfs_path):
        build_romfs_path = shutil.which("build_romfs")
    if not os.path.exists(elf2nro_path):
        elf2nro_path = shutil.which("elf2nro")
        
    if not build_romfs_path or not elf2nro_path:
        print("Error: build_romfs or elf2nro tool not found!")
        print("Please ensure devkitPro tools are installed and on your PATH or DEVKITPRO is set.")
        sys.exit(1)
        
    # 6. Build RomFS binary image
    romfs_bin = os.path.join(build_dir, "romfs.bin")
    print(f"Building RomFS binary image: {romfs_bin} ...")
    build_romfs_cmd = [build_romfs_path, romfs_dir, romfs_bin]
    try:
        subprocess.run(build_romfs_cmd, check=True)
    except subprocess.CalledProcessError as e:
        print("Error: build_romfs failed!")
        sys.exit(1)
        
    # 7. Execute elf2nro to create base NRO (without romfs)
    print(f"Packaging ELF into base NRO: {output_nro}")
    cmd = [
        elf2nro_path,
        elf_path,
        output_nro
    ]
    
    print(f"Running command: {' '.join(cmd)}")
    try:
        subprocess.run(cmd, check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    except subprocess.CalledProcessError as e:
        print("Error: base elf2nro failed!")
        print(e.stderr.decode('utf-8'))
        sys.exit(1)
        
    # 8. Append RomFS using custom Python logic to bypass Windows 2GB limits
    print("Embedding RomFS into NRO using 64-bit Python streaming...")
    import struct
    try:
        romfs_size = os.path.getsize(romfs_bin)
        # Construct the 56-byte ASET header
        # magic: "ASET" (0x54455341)
        # version: 0
        # icon: offset=0, size=0
        # nacp: offset=0, size=0
        # romfs: offset=56, size=romfs_size
        aset_header = struct.pack('<IIQQQQQQ', 0x54455341, 0, 0, 0, 0, 0, 56, romfs_size)
        
        # Append to the NRO file
        with open(output_nro, 'ab') as nro_file:
            nro_file.write(aset_header)
            # Stream romfs.bin to avoid loading the entire 3GB file in memory at once
            with open(romfs_bin, 'rb') as rfs_file:
                while True:
                    chunk = rfs_file.read(64 * 1024 * 1024) # 64MB chunks
                    if not chunk:
                        break
                    nro_file.write(chunk)
                    
        print(f"Success! Nintendo Switch Homebrew package with embedded assets created: {output_nro}")
        print("You can run this file directly in the Ryujinx or Yuzu emulator, or copy it to your Switch SD card.")
    except Exception as e:
        print(f"Error appending RomFS: {e}")
        sys.exit(1)
    finally:
        # Clean up temporary romfs.bin to save space
        if os.path.exists(romfs_bin):
            try:
                os.remove(romfs_bin)
            except Exception:
                pass

if __name__ == "__main__":
    main()
