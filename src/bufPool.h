/*
	Copyright (c) 2016 Cenacle Research India Private Limited
*/

#ifndef _BUFPOOL_H__CBA8E586_437B_491E_B3BC_2C039526D9FD__
#define _BUFPOOL_H__CBA8E586_437B_491E_B3BC_2C039526D9FD__
#include <cassert>
#include <stdlib.h>
#include <deque>
#include <algorithm>
#include <map>

#if defined(DEBUG) || defined(_DEBUG)
#ifndef BUFPOOL_TRACK_MEMORY
#define BUFPOOL_TRACK_MEMORY 1
#endif
#endif

template<typename T>
struct bufPoolT
{
protected:
	inline bufPoolT()
	{
	}
	inline ~bufPoolT()
	{
#if BUFPOOL_TRACK_MEMORY
		assert(inuseQ.size() <= 0); // this means some blocks are still in-use
#endif
		for (T* obj : freeQ)
		{
			free(obj);
		}
	}
public:
	static inline bufPoolT& getObject()
	{
		static bufPoolT obj;
		return obj;
	}
public:
	template<class... Args>
	inline T* acquire(Args&&... args)
	{
		T* pBuf = nullptr;
		if (!freeQ.empty())
		{
			pBuf = freeQ.front();
			freeQ.pop_front();
		}
		else	// no free buffer available - rely on the OS
		{
			pBuf = (T*) malloc(sizeof(T));
			if (pBuf == nullptr) return nullptr;
		}
#if BUFPOOL_TRACK_MEMORY
		inuseQ.push_front(pBuf);
#endif
		// reconstruct the object in-place
		new (pBuf) T(std::forward<Args>(args)...);
		return pBuf;
	}

	inline void release(T* pBuf)
	{
		if (pBuf == nullptr) return;
		pBuf->~T();
		freeQ.push_front(pBuf);
#if BUFPOOL_TRACK_MEMORY
		auto foundIter = std::find(inuseQ.cbegin(), inuseQ.cend(), pBuf);
		assert(foundIter != inuseQ.cend());  // if this fails, it means you are releasing a buffer that we did not allocate !!
		inuseQ.erase(foundIter);
#endif
	}
#if BUFPOOL_TRACK_MEMORY
	inline bool isInUse(const T* pBuf) const
	{
		return pBuf!=nullptr && inuseQ.cend() != std::find(inuseQ.cbegin(), inuseQ.cend(), pBuf);
	}
#endif
protected:
	std::deque<T*> freeQ;
#if BUFPOOL_TRACK_MEMORY
	std::deque<T*> inuseQ;
#endif
};

#define PAGE_ROUND_DOWN(x, PAGE_SIZE)	((x) & (~(PAGE_SIZE-1)))
#define PAGE_ROUND_UP(x, PAGE_SIZE)		( ((x) + PAGE_SIZE-1)  & (~(PAGE_SIZE-1)) )

#define PAGE_SIZE_1K 0x0400
#define PAGE_SIZE_2K 0x0800
#define PAGE_SIZE_4K 0x1000

// bufPoolChunk allocates memory of any size. Internally it rounds up sizes
// to some fixed numbers and maintains separate queues for each size. Unlike
// bufPoolT, this does not construct any objects on the allocated memory.
// Each allocated chunk of requested size is prepended with a size variable
// that indicates which size-queue this chunk belongs to. Caller may not know
// that more memory is allocated than requested. But repeated calls for similar
// sizes all will end-up returning the same block (since sizes are rounded up
// to nearest page size).
struct bufPoolChunk
{
protected:
	inline bufPoolChunk()
	{
	}
	inline ~bufPoolChunk()
	{
#if BUFPOOL_TRACK_MEMORY
		for(auto sizedQ: inuseQ)
			assert(sizedQ.second.size() <= 0); // this means some blocks are still in-use
#endif
		for (auto sizedQ : freeQ)
		{
			for (auto pChunk : sizedQ.second)
				free (pChunk);
		}
	}
	inline size_t roundedSize(size_t size)
	{
		return PAGE_ROUND_UP(size, PAGE_SIZE_1K);
	}
public:
	static inline bufPoolChunk& getObject()
	{
		static bufPoolChunk obj;
		return obj;
	}
	inline void* acquire(int size)
	{
		size = roundedSize(size) + sizeof(int);	// round to a size so that we can group multiple nearby sizes into single queue
		void* pChunk = nullptr;
		TQueue& queue = freeQ[size];
		if (!queue.empty())
		{
			pChunk = queue.front();
			queue.pop_front();
		}
		else	// no free buffer available - rely on the OS
		{
			pChunk = malloc(size);
			if (pChunk == nullptr) return nullptr;
			*((int*)pChunk) = size;
		}
#if BUFPOOL_TRACK_MEMORY
		inuseQ[size].push_front(pChunk);
#endif
		// clear the buffer to zero before handing over to client
		void *pBuf = (char*)pChunk + sizeof(int);
		memset(pBuf, 0, size - sizeof(int));
		return pBuf;
	}
	inline void release(void* pBuf)
	{
		if (pBuf == nullptr) return;
		void* pChunk = ((char*)pBuf - sizeof(int));
		int size = *((int*)pChunk);
		freeQ[size].push_front(pChunk);
#if BUFPOOL_TRACK_MEMORY
		TQueue& inuseQueue = inuseQ[size];
		auto foundIter = std::find(inuseQueue.cbegin(), inuseQueue.cend(), pChunk);
		assert(foundIter != inuseQueue.cend()); // if this fails, it means you are releasing a buffer that we did not allocate !!
		inuseQueue.erase(foundIter);
#endif
	}
	inline size_t allocatedSize(void* pBuf)
	{
		void* pChunk = ((char*)pBuf - sizeof(int));
		return *((int*)pChunk);
	}
#if BUFPOOL_TRACK_MEMORY
	inline bool isInUse(const void* pBuf) const
	{
		if (pBuf == nullptr) return false;
		void* pChunk = ((char*)pBuf - sizeof(int));
		int size = *((int*)pChunk);
		auto inUserIter = inuseQ.find(size);
		if (inUserIter == inuseQ.cend()) return false;
		const TQueue& inuseQueue = inUserIter->second;
		return inuseQueue.cend() != std::find(inuseQueue.cbegin(), inuseQueue.cend(), pChunk);
	}
#endif
protected:
	typedef std::deque<void*> TQueue;
	std::map<size_t, TQueue> freeQ;
#if BUFPOOL_TRACK_MEMORY
	std::map<size_t, TQueue> inuseQ;
#endif
}; //TODO: std::dequeue may not be thread-safe. Checkout any high-performance fast-inert/remove containers that are thread-safe.

