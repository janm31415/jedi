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
  multiline_tag_column_command,
  multiline_tag_topline_command,
  scroll_bar_b_editor,
  scroll_bar_f_editor,
  comment_color,
  string_color,
  keyword_color,
  keyword_2_color,
  command_plus,
  column_command_plus,
  topline_command_plus,
  command_icon,
  command_icon_modified,
  column_command_icon,
  column_command_icon_modified,
  topline_command_icon,
  topline_command_icon_modified,
  editor_icon,
  editor_icon_modified
  };
  
enum jedi_colors
  {
  jedi_editor_text = 16,
  jedi_editor_bg = 17,
  jedi_editor_tag = 18,
  jedi_command_text = 19,
  jedi_command_bg = 20,
  jedi_command_tag = 21,
  jedi_column_command_text = 22,
  jedi_column_command_bg = 23,
  jedi_column_command_tag = 24,
  jedi_topline_command_text = 25,
  jedi_topline_command_bg = 26,
  jedi_topline_command_tag = 27,
  jedi_line_numbers = 28,
  jedi_scrollbar = 29,
  jedi_scrollbar_background = 30,
  jedi_icon = 31,
  jedi_icon_modified = 32,
  jedi_plus = 33,
  jedi_comment = 34,
  jedi_string = 35,
  jedi_keyword = 36,
  jedi_keyword_2 = 37
  };

void init_colors(const settings& s);
