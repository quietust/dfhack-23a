#define _CRT_SECURE_NO_WARNINGS
#define _WIN32_WINNT _WIN32_WINNT_WINXP

#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers
#include <windows.h>
#include <stdio.h>

struct hooksData
{
	char dll_name[16];	// 00 - "dfhack.dll"
	char init_name[16];	// 10 - "dfhackInit"
	char update_name[16];	// 20 - "dfhackUpdate"
	char render_name[16];	// 30 - "dfhackRender"
	char unload_name[16];	// 40 - "dfhackUnload"

	void *updateFunc;	// 50
	void *renderFunc;	// 54
	void *unloadFunc;	// 58
};

#define	HOOKSDATA_DLL_NAME	ebx+0x00
#define	HOOKSDATA_INIT_NAME	ebx+0x10
#define	HOOKSDATA_UPDATE_NAME	ebx+0x20
#define	HOOKSDATA_RENDER_NAME	ebx+0x30
#define	HOOKSDATA_UNLOAD_NAME	ebx+0x40
#define	HOOKSDATA_UPDATE_FUNC	ebx+0x50
#define	HOOKSDATA_RENDER_FUNC	ebx+0x54
#define	HOOKSDATA_UNLOAD_FUNC	ebx+0x58

/*
0.23.130.23a:

0x74F074 == LoadLibraryA
0x74F0C8 == GetProcAddress
Update hook: inject call at 0x4B7E61, wrap around routine 0x514340 (no parameters)
Unload hook: inject call at 0x4B7F34, wrap around routine 0x4BB0B0 (__thiscall with 1 parameter)
hooksData pointer: 0x7A43FC
*/

#ifdef DFHACK_23a
#define ADDR_LoadLibraryA	0x74F074
#define ADDR_GetProcAddress	0x74F0C8
#define ADDR_UpdateInjectAddr	0x4B7E61
#define ADDR_UpdateOriginalAddr 0x514340
#define ADDR_RenderInjectAddr	0x57E425
#define ADDR_RenderOriginalAddr	0x57DF00
#define ADDR_UnloadInjectAddr	0x4B7F34
#define ADDR_UnloadOriginalAddr	0x4BB0B0
#define ADDR_HooksData		0x7A43FC
#define	ADDR_CurrentVersion	0xEA055C
#define	ADDR_MinVersion		0xF16404
#define	VAL_CurrentVersion	1169
#define	VAL_MinVersion		1083
#endif

/*
0.28.181.40d:

0x96E0A0 == LoadLibraryA
0x96E170 == GetProcAddress
Update hook: inject call at 0x518F46, wrap around routine 0x6B8A50 (no parameters)
Unload hook: inject call at 0x51905C, wrap around routine 0x51C6B0 (single parameter in esi)
hooksData pointer: 0x9FCB0C
*/

#ifdef DFHACK_40d
#define ADDR_LoadLibraryA	0x96E0A0
#define ADDR_GetProcAddress	0x96E170
#define ADDR_UpdateInjectAddr	0x518F46
#define ADDR_UpdateOriginalAddr 0x6B8A50
// The render hook is special in 40d, since we need to inject it in the middle of the code
#define ADDR_RenderInjectAddr	0x659027
#define ADDR_RenderOriginalAddr	0x65902F
#define ADDR_UnloadInjectAddr	0x51905C
#define ADDR_UnloadOriginalAddr	0x51C6B0
#define ADDR_HooksData		0x9FCB0C
#define	ADDR_CurrentVersion	0xD5975C
#define	ADDR_MinVersion		0xD8DD34
#define	VAL_CurrentVersion	1268
#define	VAL_MinVersion		1205
#endif

#if !defined(DFHACK_23a) && !defined(DFHACK_40d)
#error You must define either DFHACK_23a or DFHACK_40d!
#endif

