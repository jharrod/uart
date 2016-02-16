/******************************************************************************
*
* Copyright (C) 2009 - 2014 Xilinx, Inc.  All rights reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* Use of the Software is limited solely to applications:
* (a) running on a Xilinx device, or
* (b) that interact with a Xilinx device through a bus or interconnect.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* XILINX  BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
* WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
* OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*
* Except as contained in this notice, the name of the Xilinx shall not be used
* in advertising or otherwise to promote the sale, use or other dealings in
* this Software without prior written authorization from Xilinx.
*
******************************************************************************/

/*
 * helloworld.c: simple test application
 *
 * This application configures UART 16550 to baud rate 9600.
 * PS7 UART (Zynq) is not initialized by this application, since
 * bootrom/bsp configures it to baud rate 115200
 *
 * ------------------------------------------------
 * | UART TYPE   BAUD RATE                        |
 * ------------------------------------------------
 *   uartns550   9600
 *   uartlite    Configurable only in HW design
 *   ps7_uart    115200 (configured by bootrom/bsp)
 */
#include <string.h>
#include <stdio.h>
#include "platform.h"
#include "xuartps.h"
#include "xparameters.h"
#include "xparameters_ps.h"
#include "platform_config.h"
#include "xstatus.h"
#include "xil_exception.h"
#include "xscugic.h" // try adding interrupt controller

/************************** Constant Definitions **************************/

/*
 * The following constants map to the XPAR parameters created in the
 * xparameters.h file. They are defined here such that a user can easily
 * change all the needed parameters in one place.
 */

#define UART_DEVICE_ID 0   //also in platform_config.h
#define INTC_DEVICE_ID		XPAR_SCUGIC_SINGLE_DEVICE_ID //xparameters_ps.h
#define UART_INT_IRQ_ID		XPAR_XUARTPS_1_INTR //xparameters_ps.h

/*
 * The following constant controls the length of the buffers to be sent
 * and received with the UART,
 */
#define TEST_BUFFER_SIZE	3

/************************** Function Prototypes *****************************/
int UartPsIntrExample(XScuGic *IntcInstPtr, XUartPs *UartInstPtr,
			u16 DeviceId, u16 UartIntrId);

static int SetupInterruptSystem(XScuGic *IntcInstancePtr,
				XUartPs *UartInstancePtr,
				u16 UartIntrId);

void Handler(void *CallBackRef, u32 Event, unsigned int EventData);


/************************** Variable Definitions ***************************/


XUartPs uart	;		/* Instance of the UART Device */
XScuGic intc;	/* Instance of the Interrupt Controller */

/*
 * The following buffers are used in this example to send and receive data
 * with the UART.
 */
static u8 SendBuffer[TEST_BUFFER_SIZE];	/* Buffer for Transmitting Data */
static u8 RecvBuffer[TEST_BUFFER_SIZE];	/* Buffer for Receiving Data */
static u8 ch[TEST_BUFFER_SIZE];
volatile int send = 0;

/*
 * The following counters are used to determine when the entire buffer has
 * been sent and received.
 */
volatile int TotalReceivedCount;
volatile int TotalSentCount;
volatile int data2Read = 0;
int TotalErrorCount;
volatile int count = 0;

//void print(char *str);

