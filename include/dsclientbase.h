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

#include "uv.h"
#include "bufPool.h"
#include "trie_array.h"

namespace DSCPP
{
	enum DS_SEPARATOR { DS_MESSAGE_SEPERATOR = 30, DS_MESSAGE_PART_SEPERATOR = 31 };

	struct _statemachine
	{
		enum STATE { 
			DS_INVALID_STATE = -1, 
			DS_CONNECTED = 0, 
			DS_SERVER_NEEDS_AUTH, 
			DS_LOGIN_SUCCESSFUL,
			DS_LOGIN_INVALIDUSER,
			DS_LOGIN_ATTEMPTSLIMITED,
			DS_DISCONNECTED,
			DS_STATES_MAX }; // DS_STATES_MAX should always be the last

		std::map<STATE, trie_prefixed_hash> stateDirectiveMap;

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

	template<typename T>
	struct _singleton
	{
		T value;
		static _singleton & getObject()
		{
			static _singleton obj;	// Thread-safe since C++11
			return obj;
		}
		// delete the copy, move constructors and assign operators
		_singleton(_singleton const&) = delete;             // Copy construct
		_singleton(_singleton&&) = delete;                  // Move construct
		_singleton& operator=(_singleton const&) = delete;  // Copy assign
		_singleton& operator=(_singleton &&) = delete;      // Move assign
	protected:
		inline _singleton()
		{	}
		inline ~_singleton()
		{	}
	public:
		typedef T _ValueType;
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

	// Provides base functionality with extensible IO and Credential handlers.
	// You would need a driver (or loop) to use this correctly.
	template<class TIOHandler = simpleIOHandler, class TCredentialsSupplier = simpleCredentialsSupplier>
	class _dsclientBase
	{
		typedef _dsclientBase<TIOHandler, TCredentialsSupplier> _MyType;
		struct _statehandlers
		{
			typedef int(_MyType::*LPFNStateHandler)();
			LPFNStateHandler handler[_statemachine::STATE::DS_STATES_MAX];
			inline _statehandlers()
			{
				handler[_statemachine::DS_CONNECTED] = &_MyType::on_connected;
				handler[_statemachine::DS_SERVER_NEEDS_AUTH] = &_MyType::on_server_needs_auth;
				handler[_statemachine::DS_LOGIN_SUCCESSFUL] = &_MyType::on_login_successful;
				handler[_statemachine::DS_LOGIN_INVALIDUSER] = &_MyType::on_login_invalid;
				handler[_statemachine::DS_LOGIN_ATTEMPTSLIMITED] = &_MyType::on_disconnected;
				handler[_statemachine::DS_DISCONNECTED] = &_MyType::on_disconnected;
			}
			inline LPFNStateHandler operator[](_statemachine::STATE state) const
			{
				assert(state > _statemachine::DS_INVALID_STATE && state < _statemachine::DS_STATES_MAX);
				return handler[state];
			}
		};
		typedef _singleton<_statehandlers> TStateHandlers;

		_statemachine::STATE	m_currentState; // tracks the current state
		_statehandlers&			m_stateHandlers;// the state handler methods
		TIOHandler&				m_IO;			// delegates IO handling to external providers
		TCredentialsSupplier&	m_CS;			// delegates Credential handling to external providers

		int						m_nLoginRetryCount;

	public:
		inline _dsclientBase(TIOHandler& io, TCredentialsSupplier& cs, _statemachine::STATE initState = _statemachine::STATE::DS_CONNECTED): 
			m_IO(io), 
			m_CS(cs), 
			m_stateHandlers(TStateHandlers::getObject().value),
			m_nLoginRetryCount(0)
		{
			// set the initial state
			push_to_state(initState);
		}
		int provide_rpc_method(const char* szMethodName, )
		{

		}
		inline int handle_server_directive(const char* buf, size_t size)
		{
			int targetState = gStateMachine.stateDirectiveMap[m_currentState].prefixMatch(buf, size);
			if (targetState < 0) return -1; // don't know what the server is talking about. return error
			return push_to_state((_statemachine::STATE)targetState);
		}
	protected:
		inline int push_to_state(_statemachine::STATE targetState)
		{
			return (this->*m_stateHandlers[m_currentState = targetState])();
		}
		inline int send_auth()
		{
			char* buf = (char*) m_IO.alloc_send_buffer(4096); // request buffer from the IO handler
			std::string strUsername = m_CS.getUsername();
			std::string strPassword = m_CS.getPassword();
			int len = sprintf(buf, "A%cREQ%c{\"username\":\"%s\",\"password\":\"%s\"}%c", DS_MESSAGE_PART_SEPERATOR, DS_MESSAGE_PART_SEPERATOR, strUsername.c_str(), strPassword.c_str(), DS_MESSAGE_SEPERATOR);
			return m_IO.send(buf, len); // let the IO handler do the send
		}

		int on_connected()
		{
			return 0;
		}

		int on_server_needs_auth()
		{
			// server is asking for Auth, let us give one
			return this->send_auth();
		}
		int on_login_successful()
		{
			m_nLoginRetryCount = 0; // reset the login retry count
			return 0;
		}
		int on_login_invalid()
		{
			if(++m_nLoginRetryCount <= m_CS.getMaxRetries())
				return push_to_state(_statemachine::STATE::DS_SERVER_NEEDS_AUTH);
			return push_to_state(_statemachine::STATE::DS_DISCONNECTED);
		}
		int on_disconnected()
		{
			m_IO.disconnect();
			return 0;
		}
	};
	typedef _dsclientBase<> DSClientBase;
} // name-space DSCPP

#endif // !_DSCPPCLIENT_H_A29CB5C0_020E_407F_B75C_BD91967B6EE8_
