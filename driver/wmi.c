///////////////////////////////////////////////////////////////////////////////
//
// wmi.c
//   DelayMan's WMI implementation for communication between usermode tools
//   and the driver.
//
// TODO
//   * Implement methods that the tool can call
//   * Implement items that the tool can set
//   * Implement a function that uses WdfWmiInstanceFireEvent to send events
//     to the tool (e.g. when the lid closes)
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

#include "wmi.h"

//
// Initializes the WMI instance
//
NTSTATUS WmiInitialize(
	IN WDFDEVICE Device
	)
{
	WDF_WMI_PROVIDER_CONFIG providerConfig;
	WDF_WMI_INSTANCE_CONFIG instanceConfig;
	WDFWMIINSTANCE wmi;
	NTSTATUS status;
	WDF_OBJECT_ATTRIBUTES attributes;
	PDELAYMAN_CONTEXT deviceContext;
	DECLARE_CONST_UNICODE_STRING( mofName, DELAYMAN_MOF );
	
	PAGED_CODE();
	
	//
	// Register WMI MOF stuff for this device
	//
	status = WdfDeviceAssignMofResourceName( Device, &mofName );
	if( !NT_SUCCESS( status ) ) {
		KdPrint( ("Could not assign MOF resource name (%x)\n", status) );
		return status;
	}
	
	//
	// Initialize the WMI provider config
	//
    WDF_WMI_PROVIDER_CONFIG_INIT( &providerConfig, &DelayManWMI_GUID );
	providerConfig.MinInstanceBufferSize = sizeof( DelayManWMI );
	
    WDF_WMI_INSTANCE_CONFIG_INIT_PROVIDER_CONFIG( &instanceConfig, &providerConfig );
    instanceConfig.Register = TRUE;
	instanceConfig.EvtWmiInstanceExecuteMethod = EvtWmiInstanceExecuteMethod;
    instanceConfig.EvtWmiInstanceSetItem = EvtWmiInstanceSetItem;
	
	//
	// Store our WMI class in the object attributes we're about to pass to the
	// new instance.
	//
	WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE( &attributes, DelayManWMI );

	//
	// Create the WMI instance
	//
	status = WdfWmiInstanceCreate(
					Device,
					&instanceConfig,
					&attributes,
					&wmi );
	if( !NT_SUCCESS( status ) ) {
		KdPrint( ("Could not create WMI instance (%x)\n", status) );
		return status;
	}
	
	//
	// Store the WMI instance in the device context so that the driver can use
	// it when appropriate.
	//
	deviceContext = GetContextFromDevice( Device );
	deviceContext->WmiInstance = wmi;
	
	return status;
}

//
// WMI callback that is called whenever a method is executed
//
NTSTATUS
EvtWmiInstanceExecuteMethod (
	IN WDFWMIINSTANCE WmiInstance,
	IN ULONG MethodId,
	IN ULONG InBufferSize,
	IN ULONG OutBufferSize,
	IN OUT PVOID Buffer,
	OUT PULONG BufferUsed
	)
{
    NTSTATUS status;
    DelayManWMI * pInfo;

    PAGED_CODE();
	
	KdPrint( ("EvtWmiInstanceExecuteMethod called\n") );

    pInfo = GetWMIFromInstance( WmiInstance );

	//
	// Handle the method
	//
	if( MethodId == stopShutdown ) {
		//
		// Cancels the delay timer
		//
		StopShutdown();
		status = STATUS_SUCCESS;
	} else {
		//
		// Unrecognized method
		//
		status = STATUS_WMI_ITEMID_NOT_FOUND;
	}
	return status;

}

//
// WMI callback that is called whenever the tool sets a single item
//
NTSTATUS
EvtWmiInstanceSetItem(
    IN  WDFWMIINSTANCE WmiInstance,
    IN  ULONG DataItemId,
    IN  ULONG InBufferSize,
    IN  PVOID InBuffer
    )
{
    NTSTATUS status;
    DelayManWMI * pInfo;

    PAGED_CODE();
	
	KdPrint( ("EvtWmiInstanceSetItem called\n") );

    pInfo = GetWMIFromInstance( WmiInstance );
	
	//
	// Handle the item
	//
	if( DataItemId == 1 ) {
		//
		// timerDelay: sets the new delay time and stops the shutdown
		//
        if( InBufferSize < DelayManWMI_timerDelay_SIZE ) {
			//
			// The supplied input buffer was too small to contain the value
			//
            status = STATUS_BUFFER_TOO_SMALL;
        } else {
			//
			// Set the delay and stop the shutdown
			//
			SetDelay( *(PULONG) InBuffer );
			StopShutdown();
			status = STATUS_SUCCESS;
		}
	} else {
		//
		// Unrecognized item
		//
		status = STATUS_INVALID_DEVICE_REQUEST;
	}

	return status;
}
