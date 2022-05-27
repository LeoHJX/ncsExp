demo watch dog enabled within mcuboot and application
Tested with NCS v1.9.1 on nrf52840DK

please replace files under mcuboot_patch to <ncs>/bootloader/mcuboot/boot/zephyr

then build and test this example. 

This example enables watchdog from mcuboot, and as well as in the application as well ( need to feed watch dog in both to prevent reset). 


More notes/hints
○ Suggest to make a patch and store the patch within the project, and then apply these patches before build the project. 

○ bl z_arm_watchdog_init would be the earliest from reset.S. this requires to add z_arm_watchdog_init and write watchdog register directly. 
	§ This requires change zephyr source. 
	§ /ncs/zephyr/arch/arm/core/aarch32/cortex_m/reset.S
○ Otherwise  
	§ SYS_INIT(init_wdt, PRE_KERNEL_2, CONFIG_KERNEL_INIT_PRIORITY_OBJECTS);  /* PRE_KERNEL_1 won't work, except use nrfx driver directly   */
		□ Calling point from init.c
		□ ncs/zephyr/kernel/init.c
			® Where 
			/* perform basic hardware initialization */
			z_sys_init_run_level(_SYS_INIT_LEVEL_PRE_KERNEL_1);
			z_sys_init_run_level(_SYS_INIT_LEVEL_PRE_KERNEL_2);
This requires change mcuboot source. 