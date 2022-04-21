# bash

nrfutil pkg generate --hw-version 52 --sd-req=0x00 \
        --application build/zephyr/zephyr.hex \
        --application-version 1 nrf5340_dm.zip

#flash command // please copy to command line if don't know the COMs
# nrfutil dfu usb-serial -pkg nrf5340_dm.zip -p COMxx