tested with NCS v1.9.1

west build nrf52840dk_nrf52840 -p 

west flash
// run check the output. 
change the output from main.c. 

press reset button 1 and hold it, then press reset button. 

west build nrf52840dk_nrf52840

enter folder ./build/zephyr/

sudo /home/lx/go/bin/mcumgr --conntype=serial --connstring='dev=/dev/ttyACM1,baud=115200'        image upload  -e app_update.bin 
sudo /home/lx/go/bin/mcumgr  --conntype=serial --connstring='dev=/dev/ttyACM1,baud=115200' reset