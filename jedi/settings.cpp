#include "settings.h"

#include "pref_file.h"

#include <jtk/file_utils.h>

settings::settings()
  {
  show_all_characters = false;
  tab_space = 2;
  use_spaces_for_tab = true;
  show_line_numbers = true;
  wrap = false;
  syntax = true;
  case_sensitive = false;
  mario = true;
  w = 80;
  h = 25;
  x = 100;
  y = 100;
  font_size = 17;
  font = jtk::get_folder(jtk::get_executable_path()) + "fonts/FiraCode-Regular.ttf";
  mouse_scroll_steps = 3;

  color_editor_text = 0xfff2f8f8;
  color_editor_background = 0xff362a28;
  color_editor_tag = 0xfffde98b;

  color_command_text = 0xfff2f8f8;
  color_command_background = 0xff5a4744;
  color_command_tag = 0xfffde98b;

  color_column_command_text = 0xfff2f8f8;
  color_column_command_background = 0xff6a5754;
  color_column_command_tag = 0xfffde98b;

  color_topline_command_text = 0xfff2f8f8;
  color_topline_command_background = 0xff7a6764;
  color_topline_command_tag = 0xfffde98b;

  color_line_numbers = 0xff5a4744;
  color_scrollbar = 0xffc679ff;
  color_scrollbar_background = 0xffa47262;
  color_icon = 0xfffde98b;
  color_icon_modified = 0xff5555ff;
  color_plus = 0xfffde98b;

  color_comment = 0xffa47262;
  color_string = 0xff8cfaf1;
  color_keyword = 0xffc679ff;
  color_keyword_2 = 0xfff993bd;
  }

void update_settings_if_different(settings& s, const settings& new_settings, const settings& old_settings)
  {
  if (new_settings.use_spaces_for_tab != old_settings.use_spaces_for_tab)
    s.use_spaces_for_tab = new_settings.use_spaces_for_tab;

  if (new_settings.tab_space != old_settings.tab_space)
    s.tab_space = new_settings.tab_space;

  if (new_settings.show_all_characters != old_settings.show_all_characters)
    s.show_all_characters = new_settings.show_all_characters;

  if (new_settings.mario != old_settings.mario)
    s.mario = new_settings.mario;
    
  if (new_settings.case_sensitive != old_settings.case_sensitive)
    s.case_sensitive = new_settings.case_sensitive;

  if (new_settings.w != old_settings.w)
    s.w = new_settings.w;

  if (new_settings.h != old_settings.h)
    s.h = new_settings.h;

  if (new_settings.show_line_numbers != old_settings.show_line_numbers)
    s.show_line_numbers = new_settings.show_line_numbers;

  if (new_settings.wrap != old_settings.wrap)
    s.wrap = new_settings.wrap;

  if (new_settings.syntax != old_settings.syntax)
    s.syntax = new_settings.syntax;

  if (new_settings.x != old_settings.x)
    s.x = new_settings.x;

  if (new_settings.y != old_settings.y)
    s.y = new_settings.y;

  if (new_settings.font_size != old_settings.font_size)
    s.font_size = new_settings.font_size;
    
  if (new_settings.font != old_settings.font)
    s.font = new_settings.font;

  if (new_settings.mouse_scroll_steps != old_settings.mouse_scroll_steps)
    s.mouse_scroll_steps = new_settings.mouse_scroll_steps;

  if (new_settings.last_find != old_settings.last_find)
    s.last_find = new_settings.last_find;

  if (new_settings.last_replace != old_settings.last_replace)
    s.last_replace = new_settings.last_replace;

  if (new_settings.color_editor_text != old_settings.color_editor_text)
    s.color_editor_text = new_settings.color_editor_text;

  if (new_settings.color_editor_background != old_settings.color_editor_background)
    s.color_editor_background = new_settings.color_editor_background;

  if (new_settings.color_editor_tag != old_settings.color_editor_tag)
    s.color_editor_tag = new_settings.color_editor_tag;

  if (new_settings.color_scrollbar != old_settings.color_scrollbar)
    s.color_scrollbar = new_settings.color_scrollbar;

  if (new_settings.color_scrollbar_background != old_settings.color_scrollbar_background)
    s.color_scrollbar_background = new_settings.color_scrollbar_background;

  if (new_settings.color_icon != old_settings.color_icon)
    s.color_icon = new_settings.color_icon;
    
  if (new_settings.color_icon_modified != old_settings.color_icon_modified)
    s.color_icon_modified = new_settings.color_icon_modified;
    
  if (new_settings.color_plus != old_settings.color_plus)
    s.color_plus = new_settings.color_plus;

  if (new_settings.color_line_numbers != old_settings.color_line_numbers)
    s.color_line_numbers = new_settings.color_line_numbers;

  if (new_settings.color_command_text != old_settings.color_command_text)
    s.color_command_text = new_settings.color_command_text;

  if (new_settings.color_command_background != old_settings.color_command_background)
    s.color_command_background = new_settings.color_command_background;

  if (new_settings.color_command_tag != old_settings.color_command_tag)
    s.color_command_tag = new_settings.color_command_tag;

  if (new_settings.color_column_command_text != old_settings.color_column_command_text)
    s.color_column_command_text = new_settings.color_column_command_text;

  if (new_settings.color_column_command_background != old_settings.color_column_command_background)
    s.color_column_command_background = new_settings.color_column_command_background;

  if (new_settings.color_column_command_tag != old_settings.color_column_command_tag)
    s.color_column_command_tag = new_settings.color_column_command_tag;
    
  if (new_settings.color_topline_command_text != old_settings.color_topline_command_text)
    s.color_topline_command_text = new_settings.color_topline_command_text;

  if (new_settings.color_topline_command_background != old_settings.color_topline_command_background)
    s.color_topline_command_background = new_settings.color_topline_command_background;

  if (new_settings.color_topline_command_tag != old_settings.color_topline_command_tag)
    s.color_topline_command_tag = new_settings.color_topline_command_tag;

  if (new_settings.color_comment != old_settings.color_comment)
    s.color_comment = new_settings.color_comment;

  if (new_settings.color_string != old_settings.color_string)
    s.color_string = new_settings.color_string;

  if (new_settings.color_keyword != old_settings.color_keyword)
    s.color_keyword = new_settings.color_keyword;

  if (new_settings.color_keyword_2 != old_settings.color_keyword_2)
    s.color_keyword_2 = new_settings.color_keyword_2;
  }

