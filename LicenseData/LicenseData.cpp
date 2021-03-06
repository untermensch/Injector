//
/* This program is free software. It comes without any warranty, to
* the extent permitted by applicable law. You can redistribute it
* and/or modify it under the terms of the Do What The Fuck You Want
* To Public License, Version 2, as published by Sam Hocevar. See
* http://sam.zoy.org/wtfpl/COPYING for more details. */
//
#include "stdafx.h"
#include "LicenseData.h"
#include <VersionHelpers.h>
#include <iostream>
#include <fstream>
using namespace std;
//
GUID MarkerGuid = OA_MARKER;
GUID PublicKeyGuid = OA_PUBLIC_KEY;
GUID SlpGuid = OA_SLP10;
//
class InstallLib {
	typedef NTSTATUS(WINAPI *QuerySystemInformation)(
		SYSTEM_INFORMATION_CLASS SystemInformationClass,
		PVOID SystemInformation,
		ULONG SystemInformationLength,
		PULONG ReturnLength);
	typedef BOOL(WINAPI *nGetFirmwareType)(
		PFIRMWARE_TYPE FirmwareType);
public:
	InstallLib();
	~InstallLib();
	BOOL AcquirePrivilage(VOID);
	BOOL IsEFI;
private:
	HMODULE hNtdll = NULL;
	HMODULE hKernel32 = NULL;
	nGetFirmwareType pGetFirmwareType = NULL;
	QuerySystemInformation pNtQuerySystemInformation = NULL;
	BOOL isEfi(VOID);
};
InstallLib::InstallLib() {
	IsEFI = isEfi();
}
InstallLib::~InstallLib() {
	if (hNtdll) {
		pNtQuerySystemInformation = NULL;
		FreeLibrary(hNtdll);
		hNtdll = NULL;
	}
	if (hKernel32) {
		pGetFirmwareType = NULL;
		FreeLibrary(hKernel32);
		hKernel32 = NULL;
	}
}
BOOL InstallLib::AcquirePrivilage(VOID) {
		// http://msdn.microsoft.com/en-us/library/windows/desktop/bb530716(v=vs.85).aspx
		TOKEN_PRIVILEGES NewState;
		LUID luid;
		HANDLE hToken = NULL;
		//
		if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY | TOKEN_QUERY_SOURCE, &hToken)) {
			CloseHandle(hToken);
			return FALSE;
		}
		if (!LookupPrivilegeValue(NULL, TEXT("SeSystemEnvironmentPrivilege"), &luid)) {
			CloseHandle(hToken);
			return FALSE;
		}
		NewState.PrivilegeCount = 1;
		NewState.Privileges[0].Luid = luid;
		NewState.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
		if (!AdjustTokenPrivileges(hToken, FALSE, &NewState, sizeof(NewState), NULL, NULL)) {
			CloseHandle(hToken);
			return FALSE;
		}
		CloseHandle(hToken);
		return TRUE;
	}
BOOL InstallLib::isEfi(VOID)
	{
		BOOL RetVal = FALSE;

		if (IsWindows8OrGreater()) {
			if (!hKernel32) {
				hKernel32 = LoadLibrary(L"Kernel32.dll");
			}
			if (hKernel32) {
				pGetFirmwareType = (nGetFirmwareType)GetProcAddress(hKernel32, "GetFirmwareType");
			}
			FIRMWARE_TYPE FirmwareType;
			pGetFirmwareType(&FirmwareType);
			RetVal = FirmwareType == FirmwareTypeUefi;
		}
		else if (IsWindows7OrGreater()) {
			if (!hNtdll) {
				hNtdll = LoadLibrary(L"ntdll.dll");
			}
			if (hNtdll) {
				pNtQuerySystemInformation = (QuerySystemInformation)GetProcAddress(hNtdll, "NtQuerySystemInformation");
			}
			DWORD buffer[5] = {};
			if (pNtQuerySystemInformation((SYSTEM_INFORMATION_CLASS)90, buffer, sizeof(buffer), NULL) == 0 && buffer[4] == 2) {
				RetVal = TRUE;
			}
		}
		return RetVal;
	}