struct bufPool
{
	template<typename T, class... Args>
	inline static T* acquire(Args&&... args)
	{
		return bufPoolT<T>::getObject().acquire(std::forward<Args>(args)...);
	}
	template<typename T>
	inline static void release(T* buf)
	{
		bufPoolT<T>::getObject().release(buf);
	}
};

#define _NEW(type)								bufPoolT<type>::getObject().acquire()
#define _DELETE(ptr)							bufPool::release(ptr)
#define _NEW1(type, arg1)						bufPoolT<type>::getObject().acquire(arg1)
#define _NEW2(type, arg1, arg2)					bufPoolT<type>::getObject().acquire(arg1, arg2)
#define _NEW3(type, arg1, arg2, arg3)			bufPoolT<type>::getObject().acquire(arg1, arg2, arg3)
#define _NEW4(type, arg1, arg2, arg3, arg4)		bufPoolT<type>::getObject().acquire(arg1, arg2, arg3, arg4)

#define POOLED_ALLOC(size)			bufPoolChunk::getObject().acquire(size)
#define POOLED_FREE(ptr)			bufPoolChunk::getObject().release(ptr)
#define POOLED_ALLOCATED_SIZE(ptr)	bufPoolChunk::getObject().allocatedSize(ptr)


// unique_bufptr is useful for transferring ownership. Use it as function parameter (by value).
// std::unique_ptr (at least on MSVC) takes additional memory to hold the deleter object, which is not required
template<typename T = void>
struct unique_ptr
{
	typedef unique_ptr _Myt;
	typedef T* pointer;

	template<typename objType>
	struct deleter
	{
		inline static void _free(objType* ptr) { _DELETE(ptr); }
	};
	template<>
	struct deleter<void>
	{
		inline static void _free(void* ptr) { POOLED_FREE(ptr); }
	};

	inline explicit operator bool() const 	{	return pChunk != nullptr;	}
	inline pointer get() const { return pChunk; }
	inline pointer release() { pointer temp = pChunk; pChunk = nullptr; return temp; } // yield ownership of pointer
	inline void reset(pointer ptr = nullptr) { if (pChunk != ptr) { deleter<T>::_free(pChunk);  pChunk = ptr; } } // release the pointer (and hold some other pointer)

	inline _Myt& operator=(_Myt&& other) { assert(this != &other); reset(other.release()); return *this; }
	inline _Myt& operator=(pointer ptr) { reset(ptr); return *this; }
	inline bool operator!=(nullptr_t ptr) const { return pChunk != ptr; }
	inline pointer operator->() { return pChunk; }
protected:
	T* pChunk = nullptr;

public:
#if BUFPOOL_TRACK_MEMORY
	template<typename objType>
	struct validator
	{
		inline static bool isInUse(objType* ptr) { return bufPoolT<objType>::getObject().isInUse(ptr); }
	};
	template<>
	struct validator<void>
	{
		inline static bool isInUse(void* ptr) { return bufPoolChunk::getObject().isInUse(ptr); }
	};
#endif
	inline explicit unique_ptr(pointer ptr) : pChunk(ptr)
	{
#if BUFPOOL_TRACK_MEMORY 
		// unique_bufptr can only deal with buffers that are allocated through one of bufPoolChunk or bufPoolT acquire() methods
		assert(validator<T>::isInUse(ptr)); //  Failure here means ptr is some externally allocated buffer that we do not know how to deal with
#endif
	}
	inline unique_ptr(_Myt&& other) : pChunk(other.pChunk)
	{ 
		other.pChunk = nullptr; 
	}
	inline ~unique_ptr()
	{ 
		reset(nullptr); 
	}
	unique_ptr(const _Myt&) = delete;
	_Myt& operator=(const _Myt&) = delete;
};

#endif // !_BUFPOOL_H__CBA8E586_437B_491E_B3BC_2C039526D9FD__