__declspec(naked) DWORD __stdcall initFunc(struct hooksData *self) {
__asm {
	push	ebx
	push	esi

	mov	ebx, [esp+0xC]

	mov	DWORD PTR [HOOKSDATA_UPDATE_FUNC], 0
	mov	DWORD PTR [HOOKSDATA_RENDER_FUNC], 0
	mov	DWORD PTR [HOOKSDATA_UNLOAD_FUNC], 0

	lea	eax, [HOOKSDATA_DLL_NAME]
	push	eax
	call	ds:ADDR_LoadLibraryA
	mov	esi, eax
	test	esi, esi
	jz	fail

	lea	eax, [HOOKSDATA_UNLOAD_NAME]
	push	eax
	push	esi
	call	ds:ADDR_GetProcAddress
	mov	[HOOKSDATA_UNLOAD_FUNC], eax

	lea	eax, [HOOKSDATA_RENDER_NAME]
	push	eax
	push	esi
	call	ds:ADDR_GetProcAddress
	mov	[HOOKSDATA_RENDER_FUNC], eax

	lea	eax, [HOOKSDATA_UPDATE_NAME]
	push	eax
	push	esi
	call	ds:ADDR_GetProcAddress
	mov	[HOOKSDATA_UPDATE_FUNC], eax

	lea	eax, [HOOKSDATA_INIT_NAME]
	push	eax
	push	esi
	call	ds:ADDR_GetProcAddress

	test	eax, eax
	jz	end
	call	eax
	test	al, al
	jz	fail
end:
	mov	ds:ADDR_HooksData, ebx
	pop	esi
	pop	ebx
	ret	4

fail:
	xor	ebx, ebx
	jmp	end
} }

/*

	dfhack = LoadLibrary("dfhack.dll");
	if (!dfhack)
		return false;
	initFunc = GetProcAddress(dfhack, "dfhackInit");
	updateFunc = GetProcAddress(dfhack, "dfhackUpdate");
	unloadFunc = GetProcAddress(dfhack, "dfhackUnload");
	if (initfunc)
		return initfunc();
*/

__declspec(naked) void updateFunc() {
__asm {
	push	ebx
	mov	ebx, ds:ADDR_HooksData
	mov	eax, [HOOKSDATA_UPDATE_FUNC]
	test	eax, eax
	jz	skip
	call	eax
skip:
	pop	ebx
	mov	eax, ADDR_UpdateOriginalAddr
	jmp	eax
} }

__declspec(naked) void renderFunc() {
__asm {
	push	ebx
	mov	ebx, ds:ADDR_HooksData
	mov	eax, [HOOKSDATA_RENDER_FUNC]
	test	eax, eax
	jz	skip
	call	eax
skip:
	pop	ebx
	mov	eax, ADDR_RenderOriginalAddr
#ifdef DFHACK_40d
	xor	ebx, ebx
	cmp	ds:0x1787244, bl
#endif
	jmp	eax
} }

__declspec(naked) void unloadFunc() {
__asm {
	push	ebx
	mov	ebx, ds:ADDR_HooksData
	mov	eax, [HOOKSDATA_UNLOAD_FUNC]
	test	eax, eax
	jz	skip
	call	eax
skip:
	pop	ebx
	mov	eax, ADDR_UnloadOriginalAddr
	jmp	eax
} }

#define	FUNC_LEN	256
#define	DF_PROC_NAME	"Dwarf Fortress"

HANDLE h_process;
DWORD pid;

typedef LONG (NTAPI *NtSuspendProcess)(IN HANDLE ProcessHandle);
typedef LONG (NTAPI *NtResumeProcess)(IN HANDLE ProcessHandle);

bool suspend()
{
	NtSuspendProcess pfnNtSuspendProcess = (NtSuspendProcess)GetProcAddress(GetModuleHandle("ntdll"), "NtSuspendProcess");
	if (pfnNtSuspendProcess)
		return !pfnNtSuspendProcess(h_process);
	return false;
}

bool resume()
{
	NtResumeProcess pfnNtResumeProcess = (NtResumeProcess)GetProcAddress(GetModuleHandle("ntdll"), "NtResumeProcess");
	if (pfnNtResumeProcess)
		return !pfnNtResumeProcess(h_process);
	return false;
}


bool	ReadDFMemory (const void *address, void *buffer, size_t len)
{
	SIZE_T bytesRead;
	if (!ReadProcessMemory(h_process, address, buffer, len, &bytesRead))
		return false;
	if (bytesRead != len)
		return false;
	return true;
}

bool	WriteDFMemory (void *address, void *buffer, size_t len)
{
	SIZE_T bytesWritten;
	if (!WriteProcessMemory(h_process, address, buffer, len, &bytesWritten))
		return false;
	if (bytesWritten != len)
		return false;
	return true;
}

