import os
import re

# Files and regions to guard manually
# Strategy: For each file, find all unguarded DX8Wrapper::stats occurrences
# and wrap them, taking care of:
# 1. Already inside EXTENDED_STATS blocks
# 2. if-block vs bare assignments

def wrap_line_with_guard(line, indent=None):
    """Return (guard_open, line, guard_close)"""
    if indent is None:
        indent = re.match(r'^\s*', line).group(0)
    nl = '\r\n' if '\r\n' in line else '\n'
    return f"{indent}#if !defined(RTS_USE_BGFX){nl}", line, f"{indent}#endif{nl}"

def is_guarded_at(lines, i):
    """Check if line i is already inside a !defined(RTS_USE_BGFX) guard"""
    depth = 0
    for j in range(i - 1, -1, -1):
        ln = lines[j].strip()
        if ln.startswith('#endif'):
            depth += 1
        elif ln.startswith('#if '):
            if depth == 0:
                if '!defined(RTS_USE_BGFX)' in ln:
                    return True
                return False
            depth -= 1
    return False

def process_file(filepath):
    try:
        with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
            original = f.read()
    except Exception as e:
        print(f"  ERROR reading {filepath}: {e}")
        return False

    if 'DX8Wrapper::stats' not in original:
        return False

    lines = original.splitlines(keepends=True)
    new_lines = []
    i = 0
    modified = False

    while i < len(lines):
        line = lines[i]
        if 'DX8Wrapper::stats' in line and not line.strip().startswith('//'):
            if is_guarded_at(lines, i):
                new_lines.append(line)
                i += 1
                continue
            
            indent_m = re.match(r'^\s*', line)
            indent = indent_m.group(0) if indent_m else ''
            nl = '\r\n' if line.endswith('\r\n') else '\n'

            # Check if it's an if/else if/while
            if re.search(r'\b(if|else if|while|for)\b', line):
                # Collect the entire block
                block = [line]
                j = i + 1
                
                # Count brace depth in this line
                brace_depth = line.count('{') - line.count('}')
                has_open_brace = '{' in line
                
                if not has_open_brace and j < len(lines):
                    # The { might be on next line, or it's a single-statement if
                    next_stripped = lines[j].strip() if j < len(lines) else ''
                    if next_stripped == '{':
                        block.append(lines[j])
                        brace_depth += 1
                        has_open_brace = True
                        j += 1
                
                if has_open_brace:
                    while j < len(lines) and brace_depth > 0:
                        curr = lines[j]
                        block.append(curr)
                        brace_depth += curr.count('{') - curr.count('}')
                        j += 1
                else:
                    # Single-statement if: next line is the body
                    if j < len(lines):
                        block.append(lines[j])
                        j += 1

                modified = True
                new_lines.append(f"{indent}#if !defined(RTS_USE_BGFX){nl}")
                new_lines.extend(block)
                new_lines.append(f"{indent}#endif{nl}")
                i = j
            else:
                # Sequence of assignment statements
                block = []
                j = i
                while j < len(lines) and 'DX8Wrapper::stats' in lines[j] and not re.search(r'\b(if|else if|while|for)\b', lines[j]):
                    block.append(lines[j])
                    j += 1
                
                modified = True
                new_lines.append(f"{indent}#if !defined(RTS_USE_BGFX){nl}")
                new_lines.extend(block)
                new_lines.append(f"{indent}#endif{nl}")
                i = j
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
    exclude_files = {'dx8wrapper.cpp', 'dx8wrapper.h', 'statistics.h', 'statistics.cpp'}

    modified_count = 0
    for t_dir in target_dirs:
        abs_t_dir = os.path.join(root_dir, t_dir)
        if not os.path.exists(abs_t_dir):
            continue
        for root, dirs, files in os.walk(abs_t_dir):
            # Skip build directories and .git
            dirs[:] = [d for d in dirs if d not in ('build', '.git', 'Release', 'Debug')]
            for file in files:
                if file in exclude_files:
                    continue
                if file.endswith(('.cpp', '.h')):
                    fp = os.path.join(root, file)
                    if process_file(fp):
                        modified_count += 1
                        print(f"Modified: {os.path.relpath(fp, root_dir)}")

    print(f"\nTotal files modified: {modified_count}")


if __name__ == "__main__":
    main()
