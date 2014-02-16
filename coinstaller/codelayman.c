///////////////////////////////////////////////////////////////////////////////
//
// DelayMan Co-installer
//   Co-installer for the DelayMan filter driver. This installer enumerates
//   all system button devices and adds the DelayMan driver to the LowerFilters
//   list, so that the driver can intercept IRPs.
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

#include <stdio.h> 
#include <stdlib.h> 
#include <stddef.h>
#include <windows.h>  
#include <initguid.h>
#include <devguid.h>
#include <setupapi.h>
#include <cfgmgr32.h>
#include <devioctl.h>  
#include <poclass.h>
#include <tchar.h>
#include <strsafe.h>
#include "codelayman.h"

//
// Only enable tracing in a debug build
//
#ifdef DBG
	#define LogMessage LogTrace
#else
	#define LogMessage
#endif

//
// Log tracer (based on work by Larry Osterman)
//
void LogTrace(LPCSTR FormatString, ...)
#define LAST_NAMED_ARGUMENT FormatString
{
    CHAR outputBuffer[4096];
    LPSTR outputString;
    size_t bytesRemaining;
    ULONG bytesWritten;
    BOOL traceLockHeld;
    HANDLE traceLogHandle;
    va_list parmPtr;                    // Pointer to stack parms.
	DWORD lengthToWrite;
	
	traceLockHeld = FALSE;
	traceLogHandle = NULL;
	bytesRemaining = sizeof(outputBuffer);
	outputString = outputBuffer;

    //EnterCriticalSection(&g_TraceLock);
    traceLockHeld = TRUE;
    //
    // Open the trace log file (may need a security descriptor to work correctly).
    //
    traceLogHandle = CreateFile(TRACELOG_FILE_NAME, FILE_APPEND_DATA, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (traceLogHandle == INVALID_HANDLE_VALUE)
    {
        goto Exit;
    }
    //
    // printf the information requested by the caller onto the buffer
    //
    va_start(parmPtr, FormatString);
    StringCbVPrintfEx(outputString, bytesRemaining, &outputString, &bytesRemaining, 0, FormatString, parmPtr);
    va_end(parmPtr);
    //
    // Actually write the bytes.
    //
    lengthToWrite = (DWORD)(sizeof(outputBuffer) - bytesRemaining);
    if (!WriteFile(traceLogHandle, outputBuffer, lengthToWrite, &bytesWritten, NULL))
    {
        goto Exit;
    }
    if (bytesWritten != lengthToWrite)
    {
        goto Exit;
    }
Exit:
    if (traceLogHandle)
    {
        CloseHandle(traceLogHandle);
    }
    if (traceLockHeld)
    {
        //LeaveCriticalSection(&g_TraceLock);
        traceLockHeld = FALSE;
    }
}

//
// Finds all suitable devices and installs the filters
//
DWORD FindCompatibleDevices( BOOL Install )
{
    DWORD Index = 0;

	//
	// Get the ACPI system button devices
	//
    HDEVINFO hDevInfo = SetupDiGetClassDevs(
                 &GUID_DEVICE_SYS_BUTTON,
                 NULL,
                 NULL,
                 DIGCF_PRESENT | DIGCF_DEVICEINTERFACE
                 );

    if( hDevInfo == INVALID_HANDLE_VALUE ) {
        LogMessage( "SetupDiGetClassDevs failed with error: %d\n", GetLastError() );
        return GetLastError();
    }

    //
    // Enumerate the devices
    //
    do
    {
        ++Index;
    } while( SetupFilter( hDevInfo, Index, Install ) != SF_LAST );
    LogMessage( "Enumerated %u devices\n", Index - 1 );

    SetupDiDestroyDeviceInfoList( hDevInfo );
    return NO_ERROR;
}

//
// Installs or uninstalls the DelayMan as LowerFilters for a device
//
DWORD SetupFilter( HDEVINFO hDevInfo, DWORD Index, BOOL Install )
{
	DWORD DataSize;
	TCHAR DeviceDesc[sizeof( DeviceName )];
	LPTSTR Data;

	SP_DEVINFO_DATA dataDevInfo;
	DWORD Error = ERROR_SUCCESS;
	
	//
	// Get the devinfo structure
	//
	dataDevInfo.cbSize = sizeof( SP_DEVINFO_DATA );
	if( SetupDiEnumDeviceInfo( hDevInfo, Index, &dataDevInfo ) == FALSE ) {
		Error = GetLastError();
		if( Error != ERROR_NO_MORE_ITEMS ) {
			LogMessage( "SetupDiEnumDeviceInfo failed with error: %d\n", Error );
		}
		return SF_LAST;
	}
	
	//
	// Get the device description property and compare it against our driver name
	//
	if( SetupDiGetDeviceRegistryProperty(
			hDevInfo,
			&dataDevInfo,
			SPDRP_DEVICEDESC,
			NULL,
			(PBYTE)DeviceDesc,
			sizeof( DeviceName ),
			&DataSize
			) == FALSE || _tcscmp( DeviceDesc, DeviceName ) != 0 ) {
		return SF_SKIP;
	}
	LogMessage( "Found matching driver '%s'\n", DeviceDesc );
	
	//
	// Get the LowerFilters property size
	//
	DataSize = 0;
	if( SetupDiGetDeviceRegistryProperty(
			hDevInfo,
			&dataDevInfo,
			SPDRP_LOWERFILTERS,
			NULL,
			NULL,
			0,
			&DataSize
			) == FALSE ) {
		Error = GetLastError();
	}

	//
	// Check whether we're installing or uninstalling
	//   Install:   append filter to list
	//   Uninstall: remove filter from list
	//
	if( Install == TRUE ) {
		//
		// Do the buffer allocation
		// Buffer contents: [Current contents] + [Our entry]
		//
		DataSize += sizeof( FilterName ) + 1;
		Data = (LPTSTR) LocalAlloc( LPTR, DataSize );
		
		//
		// Check if something useful was read
		//	
		if( Error == ERROR_SUCCESS || Error == ERROR_INSUFFICIENT_BUFFER ) {
			//
			// Read the property
			//
			SetupDiGetDeviceRegistryProperty(
					hDevInfo,
					&dataDevInfo,
					SPDRP_LOWERFILTERS,
					NULL,
					(PBYTE)Data,
					DataSize,
					&DataSize
					);
			_tcscat( Data, TEXT( " " ) );	// Append a space delimiter
		}
		else if( Error != ERROR_INVALID_DATA ) {
			LogMessage( "SetupDiGetDeviceRegistryProperty failed with error: %d\n", GetLastError() );
			return SF_SKIP;
		}
		
		//
		// Make sure the buffer doesn't already contain our filter,
		// otherwise we will append it to the current list
		//
		if( _tcsstr( Data, FilterName ) != NULL ) {
			// Filter already exists in the list
			LogMessage( "\tFilter was already installed\n" );
			return SF_SKIP;
		}
		_tcscat( Data, FilterName );
		LogMessage( "\tUpdating LowerFilters: %s\n", Data );
	} else {
		//
		// Just clear the entire property for now
		//
		Data = NULL;
		DataSize = 0;
		LogMessage( "\tRemoving LowerFilters\n" );
	}
		
	//
	// Set the LowerFilters device property
	//
	if( SetupDiSetDeviceRegistryProperty(
			hDevInfo,
			&dataDevInfo,
			SPDRP_LOWERFILTERS,
			(CONST PBYTE)Data,
			DataSize
			) == FALSE ) {
		LogMessage( "SetupDiSetDeviceRegistryProperty failed with error: %d\n", GetLastError() );
		return SF_SKIP;
	}

	LocalFree( Data );
	return SF_MATCH;
}

//
// Co-installer routine
//
DWORD
__stdcall CoInstFilter(
	   IN     DI_FUNCTION               InstallFunction,
	   IN     HDEVINFO                  DeviceInfoSet,
	   IN     PSP_DEVINFO_DATA          DeviceInfoData,
	   IN OUT PCOINSTALLER_CONTEXT_DATA Context
	   )
{
    DWORD Status = NO_ERROR;
	
	LogMessage( "CoInstFilter was called\n" );

    switch( InstallFunction )
	{
		case DIF_REMOVE:
			//
			// Remove DelayMan from the filters list
			//
			LogMessage( "Removing DelayMan device\n" );
			FindCompatibleDevices( FALSE );
			break;
        case DIF_INSTALLDEVICE:
			//
			// Add DelayMan to the filters list after it has been installed (post processing)
			//
            if( !Context->PostProcessing )
			{
				//
				// Let the installer know that we require post-processing
				//
				LogMessage( "Requiring post-processing\n" );
				Status = ERROR_DI_POSTPROCESSING_REQUIRED;                
            }
            else
            {
				//
				// We are now in post-processing mode. Make sure that the installation didn't
				// return any errors.
				//
                if( Context->InstallResult != NO_ERROR )
				{
                    Status = Context->InstallResult;
                    LogMessage( "Installation failed with error: 0x%x\n", Status );
                }
				else
				{
					Status = FindCompatibleDevices( TRUE );
				}
            }
        break;

    default:
        break;
    }

    return Status;
}
