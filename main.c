#include <stdio.h>
#include <windows.h>
//#include <conio.h>//getch
#include <Tlhelp32.h>//snapshot
typedef void * HANDLE;
/* Get dest limit addr */
int getLimitAddr(BYTE* pModBuf, DWORD bufSize, void** ppMaxDestAddr_out)
{
	//only work for 32bit now
	BYTE* pattern = (BYTE*)"\xF3\x0F\x5C\xEA\xF3\x0F\x10\x05\xFF\xFF\xFF\xFF\x0F\x2F\xE8";
	int patternSize = 15;
	char* mask = "111111110000111"; //1 for opcode, 0 for address (32bit, small endian)

	int pointerSize = sizeof(void*);
	int copiedPointerBytes = 0;
	BYTE* pByMaxDestAddr = malloc(pointerSize);

	int match = 0;
	DWORD offset = 0;

	if (pByMaxDestAddr == NULL) return -1;

	while (offset != bufSize) {
		if (mask[match] == *"0" || pModBuf[offset] == pattern[match]) {
			if (++match == patternSize) {
				//matched, copy pointer
				while(copiedPointerBytes != pointerSize) {
					if (mask[--match] == *"0") {
						pByMaxDestAddr[pointerSize - 1 - copiedPointerBytes++] = pModBuf[offset];
						if (copiedPointerBytes == pointerSize){
							*ppMaxDestAddr_out = (void*)*(DWORD*)pByMaxDestAddr;//...又尼玛反过来了
							free(pByMaxDestAddr);
							return 0;
						}
						
					}
					offset--;
				}
			}
		}else{
			match = 0;
		}
		offset++;
	}

	return -1;
}

int dumpExeMod(HANDLE hProcess, void* modAddr, DWORD modSize, BYTE* pModBuffer)
{
	DWORD readSize = 0;     //mod buffer read size
	if (modAddr == NULL || modSize == 0)return -1;
	ReadProcessMemory(hProcess,modAddr,pModBuffer,modSize,&readSize);
	if (readSize != modSize) {
		return -1;
	}
	return 0;
}

int getModEty(DWORD pid, MODULEENTRY32* pLolModEty_out)
{
	HANDLE h=CreateToolhelp32Snapshot(TH32CS_SNAPALL, pid);
	MODULEENTRY32 me;

	me.dwSize = sizeof( MODULEENTRY32 );
	int ret = Module32First(h, &me);
	printf("Pid = %d Module32First = %d\n", (int)pid, (int)ret);
	while (ret) {
		if (strcmp(me.szModule, "League of Legends.exe") == 0) {
			*pLolModEty_out = me;
			return 0;
		}
		ret = Module32Next(h, &me);
	}
	CloseHandle(h);
	return -1;
}


/* Get exe base address */
/*
int getModAddr(HANDLE hProcess, DWORD pid, void** out_addr)
{
	HANDLE h=CreateToolhelp32Snapshot(TH32CS_SNAPALL, pid);
	MODULEENTRY32 me;
	me.dwSize = sizeof( MODULEENTRY32 );
	int ret = Module32First(h, &me);
	printf("Pid = %d  hProcess = %d Module32First = %d\n", (int)pid, (int)hProcess,(int)ret);
	while (ret) {
		//printf("%p\t\%s\n", me.modBaseAddr, me.szModule);
		if (strcmp(me.szModule, "League of Legends.exe") == 0) {
			*out_addr = me.modBaseAddr;
			printf("Exe base addr is %08x\n", (unsigned int)*out_addr);
			return 0;
		}
		ret = Module32Next(h, &me);
	}
	CloseHandle(h);
	return -1;
}
*/

/* Change max dest value */
int memModify(HANDLE hProcess, void* addr, float myValue)
{
	float targetValue = 2250;
	float readValue = 0;

	if (ReadProcessMemory(hProcess,(void*)addr,&readValue,sizeof(readValue),0) == 0) {
		printf("ReadProcessMemory failed \n");
		return -1;
	}
	if (readValue != targetValue && readValue != myValue) {
		printf("Wait till game fully initialized.\n");
		return -1;
	}
	if (readValue == myValue) {
		printf("Memory has been modified.\n");
		return 0;
	}

	DWORD dwOldProtect;
	DWORD dwMyProtect;

	if (VirtualProtectEx(hProcess, (LPVOID)addr ,sizeof(float),PAGE_READWRITE, &dwOldProtect) == 0) {
		printf("Unprotect memory failed \n");
		return -1;
	}


	if (WriteProcessMemory(hProcess,(void*)addr,&myValue,sizeof(myValue),0) == 0) {
		printf("WriteProcessMemory failed \n");
		return -1;
	}

	if (VirtualProtectEx(hProcess, (LPVOID)addr ,sizeof(float),dwOldProtect, &dwMyProtect) == 0) {
		printf("Protect memory failed \n");
		return -1;
	}
	printf("Succeed.\n");
	return 0;
}


DWORD getPidByName(char *pName)
{
	DWORD pid = (DWORD)NULL;
	PROCESSENTRY32 entry;
	entry.dwSize = sizeof(PROCESSENTRY32);
	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, (DWORD)NULL);

	if (Process32First(snapshot, &entry) == TRUE) {
		while (Process32Next(snapshot, &entry) == TRUE) {
			if (stricmp(entry.szExeFile, pName) == 0) {
				pid = entry.th32ProcessID;
			}
		}
	}

	CloseHandle(snapshot);
	return pid;
}


int worker()
{
	char *pTargetName = "League of Legends.exe";
	DWORD pid = (DWORD)NULL;
	HANDLE hProcess = NULL;

	printf("Looking for %s.\n",pTargetName);
	do {
		pid = getPidByName(pTargetName);
		//pid = (DWORD)12976;
		Sleep(3000);
	} while(pid == (DWORD)NULL);
	printf("Found %s, trying to modify memory..\n", pTargetName);
	hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);

	if (hProcess == NULL)  {
		printf("OpenProcess failed, please run this program as admin.\n");
		return -1;
	}

	void* pMaxDestAddr = NULL;
	BYTE* pModBuffer = NULL;
	MODULEENTRY32 lolModEty;

	if (getModEty(pid, &lolModEty) == -1) {
		printf("Get mod entry error");
		return -1;
	}
	pModBuffer = malloc(lolModEty.modBaseSize);
	if (pModBuffer == NULL)return -1;

	if ( dumpExeMod(hProcess, lolModEty.modBaseAddr, lolModEty.modBaseSize, pModBuffer) == -1 ) {
		printf("Dump memory error.\n");
		free(pModBuffer);
		return -1;
	}

	if (getLimitAddr(pModBuffer, lolModEty.modBaseSize, &pMaxDestAddr) == -1) {
		printf("Find max dest addr error.\n");
		free(pModBuffer);
		return -1;
	}
	memModify(hProcess, pMaxDestAddr, 4000);
	free(pModBuffer);
	CloseHandle(hProcess);
	return 0;
}


int main(int argc, char **argv)
{
	printf("A memory patch for LoL by Kamoo, extends the default max camera distance.\n\n");
	printf("Program started.\n");

	while(1) {
		worker();
	}
	return 0;
}
