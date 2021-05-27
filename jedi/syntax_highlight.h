#pragma once

#include <string>
#include <map>
#include <vector>

struct comment_data
  {
  comment_data() : uses_quotes_for_chars(false) {}
  std::string multiline_begin, multiline_end;
  std::string multistring_begin, multistring_end;
  std::string single_line;
  bool uses_quotes_for_chars;
  };

struct keyword_data
  {
  std::vector<std::wstring> keywords_1, keywords_2;
  };

class syntax_highlighter
  {
  public:
    syntax_highlighter();
    ~syntax_highlighter();

    bool extension_or_filename_has_syntax_highlighter(const std::string& ext_or_filename) const;
    bool extension_or_filename_has_keywords(const std::string& ext_or_filename) const;

    comment_data get_syntax_highlighter(const std::string& ext_or_filename) const;

    const keyword_data& get_keywords(const std::string& ext_or_filename) const;

  private:
    std::map<std::string, comment_data> extension_to_data;
    std::map<std::string, keyword_data> extension_to_keywords;
  };