//{{NO_DEPENDENCIES}}
// Microsoft Visual C++ generated include file.
// Used by dxapim.rc


// TODO: Add SVN version updating
#include "version.h"
#define VER_SVN_VERSION SVN_VERSION
#define VER_SVN_VERSION_STR SVN_VERSION_STR

#define VER_FILEVERSION             1,0,0,VER_SVN_VERSION
#define VER_FILEVERSION_STR         "1.0.0." VER_SVN_VERSION_STR

#define VER_PRODUCTVERSION          1,0,0,VER_SVN_VERSION
#define VER_PRODUCTVERSION_STR      "1.0.D0 " VER_SVN_VERSION_STR

#define VER_PRIVATEBUILD VS_FF_PRIVATEBUILD 
#define VER_PRERELEASE VS_FF_PRERELEASE

#define VER_FILEDESCRIPTION_STR "Timebase Client library (" VER_BUILD_CONFIGURATION_STR ")"
#define VER_INTERNALNAME_STR "dxc"
#define VER_PRODUCTNAME_STR "QuantServer"

#define VER_COMPANYNAME_STR "Deltix Inc."
#define VER_LEGALCOPYRIGHT_STR "Copyright (C) Deltix Inc. 2015-2016"
#define VER_LEGALTRADEMARKS1_STR ""
#define VER_LEGALTRADEMARKS2_STR ""
#define VER_ORIGINALFILENAME_STR VER_INTERNALNAME_STR ".dll"


#define DX_PLATFORM_WINDOWS 1
#if (_MANAGED == 1) || (_M_CEE == 1)
// Managed windows environment
#define DX_PLATFORM_MANAGED
#define DX_PLATFORM_WINDOWS_MANAGED
#else
// Native windows environment
#define DX_PLATFORM_NATIVE 1
#define DX_PLATFORM_WINDOWS_NATIVE 1
#endif // (_MANAGED == 1) || (_M_CEE == 1)

#if !defined(_M_X64)
#define DX_PLATFORM_32 1
#else
#define DX_PLATFORM_64 1
// const assert for word size
#endif


#ifdef APSTUDIO_INVOKED
#ifndef APSTUDIO_READONLY_SYMBOLS
#define _APS_NEXT_RESOURCE_VALUE        101
#define _APS_NEXT_COMMAND_VALUE         40001
#define _APS_NEXT_CONTROL_VALUE         1001
#define _APS_NEXT_SYMED_VALUE           101
#endif
#endif


#if defined(DEBUG) || defined(_DEBUG)
#if DX_PLATFORM_64
#define VER_BUILD_CONFIGURATION_STR "Debug-x64"
#else
#define VER_BUILD_CONFIGURATION_STR "Debug-x86"
#endif
#define VER_DEBUG                   VS_FF_DEBUG
#else
#if DX_PLATFORM_64
#define VER_BUILD_CONFIGURATION_STR "Release-x64"
#else
#define VER_BUILD_CONFIGURATION_STR "Release-x86"
#endif
#define VER_DEBUG                   0
#endif


//#define VER_FILEFLAGS (VER_PRIVATEBUILD | VER_PRERELEASE | VER_DEBUG)
#if defined(DEBUG) || defined(_DEBUG)
#define VER_FILEFLAGS2 (VER_PRIVATEBUILD | VER_PRERELEASE | VER_DEBUG)
#else
#define VER_FILEFLAGS2 (VER_PRIVATEBUILD | VER_PRERELEASE)
#endif

