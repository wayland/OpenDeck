#!/usr/bin/env bash

if [[ ! -f compile_commands.json ]]
then
    echo "compile_commands.json file not found"
    exit 1
fi

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &> /dev/null && pwd)

for arg in "$@"; do
    case "$arg" in
        --mcu=*)
            mcu=${arg#--mcu=}
            ;;

        --compiler=*)
            compiler=${arg#--compiler=}
            ;;

        --output=*)
            output_file=${arg#--output=}
            ;;

        --option-files-dir=*)
            option_files_dir=${arg#--option-files-dir=}
            ;;

    esac
done

if [[ -z $output_file ]]
then
  echo "Output file not specified"
  exit 1
fi

extra_args=()

# Some hackery is needed for linting to work on embedded targets.
# -DCORE_ARCH_AVR will be removed from defines (passed via
# .clang-tidy-compile-commands-clean) because otherwise lots of
# errors/warnings get generated by clang-tidy due to the code in
# libraries which are beyond the control of the
# project. This causes another problem: the generated
# MCU file will not get included, since arch in core module
# is disabled, so stubs are included and some information from that header
# is then missing. To bypass this, generated MCU file is added via include
# flag, and CORE_MCU_STUB symbol is explicitly defined so that only stub
# implementations from core are available.
extra_args+=(-extra-arg="-include$script_dir/../src/board/gen/mcu/$mcu/MCU.h")
extra_args+=(-extra-arg="-DCORE_MCU_STUB")

case $compiler in
  arm-none-eabi-gcc)
    toolchain_dir=$(dirname "$(which arm-none-eabi-gcc)" | rev | cut -d/ -f2- | rev)
    toolchain_ver=$(find "$toolchain_dir"/arm-none-eabi/include/c++ -mindepth 1 -maxdepth 1 | rev | cut -d/ -f 1 | rev)
    extra_args+=(-extra-arg="-I$toolchain_dir/arm-none-eabi/include/c++/$toolchain_ver")
    extra_args+=(-extra-arg="-I$toolchain_dir/arm-none-eabi/include/c++/$toolchain_ver/arm-none-eabi")
    extra_args+=(-extra-arg="-I$toolchain_dir/arm-none-eabi/include/c++/$toolchain_ver/backward")
    extra_args+=(-extra-arg="-I$toolchain_dir/lib/gcc/arm-none-eabi/$toolchain_ver/include")
    extra_args+=(-extra-arg="-I$toolchain_dir/lib/gcc/arm-none-eabi/$toolchain_ver/include-fixed")
    extra_args+=(-extra-arg="-I$toolchain_dir/arm-none-eabi/sys-include")
    extra_args+=(-extra-arg="-I$toolchain_dir/arm-none-eabi/include")
    extra_args+=(-extra-arg="-Wno-unknown-attributes")
    ;;

  avr-gcc)
    toolchain_dir=$(dirname "$(which avr-gcc)" | rev | cut -d/ -f2- | rev)
    extra_args+=(-extra-arg="-I$toolchain_dir/avr/include/avr")
    extra_args+=(-extra-arg="-I$toolchain_dir/avr/include")
    extra_args+=(-extra-arg="-I$toolchain_dir/lib/gcc/avr/7.3.0/include")
    extra_args+=(-extra-arg="-Wno-unknown-attributes")
    extra_args+=(-extra-arg="--target=avr")
    ;;

  *)
    # nothing to add
    ;;
esac

if [[ -n $option_files_dir ]]
then
  readarray -t ignore_paths <"$option_files_dir"/.clang-tidy-ignore
  ignore_regex="^((?!"

  for line in "${ignore_paths[@]}"
  do
    ignore_regex+=$line
    ignore_regex+="|"
  done

  # Last '|' must be removed
  ignore_regex=${ignore_regex::-1}
  ignore_regex+=").)*$"

  # Clean up compile_commands.json
  readarray -t cleanup_args <"$option_files_dir"/.clang-tidy-compile-commands-clean

  for line in "${cleanup_args[@]}"
  do
      sed -i "s/$line//g" compile_commands.json
  done
fi

run-clang-tidy \
-style=file \
-fix \
-format \
"${extra_args[@]}" \
-export-fixes "$output_file" \
"$ignore_regex"

exit 0
