#include "settings.h"

#include "pref_file.h"

#include <jtk/file_utils.h>

settings::settings()
  {
  show_all_characters = false;
  tab_space = 2;
  use_spaces_for_tab = true;
  show_line_numbers = false;
  wrap = false;
  w = 80;
  h = 25;
  x = 100;
  y = 100;
  command_buffer_rows = 1;
  command_text = "New Open Save Exit";
  font_size = 17;
  mouse_scroll_steps = 3;
  startup_folder = "";

  color_editor_text = 0xffc0c0c0;
  color_editor_background = 0xff000000;
  color_editor_tag = 0xfff18255;
  color_editor_text_bold = 0xffffffff;
  color_editor_background_bold = 0xff000000;
  color_editor_tag_bold = 0xffff9b73;

  color_line_numbers = 0xff505050;

  color_command_text = 0xffc0c0c0;
  color_command_background = 0xff282828;
  color_command_tag = 0xfff18255;

  color_titlebar_text = 0xffc0c0c0;
  color_titlebar_background = 0xff282828;

  color_comment = 0xff64c385;
  color_string = 0xff6464db;
  color_keyword = 0xffff8080;
  color_keyword_2 = 0xffffc0c0;
  }

void update_settings_if_different(settings& s, const settings& new_settings, const settings& old_settings)
  {
  if (new_settings.use_spaces_for_tab != old_settings.use_spaces_for_tab)
    s.use_spaces_for_tab = new_settings.use_spaces_for_tab;

  if (new_settings.tab_space != old_settings.tab_space)
    s.tab_space = new_settings.tab_space;

  if (new_settings.show_all_characters != old_settings.show_all_characters)
    s.show_all_characters = new_settings.show_all_characters;

  if (new_settings.w != old_settings.w)
    s.w = new_settings.w;

  if (new_settings.h != old_settings.h)
    s.h = new_settings.h;

  if (new_settings.show_line_numbers != old_settings.show_line_numbers)
    s.show_line_numbers = new_settings.show_line_numbers;

  if (new_settings.wrap != old_settings.wrap)
    s.wrap = new_settings.wrap;

  if (new_settings.x != old_settings.x)
    s.x = new_settings.x;

  if (new_settings.y != old_settings.y)
    s.y = new_settings.y;

  if (new_settings.command_buffer_rows != old_settings.command_buffer_rows)
    s.command_buffer_rows = new_settings.command_buffer_rows;

  if (new_settings.command_text != old_settings.command_text)
    s.command_text = new_settings.command_text;

  if (new_settings.font_size != old_settings.font_size)
    s.font_size = new_settings.font_size;

  if (new_settings.mouse_scroll_steps != old_settings.mouse_scroll_steps)
    s.mouse_scroll_steps = new_settings.mouse_scroll_steps;

  if (new_settings.startup_folder != old_settings.startup_folder)
    s.startup_folder = new_settings.startup_folder;

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

  if (new_settings.color_editor_text_bold != old_settings.color_editor_text_bold)
    s.color_editor_text_bold = new_settings.color_editor_text_bold;

  if (new_settings.color_editor_background_bold != old_settings.color_editor_background_bold)
    s.color_editor_background_bold = new_settings.color_editor_background_bold;

  if (new_settings.color_editor_tag_bold != old_settings.color_editor_tag_bold)
    s.color_editor_tag_bold = new_settings.color_editor_tag_bold;

  if (new_settings.color_line_numbers != old_settings.color_line_numbers)
    s.color_line_numbers = new_settings.color_line_numbers;

  if (new_settings.color_command_text != old_settings.color_command_text)
    s.color_command_text = new_settings.color_command_text;

  if (new_settings.color_command_background != old_settings.color_command_background)
    s.color_command_background = new_settings.color_command_background;

  if (new_settings.color_command_tag != old_settings.color_command_tag)
    s.color_command_tag = new_settings.color_command_tag;

  if (new_settings.color_titlebar_text != old_settings.color_titlebar_text)
    s.color_titlebar_text = new_settings.color_titlebar_text;

  if (new_settings.color_titlebar_background != old_settings.color_titlebar_background)
    s.color_titlebar_background = new_settings.color_titlebar_background;

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
  f["width"] >> s.w;
  f["height"] >> s.h;
  f["command_buffer_rows"] >> s.command_buffer_rows;
  f["command_text"] >> s.command_text;
  f["x"] >> s.x;
  f["y"] >> s.y;
  f["font_size"] >> s.font_size;
  f["mouse_scroll_steps"] >> s.mouse_scroll_steps;
  f["startup_folder"] >> s.startup_folder;
  f["last_find"] >> s.last_find;
  f["last_replace"] >> s.last_replace;
  f["show_line_numbers"] >> s.show_line_numbers;
  f["wrap"] >> s.wrap;

  f["color_editor_text"] >> s.color_editor_text;
  f["color_editor_background"] >> s.color_editor_background;
  f["color_editor_tag"] >> s.color_editor_tag;
  f["color_editor_text_bold"] >> s.color_editor_text_bold;
  f["color_editor_background_bold"] >> s.color_editor_background_bold;
  f["color_editor_tag_bold"] >> s.color_editor_tag_bold;
  f["color_command_text"] >> s.color_command_text;
  f["color_command_background"] >> s.color_command_background;
  f["color_command_tag"] >> s.color_command_tag;
  f["color_titlebar_text"] >> s.color_titlebar_text;
  f["color_titlebar_background"] >> s.color_titlebar_background;
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
  f << "width" << s.w;
  f << "height" << s.h;
  f << "command_buffer_rows" << s.command_buffer_rows;
  f << "command_text" << s.command_text;
  f << "x" << s.x;
  f << "y" << s.y;
  f << "font_size" << s.font_size;
  f << "mouse_scroll_steps" << s.mouse_scroll_steps;
  f << "startup_folder" << s.startup_folder;
  f << "last_find" << s.last_find;
  f << "last_replace" << s.last_replace;
  f << "show_line_numbers" << s.show_line_numbers;
  f << "wrap" << s.wrap;

  f << "color_editor_text" << s.color_editor_text;
  f << "color_editor_background" << s.color_editor_background;
  f << "color_editor_tag" << s.color_editor_tag;
  f << "color_editor_text_bold" << s.color_editor_text_bold;
  f << "color_editor_background_bold" << s.color_editor_background_bold;
  f << "color_editor_tag_bold" << s.color_editor_tag_bold;
  f << "color_command_text" << s.color_command_text;
  f << "color_command_background" << s.color_command_background;
  f << "color_command_tag" << s.color_command_tag;
  f << "color_titlebar_text" << s.color_titlebar_text;
  f << "color_titlebar_background" << s.color_titlebar_background;
  f << "color_comment" << s.color_comment;
  f << "color_string" << s.color_string;
  f << "color_keyword" << s.color_keyword;
  f << "color_keyword_2" << s.color_keyword_2;
  f << "color_line_numbers" << s.color_line_numbers;
  f.release();
  }
