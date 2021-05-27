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

  }


void init_colors(const settings& s)
  {

  init_color(jed_title_text, rgb(s.color_titlebar_text));
  init_color(jed_title_bg, rgb(s.color_titlebar_background));
  init_color(jed_title_text_bold, rgb(s.color_titlebar_text));
  init_color(jed_title_bg_bold, rgb(s.color_titlebar_background));

  init_color(jed_editor_text, rgb(s.color_editor_text));
  init_color(jed_editor_bg, rgb(s.color_editor_background));
  init_color(jed_editor_tag, rgb(s.color_editor_tag));

  init_color(jed_command_text, rgb(s.color_command_text));
  init_color(jed_command_bg, rgb(s.color_command_background));
  init_color(jed_command_tag, rgb(s.color_command_tag));

  init_color(jed_editor_text_bold, rgb(s.color_editor_text_bold));
  init_color(jed_editor_bg_bold, rgb(s.color_editor_background_bold));
  init_color(jed_editor_tag_bold, rgb(s.color_editor_tag_bold));

  init_color(jed_line_colors, rgb(s.color_line_numbers));

  init_color(jed_command_text_bold, rgb(s.color_command_text));
  init_color(jed_command_bg_bold, rgb(s.color_command_background));
  init_color(jed_command_tag_bold, rgb(s.color_command_tag));

  init_pair(default_color, jed_editor_text, jed_editor_bg);
  init_pair(command_color, jed_command_text, jed_command_bg);
  init_pair(multiline_tag_editor, jed_editor_tag, jed_editor_bg);
  init_pair(multiline_tag_command, jed_command_tag, jed_command_bg);

  init_pair(scroll_bar_b_editor, jed_command_bg, jed_editor_bg);
  init_pair(scroll_bar_f_editor, jed_editor_tag, jed_editor_bg);

  init_pair(title_bar, jed_title_text, jed_title_bg);

  init_color(jed_comment, rgb(s.color_comment));
  init_color(jed_string, rgb(s.color_string));
  init_color(jed_keyword, rgb(s.color_keyword));
  init_color(jed_keyword_2, rgb(s.color_keyword_2));

  init_pair(comment_color, jed_comment, jed_editor_bg);
  init_pair(string_color, jed_string, jed_editor_bg);
  init_pair(keyword_color, jed_keyword, jed_editor_bg);
  init_pair(keyword_2_color, jed_keyword_2, jed_editor_bg);

  init_pair(linenumbers_color, jed_line_colors, jed_editor_bg);
  }