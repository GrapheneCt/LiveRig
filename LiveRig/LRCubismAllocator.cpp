#include <stdlib.h>

#include "LRCubismAllocator.hpp"

using namespace Csm;

void* LRCubismAllocator::Allocate(const csmSizeType size)
{
	return malloc(size);
}

void LRCubismAllocator::Deallocate(void* memory)
{
	free(memory);
}

void* LRCubismAllocator::AllocateAligned(const csmSizeType size, const csmUint32 alignment)
{
	return memalign(alignment, size);
}

void LRCubismAllocator::DeallocateAligned(void* alignedMemory)
{
	Deallocate(alignedMemory);
}