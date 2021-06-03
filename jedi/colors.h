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
  
enum jedi_colors
  {
  jedi_editor_text = 16,
  jedi_editor_bg = 17,
  jedi_editor_tag = 18,
  jedi_command_text = 19,
  jedi_command_bg = 20,
  jedi_command_tag = 21,
  jedi_title_text = 22,
  jedi_title_bg = 23,

  jedi_editor_text_bold = 24,
  jedi_editor_bg_bold = 25,
  jedi_editor_tag_bold = 26,
  jedi_command_text_bold = 27,
  jedi_command_bg_bold = 28,
  jedi_command_tag_bold = 29,
  jedi_title_text_bold = 30,
  jedi_title_bg_bold = 31,

  jedi_comment = 32,
  jedi_string = 33,
  jedi_keyword = 34,
  jedi_keyword_2 = 35,
  jedi_line_colors = 36
  };

void init_colors(const settings& s);
