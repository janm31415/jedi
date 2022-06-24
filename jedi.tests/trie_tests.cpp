#include "trie_tests.h"
#include "../jedi/trie.h"
#include "test_assert.h"

void trie_test()
  {
  trie t;
  t.insert("Hello");

  TEST_ASSERT(t.find("Hello"));
  TEST_ASSERT(!t.find("hello"));

  t.insert("World");
  
  TEST_ASSERT(t.find("Hello"));
  TEST_ASSERT(t.find("World"));
  TEST_ASSERT(!t.find("hello"));
  TEST_ASSERT(!t.find("wor"));

  TEST_ASSERT(!t.find("Wo")); // Wo is in the trie, but it is not a valid word

  t.insert("Helm");
  t.insert("Helm");
  t.insert("Helm");
  t.insert("Hero");
  t.insert("Hero");

  std::vector<std::string> completions = t.predict("He", 100);

  TEST_EQ(3, (int)completions.size());
  TEST_ASSERT(completions[0] == std::string("Helm"));
  TEST_ASSERT(completions[1] == std::string("Hero"));
  TEST_ASSERT(completions[2] == std::string("Hello"));

  t.insert("Hello");
  t.insert("Hello");
  t.insert("Hello");
  t.insert("Hello");

  completions = t.predict("He", 100);

  TEST_EQ(3, (int)completions.size());
  TEST_ASSERT(completions[0] == std::string("Hello"));
  TEST_ASSERT(completions[1] == std::string("Helm"));
  TEST_ASSERT(completions[2] == std::string("Hero"));

  t.insert("He");

  completions = t.predict("He", 100);

  TEST_EQ(4, (int)completions.size());
  TEST_ASSERT(completions[0] == std::string("Hello"));
  TEST_ASSERT(completions[1] == std::string("Helm"));
  TEST_ASSERT(completions[2] == std::string("Hero"));
  TEST_ASSERT(completions[3] == std::string("He"));

  completions = t.predict("He", 1);

  TEST_EQ(1, (int)completions.size());
  TEST_ASSERT(completions[0] == std::string("Hello"));

  completions = t.predict("He", 0);

  TEST_EQ(0, (int)completions.size());

  completions = t.predict("World!", 100);

  TEST_EQ(0, (int)completions.size());
  }

void run_all_trie_tests()
  {
  trie_test();
  }