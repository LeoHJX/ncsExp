#
# Copyright (c) 2018 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

cmake_minimum_required(VERSION 3.13.1)

set(DTC_OVERLAY_FILE "dts.overlay")

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(netcore)

target_sources(app PRIVATE src/main.c)
target_sources(app PRIVATE src/cmds.c)
target_sources(app PRIVATE ../throughput_service/throughput.c)
zephyr_include_directories(../throughput_service)



zephyr_library_include_directories(${ZEPHYR_BASE}/samples/bluetooth)
