///////////////////////////////////////////////////////////////////////////////
//
// delayman.c
//   A KMDF filter driver that delays the NT shutdown process initiated by any
//   of the system power buttons.
//
// TODO
//   * Check if the timer was cancelled
//   * Verify compatibility on newer platforms
//
///////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2009, Bastage Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//     * Neither the name of Bastage Inc. nor the
//       names of its contributors may be used to endorse or promote products
//       derived from this software without specific prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY Bastage Inc. ''AS IS'' AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL Bastage Inc. BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
///////////////////////////////////////////////////////////////////////////////
//
// Whenever the system boots, the NT power manager sends out a
// IOCTL_GET_SYS_BUTTON_CAPS request to the driver stack to figure out which
// system buttons are available on the machine. The request gets handled by any
// suitable driver that will in turn set the buffer contents using the
// following (bitwise ORed) ULONG constants:
//     SYS_BUTTON_POWER 0x00000001 // Power button
//     SYS_BUTTON_SLEEP 0x00000002 // Sleep button
//     SYS_BUTTON_LID   0x00000004 // Lid button
//     SYS_BUTTON_WAKE  0x80000000 // Wake button (any key)
//
// After the IOCTL_GET_SYS_BUTTON_CAPS travels back to the power manager, the
// power manager sends out a IOCTL_GET_SYS_BUTTON_EVENT request (if any system
// buttons were available). By registering a completion routine for this
// request, it is put on hold while the system continues the booting process.
//
// The IOCTL_GET_SYS_BUTTON_EVENT request is pended and left unanswered until
// a system button state change occurs (e.g. lid or power button is pressed).
// As soon as a button state changes, the ACPI BIOS fires an interrupt (or
// something alike) and the ACPI Lid driver will know and will complete the
// pended request (modifying the request buffer along the way, indicating
// which button was pressed using the above constants). Eventually, the routine
// that was registered earlier by the NT power manager is called and a power
// state change is issued putting the system to sleep.
//
// http://blogs.msdn.com/doronh/archive/2006/09/08/746961.aspx
//
///////////////////////////////////////////////////////////////////////////////

#include "delayman.h"
#include "poclass.h"

//
// Delay time variable
//
LARGE_INTEGER s_DelayTime;

//
// Handle to the thread currently suspended in EvtCompletionRoutine
//
HANDLE s_DelayThread;
#define INVALID_HANDLE_VALUE NULL

///////////////////////////////////////////////////////////////////////////////
// Driver functions
///////////////////////////////////////////////////////////////////////////////
NTSTATUS
DriverEntry(
	PDRIVER_OBJECT DriverObj,
	PUNICODE_STRING RegistryPath
	)
/*++

Routine Description:

    Installable driver initialization entry point.
    This entry point is called directly by the I/O system.

Arguments:

    DriverObject - pointer to the driver object

    RegistryPath - pointer to a unicode string representing the path,
                   to driver-specific key in the registry.

Return Value:

    STATUS_SUCCESS if successful,
    STATUS_UNSUCCESSFUL otherwise.

--*/
{
	NTSTATUS status;
	WDF_DRIVER_CONFIG driverConfig;

	KdPrint( ("\nDelayMan v" DELAYMAN_VERSION " -- Compiled %s %s\n", __DATE__, __TIME__) );

	//
	// Initialize the driver object (and set the DeviceAdd and Unload functions)
	//
	WDF_DRIVER_CONFIG_INIT( &driverConfig, EvtDeviceAdd );
	driverConfig.EvtDriverUnload = EvtUnload;

	//
	// Create a WDFDRIVER object
	//
	// We specify no object attributes, because we do not need a cleanup
	// or destroy event callback, or any per-driver context.
	//
	status = WdfDriverCreate( DriverObj,
							  RegistryPath,
							  WDF_NO_OBJECT_ATTRIBUTES,
							  &driverConfig,
							  NULL
							  );
	if( !NT_SUCCESS(status) ) {
		KdPrint( ("WdfDriverCreate failed with status 0x%0x\n", status) );
	}

	return status;
}

NTSTATUS
EvtDeviceAdd(
	WDFDRIVER Driver,
	PWDFDEVICE_INIT DeviceInit
	)
