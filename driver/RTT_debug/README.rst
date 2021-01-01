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

CONFIG_SERIAL=n
CONFIG_UART_CONSOLE=n
CONFIG_RTT_CONSOLE=y
CONFIG_USE_SEGGER_RTT=y

