/*
	Copyright (c) 2016 Cenacle Research India Private Limited
*/

#ifndef _TRIE_ARRAY_H__Guid__F3C84037_D677_4A13_8D9F_CBA6FE1F5944___
#define _TRIE_ARRAY_H__Guid__F3C84037_D677_4A13_8D9F_CBA6FE1F5944___

#pragma warning( disable : 4800) // ignore performance warning: forcing value to bool 'true' or 'false'

#include <vector>
#include <algorithm>
#include <map>
#include "cedarpp.h"

#ifndef _ALLOC
#define _ALLOC		std::malloc
#endif
#ifndef _FREE
#define _FREE		std::free
#endif
#ifndef _REALLOC
#define _REALLOC	std::realloc
#endif

// trie_hash: A trie-based hash generator for strings.
//	Generates a perfect hash for strings. 
//	Users can also supply their own values as hash.
/* Advantages:
	+ No collisions
	+ Grows dynamically
	+ Hash values are sequential (in the order of insertion)
	+ User-defined hashes (if required). Useful to generate same hash for multiple strings.
	+ Very scalable. Lookup in O(m) time, where m is length of string.

  Disadvantages for common-hash functions (implemented in usual means by others):
	+ Cannot guarantee collision-free
	+ No control over collisions
	+ May not grow dynamically
	+ No control over the range of generated hash values
	+ Hash lookups may be O(n), where n is the size of all keys

   Use-case for controlled collisions (generating same hash for multiple strings):
 	+ case insensitive mappings.
	+ URL redirections etc. where multiple keys need to map to same value.
*/
struct trie_hash
{
	typedef int HASH;
	typedef cedar::da<HASH, -1, -2, false> TRIE;
protected:
	TRIE m_trie;
public:
	/// <summary>
	/// Returns the hash for a given string.
	///	 + If a hash value has been specified manually earlier, then it will be returned.
	///  + If no hash was specified earlier, a new hash will be calculate and returned.
	///	 + In such case, caller can specify the hash value to be used (instead of new hash calculation).
	/// @param szKey the string for which to compute the hash
	///	@param keyLen the length of the string
	/// @param defaultHash optional new hash value to be used
	/// @return the hash value for the given string. If a hash value has been already
	///	 set earlier, it will be returned. If no hash was specified earlier for the given
	///	 string, a new hash value will be calculated and returned. By default, it will be the 
	///	 number of strings hashed till now +1. However, caller can override this default
	///	 behavior and specify their own hash to be used for the new string, by supplying a
	///	 positive integer value in defaultHash (Collisions may happen and will not be resolved).
	///	 The defaultHash value supplied is used only for new strings whose hash not yet been 
	///	 specified manually or calculated earlier. For strings whose hash already been 
	///	 calculated once or specified manually earlier, the old values will be returned and
	///  the defaultHash value will be ignored.
	/// </summary>
	inline HASH hash(const char* szKey, size_t keyLen, HASH defaultHash=-1)
	{
		HASH result = getHash(szKey, keyLen);
		if (result < 0)
		{
			result = (defaultHash == -1 ? (m_trie.num_keys()) : defaultHash);
			m_trie.update(szKey, keyLen) = result;
		}
		return result;
	}
	inline void hash(const char* keys[], int arrayLen)
	{
		for (int i = 0; i < arrayLen; ++i)
			hash(keys[i], std::strlen(keys[i]));
	}
	inline void hash(const char* keys[], size_t keylen[], int arrayLen)
	{
		for (int i = 0; i < arrayLen; ++i)
			hash(keys[i], keylen[i]);
	}
	/// <summary>Returns the hash for a string if it exists in the Trie. Else returns -1</summary>
	inline HASH getHash(const char* szKey) const
	{
		return getHash(szKey, std::strlen(szKey));
	}
	inline HASH getHash(const char* szKey, size_t keyLen) const
	{
		return m_trie.exactMatchSearch<TRIE::result_type>(szKey, keyLen);
	}
	/// <summary>Sets the hash for a string to the given value. Overwrites the old hash value if exists.</summary>
	inline void setHash(const char* keys[], const HASH hash[], int arrayLen)
	{
		for (int i = 0; i < arrayLen; ++i)
			m_trie.update(keys[i]) = hash[i];
	}
	inline void setHash(const char* keys[], size_t keylen[], const HASH hash[], int arrayLen)
	{
		for (int i = 0; i < arrayLen; ++i)
			m_trie.update(keys[i], keylen[i]) = hash[i];
	}
	/// <summary>clears all keys and values (without releasing the memory)</summary>
	inline void clear()
	{
		m_trie.reset();
	}

