#include "trie_tests.h"
#include "../jedi/trie.h"
#include "test_assert.h"

void trie_test()
  {
  trie t;
  t.insert(L"Hello");

  TEST_ASSERT(t.find(L"Hello"));
  TEST_ASSERT(!t.find(L"hello"));

  t.insert(L"World");
  
  TEST_ASSERT(t.find(L"Hello"));
  TEST_ASSERT(t.find(L"World"));
  TEST_ASSERT(!t.find(L"hello"));
  TEST_ASSERT(!t.find(L"wor"));

  TEST_ASSERT(!t.find(L"Wo")); // Wo is in the trie, but it is not a valid word

  t.insert(L"Helm");
  t.insert(L"Helm");
  t.insert(L"Helm");
  t.insert(L"Hero");
  t.insert(L"Hero");

  std::vector<std::wstring> completions = t.predict(L"He", 100);

  TEST_EQ(3, (int)completions.size());
  TEST_ASSERT(completions[0] == std::wstring(L"Helm"));
  TEST_ASSERT(completions[1] == std::wstring(L"Hero"));
  TEST_ASSERT(completions[2] == std::wstring(L"Hello"));

  t.insert(L"Hello");
  t.insert(L"Hello");
  t.insert(L"Hello");
  t.insert(L"Hello");

  completions = t.predict(L"He", 100);

  TEST_EQ(3, (int)completions.size());
  TEST_ASSERT(completions[0] == std::wstring(L"Hello"));
  TEST_ASSERT(completions[1] == std::wstring(L"Helm"));
  TEST_ASSERT(completions[2] == std::wstring(L"Hero"));

  t.insert(L"He");

  completions = t.predict(L"He", 100);

  TEST_EQ(4, (int)completions.size());
  TEST_ASSERT(completions[0] == std::wstring(L"Hello"));
  TEST_ASSERT(completions[1] == std::wstring(L"Helm"));
  TEST_ASSERT(completions[2] == std::wstring(L"Hero"));
  TEST_ASSERT(completions[3] == std::wstring(L"He"));

  completions = t.predict(L"He", 1);

  TEST_EQ(1, (int)completions.size());
  TEST_ASSERT(completions[0] == std::wstring(L"Hello"));

  completions = t.predict(L"He", 0);

  TEST_EQ(0, (int)completions.size());

  completions = t.predict(L"World!", 100);

  TEST_EQ(0, (int)completions.size());
  }

void run_all_trie_tests()
  {
  trie_test();
  }