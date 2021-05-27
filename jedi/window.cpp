#include "window.h"


window make_window(uint32_t buffer_id, int x, int y, int cols, int rows, e_window_type wt) {
  window w;
  w.buffer_id = buffer_id;
  w.x = x;
  w.y = y;
  w.cols = cols;
  w.rows = rows;
  w.wt = wt;
  return w;
}
