import os
import re

def process_file(filepath):
    with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
        content = f.read()

    # Pre-clean existing guards to avoid nesting issues if re-run
    # This is a bit risky but helps avoid the double-wrapping seen in W3DDisplay.cpp
    content = re.sub(r'#if !defined\(RTS_USE_BGFX\)\s*\n\s*#if !defined\(RTS_USE_BGFX\)', '#if !defined(RTS_USE_BGFX)', content)
    
    lines = content.splitlines()
    new_lines = []
    i = 0
    while i < len(lines):
        line = lines[i]
        
        # Skip if already guarded in a way we recognize
        if '#if !defined(RTS_USE_BGFX)' in line:
            new_lines.append(line)
            i += 1
            continue

        if 'DX8Wrapper::stats' in line:
            # Determine indentation
            indent = re.match(r'^\s*', line).group(0)
            
            # Case 1: if/else if with block
            if re.search(r'\b(if|else if)\b.*\{', line):
                new_lines.append(f"{indent}#if !defined(RTS_USE_BGFX)")
                new_lines.append(line)
                brace_count = line.count('{') - line.count('}')
                i += 1
                while i < len(lines) and brace_count > 0:
                    new_lines.append(lines[i])
                    brace_count += lines[i].count('{')
                    brace_count -= lines[i].count('}')
                    i += 1
                new_lines.append(f"{indent}#endif")
                continue
            
            # Case 2: Simple assignment block
            if '=' in line and ';' in line and not re.search(r'\b(if|else if|while|for)\b', line):
                block = []
                j = i
                while j < len(lines) and 'DX8Wrapper::stats' in lines[j] and '=' in lines[j] and ';' in lines[j] and not re.search(r'\b(if|else if|while|for)\b', lines[j]):
                    block.append(lines[j])
                    j += 1
                
                new_lines.append(f"{indent}#if !defined(RTS_USE_BGFX)")
                new_lines.extend(block)
                new_lines.append(f"{indent}#endif")
                i = j
                continue
            
            # Case 3: Single line usage (if no block, or return, or expression)
            new_lines.append(f"{indent}#if !defined(RTS_USE_BGFX)")
            new_lines.append(line)
            new_lines.append(f"{indent}#endif")
            i += 1
        else:
            new_lines.append(line)
            i += 1

    new_content = '\n'.join(new_lines)
    if content != new_content:
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(new_content)
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
                        print(f"Modified: {file}")
                        
    print(f"Total files modified: {modified_count}")

if __name__ == "__main__":
    main()