int main()
{
	char buff[4];
    init_platform();




    int Status;

	XUartPs_Config *Config;
	int Index;
	u32 IntrMask = 0;
	int BadByteCount = 0;

    if (XGetPlatform_Info() == XPLAT_ZYNQ_ULTRA_MP) {
    	#ifdef XPAR_XUARTPS_1_DEVICE_ID
    		DeviceId = XPAR_XUARTPS_1_DEVICE_ID;
    		printf("IFDEF\n")
    	#endif
    }

    	/*
    	 * Initialize the UART driver so that it's ready to use
    	 * Look up the configuration in the config table, then initialize it.
    	 */
    	Config = XUartPs_LookupConfig(UART_DEVICE_ID);
    	if (NULL == Config) {
    		return XST_FAILURE;
    	}

    	Status = XUartPs_CfgInitialize(&uart, Config, Config->BaseAddress);
    	if (Status != XST_SUCCESS) {
    		return XST_FAILURE;
    	}

    	/* Check hardware build */
    	Status = XUartPs_SelfTest(&uart);
    	if (Status != XST_SUCCESS) {
    		return XST_FAILURE;
    	}

    	/*
    	 * Connect the UART to the interrupt subsystem such that interrupts
    	 * can occur. This function is application specific.
    	 */
    	Status = SetupInterruptSystem(&intc, &uart, UART_INT_IRQ_ID);
    	if (Status != XST_SUCCESS) {
    		return XST_FAILURE;
    	}

    	/*
    	 * Setup the handlers for the UART that will be called from the
    	 * interrupt context when data has been sent and received, specify
    	 * a pointer to the UART driver instance as the callback reference
    	 * so the handlers are able to access the instance data
    	 */
    	XUartPs_SetHandler(&uart, (XUartPs_Handler)Handler, &uart);

    	/*
    	 * Enable the interrupt of the UART so interrupts will occur, setup
    	 * a local loopback so data that is sent will be received.
    	 */
    	IntrMask =
    		XUARTPS_IXR_TOUT | XUARTPS_IXR_PARITY | XUARTPS_IXR_FRAMING |
    		XUARTPS_IXR_OVER | XUARTPS_IXR_TXEMPTY | XUARTPS_IXR_RXFULL |
    		XUARTPS_IXR_RXOVR;

    	if (uart.Platform == XPLAT_ZYNQ_ULTRA_MP) {
    		IntrMask |= XUARTPS_IXR_RBRK;
    	}

    	XUartPs_SetInterruptMask(&uart, IntrMask);

    	//XUartPs_SetOperMode(UartInstPtr, XUARTPS_OPER_MODE_LOCAL_LOOP);
    	/* Set the UART in Normal Mode */
    	XUartPs_SetOperMode(&uart, XUARTPS_OPER_MODE_NORMAL);

    	/*
    	 * Set the receiver timeout. If it is not set, and the last few bytes
    	 * of data do not trigger the over-water or full interrupt, the bytes
    	 * will not be received. By default it is disabled.
    	 *
    	 * The setting of 8 will timeout after 8 x 4 = 32 character times.
    	 * Increase the time out value if baud rate is high, decrease it if
    	 * baud rate is low.
    	 */
    	//XUartPs_SetRecvTimeout(&uart, 8);



    	/* Run the UartPs Interrupt example, specify the the Device ID */
    	Status = UartPsIntrExample(&intc, &uart, //currently sets up some of uart.Need to pull out what is needed
    				UART_DEVICE_ID, UART_INT_IRQ_ID);

    	/* if (Status != XST_SUCCESS) {
    		xil_printf("UART Interrupt Example Test Failed\r\n");
    		return XST_FAILURE;
    	} */
    	printf("Hello World\n\r");
    	printf("\n\rhello\n\r");

    	xil_printf("Successfully ran UART Interrupt Example Test\r\n");
    	xil_printf("count = %i\r\n", count);
    	return XST_SUCCESS;



    cleanup_platform();
    return 0;
}






/**************************************************************************/
/**
*
* This function does a minimal test on the UartPS device and driver as a
* design example. The purpose of this function is to illustrate
* how to use the XUartPs driver.
*
* This function sends data and expects to receive the same data through the
* device using the local loopback mode.
*
* This function uses interrupt mode of the device.
*
* @param	IntcInstPtr is a pointer to the instance of the Scu Gic driver.
* @param	UartInstPtr is a pointer to the instance of the UART driver
*		which is going to be connected to the interrupt controller.
* @param	DeviceId is the device Id of the UART device and is typically
*		XPAR_<UARTPS_instance>_DEVICE_ID value from xparameters.h.
* @param	UartIntrId is the interrupt Id and is typically
*		XPAR_<UARTPS_instance>_INTR value from xparameters.h.
*
* @return	XST_SUCCESS if successful, otherwise XST_FAILURE.
*
* @note
*
* This function contains an infinite loop such that if interrupts are not
* working it may never return.
*
**************************************************************************/
int UartPsIntrExample(XScuGic *IntcInstPtr, XUartPs *UartInstPtr,
			u16 DeviceId, u16 UartIntrId)
{
	int Index;
	u32 IntrMask = 0;
	int BadByteCount = 0;


	/*
	 * Initialize the send buffer bytes with a pattern and the
	 * the receive buffer bytes to zero to allow the receive data to be
	 * verified
	 */
//	for (Index = 0; Index < TEST_BUFFER_SIZE; Index++) {
//
//		SendBuffer[Index] = (Index % 26) + 'A';
//
//		RecvBuffer[Index] = 0;
//	}

	/*
	 * Start receiving data before sending it since there is a loopback,
	 * ignoring the number of bytes received as the return value since we
	 * know it will be zero
	 */
	//XUartPs_Recv(UartInstPtr, RecvBuffer, TEST_BUFFER_SIZE);

	/*
	 * Send the buffer using the UART and ignore the number of bytes sent
	 * as the return value since we are using it in interrupt mode.
	 */
	//XUartPs_Send(UartInstPtr, SendBuffer, TEST_BUFFER_SIZE);

	/*
	 * Wait for the entire buffer to be received, letting the interrupt
	 * processing work in the background, this function may get locked
	 * up in this loop if the interrupts are not working correctly.
	 */
/*	while (1) {
		if ((TotalSentCount == TEST_BUFFER_SIZE) &&
		    (TotalReceivedCount == TEST_BUFFER_SIZE)) {
			break;
		}
	} */

	/* Verify the entire receive buffer was successfully received */
/*	for (Index = 0; Index < TEST_BUFFER_SIZE; Index++) {
		if (RecvBuffer[Index] != SendBuffer[Index]) {
			BadByteCount++;
		}
	}

	*/

	//echo
	//while (ch != 'q' || ch != 'Q') {
		//XUartPs_Recv(UartInstPtr, ch, TEST_BUFFER_SIZE);

		if (send == 1){
			XUartPs_Send(UartInstPtr, ch, TEST_BUFFER_SIZE);
			printf("here\r\n");
			send = 0;
		}
	//}


	/* Set the UART in Normal Mode */
	//XUartPs_SetOperMode(UartInstPtr, XUARTPS_OPER_MODE_NORMAL);


	/* If any bytes were not correct, return an error */
	if (BadByteCount != 0) {
		return XST_FAILURE;
	}

	return XST_SUCCESS;
}

