#!/usr/bin/env python3
"""Generate message_ids.h by compiling all .msg files and combining them.

This wraps the existing decomp build tools:
  - tools/build/msg/parse_compile.py  (compiles .msg -> .msgpack)
  - tools/build/msg/combine.py        (combines .msgpack files -> .bin + .h)

The .msg files are compiled in sorted filename order (matching the build.ninja
ordering), then combined into message_ids.h.

Usage:
    python tools/pc/gen_message_ids.py
"""
import os
import sys
import subprocess
import glob


def main():
    project_root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    os.chdir(project_root)

    msg_dir = os.path.join("assets", "us", "msg")
    build_dir = os.path.join("ver", "us", "build", "assets", "us", "msg")
    output_bin = os.path.join("ver", "us", "build", "assets", "us", "msg.bin")
    output_header = os.path.join("ver", "us", "build", "include", "message_ids.h")

    os.makedirs(build_dir, exist_ok=True)
    os.makedirs(os.path.dirname(output_header), exist_ok=True)

    # Find all .msg files sorted by name
    msg_files = sorted(glob.glob(os.path.join(msg_dir, "*.msg")))
    if not msg_files:
        print(f"  ERROR: No .msg files found in {msg_dir}", file=sys.stderr)
        sys.exit(1)

    print(f"  Compiling {len(msg_files)} message files...")

    compile_script = os.path.join("tools", "build", "msg", "parse_compile.py")
    combine_script = os.path.join("tools", "build", "msg", "combine.py")

    env = {**os.environ, "PYTHONUTF8": "1"}
    compiled_bins = []

    for msg_file in msg_files:
        # Extract the base name (e.g., "00" from "00_Misc.msg" or "2E_Credits.msg")
        basename = os.path.basename(msg_file)
        # The output bin uses just the hex prefix (before the first underscore)
        parts = os.path.splitext(basename)[0].split("_", 1)
        out_name = parts[0]
        out_bin = os.path.join(build_dir, out_name + ".bin")

        result = subprocess.run(
            [sys.executable, compile_script, "us", msg_file, out_bin],
            check=False,
            env=env,
            capture_output=True,
            text=True,
        )
        if result.returncode != 0:
            print(f"  ERROR compiling {msg_file}:", file=sys.stderr)
            print(result.stderr, file=sys.stderr)
            sys.exit(1)

        compiled_bins.append(out_bin)

    # Combine all compiled message bins into message_ids.h
    # combine.py usage: combine.py [out.bin] [out.h] [compiled...]
    print(f"  Combining {len(compiled_bins)} compiled message files...")

    result = subprocess.run(
        [sys.executable, combine_script, output_bin, output_header] + compiled_bins,
        check=False,
        env=env,
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        print(f"  ERROR combining messages:", file=sys.stderr)
        print(result.stderr, file=sys.stderr)
        sys.exit(1)

    print(f"  Generated {output_header}")


if __name__ == "__main__":
    main()
