#!/usr/bin/env python3

import argparse
import subprocess
from pathlib import Path

SCRIPT_DIR = Path(__file__).parent
ROOT_DIR = SCRIPT_DIR / ".." / ".."
SCRIPTS_RUN_DIR = ROOT_DIR / "scripts" / "run"
COYOTE_DIR = ROOT_DIR / "submodules" / "Coyote" / "examples" / "jigsaw_baseline" 

def create_parsers():

    parser = argparse.ArgumentParser(
            description="Run script for baseline setup.",
            epilog="You can also do \'run.py -h\' to get the device specific option.",
            formatter_class=argparse.RawDescriptionHelpFormatter,
            )

    subparsers = parser.add_subparsers(dest='command', required=True)

    # Create subcommands
    parser_vm = subparsers.add_parser('vm', help='Run the QEMU VM')
    parser_proxy = subparsers.add_parser('proxy', help='Run the Proxy service')

    #
    # Options for proxy subcommand
    #
    parser_proxy.add_argument('-l', '--load_driver', help='Load Coyote Driver', action='store_true')
    parser_proxy.add_argument('-r', '--remove_driver', help='Remove Coyote Driver', action='store_true')
    parser_proxy.add_argument('-f', '--flash_bitstream', help='Flash the Coyote bitstream', action='store_true')
    parser_proxy.add_argument('-s', '--run_software', help='Run the Coyote Software', action='store_true')

    #
    # Options for vm subcommand
    #
    parser_vm.add_argument('image_path')
    parser_vm.add_argument('ovmf_path')

    return parser

parser = create_parsers()
args = parser.parse_args()

if args.command == 'proxy':

    # Cannot do teardown while flashing, setup or running the software
    if args.remove_driver and (args.load_driver or args.flash_bitstream or args.run_software):
        parser.error("Cannot remove driver (--remove_driver) while doing one of the other actions")

    # If user did not specify anything, just run in order
    if not (args.remove_driver or args.load_driver or args.flash_bitstream or args.run_software):
        args.flash_bitstream = True
        args.load_driver = True
        args.run_software = True

    if args.remove_driver:
        print("Removing driver:")
        subprocess.run([SCRIPTS_RUN_DIR / "teardown_coyote.sh"], check=True)

    if args.flash_bitstream:
        print("Flashing bitstream:")
        subprocess.run([SCRIPTS_RUN_DIR / "coyote-flash-bitstream.sh", "jigsaw_baseline"], check=True)

    if args.load_driver:
        print("Loading driver:")
        subprocess.run([SCRIPTS_RUN_DIR / "setup_coyote.sh"], check=True)

    if args.run_software:
        print("Running software:")
        subprocess.run([COYOTE_DIR / "sw" / "build" / "test"], check=True)

elif args.command == 'vm':

    subprocess.run([SCRIPTS_RUN_DIR / "vm.sh", "VM", args.image_path, args.ovmf_path], check=True)

