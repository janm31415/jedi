#pragma once

#include <stdint.h>

struct window_pair {
  uint32_t window_id, command_window_id;
};

enum e_window_type
  {
  wt_normal,
  wt_command,
  wt_column_command,
  wt_topline
  };
  
struct window {
  uint32_t buffer_id;
  int x,y,cols,rows;
  e_window_type wt;
};


window make_window(uint32_t buffer_id, int x, int y, int cols, int rows, e_window_type wt);
