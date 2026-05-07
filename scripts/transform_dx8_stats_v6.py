import os
import re

def process_file(filepath):
    try:
        with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read()
    except Exception:
        return False

    if 'DX8Wrapper::stats' not in content:
        return False

    # Split into guarded and unguarded parts to avoid double-wrapping
    parts = re.split(r'(#if !defined\(RTS_USE_BGFX\).*?#endif)', content, flags=re.DOTALL)
    
    modified = False
    new_parts = []
    
    for part in parts:
        if part.startswith('#if !defined(RTS_USE_BGFX)'):
            new_parts.append(part)
            continue
            
        if 'DX8Wrapper::stats' not in part:
            new_parts.append(part)
            continue
            
        modified = True
        lines = part.splitlines(keepends=True)
        part_modified_lines = []
        i = 0
        while i < len(lines):
            line = lines[i]
            if 'DX8Wrapper::stats' in line:
                indent = re.match(r'^\s*', line).group(0)
                
                # Check if it's an if/else if statement
                if re.search(r'\b(if|else if|while|for)\b', line):
                    block_lines = [line]
                    temp_i = i + 1
                    brace_depth = 0
                    has_brace = False
                    
                    if '{' in line:
                        has_brace = True
                        brace_depth += line.count('{') - line.count('}')
                    
                    if not has_brace and temp_i < len(lines) and '{' in lines[temp_i]:
                        has_brace = True
                        # We'll continue to the loop to pick it up
                    
                    if has_brace:
                        while temp_i < len(lines):
                            curr_line = lines[temp_i]
                            block_lines.append(curr_line)
                            brace_depth += curr_line.count('{')
                            brace_depth -= curr_line.count('}')
                            temp_i += 1
                            if brace_depth <= 0:
                                break
                    else:
                        # Single line statement following if
                        if temp_i < len(lines):
                            block_lines.append(lines[temp_i])
                            temp_i += 1
                    
                    part_modified_lines.append(f"{indent}#if !defined(RTS_USE_BGFX)\n")
                    part_modified_lines.extend(block_lines)
                    part_modified_lines.append(f"{indent}#endif\n")
                    i = temp_i
                else:
                    # Sequential assignments or other standalone uses
                    block_lines = []
                    temp_i = i
                    while temp_i < len(lines) and 'DX8Wrapper::stats' in lines[temp_i] and not re.search(r'\b(if|else if|while|for)\b', lines[temp_i]):
                        block_lines.append(lines[temp_i])
                        temp_i += 1
                    
                    part_modified_lines.append(f"{indent}#if !defined(RTS_USE_BGFX)\n")
                    part_modified_lines.extend(block_lines)
                    part_modified_lines.append(f"{indent}#endif\n")
                    i = temp_i
            else:
                part_modified_lines.append(line)
                i += 1
        new_parts.append("".join(part_modified_lines))
        
    if modified:
        final_content = "".join(new_parts)
        # Final cleanup: remove any double-newlines or empty guards if they were somehow created
        # (Though the logic above should prevent it)
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(final_content)
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
