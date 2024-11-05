.. zephyr:code-sample:: mspi-test
   :name: MSPI preview sample
   :relevant-api: mspi_interface

   Use the MSPI API to interact with MSPI device changing number of data
   lines between transfers.

Overview
********

This sample demonstrates using the :ref:`MSPI API <mspi_api>` on a MSPI
device. The sample writes configuration data using a single line transfer.
Then it writes data using a 4 line transfer.

Building and Running
********************

.. zephyr-app-commands::
   :zephyr-app: samples/drivers/mspi/mspi_test
   :board: nrf54h20dk_nrf54h20_cpuapp
   :goals: build flash
   :compact:

Sample Output
=============

.. code-block:: console

   *** Booting Zephyr OS build zephyr-v3.5.0-8581-gc80b243c7598 ***
   MSPI test completed
