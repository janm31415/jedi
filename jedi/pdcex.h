#pragma once

#include "buffer.h"
#include <vector>

enum screen_ex_type
  {  
  SET_NONE,
  SET_SCROLLBAR_EDITOR,
  SET_TEXT_EDITOR,
  SET_TEXT_COMMAND,
  SET_TEXT_OPERATION,
  SET_LINENUMBER
  };

struct screen_ex_pixel
  {
  screen_ex_pixel() : type(SET_NONE), pos() {}
  position pos;
  screen_ex_type type;
  };

struct screen_ex
  {
  screen_ex(int ilines, int icols);

  int lines;
  int cols;
  std::vector<screen_ex_pixel> data;
  };

extern screen_ex pdc_ex;

void resize_term_ex(int ilines, int icols);
void add_ex(position pos, screen_ex_type type);
screen_ex_pixel get_ex(int row, int col);
void invalidate_range(int x, int y, int cols, int rows);
void invalidate_ex();