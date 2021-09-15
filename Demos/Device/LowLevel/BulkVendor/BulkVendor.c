/*
             LUFA Library
     Copyright (C) Dean Camera, 2021.

  dean [at] fourwalledcubicle [dot] com
           www.lufa-lib.org
*/

/*
  Copyright 2021  Dean Camera (dean [at] fourwalledcubicle [dot] com)

  Permission to use, copy, modify, distribute, and sell this
  software and its documentation for any purpose is hereby granted
  without fee, provided that the above copyright notice appear in
  all copies and that both that the copyright notice and this
  permission notice and warranty disclaimer appear in supporting
  documentation, and that the name of the author not be used in
  advertising or publicity pertaining to distribution of the
  software without specific, written prior permission.

  The author disclaims all warranties with regard to this
  software, including all implied warranties of merchantability
  and fitness.  In no event shall the author be liable for any
  special, indirect or consequential damages or any damages
  whatsoever resulting from loss of use, data or profits, whether
  in an action of contract, negligence or other tortious action,
  arising out of or in connection with the use or performance of
  this software.
*/

/** \file
 *
 *  Main source file for the Bulk Vendor demo. This file contains the main tasks of the demo and
 *  is responsible for the initial application hardware configuration.
 */

#define  INCLUDE_FROM_BULKVENDOR_C
#include "BulkVendor.h"

#define STK500_HEADER_SIZE 5
#define STK500_BODY_SIZE 275
#define STK500_CHECKSUM 1
#define STK500_PACKET_SIZE (STK500_HEADER_SIZE+STK500_BODY_SIZE+STK500_CHECKSUM)
#define MESSAGE_START 0x1B
#define TOKEN 0x0E

#define TX_BUFFER_EMPTY (0xFFFF)

static uint8_t      buf[STK500_PACKET_SIZE];
volatile uint16_t    uart_txp = TX_BUFFER_EMPTY;
volatile uint16_t    bufp = 0;

/** Main program entry point. This routine configures the hardware required by the application, then
 *  enters a loop to run the application tasks in sequence.
 */
int main(void)
{
	buf[0] = MESSAGE_START;
	buf[1] = 1; // no use
	buf[4] = TOKEN;
	SetupHardware();

	GlobalInterruptEnable();

	for (;;)
	{
		USB_USBTask();
		Serial_Task();
	}
}

void Serial_Task(void)
{
	Endpoint_SelectEndpoint(VENDOR_OUT_EPADDR);

	if (Endpoint_IsOUTReceived())
	{
		uint16_t i = 0;

		while (Endpoint_Read_Stream_LE(buf + STK500_HEADER_SIZE, STK500_BODY_SIZE, &i) == ENDPOINT_RWSTREAM_IncompleteTransfer)
		{
		}
		Endpoint_ClearOUT();

		buf[2] = (i >> 8);
		buf[3] = (i & 0xFF);
		uart_txp = i + STK500_HEADER_SIZE;
		buf[uart_txp] = 0;
		for (i = 0; i < uart_txp; i++)
		{
			buf[uart_txp] ^= buf[i];
		}
		bufp = 0;
		UCSR1B |= (1 << UDRIE1);
	}

	if (uart_txp == TX_BUFFER_EMPTY && (buf[2] << 8) + buf[3] + STK500_HEADER_SIZE + STK500_CHECKSUM == bufp)
	{
		uint16_t i = 0;

		Endpoint_SelectEndpoint(VENDOR_IN_EPADDR);
		while (Endpoint_Write_Stream_LE(buf + STK500_HEADER_SIZE, bufp - STK500_HEADER_SIZE - STK500_CHECKSUM, &i) == ENDPOINT_RWSTREAM_IncompleteTransfer)
		{
		}
		Endpoint_ClearIN();

		bufp = 0;
	}
}

/** Configures the board hardware and chip peripherals for the demo's functionality. */
void SetupHardware(void)
{
#if (ARCH == ARCH_AVR8)
	/* Disable watchdog if enabled by bootloader/fuses */
	MCUSR &= ~(1 << WDRF);
	wdt_disable();

	/* Disable clock division */
	clock_prescale_set(clock_div_1);
	Serial_Init(115200, false);
	UCSR1B = ((1 << RXCIE1) | (1 << TXEN1) | (1 << RXEN1)); // enable RX interrupt
#endif

	/* Hardware Initialization */
	USB_Init();
}

/** Event handler for the USB_Connect event. This indicates that the device is enumerating via the status LEDs. */
void EVENT_USB_Device_Connect(void)
{
	/* Indicate USB enumerating */
}

/** Event handler for the USB_Disconnect event. This indicates that the device is no longer connected to a host via
 *  the status LEDs.
 */
void EVENT_USB_Device_Disconnect(void)
{
	/* Indicate USB not ready */
}

/** Event handler for the USB_ConfigurationChanged event. This is fired when the host set the current configuration
 *  of the USB device after enumeration - the device endpoints are configured.
 */
void EVENT_USB_Device_ConfigurationChanged(void)
{
	bool ConfigSuccess = true;

	/* Setup Vendor Data Endpoints */
	ConfigSuccess &= Endpoint_ConfigureEndpoint(VENDOR_IN_EPADDR,  EP_TYPE_BULK, VENDOR_IO_EPSIZE, 1);
	ConfigSuccess &= Endpoint_ConfigureEndpoint(VENDOR_OUT_EPADDR, EP_TYPE_BULK, VENDOR_IO_EPSIZE, 1);

	/* Indicate endpoint configuration success or failure */
}

ISR(USART1_RX_vect, ISR_BLOCK)
{
        buf[bufp++] = UDR1;
}

ISR(USART1_UDRE_vect, ISR_BLOCK)
{
        if (uart_txp != bufp) {
		UDR1 = buf[bufp++];
	} else {
		UCSR1B &= ~(1 << UDRIE1);
		UDR1 = buf[bufp];
		uart_txp = TX_BUFFER_EMPTY; bufp = 0;
	}
}
