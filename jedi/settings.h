#pragma once

#include <string>

struct settings
  {
  settings();

  bool use_spaces_for_tab;
  int tab_space;
  bool show_all_characters;
  bool show_line_numbers;
  bool case_sensitive;
  bool wrap;
  bool syntax;
  bool mario;
  int w, h, x, y;
  int font_size;
  std::string font;
  int mouse_scroll_steps;
  std::string last_find, last_replace;

  uint32_t color_editor_text;
  uint32_t color_editor_background;
  uint32_t color_editor_tag;
  uint32_t color_line_numbers;
  uint32_t color_scrollbar;
  uint32_t color_scrollbar_background;
  uint32_t color_icon;
  uint32_t color_icon_modified;
  uint32_t color_plus;

  uint32_t color_command_text;
  uint32_t color_command_background;
  uint32_t color_command_tag;

  uint32_t color_column_command_text;
  uint32_t color_column_command_background;
  uint32_t color_column_command_tag;
  
  uint32_t color_topline_command_text;
  uint32_t color_topline_command_background;
  uint32_t color_topline_command_tag;
  
  uint32_t color_comment;
  uint32_t color_string;
  uint32_t color_keyword;
  uint32_t color_keyword_2;
  };

void update_settings_if_different(settings& s, const settings& new_settings, const settings& old_settings);
void update_settings(settings& s, const char* filename);
settings read_settings(const char* filename);
void write_settings(const settings& s, const char* filename);
