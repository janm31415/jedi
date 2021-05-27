#include "pdcex.h"

#include <curses.h>

screen_ex::screen_ex(int ilines, int icols) : lines(ilines), cols(icols)
  {
  data.resize(ilines*icols);
  }

screen_ex pdc_ex(20, 20);


void resize_term_ex(int ilines, int icols)
  {
  pdc_ex.lines = ilines;
  pdc_ex.cols = icols;
  pdc_ex.data.resize(ilines*icols);
  }

void add_ex(position pos, screen_ex_type type)
  {
  screen_ex_pixel sp;
  sp.pos = pos;
  sp.type = type;
  int index = stdscr->_curx + pdc_ex.cols*stdscr->_cury;
  pdc_ex.data[index] = sp;
  }

screen_ex_pixel get_ex(int row, int col)
  {
  int index = col + pdc_ex.cols*row;
  if (index >= pdc_ex.data.size())
    return screen_ex_pixel();
  return pdc_ex.data[index];
  }

void invalidate_range(int x, int y, int cols, int rows)
  {
  for (int r = 0; r < rows; ++r)
    {
    for (int c = 0; c < cols; ++c)
      {
      int index = (c + x) + pdc_ex.cols*(r + y);
      pdc_ex.data[index].pos.row = -1;
      pdc_ex.data[index].pos.col = -1;
      pdc_ex.data[index].type = SET_NONE;
      }
    }
  }

void invalidate_ex()
  {
  for (auto& p : pdc_ex.data)
    {
    p.type = SET_NONE;
    }
  }