	/// <summary>iterator support for trie_hash</summary>
	struct iterator
	{
	protected:
		cedar::npos_t	m_from;
		size_t			m_len;
		HASH			m_hash;
		trie_hash*		m_pTrie;
	public:
		inline iterator(trie_hash* _trie): m_from(0), m_len(0), m_hash(trie_hash::TRIE::CEDAR_NO_PATH), m_pTrie(_trie) { }
		inline iterator(cedar::npos_t _from, size_t _len, trie_hash* _trie): m_from(_from), m_len(_len), m_hash(trie_hash::TRIE::CEDAR_NO_PATH), m_pTrie(_trie)
		{
			m_hash = m_pTrie->m_trie.begin(m_from, m_len);
		}
		inline iterator& operator++()
		{
			if(m_hash != trie_hash::TRIE::CEDAR_NO_PATH)
				m_hash = m_pTrie->m_trie.next(m_from, m_len);
			return *this;
		}
		inline iterator& operator++(int) // post_increment
		{
			iterator temp = *this;
			++*this;
			return temp;
		}
		inline bool operator==(const iterator& other) const
		{
			return (m_pTrie == other.m_pTrie) && m_hash == other.m_hash;
		}
		inline bool operator!=(const iterator& other) const
		{
			return !(*this == other);
		}
		inline const iterator& operator*() const
		{
			return *this;
		}
		inline const iterator* operator->() const
		{
			return this;
		}
		inline HASH hash() const { return m_hash;  }
		inline size_t keylen() const { return m_len; }
		inline const char* key(char* buf, size_t bufLen) const
		{
			assert(bufLen >= (keylen() + 1)); // buf should be preallocated by user to be large enough. 
			assert('\0' == (buf[keylen()] = '\0')); // try to raise a memory corruption error for small buffers
			if(m_hash != trie_hash::TRIE::CEDAR_NO_PATH) m_pTrie->m_trie.suffix(buf, m_len, m_from);
			return buf;
		}
	};

	inline iterator begin()
	{
		return iterator(0, 0, this);
	}
	inline iterator end()
	{
		return iterator(this);
	}
};

// trie_prefixed_hash is same as trie_hash except strings are hashed based 
// on prefix. Two strings yield same hash if one is the prefix of other.
struct trie_prefixed_hash: public  trie_hash
{
	// returns the HASH of the found match. -1 if none.
	// Updates the keyLen to be the length of the match found. Zero if no match found.
	inline HASH prefixMatch(const char* szKey, size_t& keyLen) const
	{
		TRIE::result_pair_type result = { (HASH)-1, 0 };
		m_trie.commonPrefixSearch(szKey, &result, 1, keyLen);
		keyLen = result.length;
		return result.value;
	}
	inline TRIE::result_pair_type prefixMatch(const char* szKey, char sentinel = '\0') const
	{
		TRIE::result_pair_type result = { (HASH)-1, 0 };
		m_trie.commonPrefixSearch(szKey, &result, 1, sentinel);
		return result;
	}
	inline size_t prefixMatch(const char* szKey, size_t keyLen, TRIE::result_pair_type* resultArray, size_t resultArrayLen) const
	{
		return m_trie.commonPrefixSearch(szKey, resultArray, resultArrayLen, keyLen, 0);
	}
	inline size_t prefixMatch(const char* szKey, TRIE::result_pair_type* resultArray, size_t resultArrayLen, char sentinel = '\0') const
	{
		return m_trie.commonPrefixSearch(szKey, resultArray, resultArrayLen, sentinel);
	}
};

struct map_hash
{
	typedef int HASH;
	typedef std::map<std::string, HASH> TRIE;
	TRIE m_trie;
public:
	inline HASH hash(const char* szKey, HASH defaultHash = -1)
	{
		HASH result = getHash(szKey);
		if (result < 0)
		{
			result = (defaultHash == -1 ? (HASH)m_trie.size() : defaultHash);
			m_trie[szKey] = result;
		}
		return result;
	}
	inline HASH getHash(const char* szKey) const
	{
		TRIE::const_iterator pos = m_trie.find(szKey);
		if (pos == m_trie.cend()) return -1;
		return pos->second;
	}
	inline void setHash(const char* keys[], const HASH hash[], int arrayLen)
	{
		for (int i = 0; i < arrayLen; ++i)
			m_trie[keys[i]] = hash[i];
	}
	// clears all keys and values
	inline void clear()
	{
		m_trie.clear();
	}
};

