/*
	Copyright (c) 2016 Cenacle Research India Private Limited
*/

#ifndef _DSCPPCLIENT_H_A29CB5C0_020E_407F_B75C_BD91967B6EE8_
#define _DSCPPCLIENT_H_A29CB5C0_020E_407F_B75C_BD91967B6EE8_

#include <iostream>
#include <sstream>
#include <iomanip>
#include <signal.h>
#include <future>
#include <memory>
#include <type_traits>

#include "singleton.h"
#include "rpc.h"
#include "uv.h"

namespace DSCPP
{
	enum DS_SEPARATOR { DS_MESSAGE_SEPERATOR = 30, DS_MESSAGE_PART_SEPERATOR = 31 };

	// simpleCredentialsSupplier takes care of credentials data.
	// Implement your own and supply it to the _dsclientBase class
	// as template argument and constructor parameter. 
	struct simpleCredentialsSupplier
	{
		std::string strUsername;
		std::string strPassword;
		inline simpleCredentialsSupplier()
		{	}
		inline std::string getUsername() const
		{
			return strUsername;
		}
		inline std::string getPassword() const
		{
			return strPassword;
		}
		inline int getMaxRetries() const // how many times should retry in case of Auth-failures
		{
			return 2;
		}
	};

	// gets invoked when the send() is complete by IOHandler
	typedef void(*LPFN_SEND_COMPLETE)(void*, size_t);

	// IOHandler that takes care of sending and receiving data.
	// Implement your own and supply it to the _dsclientBase class
	// as template argument and constructor parameter. 
	// For the send and recv methods, return 
	//		-1  to indicate failure, and
	//		>=0 to indicate success.
	// The alloc_send_buffer should allocate a buffer of given size, which
	// will then be used in the call to the send.
	struct simpleIOHandler
	{
		static inline void* alloc_send_buffer(size_t size)
		{
			return POOLED_ALLOC(size);
		}
		static inline void release_send_buffer(void* buf, size_t s)
		{
			POOLED_FREE(buf);
		}
		int send(void* buf, size_t len, LPFN_SEND_COMPLETE cb = release_send_buffer)
		{
			if(len >0 && buf != nullptr) // ensure valid buffer (usually obtained from alloc_send_buffer())
				fprintf(stderr, "sending: %*s", len, (char*)buf);
			(*cb)(buf, len);
			return 0;
		}
		int disconnect()
		{
			// stop any pending send/recv operations. 
		}
	};


	// Provides base functionality with extensible IO and Credential handlers.
	// You would need a driver (or loop) to use this correctly.
	template<typename TIOHandler = simpleIOHandler,	typename TCredentialsSupplier = simpleCredentialsSupplier>
	class _dsclientBase : public TIOHandler, public TCredentialsSupplier
	{
	public:
		typedef _dsclientBase _MyType;
		typedef _dsclientBase TBase;
		typedef typename TIOHandler IO;
		typedef typename TCredentialsSupplier CS;

		typedef unique_ptr<void> unique_bufptr;

		typedef int(*LPFNRPCMethod)(unique_ptr<_rpcCall>, _MyType*);
		typedef trie_array<LPFNRPCMethod> TRPCTrieArray;

		enum { SENDBUF_SIZE = 4096, MAX_UID_LEN = 64, MAX_METHODNAME_LEN = 128, MAX_USERNAME_LEN = 32, MAX_PASSWORD_LEN = 32 };

