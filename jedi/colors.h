#pragma once

#include "settings.h"

enum e_color
  {
  default_color = 1,
  command_color,
  column_command_color,
  topline_command_color,
  linenumbers_color,
  multiline_tag_editor,
  multiline_tag_command,
  scroll_bar_b_editor,
  scroll_bar_f_editor,
  title_bar,
  comment_color,
  string_color,
  keyword_color,
  keyword_2_color,
  command_plus,
  command_plus_modified,
  column_command_plus,
  topline_command_plus
  };
  
enum jed_colors
  {
  jed_editor_text = 16,
  jed_editor_bg = 17,
  jed_editor_tag = 18,
  jed_command_text = 19,
  jed_command_bg = 20,
  jed_command_tag = 21,
  jed_title_text = 22,
  jed_title_bg = 23,

  jed_editor_text_bold = 24,
  jed_editor_bg_bold = 25,
  jed_editor_tag_bold = 26,
  jed_command_text_bold = 27,
  jed_command_bg_bold = 28,
  jed_command_tag_bold = 29,
  jed_title_text_bold = 30,
  jed_title_bg_bold = 31,

  jed_comment = 32,
  jed_string = 33,
  jed_keyword = 34,
  jed_keyword_2 = 35,
  jed_line_colors = 36
  };

void init_colors(const settings& s);
