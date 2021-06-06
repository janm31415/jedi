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

void save_window_to_stream(nlohmann::json& j, const window& w) {
  j["x"] = w.x;
  j["y"] = w.y;
  j["cols"] = w.cols;
  j["rows"] = w.rows;
  j["buffer_id"] = w.buffer_id;
  j["wt"] = (int)w.wt;
}

void save_window_to_stream(std::ostream& str, const window& w)
  {
  str << w.x << std::endl;
  str << w.y << std::endl;
  str << w.cols << std::endl;
  str << w.rows << std::endl;
  str << w.buffer_id << std::endl;
  str << (int)w.wt << std::endl;
  }

window load_window_from_stream(std::istream& str)
  {
  window w = make_window(0, 0, 0, 0, 0, (e_window_type)0);
  str >> w.x >> w.y >> w.cols >> w.rows;
  int wt;
  str >> w.buffer_id >> wt;
  w.wt = (e_window_type)wt;
  return w;
  }

void save_window_pair_to_stream(nlohmann::json& j, const window_pair& w) {
  j["window_id"] = w.window_id;
  j["command_window_id"] = w.command_window_id;
}

void save_window_pair_to_stream(std::ostream& str, const window_pair& w)
  {
  str << w.window_id << std::endl;
  str << w.command_window_id << std::endl;
  }

window_pair load_window_pair_from_stream(std::istream& str)
  {
  window_pair w;

  str >> w.window_id >> w.command_window_id;
  return w;
  }
