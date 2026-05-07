import os
import re
import sys

def transform_file(filepath):
    with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
        lines = f.readlines()

    new_lines = []
    i = 0
    while i < len(lines):
        line = lines[i]
        
        # Check if line contains DX8Wrapper::stats and isn't already guarded
        if 'DX8Wrapper::stats' in line and '#if !defined(RTS_USE_BGFX)' not in line:
            # We found a line to guard. Let's see if we can group consecutive lines.
            block = [line]
            j = i + 1
            while j < len(lines) and 'DX8Wrapper::stats' in lines[j] and '#if !defined(RTS_USE_BGFX)' not in lines[j]:
                block.append(lines[j])
                j += 1
            
            # Now wrap the block
            # Determine indentation from the first line
            indent = re.match(r'^\s*', block[0]).group(0)
            
            new_lines.append(f"{indent}#if !defined(RTS_USE_BGFX)\n")
            new_lines.extend(block)
            new_lines.append(f"{indent}#endif\n")
            
            i = j # Move to the end of the block
        else:
            new_lines.append(line)
            i += 1
            
    if lines != new_lines:
        with open(filepath, 'w', encoding='utf-8') as f:
            f.writelines(new_lines)
        return True
    return False

def main():
    root_dir = r'C:\code\GGC4'
    target_dirs = [
        os.path.join(root_dir, 'Generals'),
        os.path.join(root_dir, 'GeneralsMD'),
        os.path.join(root_dir, 'Core')
    ]
    
    files_modified = 0
    for target in target_dirs:
        for root, dirs, files in os.walk(target):
            if 'build' in root or '.git' in root:
                continue
            for file in files:
                if file.endswith(('.cpp', '.h')):
                    filepath = os.path.join(root, file)
                    if transform_file(filepath):
                        print(f"Modified: {filepath}")
                        files_modified += 1
                        
    print(f"Finished. Total files modified: {files_modified}")

if __name__ == "__main__":
    main()
