.. _bluetooth-scan-adv-sample:

Bluetooth: Scan with coded phy ###########################

Overview
********

A simple scan with coded phy setup, once the beacon name matches name setup as "Test beacon", it will print out the MAC, and receiving RSSI: 



Requirements
************

to run and test on nRF52840DK, please re-configer the TX, RX pins in the nrf52840dk_nrf52840.overlay  overlay file. 

/*	tx-pin = < 0x6 >;  
    rx-pin = < 0x1 >;
  */  /* un comments this for testing with DK */

 /*        use this with ozzard board */
 /*  	*/
	tx-pin = < 0x1F >;     /* For ozark board  by default */
	rx-pin = < 0x2E >;



nrf52840 DK 
Building and Running
********************

UART @115200 and the sample output -> to the ozzark nRF91 uart specified in the nrf9 overlay file: 
scanner output example: 

RSSI: -37
recv data0 type : 09
recv data0 len :  12
recv data0 data : Test beacon
recv data1 type : ff
recv data1 len :  15
recv data1 id :  0059
recv data1 data : Hellow world
Scanned Test Beacon count = 8
adv data len: 29
recv adrs type: 1
recv adrs:8a2cabcee6f9
RSSI: -44
recv data0 type : 09
recv data0 len :  12
recv data0 data : Test beacon
recv data1 type : ff
recv data1 len :  15
recv data1 id :  0059
recv data1 data : Hellow world