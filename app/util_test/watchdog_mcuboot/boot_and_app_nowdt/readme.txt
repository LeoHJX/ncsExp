demo watch dog enabled within mcuboot and application
Tested with NCS v1.9.1 on nrf52840DK

please replace files under mcuboot_patch to <ncs>/bootloader/mcuboot/boot/zephyr

then build and test this example. 

this example only enabled watchdog in mcuboot, so application will start but will be reset by the watchdog which been enabled from mcuboot. 


More notes/hints
○ Suggest to make a patch and store the patch within the project, and then apply these patches before build the project. 

○ bl z_arm_watchdog_init would be one earlier option from reset.S. this requires to add z_arm_watchdog_init and write watchdog register directly. 
	§ This requires change zephyr source. 
	§ /ncs/zephyr/arch/arm/core/aarch32/cortex_m/reset.S
	
or maintain a patch to file system_nrf52.c under /modules/hal/nordic/nrfx/mdk (and initilize the watch dog here), but earlier than above options in reset.S

Above options only allow simple register changes, no interrupt, and floating point support (which was initlized after these points)
○ Otherwise  
	§ SYS_INIT(init_wdt, PRE_KERNEL_2, CONFIG_KERNEL_INIT_PRIORITY_OBJECTS);  /* PRE_KERNEL_1 won't work, except use nrfx driver directly   */
		□ Calling point from init.c
		□ ncs/zephyr/kernel/init.c
			® Where 
			/* perform basic hardware initialization */
			z_sys_init_run_level(_SYS_INIT_LEVEL_PRE_KERNEL_1);
			z_sys_init_run_level(_SYS_INIT_LEVEL_PRE_KERNEL_2);
This requires change mcuboot source. 