#!/usr/bin/env bash

if [[ $($yaml_parser "$project_yaml_file" i2c) != "null" ]]
then
    if [[ $($yaml_parser "$project_yaml_file" i2c.buffers) != "null" ]]
    then
        i2c_buffer_tx=$($yaml_parser "$project_yaml_file" i2c.buffers.tx)
        i2c_buffer_rx=$($yaml_parser "$project_yaml_file" i2c.buffers.rx)

        {
            printf "%s\n" "PROJECT_MCU_DEFINES += PROJECT_MCU_BUFFER_SIZE_I2C_TX=$i2c_buffer_tx"
            printf "%s\n" "PROJECT_MCU_DEFINES += PROJECT_MCU_BUFFER_SIZE_I2C_RX=$i2c_buffer_rx"
        } >> "$out_makefile"
    fi
fi