/*
	Copyright (c) 2016 Cenacle Research India Private Limited
*/

#define CATCH_CONFIG_MAIN  // This tells Catch to provide a main()
#include "catch.hpp"
#include "trie_array.h"
#include <string>
#include <vector>

TEST_CASE("Trie Array", "[trie_array]") 
{
	const char* strings[] =
	{
		"abc",
		"Hello World",
		"/",
		"/route1",
		"/route2",
		"/route3",
		"/route3/",
		"GET",
		"HTTP",
		"http",
		"https"
	};

	SECTION("trie_hash")
	{
		trie_hash th;

		SECTION("Empty Query")
		{
			trie_hash::HASH h = th.getHash("http");
			REQUIRE(h == -1);
		}
		SECTION("Bulk Insertion")
		{
			th.hash(strings, sizeof(strings) / sizeof(strings[0]));
			REQUIRE(th.getHash("http") != -1);
			REQUIRE(th.getHash("https") != -1);
			REQUIRE(th.getHash("http") != th.getHash("HTTP"));
			REQUIRE(th.getHash("http") != th.getHash("https"));
			REQUIRE(th.getHash("dsafdfdasfdsf") == -1);

			std::string s[sizeof(strings) / sizeof(strings[0])];
			trie_prefixed_hash::HASH h[sizeof(strings) / sizeof(strings[0])];
			int nCount = 0;
			trie_prefixed_hash::iterator iter = th.begin();
			trie_prefixed_hash::iterator iterEnd = th.end();
			while (iter != iterEnd)
			{
				char buf[4096];
				h[nCount] = iter->hash();
				s[nCount] = iter->key(buf, sizeof(buf));
				++nCount;
				++iter;
			}

			REQUIRE(nCount == sizeof(strings) / sizeof(strings[0]));
		}
	}
	SECTION("trie_prefixed_hash")
	{
		trie_prefixed_hash tph;

		SECTION("Empty Query")
		{
			trie_prefixed_hash::HASH h = tph.getHash("http");
			REQUIRE(h == -1);
		}
		SECTION("Bulk Insertion")
		{
			tph.hash(strings, sizeof(strings) / sizeof(strings[0]));
			REQUIRE(tph.getHash("http") != -1);
			REQUIRE(tph.getHash("https") != -1);
			REQUIRE(tph.getHash("http") != tph.getHash("HTTP"));
			REQUIRE(tph.getHash("http") != tph.getHash("https"));
			REQUIRE(tph.getHash("/") != tph.getHash("/route3"));
			REQUIRE(tph.getHash("/route3/") != tph.getHash("/route3"));
			REQUIRE(tph.getHash("dsafdfdasfdsf") == -1);

			trie_prefixed_hash::TRIE::result_pair_type r1 = tph.prefixMatch("http");
			trie_prefixed_hash::TRIE::result_pair_type r2 = tph.prefixMatch("https");
			REQUIRE( r1.value == r2.value);

			trie_prefixed_hash::TRIE::result_pair_type r3[10];
			trie_prefixed_hash::TRIE::result_pair_type r4[10];
			trie_prefixed_hash::TRIE::result_pair_type r5[10];
			size_t num3 = tph.prefixMatch("/", r3, sizeof(r3)/sizeof(r3[0]));
			size_t num4 = tph.prefixMatch("/route3", r4, sizeof(r4) / sizeof(r4[0]));
			size_t num5 = tph.prefixMatch("/route3/", r5, sizeof(r5) / sizeof(r5[0]));

			std::string s[sizeof(strings) / sizeof(strings[0])];
			trie_prefixed_hash::HASH h[sizeof(strings) / sizeof(strings[0])];
			int nCount = 0;
			trie_prefixed_hash::iterator iter = tph.begin();
			trie_prefixed_hash::iterator iterEnd = tph.end();
			while (iter != iterEnd)
			{
				char buf[4096];
				h[nCount] = iter->hash();
				s[nCount] = iter->key(buf, sizeof(buf));
				++nCount;
				++iter;
			}

			REQUIRE(nCount == sizeof(strings) / sizeof(strings[0]));

			for (int i = 0; i < nCount; ++i)
			{
				REQUIRE(s[i] == strings[i]);
				REQUIRE(i == h[i]);	// we expect the strings to have been given sequential ids
			}
		}
	}
};