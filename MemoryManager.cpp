#include "MemoryManager.h"
#include <windows.h>
#include <Dbghelp.h>
#include <math.h>

#define BACKTRACE_CALLSTACK(frame) \
	do{ \
	unsigned long _ebp , _esp; \
	size_t index = 0; \
	__asm \
		{ \
		__asm mov _ebp , ebp \
		__asm mov _esp , esp \
		}\
		for(; index < MAX_STACK_NUM; ++index) \
		{ \
		frame[index] = (void*)(*((unsigned long*)_ebp + 1)); \
		_ebp = *((unsigned long*)_ebp); \
		if(_ebp == 0 || _ebp < _esp) \
		break; \
		} \
		for(; index < MAX_STACK_NUM; ++index) \
		frame[index] = NULL; \
	}while(0);

MemoryManager MemoryManager::_sInstance;

void MemoryManager::Initialize()
{
	_pSmallBlockPages = NULL;
	AllocNewPage();
}

void MemoryManager::Release()
{
	SmallBlockPage* pNext = NULL;
	while(_pSmallBlockPages)
	{
		pNext = (SmallBlockPage*)_pSmallBlockPages->tail.pNextPage;
		VirtualFree((LPVOID)_pSmallBlockPages , sizeof(SmallBlockPage) , MEM_RELEASE);
		_pSmallBlockPages = pNext;
	}
}

MemoryManager::SmallBlockPage* MemoryManager::AllocNewPage()
{
	SmallBlockPage* pPage = (SmallBlockPage*)VirtualAlloc(NULL , sizeof(SmallBlockPage) , MEM_COMMIT , PAGE_READWRITE);
	memset(&(pPage->tail) , 0 , sizeof(BlockTail));
	pPage->tail.freeBlockCount = SMALLBLOCK_PAGE_SIZE / SMALLBLOCK_BLOCK_SIZE;
	pPage->tail.pNextPage = (void*)_pSmallBlockPages;
	pPage->tail.pPrevPage = NULL;
	_pSmallBlockPages = pPage;
	return pPage;
}

MemoryManager* MemoryManager::GetInstancePtr()
{
	return &_sInstance;
}

void* MemoryManager::Allocate(size_t size)
{
	size_t allocSize = size + MEM_SIZE_BYTE;	/// head 4 byte for recording size

	if(allocSize > SMALLBLOCK_PAGE_SIZE)
	{
		fprintf(stderr , "size is large than one page!\n");
		return NULL;
	}
	
	Chunk* pChunk = (Chunk*)VirtualAlloc(NULL , sizeof(Chunk) , MEM_COMMIT , PAGE_READWRITE);

	SmallBlockPage* pPage = _pSmallBlockPages;
	pChunk->pAddr = NULL;
	while(pPage)
	{
		if(pPage->tail.freeBlockCount * SMALLBLOCK_BLOCK_SIZE < allocSize)
			pPage = (SmallBlockPage*)pPage->tail.pNextPage;
		else
		{
			/// found continue space in this page for allocation
			int offset = 0 , count = 0;
			for(size_t i = 0; i < SMALLBLOCK_PAGE_SIZE / SMALLBLOCK_BLOCK_SIZE; ++i)
			{
				if(((pPage->tail.freeBlockBitMask[i / 8] >> (i % 8)) & 0x01) == 0) 
				{
					++count;
				}
				else 
				{
					count = 0;
					offset = i;
				}
				if(count * SMALLBLOCK_BLOCK_SIZE >= allocSize) ///< found
				{
					for(int k = 0; k < count; ++k)
						pPage->tail.freeBlockBitMask[(offset + count) / 8] |= 0x01 << ((offset + count) % 8);
					pChunk->pAddr = pPage->block + offset * SMALLBLOCK_BLOCK_SIZE;
					*((unsigned int*)pChunk->pAddr) = size;
					pChunk->pAddr = (char*)pChunk->pAddr + MEM_SIZE_BYTE;
					pPage->tail.freeBlockCount -= count;
					break;
				}
			}
			pPage = (SmallBlockPage*)pPage->tail.pNextPage;
		}
	}
	if(!pChunk->pAddr)	///< no page to fit, create new one
	{
		SmallBlockPage* pPage = AllocNewPage();
		for(int k = 0; k < (int)ceilf((float)allocSize / SMALLBLOCK_BLOCK_SIZE); ++k)
			pPage->tail.freeBlockBitMask[k / 8] |= 0x01 << (k % 8);
		pChunk->pAddr = pPage->block;
		*((unsigned int*)pChunk->pAddr) = size;
		pChunk->pAddr = (char*)pChunk->pAddr + MEM_SIZE_BYTE;
		
	}
	pChunk->size = size;
	BACKTRACE_CALLSTACK(pChunk->pStackFrame);

	AddChunk(pChunk);

	return pChunk->pAddr;	
}

