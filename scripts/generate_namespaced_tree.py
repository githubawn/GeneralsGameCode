import os
import sys
import argparse
import shutil

def find_header_guard_lines(lines):
    # Search for any #if/#ifdef/#ifndef near the top
    ifndef_idx = -1
    for i in range(min(50, len(lines))):
        stripped = lines[i].strip()
        if stripped.startswith("#ifndef") or stripped.startswith("#ifdef") or stripped.startswith("#if"):
            ifndef_idx = i
            break
            
    if ifndef_idx == -1:
        return None
        
    # Search for corresponding #endif near the bottom
    endif_idx = -1
    for i in range(len(lines) - 1, max(0, len(lines) - 10), -1):
        stripped = lines[i].strip()
        if stripped.startswith("#endif"):
            endif_idx = i
            break
            
    if endif_idx == -1:
        return None
        
    # Verify it is followed by a #define within 4 lines (header guard property)
    has_define = False
    for i in range(ifndef_idx + 1, min(ifndef_idx + 5, len(lines))):
        if lines[i].strip().startswith("#define"):
            has_define = True
            break
    if not has_define:
        return None
        
    # Verify that endif_idx is indeed the matching endif for ifndef_idx
    depth = 1
    in_preprocessor = False
    for i in range(ifndef_idx + 1, endif_idx):
        line = lines[i]
        stripped = line.strip()
        if not stripped:
            continue
            
        if in_preprocessor:
            if not stripped.endswith("\\"):
                in_preprocessor = False
            continue
            
        if stripped.startswith("#"):
            in_preprocessor = True
            if stripped.startswith("#if") or stripped.startswith("#ifdef") or stripped.startswith("#ifndef"):
                depth += 1
            elif stripped.startswith("#endif"):
                depth = max(0, depth - 1)
                
            if depth <= 0:
                # The block closed before the end of the file! Not a file-wide block.
                return None
                
            if not stripped.endswith("\\"):
                in_preprocessor = False
            continue
            
    if depth == 1:
        return ifndef_idx, endif_idx
    return None

