.. _rtt_debug:

RTT debug
###########

Overview
********

Printk message to RTT console and free up the UART for console. 

Building and Running
********************

This application can be built and executed on QEMU as follows:

.. zephyr-app-commands::
   :zephyr-app: 
   :host-os: unix
   :board: nrf boards
   :goals: run
   :compact:

To build for another board, change "qemu_x86" above to that board's name.

Sample Output
=============

.. code-block:: console

Change steps from Hello World example
********************
Add these lines in prj.conf

CONFIG_UART_CONSOLE=n
CONFIG_RTT_CONSOLE=y
CONFIG_USE_SEGGER_RTT=y

CONFIG_SERIAL=y # change to y to support zephyr Serial/UART driver. 

CONFIG_UART_NRFX=y # calling this driver at very low level. 

CONFIG_UART_INTERRUPT_DRIVEN=y
CONFIG_UART_0_INTERRUPT_DRIVEN=y
CONFIG_UART_0_NRF_TX_BUFFER_SIZE=32
CONFIG_UART_0_ENHANCED_POLL_OUT=y

and device tree over lay file for example. 

&uart1 {
	compatible = "nordic,nrf-uarte";
	status = "disabled";
	label = "UART_1";
	current-speed = < 0x1c200 >;
	tx-pin = < 0x14 >;
	rx-pin = < 0x16 >;
	rts-pin = < 0x13 >;
	cts-pin = < 0x15 >;
};

&uart0 {
	status = "okay";
	label = "UART_0";
	compatible = "nordic,nrf-uarte";
    current-speed = < 0x1c200 >;
	tx-pin = < 0x14 >;
	rx-pin = < 0x16 >;
	rts-pin = < 0x13 >;
	cts-pin = < 0x15 >;

};

and C source code in main.c. 

