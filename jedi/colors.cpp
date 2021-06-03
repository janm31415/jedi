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

/*
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
*/
  }


void init_colors(const settings& s)
  {

  init_color(jedi_title_text, rgb(s.color_titlebar_text));
  init_color(jedi_title_bg, rgb(s.color_titlebar_background));
  init_color(jedi_title_text_bold, rgb(s.color_titlebar_text));
  init_color(jedi_title_bg_bold, rgb(s.color_titlebar_background));

  init_color(jedi_editor_text, rgb(s.color_editor_text));
  init_color(jedi_editor_bg, rgb(s.color_editor_background));
  init_color(jedi_editor_tag, rgb(s.color_editor_tag));

  init_color(jedi_command_text, rgb(s.color_command_text));
  init_color(jedi_command_bg, rgb(s.color_command_background));
  init_color(jedi_command_tag, rgb(s.color_command_tag));

  init_color(jedi_editor_text_bold, rgb(s.color_editor_text_bold));
  init_color(jedi_editor_bg_bold, rgb(s.color_editor_background_bold));
  init_color(jedi_editor_tag_bold, rgb(s.color_editor_tag_bold));

  init_color(jedi_line_colors, rgb(s.color_line_numbers));

  init_color(jedi_command_text_bold, rgb(s.color_command_text));
  init_color(jedi_command_bg_bold, rgb(s.color_command_background));
  init_color(jedi_command_tag_bold, rgb(s.color_command_tag));

  init_pair(default_color, jedi_editor_text, jedi_editor_bg);
  init_pair(command_color, jedi_command_text, jedi_command_bg);
  init_pair(column_command_color, jedi_command_text, jedi_editor_bg);
  init_pair(topline_command_color, jedi_command_text, jedi_command_bg);
  init_pair(multiline_tag_editor, jedi_editor_tag, jedi_editor_bg);
  init_pair(multiline_tag_command, jedi_command_tag, jedi_command_bg);

  init_pair(scroll_bar_b_editor, jedi_command_bg, jedi_editor_bg);
  init_pair(scroll_bar_f_editor, jedi_editor_tag, jedi_editor_bg);
  init_pair(command_plus, jedi_comment, jedi_command_bg);
  init_pair(command_plus_modified, jedi_string, jedi_command_bg);  
  init_pair(column_command_plus, jedi_comment, jedi_editor_bg);
  init_pair(topline_command_plus, jedi_comment, jedi_command_bg);

  init_pair(title_bar, jedi_title_text, jedi_title_bg);

  init_color(jedi_comment, rgb(s.color_comment));
  init_color(jedi_string, rgb(s.color_string));
  init_color(jedi_keyword, rgb(s.color_keyword));
  init_color(jedi_keyword_2, rgb(s.color_keyword_2));

  init_pair(comment_color, jedi_comment, jedi_editor_bg);
  init_pair(string_color, jedi_string, jedi_editor_bg);
  init_pair(keyword_color, jedi_keyword, jedi_editor_bg);
  init_pair(keyword_2_color, jedi_keyword_2, jedi_editor_bg);

  init_pair(linenumbers_color, jedi_line_colors, jedi_editor_bg);
  }