def find_insertion_index(lines):
    guard_info = find_header_guard_lines(lines)
    guard_ifndef_idx = guard_info[0] if guard_info else -1
    guard_endif_idx = guard_info[1] if guard_info else -1

    # 1. Find the first C++ declaration line at depth 0
    first_decl_idx = len(lines)
    first_any_decl_idx = len(lines)
    in_block_comment = False
    in_preprocessor = False
    if_depth = 0
    for i, line in enumerate(lines):
        stripped = line.strip()
        if not stripped:
            continue
            
        if in_block_comment:
            if "*/" in stripped:
                parts = stripped.split("*/", 1)
                if len(parts) > 1 and parts[1].strip():
                    after = parts[1].strip()
                    if not after.startswith("//") and not after.startswith("#") and not after.startswith("/*"):
                        if first_any_decl_idx == len(lines):
                            first_any_decl_idx = i
                        if if_depth == 0:
                            first_decl_idx = i
                            break
                in_block_comment = False
            continue
            
        if stripped.startswith("/*"):
            if "*/" not in stripped:
                in_block_comment = True
            continue
            
        # Preprocessor check (including backslash line continuations)
        if stripped.startswith("#"):
            in_preprocessor = True
            
            # Track depth
            if i != guard_ifndef_idx and i != guard_endif_idx:
                if stripped.startswith("#if") or stripped.startswith("#ifdef") or stripped.startswith("#ifndef"):
                    if_depth += 1
                elif stripped.startswith("#endif"):
                    if_depth = max(0, if_depth - 1)
                    
            if not stripped.endswith("\\"):
                in_preprocessor = False
            continue
            
        if in_preprocessor:
            if not stripped.endswith("\\"):
                in_preprocessor = False
            continue
            
        if stripped.startswith("//"):
            continue
            
        # We found a declaration!
        if first_any_decl_idx == len(lines):
            first_any_decl_idx = i
        first_decl_idx = i
        break
        
    # 2. Find the last include before the first C++ declaration (that is at #if depth == 0)
    if first_decl_idx >= len(lines):
        first_decl_idx = first_any_decl_idx

    last_include_idx = -1
    if_depth = 0
    in_preprocessor_depth = False
    in_block_comment = False
    for i in range(first_decl_idx):
        line = lines[i]
        stripped = line.strip()
        if not stripped:
            continue
            
        if in_block_comment:
            if "*/" in stripped:
                in_block_comment = False
            continue
            
        if stripped.startswith("/*"):
            if "*/" not in stripped:
                in_block_comment = True
            continue
            
        if stripped.startswith("//"):
            continue
            
        if in_preprocessor_depth:
            if not stripped.endswith("\\"):
                in_preprocessor_depth = False
            continue
            
        if stripped.startswith("#"):
            in_preprocessor_depth = True
            
            # Check for include at depth == 0 (ignoring .inl inline files)
            if stripped.startswith("#include") and ".inl" not in line and if_depth == 0:
                last_include_idx = i
                
            # Track conditional block depth (ignoring header guard)
            if i != guard_ifndef_idx and i != guard_endif_idx:
                if stripped.startswith("#if") or stripped.startswith("#ifdef") or stripped.startswith("#ifndef"):
                    if_depth += 1
                elif stripped.startswith("#endif"):
                    if_depth = max(0, if_depth - 1)
                
            if not stripped.endswith("\\"):
                in_preprocessor_depth = False
            continue
            
    if first_decl_idx >= len(lines):
        # No C++ declarations found in the file! Do not wrap in namespace.
        return -1
        
    insert_idx = min(first_decl_idx, len(lines))
    if last_include_idx != -1:
        insert_idx = last_include_idx + 1
        
    # Trace depth at insert_idx
    if_depth = 0
    in_preprocessor_depth = False
    for i in range(insert_idx):
        line = lines[i]
        stripped = line.strip()
        if not stripped:
            continue
        if in_preprocessor_depth:
            if not stripped.endswith("\\"):
                in_preprocessor_depth = False
            continue
        if stripped.startswith("#"):
            in_preprocessor_depth = True
            if i != guard_ifndef_idx and i != guard_endif_idx:
                if stripped.startswith("#if") or stripped.startswith("#ifdef") or stripped.startswith("#ifndef"):
                    if_depth += 1
                elif stripped.startswith("#endif"):
                    if_depth = max(0, if_depth - 1)
            if not stripped.endswith("\\"):
                in_preprocessor_depth = False
            continue
            
    # If insert_idx is inside a conditional block, move it back before the outermost conditional block
    if if_depth > 0:
        curr_depth = if_depth
        for i in range(insert_idx - 1, -1, -1):
            line = lines[i]
            stripped = line.strip()
            if not stripped:
                continue
            if stripped.startswith("#"):
                if i != guard_ifndef_idx and i != guard_endif_idx:
                    if stripped.startswith("#if") or stripped.startswith("#ifdef") or stripped.startswith("#ifndef"):
                        curr_depth -= 1
                        if curr_depth == 0:
                            insert_idx = i
                            break
                    elif stripped.startswith("#endif"):
                        curr_depth += 1
                        
    return insert_idx

def find_closing_index(lines, is_header=True):
    # Check if the file has #pragma once near the top
    has_pragma_once = False
    for line in lines[:50]:
        if "#pragma once" in line:
            has_pragma_once = True
            break
            
    # If the file has a #pragma once, or if it is a source file (not a header), we can close the namespace at the very end
    if has_pragma_once or not is_header:
        return len(lines)
        
    # Otherwise, close before trailing #endif lines
    for i in range(len(lines) - 1, -1, -1):
        line = lines[i]
        stripped = line.strip()
        if not stripped:
            continue
        if stripped.startswith("#endif"):
            return i
        else:
            break
    return len(lines)

