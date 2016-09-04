/*
	Copyright (c) 2016 Cenacle Research India Private Limited
*/

#include "dsclientbase.h"

namespace DSCPP
{
	_statemachine gStateMachine;

	_statemachine::_statemachine()
	{
		struct
		{
			STATE currentState;
			const char* serverDirective;
			STATE nextState;
		} stateDirectives[] =
		{
			DS_CONNECTED,			"C|A+",		DS_SERVER_NEEDS_AUTH,
			DS_SERVER_NEEDS_AUTH,	"A|A+",		DS_LOGIN_SUCCESSFUL,
			DS_SERVER_NEEDS_AUTH,	"A|E|INVALID_AUTH_DATA|",	DS_LOGIN_INVALIDUSER,
			DS_SERVER_NEEDS_AUTH,	"A|E|TOO_MANY_AUTH_ATTEMPTS|",	DS_LOGIN_ATTEMPTSLIMITED,
		};
		
		// load the transitions
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
			stateDirectiveMap[directive.currentState].hash(str.c_str(), str.length(), directive.nextState);
		}
	}


} // name-space DSCPP