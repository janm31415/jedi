#pragma once

#include <string>
#include "trie.h"

struct buffer_data;
struct file_buffer;

struct code_completion_data
  {
  trie t;
  std::wstring last_prefix;
  uint32_t last_suggestion_index;
  std::vector<std::wstring> last_suggestions;
  };

void add_buffer_to_trie(trie& t, const file_buffer& b);
void add_syntax_to_trie(trie& t, const std::string& name);

std::wstring code_completion(const std::wstring& prefix, buffer_data& d);