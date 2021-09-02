# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.13.1)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(hello_world)

target_sources(app PRIVATE src/main.c)
zephyr_linker_sources_ifdef(CONFIG_FW_INFO
  ROM_START
  #SORT_KEY 0x400  # not used for now. 
  custom_flash.ld)
