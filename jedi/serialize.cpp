#include "serialize.h"
#include "window.h"
#include "grid.h"
#include "buffer.h"
#include <fstream>
#include <sstream>

#include "json.hpp"

namespace {

  std::vector<std::string> split_in_lines(const std::string& str) {
    std::vector<std::string> lines;
    std::stringstream ss(str);
    std::string line;
    while (std::getline(ss, line, '\n'))
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

  void save_buffer_to_stream(nlohmann::json& j, const buffer_data& b, bool command) {
    std::string filename = make_filename(b.buffer.name);
    j["filename"] = filename;
    j["bt"] = (int)b.bt;
    j["scroll_row"] = b.scroll_row;
    j["command"] = (int)command;
    if (command) {
      std::string command_text = to_string(b.buffer.content);
      j["command_text"] = command_text;
      }
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

  buffer_data load_buffer_from_stream(nlohmann::json& j, const settings& s) {
    buffer_data bd;
#ifdef _WIN32
    bd.process = nullptr;
#else
    bd.process = { {-1,-1,-1} };
#endif
    bd.buffer = make_empty_buffer();
    for (nlohmann::json::iterator it = j.begin(); it != j.end(); ++it)
      {
      if (it.key() == std::string("filename")) {
        bd.buffer.name = *it;
        }
      if (it.key() == std::string("bt")) {
        bd.bt = (e_buffer_type)(*it);
        }
      if (it.key() == std::string("scroll_row")) {
        bd.scroll_row = *it;
        }
      if (it.key() == std::string("command_text")) {
        std::string command_text = *it;
        bd.buffer = insert(bd.buffer, command_text, convert(s), false);
        }
      }
    return bd;
    }

  buffer_data load_buffer_from_stream(std::istream& str, const settings& s) {
    buffer_data bd;
#ifdef _WIN32
    bd.process = nullptr;
#else
    bd.process = { {-1,-1,-1} };
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
        if (i + 1 < nr_of_lines)
          command_text.push_back('\n');
        }
      bd.buffer = insert(bd.buffer, command_text, convert(s), false);
      }
    return bd;
    }

  void save_buffers_to_stream(nlohmann::json& j, const app_state& state) {
    j["active_buffer"] = state.active_buffer;
    j["last_active_editor_buffer"] = state.last_active_editor_buffer;
    nlohmann::json& buf = j["buffers"];
    for (const auto& f : state.buffers) {
      nlohmann::json jbuf;
      save_buffer_to_stream(jbuf, f, state.windows[state.buffer_id_to_window_id[f.buffer_id]].wt != e_window_type::wt_normal);
      buf.push_back(jbuf);
      }
    }

  void save_buffers_to_stream(std::ostream& str, const app_state& state) {
    str << state.active_buffer << std::endl;
    str << state.last_active_editor_buffer << std::endl;
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
    str >> state.last_active_editor_buffer;
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
  nlohmann::json j;

  j["width"] = state.w;
  j["height"] = state.h;
  nlohmann::json& win = j["windows"];
  for (const auto& w : state.windows) {
    nlohmann::json winj;
    save_window_to_stream(winj, w);
    win.push_back(winj);
    }
  nlohmann::json& buffer_to_window = j["buffer_id_to_window_id"];
  for (auto v : state.buffer_id_to_window_id)
    buffer_to_window.push_back(v);
  nlohmann::json& winpairs = j["window_pairs"];
  for (const auto& wp : state.window_pairs) {
    nlohmann::json winj;
    save_window_pair_to_stream(winj, wp);
    winpairs.push_back(winj);
    }
  nlohmann::json& gr = j["grid"];
  save_grid_to_stream(gr, state.g);
  nlohmann::json& buf = j["buffers"];
  save_buffers_to_stream(buf, state);
  str << j.dump(2);
  }

void save_to_stream_old(std::ostream& str, const app_state& state) {
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
  result.active_buffer = 0xffffffff;
  result.last_active_editor_buffer = 0xffffffff;
  nlohmann::json j;
  str >> j;
  for (nlohmann::json::iterator it = j.begin(); it != j.end(); ++it)
    {
    if (it.key() == std::string("width")) {
      result.w = *it;
      }
    if (it.key() == std::string("height")) {
      result.h = *it;
      }
    if (it.key() == std::string("buffer_id_to_window_id")) {
      for (auto it2 = it->begin(); it2 != it->end(); ++it2) {
        result.buffer_id_to_window_id.push_back(*it2);
        }
      }
    if (it.key() == std::string("windows")) {
      for (auto it2 = it->begin(); it2 != it->end(); ++it2) {
        result.windows.push_back(load_window_from_stream(*it2));
        }
      }
    if (it.key() == std::string("window_pairs")) {
      for (auto it2 = it->begin(); it2 != it->end(); ++it2) {
        result.window_pairs.push_back(load_window_pair_from_stream(*it2));
        }
      }
    if (it.key() == std::string("grid")) {
      result.g = load_grid_from_stream(*it);
      }
    if (it.key() == std::string("buffers")) {
      for (auto it2 = it->begin(); it2 != it->end(); ++it2) {
        if (it2.key() == std::string("active_buffer")) {
          result.active_buffer = *it2;
          }
        if (it2.key() == std::string("last_active_editor_buffer")) {
          result.last_active_editor_buffer = *it2;
          }
        if (it2.key() == std::string("buffers")) {
          for (auto it3 = it2->begin(); it3 != it2->end(); ++it3) {
            result.buffers.push_back(load_buffer_from_stream(*it3, s));
            result.buffers.back().buffer_id = (uint32_t)result.buffers.size() - 1;
            }
          }
        }
      }
    }
  return result;
  }

app_state load_from_stream_old(std::istream& str, const settings& s) {
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
