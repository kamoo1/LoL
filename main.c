#include <stdio.h>
#include <windows.h>
#include <conio.h>//getch
#include <Tlhelp32.h>//snapshot
typedef void * HANDLE;
//typedef DWORD unsigned int;
int getAddr(HANDLE hProcess, DWORD pid, void** out_addr)
{
	HANDLE h=CreateToolhelp32Snapshot(TH32CS_SNAPALL, pid);
	MODULEENTRY32 me;
	me.dwSize = sizeof( MODULEENTRY32 );
	int ret = Module32First(h, &me);
	printf("Pid = %d  hProcess = %d Module32First = %d\n", (int)pid, (int)hProcess,(int)ret);
	while (ret) {
		//printf("%p\t\%s\n", me.modBaseAddr, me.szModule);
		if (strcmp(me.szModule, "League of Legends.exe") == 0) {
			void* baseAddr = me.modBaseAddr + 0x0071EE54;//base
			void* readValue = NULL;
			if (ReadProcessMemory(hProcess,(void*)baseAddr,&readValue,sizeof(readValue),0) == 0) {
				printf("ReadProcessMemory failed (at get zoom addr) \n");
				return -1;
			}
			*out_addr = (void*)(readValue + 0x10);//offset
			printf("Zoom Limit Addr is %08x\n", (unsigned int)*out_addr);
			return 0;
		}
		ret = Module32Next(h, &me);
	}
	CloseHandle(h);
	return -1;
}
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
	while(1) {
		printf("Looking for %s.\n",pTargetName);
		do {
			pid = getPidByName(pTargetName);
			//pid = (DWORD)12976;
			Sleep(3000);
		} while(pid == (DWORD)NULL);
		printf("Found %s, trying to modify memory..\n", pTargetName);
		hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
		if (hProcess != NULL) {
			void* pMaxDestAddr = NULL;
			if (getAddr(hProcess, pid, &pMaxDestAddr) == 0) {
					//printf("Reading %08x\n", (unsigned int)pMaxDestAddr);
				memModify(hProcess,pMaxDestAddr,(float)4000);
			} else {
				printf("Snap module failed , please run this program as admin.\n");
			}

			CloseHandle(hProcess);
		} else {
			printf("OpenProcess failed, please run this program as admin.\n");
		}

	}

}


int main(int argc, char **argv)
{
	printf("A memory patch for LoL by Kamoo, extends the default max camera distance.\n\n");
	printf("Program started.\n");
	worker();
	getch();
	return 0;
}