// Light-weight dynamic array with control over bulk reset. Supports only trivial types (that need no special constructor)
template<typename Tobject>
struct dyn_array
{
	Tobject* m_pBase = nullptr;	// the start of array
	int m_nReserved = 0;	// allocated space for no. of objects (all need not be valid or initialized)
	typedef typename std::conditional<std::is_scalar<Tobject>::value, const Tobject, const Tobject&>::type const_TobjRef;
public:
	inline int reserved() const
	{
		return m_nReserved;
	}
	inline bool ensureValid(int nIndex)
	{
		return reserve(nIndex + 1);
	}
	// reserves space for nCount no. of objects
	inline bool reserve(int nCount)
	{
		static_assert(std::is_trivial<Tobject>::value, "Tobject must be trivial");
		assert(nCount > 0);
		if (m_nReserved < nCount)
		{
			Tobject* tmp = (Tobject*)_REALLOC(m_pBase, sizeof(Tobject) * (nCount += 8)); // reserve 8 more than asked
			if (tmp == nullptr) return false;
			m_pBase = tmp;
			m_nReserved = nCount;
		}
		return true;
	}
	inline ~dyn_array()
	{
		cleanup();
	}
	inline void cleanup()
	{
		_FREE(m_pBase);
		m_pBase = nullptr;
		m_nReserved = 0;
	}
	inline const_TobjRef operator[](int nIndex) const
	{
		assert(m_pBase != nullptr && nIndex >= 0 &&  nIndex < m_nReserved);
		return *(m_pBase + nIndex);
	}
	inline Tobject& operator[](int nIndex)
	{
		assert(m_pBase != nullptr && nIndex >= 0 &&  nIndex < m_nReserved);
		return *(m_pBase + nIndex);
	}
	inline void setAllToZero()
	{
		// assert(m_pBase != nullptr && m_nReserved > 0);
		memset(m_pBase, 0, m_nReserved*sizeof(Tobject));
	}
	inline void setAllTo(const_TobjRef obj)
	{
		for (int i = 0; i < m_nReserved; ++i)
			memcpy(m_pBase + i, &obj, sizeof(Tobject));
	}
};

template<typename Tobject, int TSize>
struct static_array
{
	Tobject m_Base[TSize];	// the start of array
	typedef typename std::conditional<std::is_scalar<Tobject>::value, const Tobject, const Tobject&>::type const_TobjRef;
public:
	inline int reserved() const
	{
		return TSize;
	}
	inline bool ensureValid(int nIndex) const
	{
		assert(nIndex >= 0 && nIndex < reserved());
		return true;
	}
	inline const_TobjRef operator[](int nIndex) const
	{
		return m_Base[nIndex];
	}
	inline Tobject& operator[](int nIndex)
	{
		return m_Base[nIndex];
	}
	inline void setAllToZero()
	{
		memset(m_Base, 0, reserved()*sizeof(Tobject));
	}
	inline void setAllTo(const_TobjRef obj)
	{
		for (int i = 0; i < m_nReserved; ++i)
			memcpy(m_Base + i, &obj, sizeof(Tobject));
	}
};

struct instanced_triehash : protected trie_hash
{
	typedef trie_hash::HASH HASH;
};

struct shared_triehash
{
	typedef trie_hash::HASH HASH;
	typedef trie_hash::iterator iterator;
protected:
	static inline trie_hash& getInstance()
	{
		static trie_hash obj;
		return obj;
	}
	static inline HASH hash(const char* szKey, size_t keyLen, HASH defaultHash = -1)
	{
		return getInstance().hash(szKey, keyLen, defaultHash);
	}
	static inline HASH getHash(const char* szKey)
	{
		return getHash(szKey, std::strlen(szKey));
	}
	static inline HASH getHash(const char* szKey, size_t keyLen)
	{
		return getInstance().getHash(szKey, keyLen);
	}
	static inline void setHash(const char* keys[], const HASH hash[], int arrayLen)
	{
		getInstance().setHash(keys, hash, arrayLen);
	}
	static inline void setHash(const char* keys[], size_t keylen[], const HASH hash[], int arrayLen)
	{
		getInstance().setHash(keys, keylen, hash, arrayLen);
	}
	static inline iterator begin()
	{
		return getInstance().begin();
	}
	static inline iterator end()
	{
		return getInstance().end();
	}
private: 
	// declared as private to avoid accidental clearing of shared keys by single instance
	static inline void clear()
	{
		getInstance().clear();
	}
};

