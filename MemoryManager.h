#ifndef __MEMORYMANAGER_H
#define __MEMORYMANAGER_H

#include <hash_map>

#define MAX_STACK_NUM 32

class MemoryManager
{
private:
	static const unsigned int MEM_SIZE_BYTE = 4;
	static const unsigned int PAGE_BLOCK_NUM = 128;

	static const unsigned int SMALLBLOCK_BLOCK_SIZE = 32;
	static const unsigned int SMALLBLOCK_PAGE_SIZE = PAGE_BLOCK_NUM * SMALLBLOCK_BLOCK_SIZE;

	static const unsigned int MEDIUMBLOCK_BLOCK_SIZE = 128;
	static const unsigned int MEDIUMBLOCK_PAGE_SIZE = PAGE_BLOCK_NUM * MEDIUMBLOCK_BLOCK_SIZE;

	struct __declspec(align(4)) BlockTail
	{
		unsigned char freeBlockBitMask[PAGE_BLOCK_NUM / 8];	///< each bit for one block
		unsigned int freeBlockCount;
		void* pPrevPage;
		void* pNextPage;
	};

	template<unsigned int PageSize> struct __declspec(align(4)) BlockPage
	{
		unsigned char block[PageSize];
		BlockTail tail;
	};

	typedef BlockPage<SMALLBLOCK_PAGE_SIZE> SmallBlockPage;
	typedef BlockPage<MEDIUMBLOCK_PAGE_SIZE> MediumBlockPage;

	SmallBlockPage* _pSmallBlockPages;

	struct __declspec(align(4)) Chunk
	{
		void* pAddr;
		size_t size;
		void* pStackFrame[MAX_STACK_NUM];
	};

	static MemoryManager _sInstance;
private:
	MemoryManager() : _pSmallBlockPages(NULL) {}
	~MemoryManager() {}
	void AddChunk(Chunk* pChunk);
	void RemoveChunk(unsigned long addr);
	SmallBlockPage* AllocNewPage();
public:
	void Initialize();
	void Release();
	static MemoryManager* GetInstancePtr();
	void* Allocate(size_t size);
	void Deallocate(void* block);
	void Report();
	void DumpMemLeak();
protected:
	stdext::hash_map<unsigned long , Chunk*> m_hmChucks;
	typedef stdext::hash_map<unsigned long , Chunk*>::iterator CHUNCK_MAP_ITER;
};

#endif