void update_settings(settings& s, const char* filename)
  {
  pref_file f(filename, pref_file::READ);
  f["use_spaces_for_tab"] >> s.use_spaces_for_tab;
  f["tab_space"] >> s.tab_space;
  f["show_all_characters"] >> s.show_all_characters;
  f["mario"] >> s.mario;
  f["width"] >> s.w;
  f["height"] >> s.h;
  f["x"] >> s.x;
  f["y"] >> s.y;
  f["font_size"] >> s.font_size;
  f["font"] >> s.font;
  f["mouse_scroll_steps"] >> s.mouse_scroll_steps;
  f["last_find"] >> s.last_find;
  f["last_replace"] >> s.last_replace;
  f["show_line_numbers"] >> s.show_line_numbers;
  f["wrap"] >> s.wrap;
  f["syntax"] >> s.syntax;
  f["case_sensitive"] >> s.case_sensitive;

  f["color_editor_text"] >> s.color_editor_text;
  f["color_editor_background"] >> s.color_editor_background;
  f["color_editor_tag"] >> s.color_editor_tag;
  f["color_scrollbar"] >> s.color_scrollbar;
  f["color_scrollbar_background"] >> s.color_scrollbar_background;
  f["color_icon"] >> s.color_icon;
  f["color_icon_modified"] >> s.color_icon_modified;
  f["color_plus"] >> s.color_plus;
  f["color_command_text"] >> s.color_command_text;
  f["color_command_background"] >> s.color_command_background;
  f["color_command_tag"] >> s.color_command_tag;
  f["color_column_command_text"] >> s.color_column_command_text;
  f["color_column_command_background"] >> s.color_column_command_background;
  f["color_column_command_tag"] >> s.color_column_command_tag;
  f["color_topline_command_text"] >> s.color_topline_command_text;
  f["color_topline_command_background"] >> s.color_topline_command_background;
  f["color_topline_command_tag"] >> s.color_topline_command_tag;  
  f["color_comment"] >> s.color_comment;
  f["color_string"] >> s.color_string;
  f["color_keyword"] >> s.color_keyword;
  f["color_keyword_2"] >> s.color_keyword_2;
  f["color_line_numbers"] >> s.color_line_numbers;

  f.release();
  }

settings read_settings(const char* filename)
  {
  settings s;  
  update_settings(s, filename);
  return s;
  }

void write_settings(const settings& s, const char* filename)
  {
  pref_file f(filename, pref_file::WRITE);
  f << "use_spaces_for_tab" << s.use_spaces_for_tab;
  f << "tab_space" << s.tab_space;
  f << "show_all_characters" << s.show_all_characters;
  f << "mario" << s.mario;
  f << "width" << s.w;
  f << "height" << s.h;
  f << "x" << s.x;
  f << "y" << s.y;
  f << "font_size" << s.font_size;
  f << "font" << s.font;  
  f << "mouse_scroll_steps" << s.mouse_scroll_steps;
  f << "last_find" << s.last_find;
  f << "last_replace" << s.last_replace;
  f << "show_line_numbers" << s.show_line_numbers;
  f << "wrap" << s.wrap;
  f << "syntax" << s.syntax;
  f << "case_sensitive" << s.case_sensitive;

  f << "color_editor_text" << s.color_editor_text;
  f << "color_editor_background" << s.color_editor_background;
  f << "color_editor_tag" << s.color_editor_tag;
  f << "color_command_text" << s.color_command_text;
  f << "color_command_background" << s.color_command_background;
  f << "color_command_tag" << s.color_command_tag;
  f << "color_column_command_text" << s.color_column_command_text;
  f << "color_column_command_background" << s.color_column_command_background;
  f << "color_column_command_tag" << s.color_column_command_tag;
  f << "color_topline_command_text" << s.color_topline_command_text;
  f << "color_topline_command_background" << s.color_topline_command_background;
  f << "color_topline_command_tag" << s.color_topline_command_tag;


  f << "color_comment" << s.color_comment;
  f << "color_string" << s.color_string;
  f << "color_keyword" << s.color_keyword;
  f << "color_keyword_2" << s.color_keyword_2;
  f << "color_line_numbers" << s.color_line_numbers;
  f << "color_scrollbar" << s.color_scrollbar;
  f << "color_scrollbar_background" << s.color_scrollbar_background;
  f << "color_icon" << s.color_icon;
  f << "color_icon_modified" << s.color_icon_modified;
  f << "color_plus" << s.color_plus;
  f.release();
  }
