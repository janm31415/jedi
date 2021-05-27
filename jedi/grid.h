#pragma once

#include <stdint.h>
#include <vector>
#include <iostream>

struct column_item
  {
  uint32_t column_id;
  double top_layer, bottom_layer;
  uint32_t window_pair_id;
  };

struct column
  {
  column() : contains_maximized_item(false) {}
  double left, right;
  std::vector<column_item> items;
  uint32_t column_command_window_id;
  bool contains_maximized_item;
  };

struct grid
  {
  uint32_t topline_window_id;
  std::vector<column> columns;
  };

void save_column_item_to_stream(std::ostream& str, const column_item& ci);
void save_column_to_stream(std::ostream& str, const column& c);
void save_grid_to_stream(std::ostream& str, const grid& g);

column_item load_column_item_from_stream(std::istream& str);
column load_column_from_stream(std::istream& str);
grid load_grid_from_stream(std::istream& str);
