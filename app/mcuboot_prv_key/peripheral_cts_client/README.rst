based on _peripheral_cts_client:

if mcuboot overlay config not work (e.g.: NCS v1.6.0), then specify the full config file instead. 

copy prj.conf from ‘ncs/bootloader/mcuboot/boot/zephyr/’, to your project folder, and rename it as ‘mcuboot.conf’. 

add other configs you need for the project. 

then add “set(mcuboot_CONF_FILE ${CMAKE_CURRENT_SOURCE_DIR}/mcuboot.conf)” to the project CMakeList.txt
