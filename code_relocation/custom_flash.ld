/* 
. = 0x400;
__custom_flash_vars_start = .;
KEEP(*(SORT_BY_NAME(.custom_flash_vars*)))
__custom_flash_vars_end = .;
*/
/*  reserve 32bytes after the fw info. */
. = CONFIG_FW_INFO_OFFSET + _fw_info_size + 32;
__custom_flash_vars_start = .;
KEEP(*(SORT_BY_NAME(.custom_flash_vars*)))
__custom_flash_vars_end = .;