//
int _tmain(int argc, _TCHAR* argv[])
{
	//
	InstallLib lib;
	size_t slicsize = 0;
	int status = 0;
	ifstream slic;
	char* buffer = NULL;
	wchar_t* markerguid = NULL;
	wchar_t* publickeyguid = NULL;
	wchar_t* slpguid = NULL;
	try {
		markerguid = new wchar_t[MAX_PATH]();
		publickeyguid = new wchar_t[MAX_PATH]();
		slpguid = new wchar_t[MAX_PATH]();
	}
	catch (bad_alloc) {
		wcout << L"out of memory\n";
		goto Exit;
	}
	status = StringFromGUID2(MarkerGuid, markerguid, MAX_PATH);
	status = StringFromGUID2(PublicKeyGuid, publickeyguid, MAX_PATH);
	status = StringFromGUID2(SlpGuid, slpguid, MAX_PATH);
	//
	if(argc < 2) {
		wcout << L"no arguments\n";
		status = 1;
		goto Exit;
	}
	//
	// delete 
	//
	if (wcscmp(argv[1], L"0") == 0) {
		if (lib.IsEFI) {
			lib.AcquirePrivilage();
			if (SetFirmwareEnvironmentVariable(TEXT("OaMarker"), markerguid, NULL, 0) != 0) {
				wcout << L"marker successfully deleted\n";
			}
			else {
				wcout << L"failed to delete marker\n";
			}
			if (SetFirmwareEnvironmentVariable(TEXT("OaPublicKey"), publickeyguid, NULL, 0) != 0) {
				wcout << L"public key successfully deleted\n";
			}
			else {
				wcout << L"failed to delete public key\n";
			}
			if (SetFirmwareEnvironmentVariable(TEXT("OaSlp"), slpguid, NULL, 0) != 0) {
				wcout << L"slp string successfully deleted\n";
			}
			else {
				wcout << L"failed to delete slp string\n";
			}
			status = 0;
			goto Exit;
		}
		else {
			wcout << L"system is not EFI\n";
			status = 2;
			goto Exit;
		}
	}
	//
	// write slp string
	//
	else if(wcscmp(argv[1], L"1") == 0 ) {
		if(lib.IsEFI) {
			lib.AcquirePrivilage();
			char slpstr[0x20] = {0};
			int len = WideCharToMultiByte(CP_ACP, 0, argv[2], -1, slpstr, 0x20, 0, NULL);
			if (SetFirmwareEnvironmentVariable(TEXT("OaSlp"), slpguid, slpstr, len) != 0) {
				wcout << L"slp string successfully written\n";
			}
			else {
				wcout << L"failed to write slp string\n";
			}
			status = 0;
			goto Exit;
		}
		else {
			wcout << L"system is not EFI\n";
			status = 2;
			goto Exit;
		}
	}
	//
	// write key & marker
	//
	else if(wcscmp(argv[1], L"2") == 0) { 
		slic = ifstream(argv[2], ios::binary);
		if (!slic.is_open()) {
			wcout << L"couldn't open " << argv[2] << L"\n";
			status = 1;
			goto Exit;
		}
		slic.seekg(0, ios::end);
		slicsize = (size_t)slic.tellg();
		slic.seekg(0, ios::beg);
		if (slicsize != sizeof(SlicTbl_t)) {
			wcout << L"incorrect file size\n";
			status = 1;
			goto Exit;
		}
		buffer = new char[slicsize];
		slic.read(buffer, slicsize);
		SlicTbl_t* slictable = (SlicTbl_t *) buffer;
		Marker_t* marker = &slictable->Marker;
		PublicKey_t* publickey = &slictable->PublicKey;

		if (lib.IsEFI) {
			lib.AcquirePrivilage();
			if (SetFirmwareEnvironmentVariable(TEXT("OaMarker"), markerguid, marker, sizeof(Marker_t)) != 0) {
				wcout << L"marker successfully written\n";
			}
			else {
				wcout << L"failed to write marker\n";
			}
			if (SetFirmwareEnvironmentVariable(TEXT("OaPublicKey"), publickeyguid, publickey, sizeof(PublicKey_t)) != 0) {
				wcout << L"public key successfully written\n";
			}
			else {
				wcout << L"failed to write public key\n";
			}
			status = 0;
			goto Exit;
		}
		else {
			wcout << L"system is not EFI\n";
			status = 2;
			goto Exit;
		}
	}
Exit:
	if(markerguid != NULL) {delete[] markerguid;};
	if(publickeyguid != NULL) {delete[] publickeyguid;};
	if(slpguid != NULL) {delete[] slpguid;}
	if(buffer != NULL) {delete[] buffer;}
	return status;
}