	protected:
		struct _statehandlers
		{
			enum 
			{ 
				MAX_DIRECTIVE_LEN = 32, // this governs the prefixMatch results array size, usually should be equal to the length of the longest directive string
				MAX_HANDLERS_COUNT = 16 // this governs the handler array size, usually should be equal to the no. of handlers (no. of directives)
			};
			typedef int(_MyType::*LPFNStateHandler)(unique_bufptr, size_t);
			typedef trie_prefixed_array<LPFNStateHandler, static_array<LPFNStateHandler, MAX_HANDLERS_COUNT>> TRIE_ARRAY;
			TRIE_ARRAY handlerArray;
			inline _statehandlers()
			{
				struct
				{
					const char* serverDirective;
					LPFNStateHandler handler;
				} stateDirectives[] =
				{
					"C|A+",								&_MyType::on_server_needs_auth,
					"A|A",								&_MyType::on_login_successful,
					"A|E|INVALID_AUTH_DATA|",			&_MyType::on_login_invalid,
					"A|E|TOO_MANY_AUTH_ATTEMPTS|",		&_MyType::on_toomany_auth_attempts,
					"P|A|S|",							&_MyType::on_provider_acknowledged,
					"P|REQ|",							&_MyType::on_rpc_call_received,
				};
				
				assert( MAX_HANDLERS_COUNT > (sizeof(stateDirectives) / sizeof(stateDirectives[0])) ); // increase the MAX_HANDLERS_COUNT value if needed

				for (int i = 0; i < sizeof(stateDirectives) / sizeof(stateDirectives[0]); ++i)
				{
					auto directive = stateDirectives[i];
					std::string str = directive.serverDirective;
					// convert human-readable to machine-readable
					std::for_each(str.begin(), str.end(), [](char& ch) {
						if (ch == '|') ch = DS_MESSAGE_PART_SEPERATOR;
						else if (ch == '+') ch = DS_MESSAGE_SEPERATOR;
					});
					// index the state transfer map
					handlerArray.insertkv(str.c_str(), str.length(), directive.handler);
					assert(str.length() < MAX_DIRECTIVE_LEN); // we use MAX_DIRECTIVE_LEN to control the lookup result array size
				}				
			}
			inline LPFNStateHandler getHandler(const char* buf, size_t size) const
			{
				typename TRIE_ARRAY::TRIE::result_pair_type results[MAX_DIRECTIVE_LEN];
				return handlerArray.prefixMatch(buf, size, results, MAX_DIRECTIVE_LEN, &_MyType::on_unknown);
			}
		};
		static _statehandlers	s_stateHandlers;	// the state handler methods
		TRPCTrieArray			m_rpcRouter;		// maps method_names -> method_handlers
		int						m_nLoginRetryCount;
		bool					m_bReadyForTransfer;	// indicates connected & successful auth state
	public:
		inline _dsclientBase() :
			m_nLoginRetryCount(0),
			m_bReadyForTransfer(false)
		{
		}

		inline bool is_ready_for_transfer() const
		{
			return m_bReadyForTransfer;
		}

		inline int handle_server_directive(unique_bufptr spbuf, size_t size)
		{
			auto handler = s_stateHandlers.getHandler((char*)spbuf.get(), size);
			return (this->*handler)(std::forward<unique_bufptr>(spbuf), size);
		}

