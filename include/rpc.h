/*
	Copyright (c) 2016 Cenacle Research India Private Limited
*/

#ifndef _RPCCACHE_H_8021275D_5703_48CD_A2BE_DE221CF462F8_
#define _RPCCACHE_H_8021275D_5703_48CD_A2BE_DE221CF462F8_

#include "bufPool.h"
#include "trie_array.h"

namespace DSCPP
{
	struct _rpcCall
	{
		typedef unique_ptr<void> unique_bufptr;
		const char*		methodName;	// points to the start of method name in the buf
		int				nameLen;
		const char*		uid;		// points to the start of uid in the buf
		int				uidLen;
		const char*		params;		// points to the start of parameters in the buf
		int				paramsLen;
		unique_bufptr	spbuf;		// the buffer received from server read (should not be modified in the RPC method)
		size_t			bufLen;
		inline _rpcCall(unique_bufptr&& buf): spbuf(std::forward<unique_bufptr>(buf)) { }
	};


	enum RPC_RESULT_TYPE : short { RPC_SINGLE_RESULT = 0, RPC_PROGRESSIVE_RESULT, RPC_STREAMED_RESULT };

	struct _rpcResult
	{
		void* buf;
		int bufLen;
	};

	struct _rpcCache
	{
		trie_array<_rpcResult> resultArray;
		typedef typename trie_array<_rpcResult>::KEY CACHEID;
	
		CACHEID find(const char* params, int paramsLen) const
		{
			return resultArray.findKey(params, paramsLen);
		}
		CACHEID saveResult(_rpcResult& result, _rpcCall& call) // adds the result to cache
		{
			resultArray.insertkv(call.params, call.paramsLen, result);
		}
		const _rpcResult& getResult(CACHEID id) const
		{
			return resultArray[id];
		}
		void clear() // clears the cache
		{
			resultArray.clear();
		}
	};	

	//struct _rpcProvider
	//{
	//	LPFNRPCMethod	handler;

	//	inline _rpcProvider(LPFNRPCMethod argHandler, bool bCache = true): handler(argHandler), pCache(bCache ? _NEW(_rpcCache) : nullptr) {}
	//	~_rpcProvider() { _DELETE(pCache); }
	//	
	//	inline _rpcCache* cache() { return pCache; }
	//protected:
	//	_rpcCache* pCache = nullptr;
	//};

} // namespace DSCPP

#endif // !_RPCCACHE_H_8021275D_5703_48CD_A2BE_DE221CF462F8_