/*++
Routine Description:

    EvtDeviceAdd is called by the framework in response to AddDevice
    call from the PnP manager. Here we can initialize the driver as
	filter and do any additional initialization.

Arguments:

    Driver - Handle to a framework driver object created in DriverEntry

    DeviceInit - Pointer to a framework-allocated WDFDEVICE_INIT structure.

Return Value:

    NTSTATUS

--*/
{
	NTSTATUS status;
	WDF_OBJECT_ATTRIBUTES objAttributes;
	WDFDEVICE device;
	WDF_IO_QUEUE_CONFIG ioCallbacks;

	UNREFERENCED_PARAMETER( Driver );

	//
	// Create our Device Object and its associated context
	//
	WDF_OBJECT_ATTRIBUTES_INIT( &objAttributes );
	WDF_OBJECT_ATTRIBUTES_SET_CONTEXT_TYPE( &objAttributes, DELAYMAN_CONTEXT );
	
	//
	// Set the default static variables
	//
	SetDelay( DELAYMAN_DEFAULT_DELAY_TIME );
	s_DelayThread = INVALID_HANDLE_VALUE;

	//
	// Let the WDF know that our driver is a filter, so it handles the requests properly
	//
	WdfFdoInitSetFilter( DeviceInit );

	//
	// Create the device
	//
	status = WdfDeviceCreate( &DeviceInit,
							  &objAttributes,
							  &device
							  );
	if( !NT_SUCCESS(status) ) {
		KdPrint( ("WdfDeviceInitialize failed 0x%0x\n", status) );
		return status;
	}

	//
	// Initialize the WMI instance
	//
	WmiInitialize( device );
	
	//
	// Set up a queue for incoming requests. All requests will be handled in parallel.
	//
	WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE( &ioCallbacks, WdfIoQueueDispatchParallel );

	//
	// Set up the callback to intercept IOCTLs meant for the ACPI Lid driver
	//
	ioCallbacks.EvtIoDeviceControl = EvtIoDeviceControl;

	status = WdfIoQueueCreate( device,
							   &ioCallbacks,
							   WDF_NO_OBJECT_ATTRIBUTES,
							   NULL
							   );

	if (!NT_SUCCESS(status)) {
		KdPrint( ("WdfIoQueueCreate for default queue failed 0x%0x\n", status) );
		return status;
	}
	return status;
}

VOID
EvtIoDeviceControl(
	WDFQUEUE Queue,
    WDFREQUEST Request,
    size_t OutputBufferLength,
    size_t InputBufferLength,
    ULONG IoControlCode
	)
/*++
Routine Description:

    This event is called when the framework receives IRP_MJ_DEVICE_CONTROL
    requests from the system.

Arguments:

    Queue - Handle to the framework queue object that is associated
            with the I/O request.
    Request - Handle to a framework request object.

    OutputBufferLength - length of the request's output buffer,
                        if an output buffer is available.
    InputBufferLength - length of the request's input buffer,
                        if an input buffer is available.

    IoControlCode - the driver-defined or system-defined I/O control code
                    (IOCTL) that is associated with the request.

Return Value:

   VOID

--*/
{
	WDFIOTARGET Target;
	WDF_REQUEST_SEND_OPTIONS options;
    NTSTATUS code;

    UNREFERENCED_PARAMETER( OutputBufferLength );
    UNREFERENCED_PARAMETER( InputBufferLength );

	KdPrint( ("Got IOCTL 0x%x\n", IoControlCode) );

	//
	// Get the I/O target for this request
	//
	Target = WdfDeviceGetIoTarget( WdfIoQueueGetDevice( Queue ) );
	
	if( IoControlCode == IOCTL_GET_SYS_BUTTON_EVENT ) {
		KdPrint( ("Received system button event\n") );

		//
		// At this point, the NT power manager has sent the IOCTL_GET_SYS_BUTTON_EVENT request
		// to the stack. In any normal situation it would eventually reach the ACPI Lid driver but
		// since our filter driver is stuck between the ACPI Lid driver and the rest of the stack,
		// we get the chance to do something with it first.
		//
		// We cannot reuse or modify the request we received, so at this point we have to create
		// an identical request and send it to the original target.
		DispatchIOCTLRequest( Target, Request );
	}
	else {
		KdPrint( ("Forwarding capabilities IOCTL\n") );

		//
		// Let the WDF copy the existing request so that we can forward it to
		// the original target.
		//
		WdfRequestFormatRequestUsingCurrentType( Request );
		WDF_REQUEST_SEND_OPTIONS_INIT( &options, WDF_REQUEST_SEND_OPTION_SEND_AND_FORGET );
		if( !WdfRequestSend( Request, Target, &options ) )
		{
			KdPrint( ("Could not forward request\n") );
			WdfRequestCompleteWithInformation( Request, STATUS_SUCCESS, 0 );
		}
	}
}

VOID
EvtCompletionRoutine(
    IN WDFREQUEST  Request,
    IN WDFIOTARGET  Target,
    IN PWDF_REQUEST_COMPLETION_PARAMS  Params,
    IN WDFCONTEXT  Context
    )
