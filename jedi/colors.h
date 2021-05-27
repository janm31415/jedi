#pragma once

#include "settings.h"

enum e_color
  {
  default_color = 1,
  command_color,
  linenumbers_color,
  multiline_tag_editor,
  multiline_tag_command,
  scroll_bar_b_editor,
  scroll_bar_f_editor,
  title_bar,
  comment_color,
  string_color,
  keyword_color,
  keyword_2_color
  };

void init_colors(const settings& s);