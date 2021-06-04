#include "serialize.h"
#include "window.h"
#include "grid.h"
#include "buffer.h"
#include <fstream>
#include <sstream>

namespace {

std::vector<std::string> split_in_lines(const std::string& str) {
  std::vector<std::string> lines;
  std::stringstream ss(str);
  std::string line;
  while(std::getline(ss,line,'\n'))
  {
    lines.push_back(line);
  }
  return lines;
}

std::string make_filename(const std::string& f)
{
  if (f.empty())
    return "+empty";
  return f;
}

void save_buffer_to_stream(std::ostream& str, const buffer_data& b, bool command) {
  std::string filename = make_filename(b.buffer.name);
  
  str << filename << std::endl;
  str << (int)b.bt << std::endl;
  str << b.scroll_row << std::endl;
  str << command << std::endl;
  if (command) {
    std::string command_text = to_string(b.buffer.content);
    std::vector<std::string> lines = split_in_lines(command_text);
    str << lines.size() << std::endl;
    for (const auto& line : lines)
      str << line << std::endl;
  }
}

buffer_data load_buffer_from_stream(std::istream& str, const settings& s) {
  buffer_data bd;
#ifdef _WIN32
  bd.process = nullptr;
#else
  bd.process = {{-1,-1,-1}};
#endif
  bd.buffer = make_empty_buffer();
  //str.ignore();
  std::string buffer_name;
  std::getline(str, buffer_name);
  if (buffer_name.empty())
    std::getline(str, buffer_name);  
  bd.buffer.name = buffer_name;
  int bt;
  str >> bt;
  bd.bt = (e_buffer_type)bt;
  str >> bd.scroll_row;
  bool command;
  str >> command;
  if (command) {
    size_t nr_of_lines;
    str >> nr_of_lines;
    str.ignore();
    std::string command_text;
    for (size_t i = 0; i < nr_of_lines; ++i) {
      std::string line;
      std::getline(str, line);
      command_text.append(line);
      if (i+1 < nr_of_lines)
        command_text.push_back('\n');
    }
    bd.buffer = insert(bd.buffer, command_text, convert(s), false);
  }
  return bd;
}

void save_buffers_to_stream(std::ostream& str, const app_state& state) {
  str << state.active_buffer << std::endl;
  str << (uint32_t)state.buffers.size() << std::endl;
  uint32_t id = 0;
  for (const auto& f : state.buffers)
  {
    save_buffer_to_stream(str, f, state.windows[state.buffer_id_to_window_id[f.buffer_id]].wt != e_window_type::wt_normal);
    ++id;
  }
}

void load_buffers_from_stream(app_state& state, std::istream& str, const settings& s) {
  str >> state.active_buffer;
  uint32_t sz;
  str >> sz;
  for (uint32_t i = 0; i < sz; ++i)
  {
    state.buffers.push_back(load_buffer_from_stream(str, s));
    state.buffers.back().buffer_id = i;
  }
}

}

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
  save_buffers_to_stream(str, state);
}

app_state load_from_stream(std::istream& str, const settings& s) {
  app_state result;
  str >> result.w >> result.h;
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
  load_buffers_from_stream(result, str, s);
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

app_state load_from_file(app_state init, const std::string& filename, const settings& s) {
  app_state result = init;
  std::ifstream f(filename);
  if (f.is_open())
  {
    result = load_from_stream(f, s);
    f.close();
  }
  return result;
}

void save_buffers_to_stream(std::ostream& str, const app_state& state)
{
  
}

app_state load_buffers_from_stream(std::istream& str);
