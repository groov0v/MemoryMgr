#include "MemoryManager.h"

void leakfunc1();
void leakfunc2();
void leakfunc3();

MemoryManager* g_pMemoryManager = MemoryManager::GetInstancePtr();

int main()
{
	g_pMemoryManager->Initialize();
	leakfunc1();
	g_pMemoryManager->DumpMemLeak();
	g_pMemoryManager->Report();
	g_pMemoryManager->Release();
	return 0;
}

void leakfunc1()
{
	char* pBuff = (char*)g_pMemoryManager->Allocate(1024);
	sprintf_s(pBuff , 1023 , "leakfunc1 - Memory Leak here!\0");
	leakfunc2();
}

void leakfunc2()
{
	char* pBuff = (char*)g_pMemoryManager->Allocate(3101);
	sprintf_s(pBuff , 3100 , "leakfunc2 - Memory Leak here!\0");
	leakfunc3();
	g_pMemoryManager->Deallocate(pBuff);
}

void leakfunc3()
{
	char* pBuff = (char*)g_pMemoryManager->Allocate(1921);
	sprintf_s(pBuff , 1920 , "leakfunc3 - Memory Leak here!\0");
}