// Trie_Array: Datastructure that holds trie result values in an array.
// Uses the trie to give sequential hashes for given strings.
// 	Procedure:	string -> trie -> array_index -> lookup_into_array -> value
// The array_index can be pre-determined to be a sequenced number, thus
// creating a perfect hash that maps strings to 
/* Advantages:
	+ We can use sequential hashes for the strings (by associating string keys with enums). 
	+ Updating the values can be done separately (just update the associated array)
	+ Queries for the value of a particular string key can be fast (use enum as the key directly).
	+ Works like a map, but lookups can be done on both strings and their enums.
*/
// Very useful when the strings are fixed (known before hand) but their mapped values 
// change rapidly.
// @param Tvalue the type of value to be held inside the array
// @param empty the default empty value to be used when creating a new object in the array
template<typename Tvalue = void*, typename Ttrie_hash_impl = instanced_triehash, typename Tarray_impl = dyn_array<Tvalue>>
struct trie_array : protected Ttrie_hash_impl
{
	typedef trie_array _Myt;
	typedef Ttrie_hash_impl Trie_Hash_Impl;
	typedef typename Trie_Hash_Impl::HASH KEY;
	
	Tarray_impl m_array;	// [key/hash/int]->value array

	typedef typename std::conditional<std::is_scalar<Tvalue>::value, const Tvalue, const Tvalue&>::type const_TValRef;
public:
	// Returns the KEY index if exists. Else returns -1
	inline KEY findKey(const char* szKey, size_t keyLen) const
	{
		return Trie_Hash_Impl::getHash(szKey, keyLen);
	}
	// Returns the value for the given key. The key *should* pre-exist
	inline const_TValRef operator[](const char* szKey) const
	{
		static_assert(std::is_trivial<Tvalue>::value, "Tvalue must be trivial");
		return operator[](Trie_Hash_Impl::getHash(szKey));
	}
	// Returns the value for the hash (it *should* pre-exist)
	inline const_TValRef operator[](const KEY keyHash) const
	{
		// trie_array is an array, not a map. 
		// Keys should be pre-loaded with loadKeys() or insert(), before accessing with []
		return m_array[keyHash];
	}
	// Returns the value for the given key. The key *should* pre-exist
	inline const_TValRef at(const char* szKey, size_t keyLen) const
	{
		return operator[](Trie_Hash_Impl::getHash(szKey, keyLen));
	}
	// Returns the value for the given key. If the key does not
	// exist already, the supplied error value will be returned.
	// Key/value will *not* be added to the array.
	inline const_TValRef at(const char* szKey, size_t keyLen, const_TValRef errVal) const
	{
		// for shared-hash-impl there is a chance that the hash
		// produced may not be within the range of the valid array extent.
		KEY hash = Trie_Hash_Impl::getHash(szKey, keyLen);
		if (hash < 0 || hash >= m_array.reserved()) return errVal;
		return operator[](hash);
	}
	// Adds a new item to array at location indexed by 
	// the given string key. Array will be resized to add 
	// room for the string, if needed. If an item already
	// exists for the given string key, it will be updated.
	inline KEY insertkv(const char* szKey, const_TValRef value)
	{
		return insertkv(szKey, strlen(szKey), value);
	}
	inline KEY insertkv(const char* szKey, size_t keyLen, const_TValRef value)
	{
		KEY index = Trie_Hash_Impl::hash(szKey, keyLen);
		m_array.ensureValid(index);	// grow the array if needed
		updateValue(index, value);
		return index;
	}
	// pre-load the keys and values. The Hash values may not be known in this
	// case, so further access to the array will always be throug strings only,
	// such as through operator [](const char*).
	inline void insertkv(const char* szKeys[], Tvalue values[], int arrayLen)
	{
		for (int i = 0; i < arrayLen; ++i)
			insertkv(szKeys[i], values[i]);
	}
	// set value associated with a string, manually choosing a hash key.
	// The value can later be updated directly using the same hash key,
	// using the updatexx() methods.
	inline void insertkhv(const char* szKey, const KEY hash, const_TValRef value)
	{
		// sets both the hash and value in a single method
		Trie_Hash_Impl::setHash(&szKey, &hash, 1);
		m_array.ensureValid(hash);	// grow the array if needed
		updateValue(hash, value);
	}
	// pre-load the keys and values with manually chosen hashes. Later
	// these same hashes can be used to update the values in bulk, using
	// updateValues() or updateValue().
	inline void insertkhv(const char* szKeys[], const KEY hash[], Tvalue values[], int arrayLen)
	{
		loadKeys(szKeys, hash, arrayLen);
		updateValues(hash, values, arrayLen);
	}
	// Updates the values associated with the given hash.
	// An item with the given hash should already have been 
	// present with an insert() or loadKeys() call before.
	inline void updateValue(const KEY hash, const_TValRef value)
	{
		m_array[hash] = value;
	}
	// Once keys has been pre-loaded with loadKeys(), the values can be
	// updated easily just by directly using the hash (instead of the string key).
	inline void updateValues(const KEY hash[], Tvalue values[], int arrayLen)
	{
		for (int i = 0; i < arrayLen; ++i)
			updateValue(hash[i], values[i]);
	}
	// updates the values for sequential hashes in the range [first, last). 
	// The keys for these hashes should have been pre-loaded with loadKeys or insert() earlier.
	inline void updateValues(KEY hashFirst, KEY hashLast, Tvalue values[])
	{
		assert(hashFirst < hashLast && hashFirst >= 0 && hashLast < m_array.reserved());
		memcpy(m_array.m_pBase + hashFirst, values, sizeof(Tvalue)*(hashLast - hashFirst));
	}
	// pre-loads the keys, so that values can be supplied later (with updateValuex()).
	// useful when the keys do not change, but values keep changing.
	inline void loadKeys(const char* szKeys[], const KEY hash[], size_t arrayLen)
	{
		Trie_Hash_Impl::setHash(szKeys, hash, arrayLen);
		setupKeyRange(hash, arrayLen);
	}
	// For shared_hash instances, before accessing or updating the values
	// make sure to call setupKeyRange(). Otherwise, some new keys might
	// have been added by other instances, and you try to lookup that value
	// leading to a crash.
	inline void setupKeyRange(const KEY hashLast)
	{
		m_array.ensureValid(hashLast);
	}
	inline void setupKeyRange(const KEY hash[], size_t arrayLen)
	{
		KEY maxIndex = *std::max_element(hash, hash + arrayLen);
		m_array.ensureValid(maxIndex); // we are not loading the values here, but ensure enough space
	}
	// clear only the values and retain the keys. All values will be set to zero/null.
	inline void clearValues() 
	{
		m_array.setAllToZero();
	}
	// clear only the values and retain the keys. All values will be set to the supplied 'empty' value.
	inline void clearValues(const_TValRef empty)
	{
		m_array.setAllTo(empty);
	}
	// clear the keys and values (without releasing the memory)
	// All keys will be erased and all values will be set to null/zero.
	// CAUTION: use with care when using shared_instance of trie_hash (since this erases shared keys).
	// For shared_trie_array you just want to do clearValues()
	inline void clear() 
	{
		Trie_Hash_Impl::clear();
		clearValues();
	}
	// clear the keys and values (without releasing the memory)
	// All keys will be erased and all values will be replaced with the supplied 'empty' value.
	// CAUTION: use with care when using shared_instance of trie_hash (since this erases shared keys).
	// For shared_trie_array you just want to do clearValues()
	inline void clear(const_TValRef empty)
	{
		Trie_Hash_Impl::clear();
		clearValues(empty);
	}

