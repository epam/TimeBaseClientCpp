// Contains minumum amount of public includes from windows.h that HAS to have global visibility

typedef unsigned long       DWORD;
#if defined(_WIN64)
    typedef unsigned __int64 SOCKET;
#else
    typedef unsigned int SOCKET;

#endif

#define INVALID_SOCKET  (SOCKET)(~0)

extern "C" __declspec(dllimport) int __stdcall IsDebuggerPresent();