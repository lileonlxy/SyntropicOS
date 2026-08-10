#!/usr/bin/env python3
import os
import sys

def filter_manifest(info_path):
    if not os.path.exists(info_path):
        print(f"Error: Coverage info file '{info_path}' not found.")
        return

    files_data = {}
    current_file = None

    with open(info_path, "r", encoding="utf-8", errors="ignore") as f:
        for line in f:
            line = line.strip()
            if line.startswith("SF:"):
                current_file = line[3:]
                if current_file.startswith("/workspace/"):
                    current_file = current_file[11:]
                files_data[current_file] = {
                    "branches_instrumented": 0,
                    "branches_covered": 0,
                }
            elif current_file and line.startswith("BRDA:"):
                parts = line[5:].split(",")
                if len(parts) >= 4:
                    files_data[current_file]["branches_instrumented"] += 1
                    taken = parts[3]
                    if taken != "-" and int(taken) > 0:
                        files_data[current_file]["branches_covered"] += 1

    # Filter out files that have 100% branch coverage (or 0 instrumented branches)
    uncovered_files = {}
    for sf, d in files_data.items():
        total = d["branches_instrumented"]
        covered = d["branches_covered"]
        if total > 0 and covered < total:
            pct = (covered / total) * 100.0
            uncovered_files[sf] = (covered, total, pct)

    # Sort files by path
    sorted_paths = sorted(uncovered_files.keys())

    # Group by subdirectory
    dirs = {}
    for path in sorted_paths:
        d = os.path.dirname(path)
        if d not in dirs:
            dirs[d] = []
        dirs[d].append(path)

    # Generate markdown content
    md = []
    md.append("# SyntropicOS Source Files with Uncovered Branches (<100% Branch Coverage)\n")
    md.append(f"Total files remaining requiring branch coverage work: **{len(sorted_paths)}**\n")
    md.append("---\n")
    md.append("## Directory Summary\n")
    md.append("| Subdirectory | Files with <100% Branch Coverage |\n")
    md.append("| :--- | :--- |\n")
    for d_name, file_list in sorted(dirs.items()):
        md.append(f"| `{d_name}` | {len(file_list)} |\n")
    md.append("\n---\n")
    md.append("## File Manifest (<100% Branch Coverage)\n")

    for d_name, file_list in sorted(dirs.items()):
        md.append(f"\n### `{d_name}`\n")
        for sf in file_list:
            covered, total, pct = uncovered_files[sf]
            basename = os.path.basename(sf)
            host_path = os.path.join("/home/cgalant/Source/SyntropicOS", sf) if not sf.startswith("/home/") else sf
            md.append(f"- [{basename}](file://{host_path}) — **{pct:.2f}%** ({covered}/{total} branches)\n")

    manifest_path = "SOURCE_FILES_MANIFEST.md"
    with open(manifest_path, "w", encoding="utf-8") as f:
        f.writelines(md)

    print(f"Updated {manifest_path}: {len(sorted_paths)} files with <100% branch coverage.")

if __name__ == "__main__":
    path = sys.argv[1] if len(sys.argv) > 1 else "build/cov/coverage_src.info"
    filter_manifest(path)
