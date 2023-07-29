/*
Nintendo Driving Wheel controller

Based on the LUFA library's Low-Level Joystick Demo
	(C) Dean Camera
Based on the HORI's Pokken Tournament Pro Pad design
	(C) HORI

This project implements a modified version of HORI's Pokken Tournament Pro Pad
USB descriptors to allow for the creation of custom controllers for the
Nintendo Switch. This also works to a limited degree on the PS3.

Since System Update v3.0.0, the Nintendo Switch recognizes the Pokken
Tournament Pro Pad as a Pro Controller. Physical design limitations prevent
the Pokken Controller from functioning at the same level as the Pro
Controller. However, by default most of the descriptors are there, with the
exception of Home and Capture. Descriptor modification allows us to unlock
these buttons for our use.
*/

/** \file
 *
 *  Main source file for the Joystick demo. This file contains the main tasks of the demo and
 *  is responsible for the initial application hardware configuration.
 */

#include <LUFA/Drivers/Peripheral/Serial.h>
#include "Joystick.h"

uint8_t target = RELEASE;
uint16_t command;

uint8_t lastLX = STICK_CENTER;
uint8_t lastLY = STICK_CENTER;
uint8_t firstButtons = 0;
uint8_t secondButtons = 0;


/* Data will come as 
	Byte 0: which type of data, analog(241, 240) or digital(242)
	Byte 1: 
	 - if analog: the analog data
	 - if digital: the first 8 buttons
	Byte 2: only digital, second 8 buttons
*/
void parseLine(char *line, uint32_t count) {
	char t[8];
	char c[16];
	uint8_t* data = (uint8_t*)line;

	if(data[0] == 241) { // Left joystick
		lastLX = data[1];
	} else if(data[0] == 240) // Right Joystick
	{
		// No right joystick data on controller
	
	} else if(data[0] == 242) { // Buttons
		if(data[1] == 3) {	// No data on the first buttons
			firstButtons = 0;
		} else {

			firstButtons = data[1];
		}

		if(data[2] == 3) { // No data on the second buttons
			secondButtons = 0;
		} else {
			secondButtons = data[2];
		}
	}
}

/* Parse characters on RX line until newline and then parse word */
#define MAX_BUFFER 1024
char b[MAX_BUFFER];
uint8_t l = 0;
ISR(USART1_RX_vect) {
	char c = fgetc(stdin);
	if (Serial_IsSendReady()) {
		// printf("%c", c);
	}
	if (c == '\n') {

		parseLine(b, l);
		l = 0;
		memset(b, 0, sizeof(b));
	} else if (c != '\r' && l < MAX_BUFFER) {
		// printf("%c ", c);
		b[l++] = c;
	}
}

// Main entry point.
int main(void) {
  Serial_Init(9600, false);
  Serial_CreateStream(NULL);

  sei();
  UCSR1B |= (1 << RXCIE1);

	// We'll start by performing hardware and peripheral setup.
	SetupHardware();
	// We'll then enable global interrupts for our use.
	GlobalInterruptEnable();
	// Once that's done, we'll enter an infinite loop.
	for (;;)
	{
		// We need to run our task to process and deliver data for our IN and OUT endpoints.
		HID_Task();
		// We also need to run the main USB management task.
		USB_USBTask();
	}
}

// Configures hardware and peripherals, such as the USB peripherals.
void SetupHardware(void) {
	// We need to disable watchdog if enabled by bootloader/fuses.
	MCUSR &= ~(1 << WDRF);
	wdt_disable();

	// We need to disable clock division before initializing the USB hardware.
	clock_prescale_set(clock_div_1);
	// We can then initialize our hardware and peripherals, including the USB stack.

	// The USB stack should be initialized last.
	USB_Init();
}

// Fired to indicate that the device is enumerating.
void EVENT_USB_Device_Connect(void) {
	// We can indicate that we're enumerating here (via status LEDs, sound, etc.).
}

// Fired to indicate that the device is no longer connected to a host.
void EVENT_USB_Device_Disconnect(void) {
	// We can indicate that our device is not ready (via status LEDs, sound, etc.).
}

// Fired when the host set the current configuration of the USB device after enumeration.
void EVENT_USB_Device_ConfigurationChanged(void) {
	bool ConfigSuccess = true;

	// We setup the HID report endpoints.
	ConfigSuccess &= Endpoint_ConfigureEndpoint(JOYSTICK_OUT_EPADDR, EP_TYPE_INTERRUPT, JOYSTICK_EPSIZE, 1);
	ConfigSuccess &= Endpoint_ConfigureEndpoint(JOYSTICK_IN_EPADDR, EP_TYPE_INTERRUPT, JOYSTICK_EPSIZE, 1);

	// We can read ConfigSuccess to indicate a success or failure at this point.
}

