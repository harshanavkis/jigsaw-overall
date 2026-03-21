#!/usr/bin/env python3

import argparse
import subprocess
from pathlib import Path

SCRIPT_DIR = Path(__file__).parent
ROOT_DIR = SCRIPT_DIR / ".." / ".."
SCRIPTS_BUILD_DIR = ROOT_DIR / "scripts" / "build"

def create_parsers():

    parser = argparse.ArgumentParser(
            description="Build script for baseline setup.",
            epilog="You can also do \'build.py -h\' to get the options.",
            formatter_class=argparse.RawDescriptionHelpFormatter,
            )
    parser.add_argument('-r', '--rebuild', help="Calls the compiler even if binary exists", action='store_true')

    parser.add_argument('-d', '--driver', help='Coyote Driver', action='store_true')
    parser.add_argument('-c', '--coyote_sw', help='Coyote Software', action='store_true')
    parser.add_argument('-b', '--bitstream', help='Coyote Hardware Bitstream', action='store_true')
    parser.add_argument('-k', '--kernel', help='Linux Kernel', action='store_true')
    parser.add_argument('-m', '--modules', help='Kernel Modules + Copying into image', action='store_true')
    parser.add_argument('-q', '--qemu', help='QEMU', action='store_true')
    parser.add_argument('image', nargs="?", help='Path to VM image. Only needed when building and copying modules')

    return parser

# Check if the image path is specified when building and copying modules
def validate(parser, args):
    flags_given = args.image is not None or args.kernel or args.modules or args.qemu or args.driver or args.coyote_sw or args.bitstream
    needs_target = not flags_given or args.modules 

    if needs_target and args.image is None:
        parser.error("image is required when no options are given, or when --modules (-m) is specified")


parser = create_parsers()
args = parser.parse_args()
validate(parser, args)

if args.rebuild:
    REBUILD = "force_rebuild"
else:
    REBUILD = ""


# If none of the flags is set, just build all of them
if not (args.kernel or args.modules or args.qemu or args.driver or args.coyote_sw or args.bitstream):
    args.driver = True
    args.coyote_sw = True
    args.bitstream = True
    args.kernel = True
    args.modules = True
    args.qemu = True

if args.driver:
    print("Building Coyote Driver:")
    subprocess.run([SCRIPTS_BUILD_DIR / "coyote-driver.sh"], check=True)

if args.coyote_sw:
    print("Building Coyote software:")
    subprocess.run([SCRIPTS_BUILD_DIR / "coyote-sw.sh", "jigsaw_baseline", REBUILD], check=True)

if args.bitstream:
    print("Generating bitstream:")
    subprocess.run([SCRIPTS_BUILD_DIR / "coyote-hw.sh", "jigsaw_baseline", REBUILD], check=True)

if args.kernel:
    print("Building kernel:")
    subprocess.run([SCRIPTS_BUILD_DIR / "kernel.sh", "jigsaw", REBUILD], check=True)

if args.modules:
    print("Building and copying kernel modules:")
    subprocess.run([SCRIPTS_BUILD_DIR / "build_and_copy_kernel_mods.sh", args.image], check=True)

if args.qemu:
    print("Building QEMU:")
    subprocess.run([SCRIPTS_BUILD_DIR / "qemu.sh", REBUILD], check=True)

