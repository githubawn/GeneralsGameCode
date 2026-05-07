import os
import re

def process_file(filepath):
    try:
        with open(filepath, 'r', encoding='utf-8', errors='ignore', newline='') as f:
            content = f.read()
    except Exception:
        return False

    if 'DX8Wrapper::stats' not in content:
        return False

    # Regex to match blocks starting with if/else if that contain DX8Wrapper::stats
    # Or just lines with DX8Wrapper::stats
    
    lines = content.splitlines(keepends=True)
    new_lines = []
    i = 0
    modified = False
    
    while i < len(lines):
        line = lines[i]
        
        # Check if already guarded in previous line
        if i > 0 and '#if !defined(RTS_USE_BGFX)' in lines[i-1]:
            new_lines.append(line)
            i += 1
            continue

        if 'DX8Wrapper::stats' in line:
            # We found an unguarded stats access.
            modified = True
            indent = re.match(r'^\s*', line).group(0)
            newline_char = '\r\n' if line.endswith('\r\n') else '\n'
            
            # Case 1: if/else if block
            match_if = re.search(r'\b(if|else if)\b', line)
            if match_if:
                # Find the full block
                block_lines = []
                temp_i = i
                
                # Check for opening brace
                has_brace = False
                brace_depth = 0
                
                # We'll collect lines until we have a complete statement or block
                while temp_i < len(lines):
                    curr_line = lines[temp_i]
                    block_lines.append(curr_line)
                    
                    if '{' in curr_line:
                        has_brace = True
                        brace_depth += curr_line.count('{')
                        brace_depth -= curr_line.count('}')
                        if brace_depth == 0:
                            break
                    elif ';' in curr_line and not has_brace:
                        # Single line if
                        break
                    elif has_brace:
                        brace_depth -= curr_line.count('}')
                        if brace_depth <= 0:
                            break
                    
                    temp_i += 1
                
                new_lines.append(f"{indent}#if !defined(RTS_USE_BGFX){newline_char}")
                new_lines.extend(block_lines)
                new_lines.append(f"{indent}#endif{newline_char}")
                i = temp_i + 1
                continue
            
            # Case 2: Sequence of assignments/calls
            block_lines = []
            temp_i = i
            while temp_i < len(lines) and 'DX8Wrapper::stats' in lines[temp_i] and not re.search(r'\b(if|else if|while|for)\b', lines[temp_i]):
                block_lines.append(lines[temp_i])
                temp_i += 1
            
            if block_lines:
                new_lines.append(f"{indent}#if !defined(RTS_USE_BGFX){newline_char}")
                new_lines.extend(block_lines)
                new_lines.append(f"{indent}#endif{newline_char}")
                i = temp_i
                continue
                
            # Fallback: just wrap the line
            new_lines.append(f"{indent}#if !defined(RTS_USE_BGFX){newline_char}")
            new_lines.append(line)
            new_lines.append(f"{indent}#endif{newline_char}")
            i += 1
        else:
            new_lines.append(line)
            i += 1

    if modified:
        with open(filepath, 'w', encoding='utf-8', newline='') as f:
            f.writelines(new_lines)
        return True
    return False

def main():
    root_dir = r'C:\code\GGC4'
    target_dirs = ['Generals', 'GeneralsMD', 'Core']
    
    modified_count = 0
    for t_dir in target_dirs:
        abs_t_dir = os.path.join(root_dir, t_dir)
        if not os.path.exists(abs_t_dir):
            continue
        for root, dirs, files in os.walk(abs_t_dir):
            if 'build' in root or '.git' in root:
                continue
            for file in files:
                if file.endswith(('.cpp', '.h')):
                    if process_file(os.path.join(root, file)):
                        modified_count += 1
                        print(f"Modified: {os.path.relpath(os.path.join(root, file), root_dir)}")
                        
    print(f"Total files modified: {modified_count}")

if __name__ == "__main__":
    main()