// Process control requests sent to the device from the USB host.
void EVENT_USB_Device_ControlRequest(void) {
	// We can handle two control requests: a GetReport and a SetReport.
	switch (USB_ControlRequest.bRequest)
	{
		// GetReport is a request for data from the device.
		case HID_REQ_GetReport:
			if (USB_ControlRequest.bmRequestType == (REQDIR_DEVICETOHOST | REQTYPE_CLASS | REQREC_INTERFACE))
			{
				// We'll create an empty report.
				USB_JoystickReport_Input_t JoystickInputData;
				// We'll then populate this report with what we want to send to the host.
				GetNextReport(&JoystickInputData);
				// Since this is a control endpoint, we need to clear up the SETUP packet on this endpoint.
				Endpoint_ClearSETUP();
				// Once populated, we can output this data to the host. We do this by first writing the data to the control stream.
				Endpoint_Write_Control_Stream_LE(&JoystickInputData, sizeof(JoystickInputData));
				// We then acknowledge an OUT packet on this endpoint.
				Endpoint_ClearOUT();
			}

			break;
		case HID_REQ_SetReport:
			if (USB_ControlRequest.bmRequestType == (REQDIR_HOSTTODEVICE | REQTYPE_CLASS | REQREC_INTERFACE))
			{
				// We'll create a place to store our data received from the host.
				USB_JoystickReport_Output_t JoystickOutputData;
				// Since this is a control endpoint, we need to clear up the SETUP packet on this endpoint.
				Endpoint_ClearSETUP();
				// With our report available, we read data from the control stream.
				Endpoint_Read_Control_Stream_LE(&JoystickOutputData, sizeof(JoystickOutputData));
				// We then send an IN packet on this endpoint.
				Endpoint_ClearIN();
			}

			break;
	}
}

// Process and deliver data from IN and OUT endpoints.
void HID_Task(void) {
	// If the device isn't connected and properly configured, we can't do anything here.
	if (USB_DeviceState != DEVICE_STATE_Configured)
	  return;

	// We'll start with the OUT endpoint.
	Endpoint_SelectEndpoint(JOYSTICK_OUT_EPADDR);
	// We'll check to see if we received something on the OUT endpoint.
	if (Endpoint_IsOUTReceived())
	{
		// If we did, and the packet has data, we'll react to it.
		if (Endpoint_IsReadWriteAllowed())
		{
			// We'll create a place to store our data received from the host.
			USB_JoystickReport_Output_t JoystickOutputData;
			// We'll then take in that data, setting it up in our storage.
			Endpoint_Read_Stream_LE(&JoystickOutputData, sizeof(JoystickOutputData), NULL);
			// At this point, we can react to this data.
			// However, since we're not doing anything with this data, we abandon it.
		}
		// Regardless of whether we reacted to the data, we acknowledge an OUT packet on this endpoint.
		Endpoint_ClearOUT();
	}

	// We'll then move on to the IN endpoint.
	Endpoint_SelectEndpoint(JOYSTICK_IN_EPADDR);
	// We first check to see if the host is ready to accept data.
	if (Endpoint_IsINReady())
	{
		// We'll create an empty report.
		USB_JoystickReport_Input_t JoystickInputData;
		// We'll then populate this report with what we want to send to the host.
		GetNextReport(&JoystickInputData);
		// Once populated, we can output this data to the host. We do this by first writing the data to the control stream.
		Endpoint_Write_Stream_LE(&JoystickInputData, sizeof(JoystickInputData), NULL);
		// We then send an IN packet on this endpoint.
		Endpoint_ClearIN();

		/* Clear the report data afterwards */
		// memset(&JoystickInputData, 0, sizeof(JoystickInputData));
	}
}

// Prepare the next report for the host.
void GetNextReport(USB_JoystickReport_Input_t* const ReportData) {

	/* Clear the report contents */
	memset(ReportData, 0, sizeof(USB_JoystickReport_Input_t));
	ReportData->LX = STICK_CENTER;
	ReportData->LY = STICK_CENTER;
	ReportData->RX = STICK_CENTER;
	ReportData->RY = STICK_CENTER;
	ReportData->HAT = HAT_CENTER;
	ReportData->Button = SWITCH_RELEASE;



	ReportData->LX = lastLX;

	if((firstButtons>>2) & 0x01) {
		// printf("BUTTON: UNKNOWN\n\r");
	} 
	
	if((firstButtons>>3) & 0x01) {
		// printf("BUTTON: START\n\r");
		ReportData->Button |= SWITCH_START;
	}
	
	if(((firstButtons>>4) & 0x0f) == 1) {
		// printf("HAT: UP\n\r");
		ReportData->HAT = HAT_TOP;
		
	} else if(((firstButtons>>4) & 0x0f) == 2) {
		// printf("HAT: RIGHT\n\r");
		ReportData->HAT = HAT_RIGHT;
	} else if(((firstButtons>>4) & 0x0f) == 4) {
		ReportData->HAT = HAT_BOTTOM;
		// printf("HAT: DOWN\n\r");
	} else if(((firstButtons>>4) & 0x0f) == 8) {
		ReportData->HAT = HAT_LEFT;
		// printf("HAT: LEFT\n\r");
	}

	if(((secondButtons>>7) & 0x01) && ((secondButtons>>6) & 0x01)) {
		ReportData->Button |= SWITCH_L;
		ReportData->Button |= SWITCH_A;

	} else {
		if((secondButtons>>6) & 0x01) {
			// printf("HAT: X\n\r");
			ReportData->Button |= SWITCH_A;

		}
		
		if((secondButtons>>7) & 0x01) {
			// printf("HAT: SQUARE\n\r");
			ReportData->Button |= SWITCH_B;
		}

	}

	if((secondButtons>>2) & 0x01) {
		// printf("BUTTON: L\n\r");
		ReportData->Button |= SWITCH_L;

	} 
	
	if((secondButtons>>3) & 0x01) {
		// printf("BUTTON: R\n\r");
		ReportData->Button |= SWITCH_R;

	}
	
	if((secondButtons>>4) & 0x01) {
		// printf("HAT: TRIANGLE\n\r");
		ReportData->Button |= SWITCH_X;

	}
	
	if((secondButtons>>5) & 0x01) {
		// printf("HAT: CIRCLE\n\r");
		ReportData->Button |= SWITCH_Y;

	}
	
}
// vim: noexpandtab
