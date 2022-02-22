cmake_minimum_required(VERSION 3.13.1)

set(BOARD nrf52833dk_nrf52833)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(test-adv)

target_sources(app PRIVATE
    src/main.c
)

zephyr_include_directories(
    src
)
