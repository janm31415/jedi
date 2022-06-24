#pragma once

#include <vector>
#include <string>

#include "jtk/containers.h"

struct trie_node
  {
  jtk::flat_map<wchar_t, uint32_t> children;
  uint32_t occurrence;
  };


class trie
  {
  public:
    trie();
    ~trie();

    void clear();

    bool empty() const;

    void insert(const std::wstring& word, uint32_t occurrence = 1); // you can insert the same word multiple times, its occurence will increase

    bool find(const std::wstring& word) const;

    std::vector<std::wstring> predict(const std::wstring& prefix, uint32_t number_of_completions);

  private:

    bool _has_child(uint32_t node_index, wchar_t ch) const;

    void _make_child(uint32_t node_index, wchar_t ch);

    bool _is_leaf(uint32_t node_index) const;

  private:
    std::vector<trie_node> _nodes;
    uint32_t _root_id;
  };