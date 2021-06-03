#include "serialize.h"
#include "window.h"
#include "grid.h"
#include <fstream>

void save_to_stream(std::ostream& str, const app_state& state) {
  str << state.w << std::endl;
  str << state.h << std::endl;
  str << (uint32_t)state.windows.size() << std::endl;
  for (const auto& w : state.windows)
    save_window_to_stream(str, w);
  str << (uint32_t)state.buffer_id_to_window_id.size() << std::endl;
  for (auto v : state.buffer_id_to_window_id)
    str << v << std::endl;
  str << (uint32_t)state.window_pairs.size() << std::endl;
  for (const auto& w : state.window_pairs)
    save_window_pair_to_stream(str, w);
  save_grid_to_stream(str, state.g);
  //save_file_state_to_stream(str, state);
}

app_state load_from_stream(std::istream& str) {
  app_state result;
  str >> result.w >> result.w;
  uint32_t sz;
  str >> sz;
  for (uint32_t i = 0; i < sz; ++i)
    {
    result.windows.push_back(load_window_from_stream(str));
    }
  str >> sz;
  for (uint32_t i = 0; i < sz; ++i)
    {
    uint32_t v;
    str >> v;
    result.buffer_id_to_window_id.push_back(v);
    }
  str >> sz;
  for (uint32_t i = 0; i < sz; ++i)
    {
    result.window_pairs.push_back(load_window_pair_from_stream(str));
    }
  result.g = load_grid_from_stream(str);
  //result.file_state = load_file_state_from_stream(str);
  return result;
}

void save_to_file(const std::string& filename, const app_state& state) {
  std::ofstream f(filename);
  if (f.is_open())
    {
    save_to_stream(f, state);
    }
  f.close();
}

app_state load_from_file(const std::string& filename) {
  app_state result;
  std::ifstream f(filename);
  if (f.is_open())
    {
    result = load_from_stream(f);
    f.close();
    }
  return result;
}
