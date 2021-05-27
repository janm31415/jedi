#include "grid.h"

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