	struct iterator : public Trie_Hash_Impl::iterator
	{
		typedef typename Trie_Hash_Impl::iterator baseIterator;
		inline const_TValRef value() const { return (*(trie_array*)(this->m_pTrie))[m_hash]; }
		inline iterator(const baseIterator& other): baseIterator(other) {}
		inline const iterator& operator*() const
		{
			return *this;
		}
		inline const iterator* operator->() const
		{
			return this;
		}
	};

	inline iterator begin()
	{
		return Trie_Hash_Impl::begin();
	}
	inline iterator end()
	{
		return Trie_Hash_Impl::end();
	}
};

template<typename Tvalue = void*, typename Tarray_impl = dyn_array<Tvalue>>
struct trie_prefixed_array : public trie_array <Tvalue, trie_prefixed_hash, Tarray_impl>
{
	typedef trie_prefixed_hash::TRIE TRIE;
	// Returns the value for the given key. If the key does not
	// exist already, the supplied error value will be returned.
	// Key/value will *not* be added to the array. The returned
	// match need not be the longest match
	inline const_TValRef prefixMatch(const char* szKey, size_t keyLen, const_TValRef errVal) const
	{
		// for shared-hash-impl there is a chance that the hash
		// produced may not be within the range of the valid array extent.
		KEY hash = Trie_Hash_Impl::prefixMatch(szKey, keyLen);
		if (hash < 0 || hash >= m_array.reserved()) return errVal;
		return operator[](hash);
	}
	inline const_TValRef prefixMatch(const char* szKey, size_t keyLen, TRIE::result_pair_type* resultArray, size_t resultArrayLen, const_TValRef errVal) const
	{
		size_t nResults = Trie_Hash_Impl::prefixMatch(szKey, keyLen, resultArray, resultArrayLen);
		if (nResults <= 0) return errVal;
		return operator[](resultArray[nResults - 1].value);
	}
};

