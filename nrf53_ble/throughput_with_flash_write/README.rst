Overview
********

This example is based on the modification of "throughput" example in NCS v1.5.0
For more detail about how to change the BLE parameter, please check the README.rst in "throughput" example.

The entire architecture:
*  NET core does throughput application, BLE host and also BLE controller
*  Once when NET in peripheral received a block of data (244bytes) from central during throughput test, NET core sends it to APP, and wait for an Ack from APP before be able to receive next block of data
*  When APP received the data from NET, it put into a queue, and send an Ack back to NET
*  When the queue is full, APP core start to write buffer into flash
*  Since NET core needs an Ack to receive a new block of data and send to APP core, so the overrun won’t happen, which means if APP core is busy on writing flash, NET core won’t put more data into APP


Requirements
************

To evaluate the BLE throughput with writing flash concurrently on nRF5340, please prepare two nRF5340-DK v0.11.0.
For setup the test environment:
1. Please extract and place the the project folder under "nrf/samples/bluetooth/"
2. Please compile and program the "netcore" to nRF5340-DK network core with following commands to both nRF5340-DK
   > west build -b nrf5340dk_nrf5340_cpunet -p
   > west flash
3. Please compile and program the "appcore" to nRF5340-DK application core with following commands to both nRF5340-DK
   > west build -b nrf5340dk_nrf5340_cpuapp -p
   > west flash

Testing
=======

After programming the sample to both kits, test it by performing the following steps:

1. Connect to both kits with a terminal emulator (for example, PuTTY), the baudrate is 1000000bps
2. Please connect both APP core and NET core with terminal emulator for both two nRF5340-DK
3. Reset both kits.
4. Waiting for APP core erased flash, until the "Flash erase succeeded!" shown on the APP terminal for both two nRF5340-DK
5. Press Button 1 on the kit to set the kit into master (tester) role.
6. Press Button 2 on the other kit to set the kit into slave (peer) mode.
7. Observe that the kits establish a connection.
   The tester outputs the following information::

      Type 'config' to change the configuration parameters.
      You can use the Tab key to autocomplete your input.
      Type 'run' when you are ready to run the test.

8. Type ``config print`` in the terminal to print the current configuration.
   Type ``config`` in the terminal to configure the test parameters to your choice.
   Use the Tab key for auto-completion and to view the options available for a parameter.
   In the defaut setting, the connection interval is 31.25ms
9. Type ``run`` in the terminal to start the test.
10. Observe the output while the tester sends data to the peer.
   At the end of the test, both tester and peer display the results of the test.
11. Once when the test finished and result displayed, please reset both DK for next test.



Sample output
==============

The result should look similar to the following output.

For the tester APP core::

*** Booting Zephyr OS build v2.4.99-ncs1  ***                                               
Application core start                                                                      
Please wait for test flash area be erased                                                   
Flash erase succeeded!                                                                      
stream flash init succeeded                                                                 
Starting application thread!   


For the tester NET core::

uart:~$ *** Booting Zephyr OS build v2.4.99-ncs1  ***                                       
Starting Bluetooth Throughput example                                                       
Bluetooth initialized                                                                       
                                                                                            
Press button 1 on the master board.                                                         
Press button 2 on the slave board.                                                          
                                                                                            
                                                                                            
[00:00:00.001,983] <inf> sdc_hci_driver: SoftDevice Controller build revision:              
                                         e5 c7 9c d9 91 00 1d 66  ea fb 6e 7b 98 2f 42 0d |.
                                         f1 60 93 c8                                      | 
[00:00:00.004,821] <inf> bt_hci_core: HW Platform: Nordic Semiconductor (0x0002)            
[00:00:00.004,821] <inf> bt_hci_core: HW Variant: nRF53x (0x0003)                           
[00:00:00.004,821] <inf> bt_hci_core: Firmware: Standard Bluetooth controller (0x00) Versio1
[00:00:00.006,072] <inf> bt_hci_core: Identity: D0:2C:DB:EA:B9:DF (random)                  
[00:00:00.006,072] <inf> bt_hci_core: HCI: version 5.2 (0x0b) revision 0x2190, manufacturer9
[00:00:00.006,103] <inf> bt_hci_core: LMP: version 5.2 (0x0b) subver 0x2190                 
uart:~$                                                                                     
Master role. Starting scanning    
Connected as master                                                                         
Conn. interval is 30 units                                                                  
Service discovery completed                                                                 
MTU exchange pending                                                                        
MTU exchange successful                                                                     
                                                                                            
Type 'config' to change the configuration parameters.                                       
You can use the Tab key to autocomplete your input.                                         
Type 'run' when you are ready to run the test.                                              
run                                                                                         
                                                                                            