def process_file(src_path, dest_path, ns_name):
    # Create destination directories if they do not exist
    os.makedirs(os.path.dirname(dest_path), exist_ok=True)
    
    _, ext = os.path.splitext(src_path.lower())
    basename = os.path.basename(src_path).lower()
    
    if basename in ['stlutils.h', 'systemallocator.h']:
        try:
            shutil.copy2(src_path, dest_path)
        except Exception as e:
            print(f"Error copying {src_path} to {dest_path}: {e}")
        return
        
    if ext in ['.cpp', '.h', '.hpp', '.hxx']:
        try:
            with open(src_path, 'r', encoding='utf-8', errors='ignore') as f:
                lines = f.readlines()
        except Exception as e:
            print(f"Error reading {src_path}: {e}")
            return
            
        # Rename WinMain to RunEngine in WinMain.cpp
        if os.path.basename(src_path).lower() == 'winmain.cpp':
            content = "".join(lines)
            content = content.replace("Int APIENTRY WinMain(", "Int RunEngine(")
            lines = content.splitlines(keepends=True)
            
        is_header = ext in ['.h', '.hpp', '.hxx']
        insert_idx = find_insertion_index(lines)
        close_idx = find_closing_index(lines, is_header)
        
        # Convert path to use forward slashes for compiler compatibility
        abs_src_path = os.path.abspath(src_path).replace('\\', '/')
        
        new_lines = []
        if insert_idx == -1:
            new_lines = lines
        else:
            # 1. Write everything up to the insertion point
            new_lines.extend(lines[:insert_idx])
            
            # 2. Open namespace and add #line directive
            new_lines.append(f"\nnamespace {ns_name} {{\n")
            new_lines.append(f'#line {insert_idx + 1} "{abs_src_path}"\n')
            
            # 3. Write lines inside the namespace, escaping global operator new/delete and system headers
            in_namespace = True
            escaped_block = False
            for offset, line in enumerate(lines[insert_idx:close_idx]):
                stripped = line.strip()
                if "NAMESPACE_ESCAPE_START" in stripped:
                    escaped_block = True
                
                is_comment = stripped.startswith("//") or stripped.startswith("/*")
                should_escape = escaped_block or (not is_comment and (
                    ("__cdecl operator new" in line or "__cdecl operator delete" in line) and "inline" not in line or 
                    (("operator new" in line or "operator delete" in line) and "void" in line and "::operator" not in line and (line and not line[0].isspace()) and "inline" not in line and "class" not in line and "struct" not in line and "friend" not in line) or 
                    (stripped.startswith("#include") and ".inl" not in line)
                ))
                
                if "NAMESPACE_ESCAPE_END" in stripped:
                    escaped_block = False
                
                if should_escape and in_namespace:
                    new_lines.append(f"\n}} // namespace {ns_name}\n")
                    in_namespace = False
                elif not should_escape and not in_namespace:
                    new_lines.append(f"\nnamespace {ns_name} {{\n")
                    # Add a new line directive to keep compiler warning lines aligned
                    current_orig_line = insert_idx + offset + 1
                    new_lines.append(f'#line {current_orig_line} "{abs_src_path}"\n')
                    in_namespace = True
                    
                new_lines.append(line)
                
            # 4. Close namespace if it was left open
            if in_namespace:
                new_lines.append(f"\n}} // namespace {ns_name}\n")
            
            # 5. Write remaining trailing lines (like #endif)
            new_lines.extend(lines[close_idx:])
        
        try:
            with open(dest_path, 'w', encoding='utf-8') as f:
                f.writelines(new_lines)
        except Exception as e:
            print(f"Error writing to {dest_path}: {e}")
    else:
        # Non-source files: copy as-is
        try:
            shutil.copy2(src_path, dest_path)
        except Exception as e:
            print(f"Error copying {src_path} to {dest_path}: {e}")

def main():
    parser = argparse.ArgumentParser(description="Namespaced Source Code Tree Generator")
    parser.add_argument("--src-dir", required=True, help="Source directory")
    parser.add_argument("--dest-dir", required=True, help="Destination directory")
    parser.add_argument("--namespace", required=True, help="Namespace name (e.g. Gen or ZH)")
    args = parser.parse_args()
    
    src_dir = os.path.abspath(args.src_dir)
    dest_dir = os.path.abspath(args.dest_dir)
    ns_name = args.namespace
    
    print(f"Generating namespaced tree for {ns_name} in {dest_dir}...")
    
    for root, _, files in os.walk(src_dir):
        for file in files:
            src_file_path = os.path.join(root, file)
            rel_path = os.path.relpath(src_file_path, src_dir)
            dest_file_path = os.path.join(dest_dir, rel_path)
            process_file(src_file_path, dest_file_path, ns_name)
            
    print("Generation complete.")

if __name__ == "__main__":
    main()
