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

#include "uv.h"
#include "bufPool.h"
#include "trie_array.h"
#include "singleton.h"

namespace DSCPP
{
	enum DS_SEPARATOR { DS_MESSAGE_SEPERATOR = 30, DS_MESSAGE_PART_SEPERATOR = 31 };

	struct _statemachine
	{
		enum STATE { 
			DS_INVALID_STATE = -1, 
			DS_DISCONNECTED = 0,
			DS_CONNECTED,			//<- connected state	->
			DS_SERVER_NEEDS_AUTH, 
			DS_LOGIN_INVALIDUSER,
			DS_LOGIN_ATTEMPTSLIMITED,
			DS_LOGIN_SUCCESSFUL,
			DS_READY_TO_TRANSFER,	//<- ready to transfer state ->
			DS_PROVIDER_ACKNOWLEDGED,
			DS_STATES_MAX 
		}; // DS_STATES_MAX should always be the last

		trie_prefixed_hash stateDirectiveMap;

		_statemachine();
	};
	extern _statemachine gStateMachine;

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
		void* alloc_send_buffer(size_t size)
		{
			return POOLED_ALLOC(size);
		}
		int send(const char* buf, size_t len)
		{
			if(len >0 && buf != nullptr) // ensure valid buffer (usually obtained from alloc_send_buffer())
				printf("%*s", len, buf);
			POOLED_FREE(buf);
			return 0;
		}
		int disconnect()
		{
			// stop any pending send/recv operations. 
		}
	};

	template
	<	typename TIOHandler = simpleIOHandler,
		typename TCredentialsSupplier = simpleCredentialsSupplier
	>
	struct HandlerJunction: public TIOHandler, public TCredentialsSupplier
	{
		typedef TIOHandler TIOHandler;
		typedef TCredentialsSupplier TCredentialsSupplier;
	};

	// Provides base functionality with extensible IO and Credential handlers.
	// You would need a driver (or loop) to use this correctly.
	template<typename THandlerJunction = HandlerJunction<>>
	class _dsclientBase : public THandlerJunction
	{
		typedef typename THandlerJunction::TIOHandler IO;
		typedef typename THandlerJunction::TCredentialsSupplier CS;

		enum { SENDBUF_SIZE = 4096, MAX_METHODNAME_LEN = 256 };

		typedef _dsclientBase _MyType;
		struct _statehandlers
		{
			enum 
			{ 
				MAX_DIRECTIVE_LEN = 32, // this governs the prefixMatch results array size, usually should be equal to the length of the longest directive string
				MAX_HANDLERS_COUNT = 16 // this governs the handler array size, usually should be equal to the no. of handlers / no. of directives
			};
			typedef int(_MyType::*LPFNStateHandler)(const char*, size_t);
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
				};
				
				static_assert(MAX_HANDLERS_COUNT > sizeof(stateDirectives) / sizeof(stateDirectives[0])); // increase the MAX_HANDLERS_COUNT value if needed

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

		_statemachine::STATE	m_currentState; // tracks the current state
		_statehandlers&			m_stateHandlers;// the state handler methods

		typedef int(*LPFNRPCMethod)(const char*);
		typedef trie_array<LPFNRPCMethod> TRPCTrieArray;
		TRPCTrieArray&			m_Router;		// maps method_names -> method_handlers

		int						m_nLoginRetryCount;
		bool					m_bReadyForTransfer;	// indicates connected & successful auth state
	public:
		inline _dsclientBase() :
			m_stateHandlers(_singleton<_statehandlers>::getObject().value),
			m_Router(_singleton<TRPCTrieArray>::getObject().value),
			m_nLoginRetryCount(0),
			m_bReadyForTransfer(false)
		{
		}

		inline bool is_ready_for_transfer() const
		{
			return m_bReadyForTransfer;
		}

		inline int handle_server_directive(const char* buf, size_t size)
		{
			auto handler = m_stateHandlers.getHandler(buf, size);
			return (this->*handler)(buf, size);
		}

	protected:
		inline int send_auth()
		{
			char* buf = (char*)IO::alloc_send_buffer(SENDBUF_SIZE); // request buffer from the IO handler
			std::string strUsername = CS::getUsername();
			std::string strPassword = CS::getPassword();
			int len = sprintf(buf, "A%cREQ%c{\"username\":\"%s\",\"password\":\"%s\"}%c", DS_MESSAGE_PART_SEPERATOR, DS_MESSAGE_PART_SEPERATOR, strUsername.c_str(), strPassword.c_str(), DS_MESSAGE_SEPERATOR);
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
		int on_connected(const char* buf, size_t size)
		{
			return 0;
		}
		int on_server_needs_auth(const char* buf, size_t size)
		{
			// server is asking for Auth, let us give one
			return this->send_auth();
		}
		int on_login_invalid(const char* buf, size_t size)
		{
			if (++m_nLoginRetryCount <= getMaxRetries())
				return this->send_auth();
			return disconnect();
		}
		int on_toomany_auth_attempts(const char* buf, size_t size)
		{
			disconnect(); // server doesn't like us trying to so many times and closed the connection
			return 0;
		}
		int on_login_successful(const char* buf, size_t size)
		{
			m_nLoginRetryCount = 0; // reset the login retry count
			m_bReadyForTransfer = true;
			send_rpc_providers(); // (re)send the rpc providers to server
			return 0;
		}
		int on_provider_acknowledged(const char* buf, size_t size)
		{
			return 0;
		}
		int on_ready_to_transfer(const char* buf, size_t size)
		{
			return 0;
		}
		int on_disconnected(const char* buf, size_t size)
		{
			disconnect();
			return 0;
		}
		int on_unknown(const char* buf, size_t size)
		{
			// no clue what server is talking about.
			fprintf(stderr, "\nUnknown Directive: [%*s]", size, buf);
			return 0;
		}

	//////////////////////////////////////////////////////////
	// RPC
	//
	public:
		// registers a handler for a given method name. 
		// If handler already assigned for the given name, it will be overwritten.
		// returns -1 in case of failure.
		inline int register_rpc_provider(const char* szMethodName, LPFNRPCMethod handler)
		{
			int methodId = m_Router.insertkv(szMethodName, handler);
			if(is_ready_for_transfer())
				return send_rpc_provider(szMethodName);
			return 0;
		}
	protected:
		inline int send_rpc_provider(const char* szMethodName)
		{
			char* buf = (char*)IO::alloc_send_buffer(SENDBUF_SIZE); // request buffer from the IO handler
			int len = sprintf(buf, "P%cS%c%s%c", DS_MESSAGE_PART_SEPERATOR, DS_MESSAGE_PART_SEPERATOR, szMethodName, DS_MESSAGE_SEPERATOR);
			return IO::send(buf, len);
		}
		inline int send_rpc_providers()
		{
			TRPCTrieArray::iterator iter = m_Router.begin();
			TRPCTrieArray::iterator iterEnd = m_Router.end();
			while (iter != iterEnd)
			{
				char methodName[MAX_METHODNAME_LEN];
				iter->key(methodName, MAX_METHODNAME_LEN);
				send_rpc_provider(methodName);
				++iter;
			}
			return 0;
		}
	};
	typedef _dsclientBase<> DSClientBase;
} // name-space DSCPP

#endif // !_DSCPPCLIENT_H_A29CB5C0_020E_407F_B75C_BD91967B6EE8_
