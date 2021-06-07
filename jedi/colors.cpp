#include "colors.h"

#include <curses.h>

namespace
  {
  short conv_rgb(int clr)
    {
    float frac = (float)clr / 255.f;
    return (short)(1000.f*frac);
    }

  struct rgb
    {
    rgb(uint32_t v) : r(v & 255), g((v >> 8) & 255), b((v >> 16)&255)
      {
      }

    rgb(int red, int green, int blue) : r(red), g(green), b(blue) {}
    int r, g, b;
    };

  void init_color(short id, rgb value)
    {
    ::init_color(id, conv_rgb(value.r), conv_rgb(value.g), conv_rgb(value.b));
    }

  }


void init_colors(const settings& s)
  {
  init_color(jedi_editor_text, rgb(s.color_editor_text));
  init_color(jedi_editor_bg, rgb(s.color_editor_background));
  init_color(jedi_editor_tag, rgb(s.color_editor_tag));

  init_color(jedi_command_text, rgb(s.color_command_text));
  init_color(jedi_command_bg, rgb(s.color_command_background));
  init_color(jedi_command_tag, rgb(s.color_command_tag));

  init_color(jedi_column_command_text, rgb(s.color_column_command_text));
  init_color(jedi_column_command_bg, rgb(s.color_column_command_background));
  init_color(jedi_column_command_tag, rgb(s.color_column_command_tag));
  
  init_color(jedi_topline_command_text, rgb(s.color_topline_command_text));
  init_color(jedi_topline_command_bg, rgb(s.color_topline_command_background));
  init_color(jedi_topline_command_tag, rgb(s.color_topline_command_tag));

  init_color(jedi_line_numbers, rgb(s.color_line_numbers));
  init_color(jedi_scrollbar, rgb(s.color_scrollbar));
  init_color(jedi_scrollbar_background, rgb(s.color_scrollbar_background));
  init_color(jedi_icon, rgb(s.color_icon));
  init_color(jedi_icon_modified, rgb(s.color_icon_modified));
  init_color(jedi_plus, rgb(s.color_plus));

  init_color(jedi_comment, rgb(s.color_comment));
  init_color(jedi_string, rgb(s.color_string));
  init_color(jedi_keyword, rgb(s.color_keyword));
  init_color(jedi_keyword_2, rgb(s.color_keyword_2));
  

  init_pair(default_color, jedi_editor_text, jedi_editor_bg);
  init_pair(command_color, jedi_command_text, jedi_command_bg);
  init_pair(column_command_color, jedi_column_command_text, jedi_column_command_bg);
  init_pair(topline_command_color, jedi_topline_command_text, jedi_topline_command_bg);
  init_pair(multiline_tag_editor, jedi_editor_tag, jedi_editor_bg);
  init_pair(multiline_tag_command, jedi_command_tag, jedi_command_bg);
  init_pair(multiline_tag_column_command, jedi_column_command_tag, jedi_column_command_bg);
  init_pair(multiline_tag_topline_command, jedi_topline_command_tag, jedi_topline_command_bg);

  init_pair(scroll_bar_b_editor, jedi_scrollbar_background, jedi_editor_bg);
  init_pair(scroll_bar_f_editor, jedi_scrollbar, jedi_editor_bg);
  
  init_pair(command_plus, jedi_plus, jedi_command_bg);
  init_pair(column_command_plus, jedi_plus, jedi_column_command_bg);
  init_pair(topline_command_plus, jedi_plus, jedi_topline_command_bg);
  
  init_pair(command_icon, jedi_icon, jedi_command_bg);  
  init_pair(command_icon_modified, jedi_icon_modified, jedi_command_bg);

  init_pair(linenumbers_color, jedi_line_numbers, jedi_editor_bg);


  init_pair(comment_color, jedi_comment, jedi_editor_bg);
  init_pair(string_color, jedi_string, jedi_editor_bg);
  init_pair(keyword_color, jedi_keyword, jedi_editor_bg);
  init_pair(keyword_2_color, jedi_keyword_2, jedi_editor_bg);
  }
