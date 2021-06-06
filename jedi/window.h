#pragma once

#include <stdint.h>
#include <iostream>

#include "json.hpp"

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

void save_window_to_stream(nlohmann::json& j, const window& w);

void save_window_to_stream(std::ostream& str, const window& w);

window load_window_from_stream(std::istream& str);

void save_window_pair_to_stream(nlohmann::json& j, const window_pair& w);

void save_window_pair_to_stream(std::ostream& str, const window_pair& w);

window_pair load_window_pair_from_stream(std::istream& str);
