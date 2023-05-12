#!/usr/bin/env bash

if [[ $($yaml_parser "$project_yaml_file" fuses) != "null" ]]
then
    fuse_unlock=$($yaml_parser "$project_yaml_file" fuses.unlock)
    fuse_lock=$($yaml_parser "$project_yaml_file" fuses.lock)
    fuse_ext=$($yaml_parser "$project_yaml_file" fuses.ext)
    fuse_high=$($yaml_parser "$project_yaml_file" fuses.high)
    fuse_low=$($yaml_parser "$project_yaml_file" fuses.low)

    {
        printf "%s\n" "PROJECT_MCU_FUSE_UNLOCK := $fuse_unlock"
        printf "%s\n" "PROJECT_MCU_FUSE_LOCK := $fuse_lock"
        printf "%s\n" "PROJECT_MCU_FUSE_EXT := $fuse_ext"
        printf "%s\n" "PROJECT_MCU_FUSE_HIGH := $fuse_high"
        printf "%s\n" "PROJECT_MCU_FUSE_LOW := $fuse_low"
    } >> "$out_makefile"
fi