///////////////////////////////////////////////////////////////////////////////
//
// delayman.h
//   Header file for the DelayMan driver.
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

#ifndef __DELAYMAN_H
#define __DELAYMAN_H

#include <ntddk.h> 
#include <wdf.h>
#include <initguid.h>
#include <wdmguid.h>

#include "wmi.h"
#include "delayman_mof.h"	// Generated by the compiler

//
// Driver version
//
#define DELAYMAN_VERSION "1.0"

//
// Default delay time (seconds)
//
#define DELAYMAN_DEFAULT_DELAY_TIME	25

//
// Context stuff
//
typedef struct _DELAYMAN_CONTEXT
{
	//
	// Store the WMI instance so we can use it for communication
	// to the systray tool.
	//
    WDFWMIINSTANCE WmiInstance;

} DELAYMAN_CONTEXT, *PDELAYMAN_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME( DELAYMAN_CONTEXT, GetContextFromDevice );
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME( DelayManWMI, GetWMIFromInstance );

//
// Function declarations
//
NTSTATUS		DispatchIOCTLRequest( IN WDFIOTARGET Target, IN WDFREQUEST OldRequest );
VOID			StopShutdown( VOID );
VOID			SetDelay( ULONG Seconds );
LARGE_INTEGER	GetDelay( VOID );

//
// WDF functions and event callbacks
//
DRIVER_INITIALIZE					DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD			EvtDeviceAdd;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL	EvtIoDeviceControl;
EVT_WDF_REQUEST_COMPLETION_ROUTINE	EvtCompletionRoutine;
EVT_WDF_DRIVER_UNLOAD				EvtUnload;

//////////////////////////////////////////////////////////////////////////////
// Undocumented NT functions
//////////////////////////////////////////////////////////////////////////////
//
// Microsoft are idiots for only exporting this function recently.
// This stuff is only supported on NT6 and higher.
//
BOOLEAN
KeAlertThread (
    __inout PKTHREAD Thread,
    __in KPROCESSOR_MODE ProcessorMode
    );

#endif
