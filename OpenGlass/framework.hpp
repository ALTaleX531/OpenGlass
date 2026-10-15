#pragma once

#pragma warning(push)
#pragma warning(disable : 4201 4324 4471)
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
	#define NOMINMAX
#endif
#include <KNSoft/NDK/NDK.h>
#include <windows.h>
#include <windowsx.h>

#include <sddl.h>
#include <aclapi.h>
#include <Psapi.h>
#include <WtsApi32.h>
#include <processsnapshot.h>
#include <DbgHelp.h>
#include <TlHelp32.h>
#include <delayimp.h>
#include <powrprof.h>

#include <combaseapi.h>
#include <oleacc.h>
#include <ObjIdl.h>
#include <comdef.h>
#include <comutil.h>
#include <initguid.h>
#include <appmodel.h>
#include <taskschd.h>
#include <shellapi.h>
#include <ShlObj.h>
#include <Shlwapi.h>
#include <PathCch.h>
#include <UIAnimation.h>
#include <wincodec.h>

#include <commctrl.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <vsstyle.h>
#include <vssym32.h>

#include <wintrust.h>
#include <softpub.h>
#include <wincrypt.h>

#include <dxgi1_6.h>
#include <d2d1_3.h>
#include <d2d1effectauthor_1.h>
#include <d2d1effecthelpers.h>
#include <d3d11_4.h>
#include <dcomp.h>
#include <dwrite_3.h>
#include <d3dcompiler.h>

#pragma comment(lib, "version.lib")

#pragma comment(lib, "delayimp.lib")
#pragma comment(lib, "DbgHelp.lib")
#pragma comment(lib, "Psapi.lib")
#pragma comment(lib, "Wtsapi32.lib")

#pragma comment(lib, "Oleacc.lib")
#pragma comment(lib, "taskschd.lib")
#pragma comment(lib, "comsuppw.lib")
#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "PathCch.lib")

#pragma comment(lib, "Advapi32.lib")

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "uxtheme.lib")

#pragma comment(lib, "wintrust.lib")
#pragma comment(lib, "crypt32.lib")

#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dcomp.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "d3dcompiler.lib")

#pragma comment(lib, "onecore.lib")

#pragma warning(pop)
