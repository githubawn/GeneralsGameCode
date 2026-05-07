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

    lines = content.splitlines(keepends=True)
    new_lines = []
    i = 0
    modified = False
    while i < len(lines):
        line = lines[i]
        
        # Avoid double wrapping if already guarded
        if i > 0 and '#if !defined(RTS_USE_BGFX)' in lines[i-1]:
            new_lines.append(line)
            i += 1
            continue

        if 'DX8Wrapper::stats' in line:
            modified = True
            indent = re.match(r'^\s*', line).group(0)
            newline_char = '\r\n' if line.endswith('\r\n') else '\n'
            
            # Case 1: if/else if
            match_if = re.search(r'\b(if|else if)\b', line)
            if match_if:
                new_lines.append(f"{indent}#if !defined(RTS_USE_BGFX){newline_char}")
                new_lines.append(line)
                
                # We need to find the end of the block.
                # If there's no '{' on this line or subsequent lines, it's a single-line if.
                
                # Check for '{' starting from the if
                temp_i = i
                found_brace = False
                brace_count = 0
                
                # Look for the opening brace of the if statement
                while temp_i < len(lines):
                    search_start = lines[temp_i].find('if') if temp_i == i else 0
                    current_line_part = lines[temp_i][search_start:]
                    if '{' in current_line_part:
                        found_brace = True
                        brace_count += current_line_part.count('{')
                        brace_count -= current_line_part.count('}')
                        break
                    elif ';' in current_line_part:
                        # Single line if: if (...) stmt;
                        break
                    temp_i += 1
                
                if found_brace:
                    # Move i to temp_i and then continue until brace_count is 0
                    while i < temp_i:
                        i += 1
                        # We don't append here because we are skipping lines already added or to be added
                        # Wait, this logic is getting complicated. Let's simplify.
                        pass
                    
                    i = temp_i + 1
                    while i < len(lines) and brace_count > 0:
                        new_lines.append(lines[i])
                        brace_count += lines[i].count('{')
                        brace_count -= lines[i].count('}')
                        i += 1
                else:
                    # Single line if or simple expression
                    i += 1
                
                new_lines.append(f"{indent}#endif{newline_char}")
                continue
            
            # Case 2: Block of assignments
            if '=' in line and ';' in line:
                block = []
                j = i
                while j < len(lines) and 'DX8Wrapper::stats' in lines[j] and '=' in lines[j] and ';' in lines[j] and not re.search(r'\b(if|else if|while|for)\b', lines[j]):
                    block.append(lines[j])
                    j += 1
                
                new_lines.append(f"{indent}#if !defined(RTS_USE_BGFX){newline_char}")
                new_lines.extend(block)
                new_lines.append(f"{indent}#endif{newline_char}")
                i = j
                continue
            
            # Case 3: Single line
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