/**************************************************************************/
/**
*
* This function is the handler which performs processing to handle data events
* from the device.  It is called from an interrupt context. so the amount of
* processing should be minimal.
*
* This handler provides an example of how to handle data for the device and
* is application specific.
*
* @param	CallBackRef contains a callback reference from the driver,
*		in this case it is the instance pointer for the XUartPs driver.
* @param	Event contains the specific kind of event that has occurred.
* @param	EventData contains the number of bytes sent or received for sent
*		and receive events.
*
* @return	None.
*
* @note		None.
*
***************************************************************************/
void Handler(void *CallBackRef, u32 Event, unsigned int EventData)
{
	/* All of the data has been sent */
	if (Event == XUARTPS_EVENT_SENT_DATA) {
		TotalSentCount = EventData;
	}

	/* All of the data has been received */
	if (Event == XUARTPS_EVENT_RECV_DATA) {
		TotalReceivedCount = EventData;
		send = 1;
	}

	/*
	 * Data was received, but not the expected number of bytes, a
	 * timeout just indicates the data stopped for 8 character times
	 */
	if (Event == XUARTPS_EVENT_RECV_TOUT) {
		TotalReceivedCount = EventData;
	}

	/*
	 * Data was received with an error, keep the data but determine
	 * what kind of errors occurred
	 */
	if (Event == XUARTPS_EVENT_RECV_ERROR) {
		TotalReceivedCount = EventData;
		TotalErrorCount++;
	}

	/*
	 * Data was received with an parity or frame or break error, keep the data
	 * but determine what kind of errors occurred. Specific to Zynq Ultrascale+
	 * MP.
	 */
	if (Event == XUARTPS_EVENT_PARE_FRAME_BRKE) {
		TotalReceivedCount = EventData;
		TotalErrorCount++;
	}

	/*
	 * Data was received with an overrun error, keep the data but determine
	 * what kind of errors occurred. Specific to Zynq Ultrascale+ MP.
	 */
	if (Event == XUARTPS_EVENT_RECV_ORERR) {
		TotalReceivedCount = EventData;
		TotalErrorCount++;
	}
}


/*****************************************************************************/
/**
*
* This function sets up the interrupt system so interrupts can occur for the
* Uart. This function is application-specific. The user should modify this
* function to fit the application.
*
* @param	IntcInstancePtr is a pointer to the instance of the INTC.
* @param	UartInstancePtr contains a pointer to the instance of the UART
*		driver which is going to be connected to the interrupt
*		controller.
* @param	UartIntrId is the interrupt Id and is typically
*		XPAR_<UARTPS_instance>_INTR value from xparameters.h.
*
* @return	XST_SUCCESS if successful, otherwise XST_FAILURE.
*
* @note		None.
*
****************************************************************************/
static int SetupInterruptSystem(XScuGic *IntcInstancePtr,
				XUartPs *UartInstancePtr,
				u16 UartIntrId)
{
	int Status;

#ifndef TESTAPP_GEN
	XScuGic_Config *IntcConfig; /* Config for interrupt controller */

	/* Initialize the interrupt controller driver */
	IntcConfig = XScuGic_LookupConfig(INTC_DEVICE_ID);
	if (NULL == IntcConfig) {
		return XST_FAILURE;
	}

	Status = XScuGic_CfgInitialize(IntcInstancePtr, IntcConfig,
					IntcConfig->CpuBaseAddress);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	/*
	 * Connect the interrupt controller interrupt handler to the
	 * hardware interrupt handling logic in the processor.
	 */
	Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT,
				(Xil_ExceptionHandler) XScuGic_InterruptHandler,
				IntcInstancePtr);
#endif

	/*
	 * Connect a device driver handler that will be called when an
	 * interrupt for the device occurs, the device driver handler
	 * performs the specific interrupt processing for the device
	 */
	Status = XScuGic_Connect(IntcInstancePtr, UartIntrId,
				  (Xil_ExceptionHandler) XUartPs_InterruptHandler,
				  (void *) UartInstancePtr);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	/* Enable the interrupt for the device */
	XScuGic_Enable(IntcInstancePtr, UartIntrId);


#ifndef TESTAPP_GEN
	/* Enable interrupts */
	 Xil_ExceptionEnable();
#endif

	return XST_SUCCESS;
}
