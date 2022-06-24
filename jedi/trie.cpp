#include "trie.h"
#include <cassert>
#include <queue>

trie::trie() : _root_id(0)
  {
  clear();
  }

trie::~trie()
  {
  }

void trie::clear()
  {
  _nodes.clear();
  _nodes.emplace_back();
  _nodes.back().occurrence = 0;
  }

bool trie::empty() const
  {
  return _nodes.size() == 1;
  }

void trie::insert(const std::wstring& word, uint32_t occurrence)
  {
  assert(occurrence>0);

  if (word.empty())
    return;

  uint32_t current_index = _root_id;
  for (const auto ch : word)
    {
    if (!_has_child(current_index, ch))
      {
      _make_child(current_index, ch);
      }
    current_index = _nodes[current_index].children.get(ch);
    }
  _nodes[current_index].occurrence += occurrence;
  }

bool trie::_has_child(uint32_t node_index, wchar_t ch) const
  {
  return _nodes[node_index].children.has(ch);
  }

void trie::_make_child(uint32_t node_index, wchar_t ch)
  {
  uint32_t new_index = (uint32_t)_nodes.size();
  _nodes.emplace_back();
  _nodes.back().occurrence = 0;
  _nodes[node_index].children.put(ch) = new_index;
  }

bool trie::_is_leaf(uint32_t node_index) const
  {
  return _nodes[node_index].occurrence > 0;
  }

bool trie::find(const std::wstring& word) const
  {
  uint32_t current_index = _root_id;
  for (const auto ch : word)
    {
    if (!_has_child(current_index, ch))
      {
      return false;
      }
    current_index = _nodes[current_index].children.get(ch);
    }
  return _is_leaf(current_index);
  }

std::vector<std::wstring> trie::predict(const std::wstring& prefix, uint32_t number_of_completions)
  {
  std::vector<std::wstring> predictions;
  if (prefix.empty() || number_of_completions==0)
    return predictions;
  uint32_t current_index = _root_id;  
  for (const auto ch : prefix)
    {
    if (!_has_child(current_index, ch))
      {
      return predictions;
      }
    current_index = _nodes[current_index].children.get(ch);
    }

  std::vector<std::pair<std::wstring, uint32_t>> candidates;

  std::queue<std::pair<uint32_t, std::wstring>> qu;
  qu.push(std::pair<uint32_t, std::wstring>(current_index, prefix));

  while (!qu.empty())
    {
    std::pair<uint32_t, std::wstring> current_word = qu.front();
    qu.pop();
    if (_is_leaf(current_word.first))
      candidates.emplace_back(current_word.second, _nodes[current_word.first].occurrence);

    auto it = _nodes[current_word.first].children.begin();
    auto it_end = _nodes[current_word.first].children.end();
    for (; it != it_end; ++it)
      {
      std::wstring new_word = current_word.second;
      new_word.push_back(it.key());
      qu.push(std::pair<uint32_t, std::wstring>(*it, new_word));
      }
    }

  std::sort(candidates.begin(), candidates.end(), [](const auto& left, const auto& right)
    {
    return left.second > right.second;
    });

  const uint32_t number_of_results = std::min((uint32_t)candidates.size(), number_of_completions);
  predictions.reserve(number_of_results);
  for (uint32_t i = 0; i < number_of_results; ++i)
    predictions.push_back(candidates[i].first);  

  return predictions;
  }