bool	checkVersion (void)
{
	DWORD current_version;
	DWORD min_version;
	DWORD hooks;

	// check current version
	if (!ReadDFMemory((LPVOID)ADDR_CurrentVersion, &current_version, 4))
		return false;
	// and minimum save version, just to be safe
	if (!ReadDFMemory((LPVOID)ADDR_MinVersion, &min_version, 4))
		return false;
	// as well as the hook data pointer DFHack sets up
	if (!ReadDFMemory((LPVOID)ADDR_HooksData, &hooks, 4))
		return false;

	if (current_version != VAL_CurrentVersion)
		return false;
	if (min_version != VAL_MinVersion)
		return false;
	if (hooks)
		return false;

	return true;
}

class DFProcess
{
public:
	DFProcess()
	{
		h_process = NULL;
		pid = -1;

		HWND h_window = FindWindow(NULL, DF_PROC_NAME);
		if (!h_window)
			return;

		GetWindowThreadProcessId(h_window, &pid);
		h_process = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
		if (!h_process)
			return;
		if (!checkVersion())
		{
			CloseHandle(h_process);
			h_process = NULL;
			pid = -1;
		}
	}
	~DFProcess()
	{
		CloseHandle(h_process);
		h_process = NULL;
		pid = -1;
	}
	bool valid()
	{
		return (h_process != NULL);
	}
};

void genHook (BYTE *hookData, DWORD hookAddr)
{
	hookData[0] = 0xE8;
	hookData[1] = (hookAddr >>  0) & 0xFF;
	hookData[2] = (hookAddr >>  8) & 0xFF;
	hookData[3] = (hookAddr >> 16) & 0xFF;
	hookData[4] = (hookAddr >> 24) & 0xFF;
}

void genHook2 (BYTE *hookData, DWORD hookAddr)
{
#ifdef DFHACK_40d
	// this hook is special - with 40d, it must be a JMP instead of a CALL
	hookData[0] = 0xE9;
#else
	hookData[0] = 0xE8;
#endif
	hookData[1] = (hookAddr >>  0) & 0xFF;
	hookData[2] = (hookAddr >>  8) & 0xFF;
	hookData[3] = (hookAddr >> 16) & 0xFF;
	hookData[4] = (hookAddr >> 24) & 0xFF;
}

