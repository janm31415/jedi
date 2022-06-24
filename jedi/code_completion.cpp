#include "code_completion.h"
#include "engine.h"
#include "buffer.h"
#include "jtk/file_utils.h"
#include "draw.h" // for getting the syntax highlighter

bool valid_word_for_tree(const std::wstring& w)
  {
  return w.size() > 2;
  }

void add_buffer_to_trie(trie& t, const file_buffer& b)
  {
  for (uint32_t row = 0; row < b.content.size(); ++row)
    {
    const auto& line = b.content[row];
    std::wstring last_word;
    for (uint32_t col = 0; col < line.size(); ++col)
      {
      wchar_t ch = line[col];
      switch (ch)
        {
        case L' ':
        case L'\n':
        case L'\r':
        case L'.':
        case L'!':
        case L'?':
        case L',':
        case L'<':
        case L'>':
        case L'(':
        case L')':
        case L'[':
        case L']':
          if (valid_word_for_tree(last_word))
            t.insert(last_word);
          last_word.clear();
          break;
        default:
          last_word.push_back(ch);
          break;
        }
      }
    if (valid_word_for_tree(last_word))
      t.insert(last_word);
    }
  }

void add_syntax_to_trie(trie& t, const std::string& name)
  {
  auto ext = jtk::get_extension(name);
  auto filename = jtk::get_filename(name);
  std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return (unsigned char)std::tolower(c); });
  std::transform(filename.begin(), filename.end(), filename.begin(), [](unsigned char c) { return (unsigned char)std::tolower(c); });
  const syntax_highlighter& shl = get_syntax_highlighter();
  if (shl.extension_or_filename_has_keywords(ext))
    {
    const keyword_data& kd = shl.get_keywords(ext);
    for (const auto& w : kd.keywords_1)
      t.insert(w);
    for (const auto& w : kd.keywords_2)
      t.insert(w);
    }
  if (shl.extension_or_filename_has_keywords(filename))
    {
    const keyword_data& kd = shl.get_keywords(filename);
    for (const auto& w : kd.keywords_1)
      t.insert(w);
    for (const auto& w : kd.keywords_2)
      t.insert(w);
    }
  }

std::wstring code_completion(const std::wstring& prefix, buffer_data& d)
  {
  if (prefix.empty())
    return std::wstring();
  if (!d.code_completion.last_suggestions.empty())
    {
    if (prefix == d.code_completion.last_suggestions[d.code_completion.last_suggestion_index])
      {
      ++d.code_completion.last_suggestion_index;
      if (d.code_completion.last_suggestion_index >= d.code_completion.last_suggestions.size())
        d.code_completion.last_suggestion_index = 0;
      return d.code_completion.last_suggestions[d.code_completion.last_suggestion_index];
      }
    }
  add_syntax_to_trie(d.code_completion.t, d.buffer.name);
  add_buffer_to_trie(d.code_completion.t, d.buffer);
  d.code_completion.last_prefix = prefix;
  d.code_completion.last_suggestions = d.code_completion.t.predict(prefix, 10);
  d.code_completion.last_suggestion_index = 0;
  if (d.code_completion.last_suggestions.empty())
    return std::wstring();
  return d.code_completion.last_suggestions.front();
  }