==== Starting throughput test ====                                                          
PHY update pending                                                                          
LE PHY updated: TX PHY LE 2M, RX PHY LE 2M                                                  
LE Data length update pending                                                               
LE data len updated: TX (len: 251 time: 2120) RX (len: 251 time: 2120)                      
Connection parameters update pending                                                        
Connection parameters updated.                                                              
 interval: 25, latency: 0, timeout: 1000                                                    
                                                                                            
                    ^.-.^                               ^..^                                
                 ^-/ooooo+:.^                       ^.--:+syo/.                             
              ^-/oooooooooooo+:.                 ^.-:::::+yyyyyy+:^                         
           ^-/+oooooooooooooooooo/-^          ^.-::::::::/yyyyyyyhhs/-                      
        ^-:/++++oooooooooooooooooooo+:.   ^.-::::::::::::/yyyyyyyhhhhhho:^                  
      ^::///++++oooooooooooooooooooooooo//:::::::::::::::/yyyyyyyhhhhhddds                  
      -::://+++ooooooooooooooooooooooooooooo+/:::::::::::/yyyyyyyhhhhhdddd^                 
      -::::::/++ooooooooooooooooooooooooooooooo+/::::::::/yyyyyyyhhhhhdddd^                 
      -:::::::::/+ooooooooooooooooooooooooooooossso+/::::/yyyyyyyhhhhhdddd^                 
      -::::::::::::/+oooooooooooooooooooooooooossssssso+//yyyyyyyhhhhhdddd^                 
      -::::::::::::::::/+ooooooooooooooooooooooossssssssssyyyyyyyhhhhhdddd.                 
      -:::::::::::::::::::/+oooooooooooooooooooossssssssssyyyyyyyhhhhhdddd.                 
      -:::::::::::::::::::::::/+ooooooooooooooosssssssssssyyyyyyyhhhhhdddd.                 
      -::::::::::::::::::::::::::/+ooooooooooooossssssssssyyyyyyyhhhhhdddd.                 
      -::::::::::::::::::::::::::::::/+ooooooooossssssssssyyyyyyyhhhhhdddd-                 
      -:::::::::::::::::::::::::::::::::/+ooooosssssssssssyyyyyyyhhhhhdddd-                 
      -:::::::::::::::::::::::::::::::::::::/+oossssssssssyyyyyyyhhhhhdddd:                 
      -::::::::::::::::::::::::::::::::::::::::/+ossssssssyyyyyyyhhhhhdddd:                 
      -::::::::::::::::::::::::::::::::::::::::::::/osssssyyyyyyyhhhhhdddd:                 
      -:::::::::::::::::::::::::::::::::::::::::::::::/+ossyyyyyyhhhhhdddd:                 
      -:::::::::::::::::o+/:::::::::::::::::::::::::::::::+oyyyyyhhhhhdddd:                 
      -:::::::::::::::::ossyso/::::::::::::::::::::::::::::::/osyhhhhhdddd/                 
      -:::::::::::::::::ossyyyyys+:::::::::::::::::::::::::::::::+shhhdddd/                 
      -:::::::::::::::::ossyyyyhhhhyo/::::::::::::::::::::::::::::::/oyddd/                 
      .-::::::::::::::::ossyyyyhhhhddddy/-::::::::::::::::::::::::::::::+y:                 
        ^.-:::::::::::::ossyyyyhhhhdhs/.  ^.--:::::::::::::::::::::::::-.^                  
           ^.--:::::::::ossyyyyhhy+-^         ^.-::::::::::::::::::--.^                     
               ^.-::::::ossyyyo/.                ^^.-:::::::::::-.^                         
                  ^..-::oss+:^                       ^.-:::::-.^                            
                      ^.:.^                             ^^.^^                               
                                                                                            
Done                                                                                        
[local] sent 612884 bytes (598 KB) in 9309 ms at 526 kbps                                   
[peer] received 612884 bytes (598 KB) in 2512 GATT writes at 497125 bps                     
                                                                                            
Type 'config' to change the configuration parameters.                                       
You can use the Tab key to autocomplete your input.                                         
Type 'run' when you are ready to run the test.                                              
uart:~$ 



For the tester APP core::

*** Booting Zephyr OS build v2.4.99-ncs1  ***                                               
Application core start                                                                      
Please wait for test flash area be erased                                                   
Flash erase succeeded!                                                                      
stream flash init succeeded                                                                 
Starting application thread!   
================================================================================            
================================================================================            
================================================================================            
================================================================================            
================================================================================            
================================================================================            
================================================================================            
================================================================================            
================================================================================            
================================================================================            
================================================================================            
================================================================================            
================================================================================            
================================================================================            
================================================================================            
================================================================================            
================================================================================            
================================================================================            
================================================================================            
================================================================================            
================================================================================            
================================================================================            
================================================================================            
================================================================================            
================================================================================            
================================================================================            
================================================================================            
================================================================================            
================================================================================            
================================================================================            
================================================================================            
===============================                                                             
Last packet received                                                                        
Flash write and throughput demo ended.                                                      
Please reboot device for next test  

For the tester NET core::

*** Booting Zephyr OS build v2.4.99-ncs1  ***                                               
Starting Bluetooth Throughput example                                                       
Bluetooth initialized                                                                       
                                                                                            
Press button 1 on the master board.                                                         
Press button 2 on the slave board.                                                          
                                                                                            
                                                                                            
[00:00:00.002,044] <inf> sdc_hci_driver: SoftDevice Controller build revision:              
                                         e5 c7 9c d9 91 00 1d 66  ea fb 6e 7b 98 2f 42 0d |.
                                         f1 60 93 c8                                      | 
[00:00:00.004,882] <inf> bt_hci_core: HW Platform: Nordic Semiconductor (0x0002)            
[00:00:00.004,882] <inf> bt_hci_core: HW Variant: nRF53x (0x0003)                           
[00:00:00.004,913] <inf> bt_hci_core: Firmware: Standard Bluetooth controller (0x00) Versio1
[00:00:00.006,134] <inf> bt_hci_core: Identity: D4:C1:8B:0A:7B:F8 (random)                  
[00:00:00.006,134] <inf> bt_hci_core: HCI: version 5.2 (0x0b) revision 0x2190, manufacturer9
[00:00:00.006,164] <inf> bt_hci_core: LMP: version 5.2 (0x0b) subver 0x2190                 
uart:~$                                                                                     
Slave role. Starting advertising                                                            
Connected as slave                                                                          
Conn. interval is 30 units                                                                  
LE PHY updated: TX PHY LE 2M, RX PHY LE 2M                                                  
Connection parameters updated.                                                              
 interval: 25, latency: 0, timeout: 1000                                                    
                                                                                            
[local] received 612884 bytes (598 KB) in 2512 GATT writes at 497125 bps                    
Please reboot for next test                                                                 

