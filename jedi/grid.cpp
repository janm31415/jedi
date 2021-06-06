#include "grid.h"

void save_grid_to_stream(nlohmann::json& j, const grid& g) {
  j["topline"] = g.topline_window_id;
  nlohmann::json& cols = j["columns"];
  for (const auto& c : g.columns) {
    nlohmann::json colj;
    save_column_to_stream(colj, c);
    cols.push_back(colj);
    }
}

void save_grid_to_stream(std::ostream& str, const grid& g)
  {
  str << g.topline_window_id << std::endl;
  str << (uint32_t)g.columns.size() << std::endl;
  for (const auto& c : g.columns)
    save_column_to_stream(str, c);
  }

grid load_grid_from_stream(std::istream& str)
  {
  grid g;
  uint32_t sz;
  str >> g.topline_window_id;
  str >> sz;
  for (uint32_t i = 0; i < sz; ++i)
    g.columns.push_back(load_column_from_stream(str));
  return g;
  }

void save_column_item_to_stream(nlohmann::json& j, const column_item& ci) {
  j["column_id"] = ci.column_id;
  j["top_layer"] = ci.top_layer;
  j["bottom_layer"] = ci.bottom_layer;
  j["window_pair_id"] = ci.window_pair_id;
  }

void save_column_item_to_stream(std::ostream& str, const column_item& ci)
  {
  str << ci.column_id << std::endl;
  str << ci.top_layer << std::endl;
  str << ci.bottom_layer << std::endl;
  str << ci.window_pair_id << std::endl;
  }

column_item load_column_item_from_stream(std::istream& str)
  {
  column_item ci;
  str >> ci.column_id;
  str >> ci.top_layer;
  str >> ci.bottom_layer;
  str >> ci.window_pair_id;
  return ci;
  }

void save_column_to_stream(nlohmann::json& j, const column& c) {
  j["left"] = c.left;
  j["right"] = c.right;
  j["column_command_window_id"] = c.column_command_window_id;
  j["contains_maximized_item"] = (int)c.contains_maximized_item;
  nlohmann::json& jitems = j["items"];
  for (const auto& ci : c.items) {
    nlohmann::json jitem;
    save_column_item_to_stream(jitem, ci);
    jitems.push_back(jitem);
    }
  }

void save_column_to_stream(std::ostream& str, const column& c)
  {
  str << c.left << std::endl;
  str << c.right << std::endl;
  str << c.column_command_window_id << std::endl;
  str << c.contains_maximized_item << std::endl;
  str << (uint32_t)c.items.size() << std::endl;
  for (const auto& ci : c.items)
    save_column_item_to_stream(str, ci);
  }

column load_column_from_stream(std::istream& str)
  {
  column c;
  str >> c.left;
  str >> c.right;
  str >> c.column_command_window_id;
  str >> c.contains_maximized_item;
  uint32_t sz;
  str >> sz;
  for (uint32_t i = 0; i < sz; ++i)
    c.items.push_back(load_column_item_from_stream(str));
  return c;
  }

column_item load_column_item_from_stream(nlohmann::json& j) {
  column_item ci;
  for (nlohmann::json::iterator it = j.begin(); it != j.end(); ++it)
    {
    if (it.key() == std::string("column_id")) {
      ci.column_id = *it;
      }
    if (it.key() == std::string("top_layer")) {
      ci.top_layer = *it;
      }
    if (it.key() == std::string("bottom_layer")) {
      ci.bottom_layer = *it;
      }
    if (it.key() == std::string("window_pair_id")) {
      ci.window_pair_id = *it;
      }
    }
  return ci;
  }

column load_column_from_stream(nlohmann::json& j) {
  column c;
  for (nlohmann::json::iterator it = j.begin(); it != j.end(); ++it)
    {
    if (it.key() == std::string("left")) {
      c.left = *it;
      }
    if (it.key() == std::string("right")) {
      c.right = *it;
      }
    if (it.key() == std::string("column_command_window_id")) {
      c.column_command_window_id = *it;
      }
    if (it.key() == std::string("contains_maximized_item")) {
      c.contains_maximized_item = *it == 0 ? false : true;
      }
    if (it.key() == std::string("items")) {
      for (auto it2 = it->begin(); it2 != it->end(); ++it2) {
        c.items.push_back(load_column_item_from_stream(*it2));
        }
      }
    }
  return c;
  }

grid load_grid_from_stream(nlohmann::json& j) {
  grid g;
  for (nlohmann::json::iterator it = j.begin(); it != j.end(); ++it)
    {
    if (it.key() == std::string("topline")) {
      g.topline_window_id = *it;
      }
    if (it.key() == std::string("columns")) {
      for (auto it2 = it->begin(); it2 != it->end(); ++it2) {
        g.columns.push_back(load_column_from_stream(*it2));
        }
      }
    }
  return g;
  }