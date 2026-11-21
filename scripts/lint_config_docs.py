#!/usr/bin/env python3
import glob
import os
import re
import sys
import xml.etree.ElementTree as ET


def find_line_number(filepath, pattern):
    """Return the 1-based line number matching regex pattern, or 1 if not found."""
    regex = re.compile(pattern)
    with open(filepath, "r", encoding="utf-8") as f:
        for idx, line in enumerate(f, start=1):
            if regex.search(line):
                return idx
    return 1


def parse_xml_metadata(metadata_dir="metadata"):
    """Return {section: {'file': str, 'static': set(), 'prefixes': set()}}."""
    spec = {}
    xml_files = sorted(
        glob.glob(os.path.join(metadata_dir, "*.xml"))
        + glob.glob(os.path.join(metadata_dir, "*.xml.in"))
    )

    for xml_path in xml_files:
        tree = ET.parse(xml_path)
        root = tree.getroot()
        for plugin in root.findall("plugin"):
            section = plugin.get("name")
            if not section:
                continue
            spec.setdefault(section, {"file": xml_path, "static": set(), "prefixes": set()})

            for option in plugin.findall(".//option"):
                opt_type = option.get("type")
                opt_name = option.get("name")

                if opt_type == "dynamic-list":
                    for entry in option.findall("entry"):
                        prefix = entry.get("prefix")
                        if prefix:
                            spec[section]["prefixes"].add(prefix)
                elif opt_name:
                    spec[section]["static"].add(opt_name)
    return spec


def parse_example_ini(ini_path="wf-shell.ini.example"):
    """Return {section: [(key, line_num), ...]} including commented-out lines."""
    ini_keys = {}
    current_section = None

    section_re = re.compile(r"^\s*\[([^\]]+)\]\s*$")
    kv_re = re.compile(r"^\s*#?\s*([a-zA-Z0-9_-]+)\s*=")

    with open(ini_path, "r", encoding="utf-8") as f:
        for line_num, line in enumerate(f, start=1):
            sec_match = section_re.match(line)
            if sec_match:
                current_section = sec_match.group(1).strip()
                ini_keys.setdefault(current_section, [])
                continue

            if current_section:
                kv_match = kv_re.match(line)
                if kv_match:
                    ini_keys[current_section].append((kv_match.group(1), line_num))

    return ini_keys


def emit_error(filepath, line, title, message):
    """Print human-readable error and GitHub Actions ::error annotation."""
    print(f"  {title}: {message}")
    if os.environ.get("GITHUB_ACTIONS") == "true":
        print(f"::error file={filepath},line={line},title={title}::{message}")


def main():
    ini_path = "wf-shell.ini.example"
    spec = parse_xml_metadata("metadata")
    ini_keys = parse_example_ini(ini_path)
    has_errors = False
    summary_rows = []

    all_sections = sorted(set(spec.keys()) | set(ini_keys.keys()))

    for section in all_sections:
        if section not in spec:
            has_errors = True
            line = find_line_number(ini_path, rf"^\s*\[{re.escape(section)}\]")
            emit_error(ini_path, line, "Undocumented section", f"[{section}] has no matching metadata XML.")
            summary_rows.append((section, f"`[{section}]`", "Exists only in INI"))
            continue

        xml_file = spec[section]["file"]

        if section not in ini_keys:
            has_errors = True
            emit_error(xml_file, 1, "Missing section", f"[{section}] is defined in {xml_file} but missing from {ini_path}.")
            summary_rows.append((section, f"`[{section}]`", "Exists only in XML"))
            continue

        static_spec = spec[section]["static"]
        prefixes = spec[section]["prefixes"]
        found_items = ini_keys[section]

        matched_static = set()
        matched_prefixes = set()
        exceeding = []

        for key, line_num in found_items:
            if key in static_spec:
                matched_static.add(key)
            else:
                matching_pfx = [p for p in prefixes if key.startswith(p) and len(key) > len(p)]
                if matching_pfx:
                    best_pfx = max(matching_pfx, key=len)
                    matched_prefixes.add(best_pfx)
                else:
                    exceeding.append((key, line_num))

        missing_static = sorted(static_spec - matched_static)
        missing_prefixes = sorted(prefixes - matched_prefixes)

        if missing_static or missing_prefixes or exceeding:
            has_errors = True
            print(f"\n=== [{section}] ===")
            section_line = find_line_number(ini_path, rf"^\s*\[{re.escape(section)}\]")

            for opt in missing_static:
                xml_line = find_line_number(xml_file, rf'name="{re.escape(opt)}"')
                emit_error(
                    xml_file,
                    xml_line,
                    "Missing in INI",
                    f"[{section}] Option '{opt}' is defined in {xml_file} (line {xml_line}) but missing from {ini_path}.",
                )
                summary_rows.append((section, f"`{opt}`", f"Exists only in XML (`{xml_file}:{xml_line}`)"))

            for pfx in missing_prefixes:
                xml_line = find_line_number(xml_file, rf'prefix="{re.escape(pfx)}"')
                emit_error(
                    xml_file,
                    xml_line,
                    "Missing in INI",
                    f"[{section}] Dynamic prefix '{pfx}<name>' is defined in {xml_file} (line {xml_line}) with no example in {ini_path}.",
                )
                summary_rows.append((section, f"`{pfx}<name>`", f"Exists only in XML (`{xml_file}:{xml_line}`)"))

            for opt, ini_line in exceeding:
                emit_error(
                    ini_path,
                    ini_line,
                    "Missing in XML",
                    f"[{section}] Option '{opt}' in {ini_path} (line {ini_line}) is not defined in {xml_file}.",
                )
                summary_rows.append((section, f"`{opt}`", f"Exists only in INI (`{ini_path}:{ini_line}`)"))

    summary_path = os.environ.get("GITHUB_STEP_SUMMARY")
    if summary_path:
        with open(summary_path, "a", encoding="utf-8") as sf:
            if has_errors:
                sf.write("### ISSUES\n\n")
                sf.write("| Section | Option | Status |\n")
                sf.write("| :--- | :--- | :--- |\n")
                for sec, opt, status in summary_rows:
                    sf.write(f"| `{sec}` | {opt} | {status} |\n")
            else:
                sf.write("### Pass`.\n")

    if has_errors:
        print("\nFailed")
        sys.exit(10)



if __name__ == "__main__":
    main()