void MemoryManager::Deallocate(void* block)
{
	void* pAllocBlock = (char*)block - MEM_SIZE_BYTE;
	unsigned int size = *((unsigned int*)pAllocBlock);
	/// found which page the block allocated
	SmallBlockPage* pPage = _pSmallBlockPages;
	int offset = 0;
	size_t startBlockIdx = offset / SMALLBLOCK_BLOCK_SIZE;
	size_t endBlockIdx = startBlockIdx + (int)ceilf((float)size / SMALLBLOCK_BLOCK_SIZE);
	while(pPage)
	{
		offset = (unsigned char*)pAllocBlock - pPage->block;
		if(offset >= 0 && offset <= SMALLBLOCK_PAGE_SIZE)
		{
			for(size_t i = startBlockIdx; i < endBlockIdx; ++i)
				pPage->tail.freeBlockBitMask[i / 8] &= ~(0x01 << (i % 8));
			break;
		}
		else
			pPage = (SmallBlockPage*)pPage->tail.pNextPage;
	}
	RemoveChunk((unsigned long)block);	
}

void MemoryManager::AddChunk(Chunk* pChunk)
{
	m_hmChucks.insert(std::make_pair((unsigned long)pChunk->pAddr , pChunk));
}

void MemoryManager::RemoveChunk(unsigned long addr)
{
	CHUNCK_MAP_ITER iter = m_hmChucks.find(addr);
	if(iter != m_hmChucks.end())
	{
		VirtualFree(iter->second , sizeof(Chunk) , MEM_RELEASE);
		m_hmChucks.erase(iter);
	}
}

void MemoryManager::Report()
{
	printf("----------------Memory Report----------------\n");
	SmallBlockPage* pNext = _pSmallBlockPages;
	int i = 1;
	while(pNext)
	{
		printf("Page %d\n" , i++);
		printf("\t Free Count:%d\n" , pNext->tail.freeBlockCount);
		pNext = (SmallBlockPage*)pNext->tail.pNextPage;
	}
}

void MemoryManager::DumpMemLeak()
{
	printf("----------------Memory Leak Dump----------------\n");
	if(m_hmChucks.empty())
		printf("No leak is detected!\n");
	else
	{
		HANDLE hProcess;
		SymSetOptions(SYMOPT_DEBUG);
		hProcess = GetCurrentProcess();

		if(SymInitialize(hProcess , NULL , TRUE))
		{	
			DWORD64 dwDisplacement64 = 0;
			DWORD   dwDisplacement = 0;
			char buffer[sizeof(SYMBOL_INFO)+ MAX_SYM_NAME * sizeof(TCHAR)];
			memset(buffer , 0 , sizeof(buffer));
			PSYMBOL_INFO pSymbol = (PSYMBOL_INFO)buffer;
			pSymbol->SizeOfStruct = sizeof(SYMBOL_INFO);
			pSymbol->MaxNameLen = MAX_SYM_NAME;

			_IMAGEHLP_LINE line;
			line.SizeOfStruct = sizeof(_IMAGEHLP_LINE);

			CHUNCK_MAP_ITER iter = m_hmChucks.begin();
			for(; iter != m_hmChucks.end(); ++iter)
			{
				printf("Leadk at address : 0x%08x , size : %d\n" , iter->second->pAddr , iter->second->size);
				for(size_t i = 0; i < MAX_STACK_NUM && iter->second->pStackFrame[i]; ++i)
				{
					if(SymFromAddr(hProcess , (DWORD)iter->second->pStackFrame[i] , &dwDisplacement64 , pSymbol))
					{
						SymGetLineFromAddr(hProcess , (DWORD)iter->second->pStackFrame[i] , &dwDisplacement , &line);
						printf("\t[%u] %s(0x%08x) Line %u in %s\n" , i , pSymbol->Name , iter->second->pStackFrame[i] , line.LineNumber , line.FileName);
					}
					else
						printf("\tSymFromAddr() failed. Error code: %u \n", GetLastError());
				}
			}
		}
		else
		{
			DWORD dwError = GetLastError();
			printf("SymInitialize returned error : %u\n", dwError);
		}

		SymCleanup(hProcess);
	}
	printf("------------------------------------------------\n");
}