	protected:
		inline int send_auth()
		{
			std::string strUsername = CS::getUsername();
			std::string strPassword = CS::getPassword();
			if (strUsername.length() >= MAX_USERNAME_LEN || strPassword.length() >= MAX_PASSWORD_LEN) return -1;

			void* buf = IO::alloc_send_buffer(SENDBUF_SIZE); // request buffer from the IO handler
			int len = sprintf((char*) buf, "A%cREQ%c{\"username\":\"%s\",\"password\":\"%s\"}%c", DS_MESSAGE_PART_SEPERATOR, DS_MESSAGE_PART_SEPERATOR, strUsername.c_str(), strPassword.c_str(), DS_MESSAGE_SEPERATOR);
			return IO::send(buf, len); // let the IO handler do the send
		}
		inline int disconnect()
		{
			m_bReadyForTransfer = false;
			return IO::disconnect();
		}
	//////////////////////////////////////////////////////////
	// State Handlers
	//
	protected:
		int on_connected(unique_bufptr spbuf, size_t size)
		{
			return 0;
		}
		int on_server_needs_auth(unique_bufptr spbuf, size_t size)
		{
			// server is asking for Auth, let us give one
			return this->send_auth();
		}
		int on_login_invalid(unique_bufptr spbuf, size_t size)
		{
			if (++m_nLoginRetryCount <= getMaxRetries())
				return this->send_auth();
			return disconnect();
		}
		int on_toomany_auth_attempts(unique_bufptr spbuf, size_t size)
		{
			disconnect(); // server doesn't like us trying to so many times and closed the connection
			return 0;
		}
		int on_login_successful(unique_bufptr spbuf, size_t size)
		{
			m_nLoginRetryCount = 0; // reset the login retry count
			m_bReadyForTransfer = true;
			send_rpc_providers(); // (re)send the rpc providers to server
			return 0;
		}
		int on_provider_acknowledged(unique_bufptr spbuf, size_t size)
		{
			return 0;
		}
		int on_ready_to_transfer(unique_bufptr spbuf, size_t size)
		{
			return 0;
		}
		int on_disconnected(unique_bufptr spbuf, size_t size)
		{
			disconnect();
			return 0;
		}
		int on_unknown(unique_bufptr spbuf, size_t size)
		{
			// no clue what server is talking about.
			fprintf(stderr, "\nUnknown Directive: [%.*s]", size, (char*) spbuf.get());
			return -1;
		}
		int on_rpc_call_received(unique_bufptr spbuf, size_t bufsize)
		{
			const char* buf = (const char*) spbuf.get();

			const char* pBuf = buf + 6; // methodName starts at 6th char

			const char* methodName = pBuf;
			int nameLen = 0;
			while (*pBuf && *pBuf++ != DS_MESSAGE_PART_SEPERATOR && ++nameLen < MAX_METHODNAME_LEN);

			if (nameLen >= MAX_METHODNAME_LEN || *pBuf == '\0') 
				return on_unknown(std::forward<unique_bufptr>(spbuf), bufsize);	// malformed RPC call, we do not respond

			const char* uid = pBuf;
			int uidLen = 0;
			while (*pBuf && *pBuf++ != DS_MESSAGE_PART_SEPERATOR && ++uidLen < MAX_UID_LEN);
			if (uidLen >= MAX_UID_LEN || *pBuf == '\0')
				return on_unknown(std::forward<unique_bufptr>(spbuf), bufsize);	// malformed RPC call, we do not respond

			// reject if provider does not exist
			LPFNRPCMethod rpcHandler = m_rpcRouter.at(methodName, nameLen, nullptr);
			 if (rpcHandler == nullptr)
				 return send_rpc_unsupported(std::forward<unique_bufptr>(spbuf), (pBuf - 1) - buf); // pBuf is pointing one past the part-separator, hence -1

			const char* params = pBuf;
			int paramsLen = bufsize - (params - buf);

			// prepare the RPC data-structure
			unique_ptr<_rpcCall>  sprpcCall(_NEW1(_rpcCall, std::forward<unique_bufptr>(spbuf)));
			sprpcCall->methodName = methodName;
			sprpcCall->nameLen = nameLen;
			sprpcCall->uid = uid;
			sprpcCall->uidLen = uidLen;
			sprpcCall->params = params;
			sprpcCall->paramsLen = paramsLen;
			sprpcCall->bufLen = bufsize;

			// tell server that we are processing the rpc
			send_rpc_call_acknowledgement(*sprpcCall.get());

			// make an actual call
			return (*rpcHandler)(std::forward<unique_ptr<_rpcCall>>(sprpcCall), this);

			return 0;
		}
	//////////////////////////////////////////////////////////
	// RPC
	//
	public:
		// registers a handler for a given method name. 
		// If handler already assigned for the given name, returns -1.
		// returns 0 on success.
		inline int register_rpc_provider(const char* szMethodName, LPFNRPCMethod handler, bool bCacheable = true)
		{
			int len = strlen(szMethodName);
			if (len >= MAX_METHODNAME_LEN) return -1;
			auto methodId = m_rpcRouter.findKey(szMethodName, len);
			if (methodId >= 0) return -1; // already registered !!
			methodId = m_rpcRouter.insertkv(szMethodName, len, handler);
			if(is_ready_for_transfer())
				return send_rpc_provider(szMethodName);
			return 0;
		}
		inline int unregister_rpc_provider(const char* szMethodName)
		{
			int len = strlen(szMethodName);
			if (len >= MAX_METHODNAME_LEN) return -1;
			auto methodId = m_rpcRouter.findKey(szMethodName, len);
			if (methodId >= 0) // unregister, only if registered
			{
				_DELETE(m_rpcRouter[methodId]);
				m_rpcRouter.updateValue(methodId, nullptr);
				if(is_ready_for_transfer())
					return send_rpc_unprovide(szMethodName);
			}
			return 0;
		}
	protected:
		inline int send_rpc_unprovide(const char* szMethodName)
		{
			char* buf = (char*)IO::alloc_send_buffer(SENDBUF_SIZE); // request buffer from the IO handler
			int len = sprintf(buf, "P%cUS%c%s%c", DS_MESSAGE_PART_SEPERATOR, DS_MESSAGE_PART_SEPERATOR, szMethodName, DS_MESSAGE_SEPERATOR);
			return IO::send(buf, len);
		}
		inline int send_rpc_provider(const char* szMethodName)
		{
			char* buf = (char*)IO::alloc_send_buffer(SENDBUF_SIZE); // request buffer from the IO handler
			int len = sprintf(buf, "P%cS%c%s%c", DS_MESSAGE_PART_SEPERATOR, DS_MESSAGE_PART_SEPERATOR, szMethodName, DS_MESSAGE_SEPERATOR);
			return IO::send(buf, len);
		}
		inline int send_rpc_providers()
		{
			TRPCTrieArray::iterator iter = m_rpcRouter.begin();
			TRPCTrieArray::iterator iterEnd = m_rpcRouter.end();
			while (iter != iterEnd)
			{
				char methodName[MAX_METHODNAME_LEN];
				iter->key(methodName, MAX_METHODNAME_LEN); // load the iterator key into the name buffer
				if(iter->value() != nullptr) send_rpc_provider(methodName);	// values with nullptr are probably the unregistered ones
				++iter;
			}
			return 0;
		}
		inline int send_rpc_call_acknowledgement(const _rpcCall& c)
		{
			char* buf = (char*)IO::alloc_send_buffer(SENDBUF_SIZE); // request buffer from the IO handler
			int len = sprintf(buf, "P%cA%c%.*s%c%.*s%c", DS_MESSAGE_PART_SEPERATOR, DS_MESSAGE_PART_SEPERATOR, c.nameLen, c.methodName, DS_MESSAGE_PART_SEPERATOR, c.uidLen, c.uid, DS_MESSAGE_SEPERATOR);
			return IO::send(buf, len); // let the IO handler do the send
		}
		public:
		inline int send_rpc_call_result(_rpcCall& c, const char* szResult, int nLen)
		{
			char* buf = (char*)c.spbuf.release();
			buf[4] = 'S'; // convert REQ -> RES
			char* result = (char*) c.params;
			*result++ = 'S'; // string type follows
			memcpy(result, szResult, nLen);
			result[nLen] = DS_MESSAGE_SEPERATOR;
			return IO::send(buf, result + nLen - buf + 1); // buf will be released after send

		}
		inline int send_rpc_unsupported(unique_bufptr spReqbuf, int nPartSepIndex)
		{
			// we do not support the method the server is asking us to execute, lets reject it
			char* rejBuf = (char*) spReqbuf.release();	// transfer the ownership 
			rejBuf[4] = 'J';					// convert REQ -> REJ
			rejBuf[nPartSepIndex] = DS_MESSAGE_SEPERATOR;
			return IO::send(rejBuf, nPartSepIndex + 1); // rejBuf will be deleted after send is done, automatically
		}
	};
	typedef _dsclientBase<> DSClientBase;

	// These could be defined as singletons but C++11 singletons are unnecessarily complex, emitting 
	// 20+ instructions (that involve TLS locking). Not worth it.
	// Simple global variables could solve the same problem more effectively any day, if one knows 
	// how to use them correctly.
	template<typename TIO, typename TCred> typename _dsclientBase<TIO, TCred>::_statehandlers _dsclientBase<TIO, TCred>::s_stateHandlers;
} // name-space DSCPP

#endif // !_DSCPPCLIENT_H_A29CB5C0_020E_407F_B75C_BD91967B6EE8_
