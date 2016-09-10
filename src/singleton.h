/*
	Copyright (c) 2016 Cenacle Research India Private Limited
*/

#ifndef _SINGLETON_H_DCC68E94_5FA1_4714_8DE8_AE7983A5FDDB__
#define _SINGLETON_H_DCC68E94_5FA1_4714_8DE8_AE7983A5FDDB__

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

#endif // !_SINGLETON_H_DCC68E94_5FA1_4714_8DE8_AE7983A5FDDB__