/*++

Routine Description:

    Completion routine

Arguments:

    Target - Target handle
    Request - Request handle
    Params - Request completion params
    Context - Driver supplied context


Return Value:

    VOID

--*/
{
	WDFREQUEST					OldRequest;
	WDF_REQUEST_REUSE_PARAMS	params;
	NTSTATUS					status;
	
	//
	// At this point, we _know_ that a system button event has occured.
	// So since we're below the ACPI Lid driver, we know that the lid button
	// state has changed.
	//
	// Theoretically, this routine is exclusive and is never running more than
	// once in parallel.
	//
	
	//
	// Retrieve the original NT power manager request
	//
	OldRequest = (WDFREQUEST)Context;
	
	KdPrint( ("Shutdown requested and pending.\n") );

	//
	// Save the current thread (so it can be alerted/interrupted) and wait for
	// the timer to finish.
	//
	s_DelayThread = KeGetCurrentThread();
	status = KeDelayExecutionThread( KernelMode, TRUE, &s_DelayTime );
	s_DelayThread = INVALID_HANDLE_VALUE;
	
	//
	// Mark the request as reusable. We cannot destroy it here because the buffer is really owned
	// by the original NT power manager request. Reusing removes the reference to the buffer
	// (WDFMEMORY object) but does not delete it. This is left to the code that created the
	// original request.
	//
	WDF_REQUEST_REUSE_PARAMS_INIT( &params, WDF_REQUEST_REUSE_NO_FLAGS, STATUS_SUCCESS );
	WdfRequestReuse( Request, &params );

	if( NT_SUCCESS( status ) ) {
		//
		// Complete the original request and let the power manager continue
		// the shutdown.
		//
		WdfRequestComplete( OldRequest, STATUS_SUCCESS );
	} else {
		//
		// Thread got interrupted so forget about completing the original
		// request.
		//
		KdPrint( ("Shutdown got interrupted and cancelled\n") );
	}

#if 0
	//
	// Send out a new IOCTL request to track new system button events
	// ACHTUNG: Causes infinite loop / freeze / BSOD
	//
	DispatchIOCTLRequest( Target, OldRequest );
#endif
}

VOID
EvtUnload(
    WDFDRIVER Driver
    )
/*++

Routine Description:

    Unload routine, called by the framework before a driver is unloaded.

Arguments:

    Driver - Handle to a framework driver object created in DriverEntry

Return Value:

    VOID

--*/
{
	//
	// TODO: Do any cleaning up
	//
}

///////////////////////////////////////////////////////////////////////////////
// Private functions
///////////////////////////////////////////////////////////////////////////////

//
// Interrupts and cancels the delay in EvtCompletionRoutine and aborts the
// shutdown (NT6 and higher only)
//
VOID
StopShutdown( VOID )
{
	//
	// Alert the delay thread (if there is any)
	//
	if( s_DelayThread != INVALID_HANDLE_VALUE ) {
		#if _WIN32_WINNT >= 0x0600
			KeAlertThread( s_DelayThread, KernelMode );
		#else
			KdPrint( ("Sorry, unsupported thanks to Microsoft.") );
		#endif
	}
}

//
// Sets the timer delay in seconds
//
VOID
SetDelay( ULONG Seconds )
{
	//
	// TODO: Make atomic
	//
	s_DelayTime.QuadPart = WDF_REL_TIMEOUT_IN_SEC( Seconds );
}

//
// Gets the timer delay in system time units
//
LARGE_INTEGER
GetDelay( VOID )
{	
	//
	// TODO: Make atomic
	//
	return s_DelayTime;
}

//
// Creates and dispatches the IOCTL_GET_SYS_BUTTON_EVENT request
//
NTSTATUS
DispatchIOCTLRequest(
	IN WDFIOTARGET Target,
	IN WDFREQUEST OldRequest
	)
{
	WDFREQUEST NewRequest;
	WDF_OBJECT_ATTRIBUTES attributes;
	WDFMEMORY OutputBuffer, InputBuffer;
	NTSTATUS status;

	//
	// Create and initialize the request
	//
	WDF_OBJECT_ATTRIBUTES_INIT( &attributes );
	attributes.ParentObject = Target;
	
	status = WdfRequestCreate( &attributes, Target, &NewRequest );
	if( !NT_SUCCESS( status ) ) {
		KdPrint( ("Could not create new request (error %x)\n", status) );
		return status;
	}
	
	//
	// The new request also needs I/O buffers (which will contain the relevant system button ids).
	// Instead of creating a new one, we will assign the original request's buffers to the new
	// request, so that any system button event values are left intact.
	// http://www.microsoft.com/whdc/driver/tips/WDFmem.mspx (Scenario 3)
	//
	InputBuffer = OutputBuffer = NULL;
	WdfRequestRetrieveInputMemory( OldRequest, &InputBuffer );
	WdfRequestRetrieveOutputMemory( OldRequest, &OutputBuffer );
	status = WdfIoTargetFormatRequestForIoctl( Target,
											   NewRequest,
											   IOCTL_GET_SYS_BUTTON_EVENT,
											   InputBuffer, NULL,
											   OutputBuffer, NULL
											   );
	if( !NT_SUCCESS( status ) ) {
		KdPrint( ("Could not format proxy request for IOCTL (error %x)\n", status) );
		return status;
	}

	//
	// Set our own completion routine so that we get notified whenever a system button event
	// occurs. We also pass the original request on to the completion routine, so that we can
	// eventually complete it ourselves (triggering the actual shutdown).
	//
	WdfRequestSetCompletionRoutine( NewRequest, EvtCompletionRoutine, OldRequest );
	
	//
	// Send the request to the ACPI Lid driver
	//
	if( !WdfRequestSend( NewRequest, Target, WDF_NO_SEND_OPTIONS ) ) {
		KdPrint( ("Could not send proxy request\n") );
		return STATUS_UNSUCCESSFUL;
	}
	return STATUS_SUCCESS;
}