bool	inject (void)
{
	// Attempt to suspend DF's execution
	if (!suspend())
	{
		printf("Unable to suspend Dwarf Fortress!\n");
		return false;
	}

	DWORD data = (DWORD)VirtualAllocEx(h_process, NULL, sizeof(hooksData), MEM_COMMIT, PAGE_READWRITE);
	if (!data)
	{
		printf("Failed to allocate hook data!\n");
		goto fail0;
	}
	DWORD init = (DWORD)VirtualAllocEx(h_process, NULL, FUNC_LEN, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
	if (!init)
	{
		printf("Failed to allocate init code!\n");
		goto fail1;
	}

	DWORD update = (DWORD)VirtualAllocEx(h_process, NULL, FUNC_LEN, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
	if (!update)
	{
		printf("Failed to allocate update hook!\n");
		goto fail2;
	}

	DWORD render = (DWORD)VirtualAllocEx(h_process, NULL, FUNC_LEN, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
	if (!render)
	{
		printf("Failed to allocate render hook!\n");
		goto fail3;
	}

	DWORD unload = (DWORD)VirtualAllocEx(h_process, NULL, FUNC_LEN, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
	if (!unload)
	{
		printf("Failed to allocate unload hook!\n");
		goto fail4;
	}

	struct hooksData hookData;
	strcpy(hookData.dll_name, "dfhack.dll");
	strcpy(hookData.init_name, "dfhackInit");
	strcpy(hookData.update_name, "dfhackUpdate");
	strcpy(hookData.unload_name, "dfhackUnload");

	if (!WriteDFMemory((LPVOID)data, &hookData, sizeof(hooksData)))
	{
		printf("Failed to write hook data!\n");
		goto fail5;
	}
	if (!WriteDFMemory((LPVOID)init, &initFunc, FUNC_LEN))
	{
		printf("Failed to write init code!\n");
		goto fail5;
	}
	if (!WriteDFMemory((LPVOID)update, &updateFunc, FUNC_LEN))
	{
		printf("Failed to write update hook!\n");
		goto fail5;
	}
	if (!WriteDFMemory((LPVOID)render, &renderFunc, FUNC_LEN))
	{
		printf("Failed to write render hook!\n");
		goto fail5;
	}
	if (!WriteDFMemory((LPVOID)unload, &unloadFunc, FUNC_LEN))
	{
		printf("Failed to write unload hook!\n");
		goto fail5;
	}

	HANDLE remoteThread = CreateRemoteThread(h_process, NULL, NULL, (LPTHREAD_START_ROUTINE)init, (LPVOID)data, NULL, NULL);
	WaitForSingleObject(remoteThread, 5000);
	CloseHandle(remoteThread);

	DWORD hooks;
	if (!ReadDFMemory((LPVOID)ADDR_HooksData, &hooks, 4))
	{
		printf("Failed to query presence of DFHack hook structure after running thread!\n");
		goto fail5;
	}
	if (!hooks)
	{
		printf("DFHack hooks initialize function failed!\n");
		goto fail5;
	}

	DWORD oldProtectUpdate, oldProtectRender, oldProtectUnload;
	if (!VirtualProtectEx(h_process, (LPVOID)ADDR_UpdateInjectAddr, 5, PAGE_EXECUTE_READWRITE, &oldProtectUpdate))
	{
		printf("Failed to remove write protection for update hook (%08x)!\n", GetLastError());
		goto fail5;
	}
	if (!VirtualProtectEx(h_process, (LPVOID)ADDR_RenderInjectAddr, 5, PAGE_EXECUTE_READWRITE, &oldProtectRender))
	{
		printf("Failed to remove write protection for render hook (%08x)!\n", GetLastError());
		goto fail5;
	}
	if (!VirtualProtectEx(h_process, (LPVOID)ADDR_UnloadInjectAddr, 5, PAGE_EXECUTE_READWRITE, &oldProtectUnload))
	{
		printf("Failed to remove write protection for unload hook (%08x)!\n", GetLastError());
		goto fail5;
	}

	// TODO: verify original data before patching

	BYTE hookBytes[5];
	genHook(hookBytes, update - ADDR_UpdateInjectAddr - 5);
	if (!WriteDFMemory((LPVOID)ADDR_UpdateInjectAddr, hookBytes, 5))
	{
		printf("Failed to install onUpdate hook! Things likely will not work!\n");
		goto fail5;
	}

	genHook2(hookBytes, render - ADDR_RenderInjectAddr - 5);
	if (!WriteDFMemory((LPVOID)ADDR_RenderInjectAddr, hookBytes, 5))
	{
		printf("Failed to install onRender hook! Things likely will not work!\n");
		goto fail5;
	}

	genHook(hookBytes, unload - ADDR_UnloadInjectAddr - 5);
	if (!WriteDFMemory((LPVOID)ADDR_UnloadInjectAddr, hookBytes, 5))
	{
		printf("Failed to install onUnload hook! Things will probably work, they might not clean up.\n");
		goto fail5;
	}

	// these aren't fatal
	if (!VirtualProtectEx(h_process, (LPVOID)ADDR_UpdateInjectAddr, 5, oldProtectUpdate, &oldProtectUpdate))
		printf("Failed to restore memory protection for update hook!\n");
	if (!VirtualProtectEx(h_process, (LPVOID)ADDR_RenderInjectAddr, 5, oldProtectRender, &oldProtectRender))
		printf("Failed to restore memory protection for render hook!\n");
	if (!VirtualProtectEx(h_process, (LPVOID)ADDR_UnloadInjectAddr, 5, oldProtectUnload, &oldProtectUnload))
		printf("Failed to restore memory protection for unload hook!\n");

	// Resume DF's execution
	if (!resume())
		printf("Failed to resume execution?\n");
	return true;

fail5:
	VirtualFreeEx(h_process, (LPVOID)unload, 0, MEM_RELEASE);
fail4:
	VirtualFreeEx(h_process, (LPVOID)render, 0, MEM_RELEASE);
fail3:
	VirtualFreeEx(h_process, (LPVOID)update, 0, MEM_RELEASE);
fail2:
	VirtualFreeEx(h_process, (LPVOID)init, 0, MEM_RELEASE);
fail1:
	VirtualFreeEx(h_process, (LPVOID)data, 0, MEM_RELEASE);
fail0:
	resume();
	return false;
}

int main (void)
{
	DFProcess proc;
	if (!proc.valid())
	{
		printf("Unable to detect appropriate version of Dwarf Fortress\n");
		return 1;
	}
	if (inject())
		printf("Successfully injected DFHack!\n");
}