typedef trie_hash TrieHash;

//// Below should work in C++11, but VC2013 has problems.
//template<typename Tvalue> typedef trie_array<Tvalue, instanced_triehash> TrieArray;
//template<typename Tvalue> typedef trie_array<Tvalue, shared_triehash> SharedTrieArray;

#endif // _TRIE_ARRAY_H__Guid__F3C84037_D677_4A13_8D9F_CBA6FE1F5944___

/********
	Design Choices
	---------------
	  + Trie_array is designed to be efficient "insert once and update many times" style of operation
	  + When the keys are pre-known they can be just inserted once and updated many times
	  + Updations is very slim (direct insertion into a native array) without any bound checks
	  + Bound checks are sacrificed (just like a native array) for raw performance
	  + Thus, To achieve safe operations, you have to follow strict rules
		+ 1. Always use insert() or loadKeys() or setupKeyRange() before calling updatexxx()
		+ 2. Always use insert() or loadKeys() or setupKeyRange() before using [] or at()
	  + Shared instances are very useful when you have same keys that take different values
	  + For shared instances, avoid adding new keys on the run. Just load keys once in the start.
	  + If adding new keys on the fly, make sure to setup the key range correctly before accessing/updating.

	  + The insertxxx() methods work like a 'dynamic growth' array, and hence can be slow. Use bulk version for speed.
	  + All updatexxx() methods use very fast hash(key) indexing, just like a fixed native array.
	  + The loadxxx() methods pre-load the keys, enabling the fast update of values.
	  + The setKeyRange() methods ensure valid range for share-trie-array.

	Performance Notes
	-----------------
	  + For the trie_array, use bulk insertions and hash-based queries. They are faster.
	  + For the trie_hash, use getHash() and setHash(), which are faster. Avoid hash().
	  + trie_hash::hash() can be avoided by specifying manual Hashes (with loadKeys(), insert()).
	  + Cache and supply the string lengths manually wherever possible. They are faster.
	  + Use bulk insertions and bulk updates(). They are faster. Single insert() is very costly.
	
	Example Code
	----------------
	{
		struct Foo	{ int m_n;	double m_x;	};

		trie_array<Foo, shared_triehash> taF1;
		trie_array<Foo, shared_triehash> taF2;
		trie_array<Foo, shared_triehash> taF3;

		const char* keys[] = { "one", "two" };
		int hash[] = { 0, 1 };
		Foo values[] = { { 1, -1.1123 }, { 2, -2.12312 } };
		taF1.insertkhv(keys, hash, values, 2);

		taF2.setupKeyRange(hash, 2);
		taF3.setupKeyRange(hash, 2);

		taF2.updateValues(hash, values, 2);
		taF3.updateValues(hash, values, 2);

		assert(taF2["two"].m_n == taF3[1].m_n);
		assert(taF2[1].m_n == 2);
		assert(taF3["one"].m_n == taF1[0].m_n);
		assert(taF3[1].m_n == 2);
	}
*******/