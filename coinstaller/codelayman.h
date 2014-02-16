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

#ifndef __CODELAYMAN_H
#define __CODELAYMAN_H

//
// Device name to search for
//
static const TCHAR DeviceName[] = TEXT( "ACPI Lid" );

//
// Filter service to add to LowerFilters
//
static const TCHAR FilterName[] = TEXT( "DelayMan" );

//
// Debug trace log filename
//
#define TRACELOG_FILE_NAME "C:\\DelayMan.log"

//
// Private functions
//
DWORD FindCompatibleDevices( BOOL Install );
DWORD SetupFilter( HDEVINFO hDevInfo, DWORD Index, BOOL Install );

#define SF_MATCH	1		// Current device is a match
#define SF_SKIP		0		// Current device is not a match
#define SF_LAST		-1		// No more devices to enumerate

#endif
