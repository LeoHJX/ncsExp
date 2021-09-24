    SECTION_PROLOGUE (shared_ram, 0x20007000 (NOLOAD),)
    {
        __shared_ram_start = .;
        KEEP(*(SORT_BY_NAME(".shared_ram*")))
        __shared_ram_end = .;
    } GROUP_DATA_LINK_IN(RAMABLE_REGION, RAMABLE_REGION)
