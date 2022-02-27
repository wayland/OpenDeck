#!/usr/bin/env bash

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &> /dev/null && pwd)
yaml_file=$1
gen_dir=$2
yaml_parser="dasel -n -p yaml --plain -f"
out_header="$gen_dir"/Target.h
out_makefile="$gen_dir"/Makefile

# Generated header contains cpp code.
# Generate USB names in dedicated header since this one is included in .c files
out_header_usb="$gen_dir"/USBnames.h

echo "Generating target definitions..."

mkdir -p "$gen_dir"
echo "" > "$out_header"
echo "" > "$out_makefile"
echo "" > "$out_header_usb"

{
    printf "%s\n\n" "#pragma once"
    printf "%s\n" "#include \"core/src/general/IO.h\""
    printf "%s\n" "#include \"board/Internal.h\""
    printf "%s\n\n" "#include <MCU.h>"
} >> "$out_header"

source "$script_dir"/target/core.sh
source "$script_dir"/target/peripherals.sh
source "$script_dir"/target/digital_inputs.sh
source "$script_dir"/target/digital_outputs.sh
source "$script_dir"/target/analog_inputs.sh
source "$script_dir"/target/unused_io.sh

printf "\n%s" "#include \"board/common/Map.h.include\"" >> "$out_header"