#include "engine.h"
#include "clipboard.h"
#include "colors.h"
#include "keyboard.h"
#include "mouse.h"
#include "pdcex.h"
#include "syntax_highlight.h"
#include "utils.h"
#include "draw.h"

#include <jtk/file_utils.h>
#include <jtk/pipe.h>

#include <map>
#include <functional>
#include <sstream>
#include <cctype>

#include <SDL.h>
#include <SDL_syswm.h>
#include <curses.h>

extern "C"
  {
#include <sdl2/pdcsdl.h>
  }


#define DEFAULT_COLOR (A_NORMAL | COLOR_PAIR(default_color))

#define COMMAND_COLOR (A_NORMAL | COLOR_PAIR(command_color))

namespace
  {
  int font_width, font_height;
  }

env_settings convert(const settings& s)
  {
  env_settings out;
  out.tab_space = s.tab_space;
  out.show_all_characters = s.show_all_characters;
  return out;
  }
  
buffer_data make_empty_buffer_data() {
  buffer_data bd;
  bd.buffer = make_empty_buffer();
  bd.scroll_row = 0;
#ifdef _WIN32
  bd.process = nullptr;
#else
  bd.process = {{-1,-1,-1}};
#endif
  bd.bt = bt_normal;
  bd.buffer.name = "";
  return bd;
}

line string_to_line(const std::string& txt)
  {
  line out;
  auto trans = out.transient();
  for (auto ch : txt)
    trans.push_back(ch);
  return trans.persistent();
  }

bool ctrl_pressed()
  {
#if defined(__APPLE__)
  if (keyb.is_down(SDLK_LGUI) || keyb.is_down(SDLK_RGUI))
    return true;
#endif
  return (keyb.is_down(SDLK_LCTRL) || keyb.is_down(SDLK_RCTRL));
  }

bool alt_pressed()
  {
  return (keyb.is_down(SDLK_LALT) || keyb.is_down(SDLK_RALT));
  }

bool shift_pressed()
  {
  return (keyb.is_down(SDLK_LSHIFT) || keyb.is_down(SDLK_RSHIFT));
  }

app_state resize_font(app_state state, int font_size, settings& s)
  {
  pdc_font_size = font_size;
  s.font_size = font_size;
#ifdef _WIN32
  TTF_CloseFont(pdc_ttffont);
  pdc_ttffont = TTF_OpenFont("C:/Windows/Fonts/consola.ttf", pdc_font_size);
#elif defined(unix)
  TTF_CloseFont(pdc_ttffont);
  pdc_ttffont = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf", pdc_font_size);
#elif defined(__APPLE__)
  TTF_CloseFont(pdc_ttffont);
  pdc_ttffont = TTF_OpenFont("/System/Library/Fonts/Menlo.ttc", pdc_font_size);
#endif

  TTF_SizeText(pdc_ttffont, "W", &font_width, &font_height);
  pdc_fheight = font_height;
  pdc_fwidth = font_width;
  pdc_fthick = pdc_font_size / 20 + 1;

  state.w = (state.w / font_width) * font_width;
  state.h = (state.h / font_height) * font_height;

  SDL_SetWindowSize(pdc_window, state.w, state.h);

  resize_term(state.h / font_height, state.w / font_width);
  resize_term_ex(state.h / font_height, state.w / font_width);

  return state;
  }
  
uint32_t get_column_id(const app_state& state, int64_t buffer_id)
  {
  for (uint32_t i = 0; i < state.g.columns.size(); ++i)
    {
    auto& c = state.g.columns[i];
    if (state.windows[c.column_command_window_id].buffer_id == buffer_id)
      return i;
    for (uint32_t j = 0; j < c.items.size(); ++j)
      {
      auto ci = c.items[j];
      const auto& wp = state.window_pairs[ci.window_pair_id];
      if (state.windows[wp.command_window_id].buffer_id == buffer_id || state.windows[wp.window_id].buffer_id == buffer_id)
        {
        return i;
        }
      }
    }
  return (uint32_t)state.g.columns.size() - (uint32_t)1;
  }
  
const file_buffer& get_active_buffer(const app_state& state) {
  return state.buffers[state.active_buffer].buffer;
  }
  
file_buffer& get_active_buffer(app_state& state) {
  return state.buffers[state.active_buffer].buffer;
  }
  
int64_t& get_active_scroll_row(app_state& state) {
  return state.buffers[state.active_buffer].scroll_row;
}
  
std::optional<app_state> command_exit(app_state state, uint32_t, settings& s)
  {
  return std::nullopt;
  }
  
app_state resize_windows(app_state state, const settings& s) {
  int rows, cols;
  getmaxyx(stdscr, rows, cols);
  rows -= 3; // make room for bottom operation stuff
  state.windows[state.g.topline_window_id].x = 0;
  state.windows[state.g.topline_window_id].y = 0;
  state.windows[state.g.topline_window_id].cols = cols;
  if (state.windows[state.g.topline_window_id].rows <= 0)
    state.windows[state.g.topline_window_id].rows = 1;
  if (state.windows[state.g.topline_window_id].rows > rows)
    state.windows[state.g.topline_window_id].rows = rows;
  int start_row = state.windows[state.g.topline_window_id].rows;
  
  for (auto& c : state.g.columns) {
    int left = (int)(c.left*cols);
    int right = (int)(c.right*cols);
    state.windows[c.column_command_window_id].x = left;
    state.windows[c.column_command_window_id].y = start_row;
    state.windows[c.column_command_window_id].cols = right-left;
    int available_rows = rows - state.windows[state.g.topline_window_id].rows;
    if (state.windows[c.column_command_window_id].rows <= 0)
      state.windows[c.column_command_window_id].rows = 1;
    if (state.windows[c.column_command_window_id].rows > available_rows)
      state.windows[c.column_command_window_id].rows = available_rows;
    available_rows = rows - state.windows[state.g.topline_window_id].rows - state.windows[c.column_command_window_id].rows;
    for (auto& ci : c.items) {
      int row_offset = start_row + state.windows[c.column_command_window_id].rows;
      int top = row_offset + (int)(available_rows*ci.top_layer);
      int bottom = row_offset + (int)(available_rows*ci.bottom_layer);
      auto wp = state.window_pairs[ci.window_pair_id];
      int available_rows_for_command = bottom-top;
      if (state.windows[wp.command_window_id].rows <= 0)
        state.windows[wp.command_window_id].rows = 1;
      if (state.windows[wp.command_window_id].rows > available_rows_for_command)
        state.windows[wp.command_window_id].rows = available_rows_for_command;
      state.windows[wp.command_window_id].cols = right-left;
      state.windows[wp.command_window_id].x = left;
      state.windows[wp.command_window_id].y = top;
      state.windows[wp.window_id].cols = right-left;
      state.windows[wp.window_id].x = left;
      state.windows[wp.window_id].y = top+state.windows[wp.command_window_id].rows;
      state.windows[wp.window_id].rows = available_rows_for_command - state.windows[wp.command_window_id].rows;
    }
  }
  return state;
}


std::wstring make_command_text(const app_state& state, uint32_t buffer_id, const settings& s)
  {
  const auto& f = state.buffers[buffer_id].buffer;
  std::stringstream out;
  out << f.name << " Del | " << s.command_text;
  return jtk::convert_string_to_wstring(out.str());
  }
  
bool this_is_a_command_window(const app_state& state, uint32_t command_id)
  {
  const auto& w = state.windows[state.buffer_id_to_window_id[command_id]];
  if (w.wt == e_window_type::wt_command)
    return true;
  return false;
  }
  
std::string get_command_text(const app_state& state, uint32_t buffer_id, const settings& s)
  {
  assert(this_is_a_command_window(state, buffer_id));
  auto& w = state.windows[state.buffer_id_to_window_id[buffer_id]];
  auto& f = state.buffers[buffer_id].buffer;
  auto& textf = state.buffers[buffer_id+1].buffer;

  bool put = textf.modification_mask != 0;
  bool undo = !textf.history.empty() && textf.undo_redo_index > 0;
  bool redo = !textf.history.empty() && textf.undo_redo_index < textf.history.size();

  bool get = jtk::is_directory(f.name);
  std::stringstream str;
  str << " Del";
  if (get)
    str << " Get";
  if (undo)
    str << " Undo";
  if (redo)
    str << " Redo";
  if (put)
    str << " Put";
  str << " ";
  return str.str();
  }

void get_first_word_positions(position& start, position& stop, const app_state& state, uint32_t buffer_id, const settings& s) {
  const auto& fb = state.buffers[buffer_id].buffer;
  position pos(0, 0);
  position lastpos = get_last_position(fb.content);
  start = position(-1, -1);
  stop = lastpos;
  bool inside_string = false;
  while (pos != lastpos) {
    wchar_t current_char = fb.content[pos.row][pos.col];
    if (current_char == L' ') {
      if (start.col >= 0 && !inside_string) {
        stop = pos;
        return;
      }
    } else {
      if (start.col < 0) {
        start = pos;
      }
      if (current_char == L'"')
        inside_string = !inside_string;
    }
    pos = get_next_position(fb, pos);
  }
}
  
bool should_update_command_text(const app_state& state, uint32_t buffer_id, const settings& s) {
  assert(this_is_a_command_window(state, buffer_id));
  const auto& fb = state.buffers[buffer_id].buffer;
  
  auto pos_del = find_next_occurence(fb.content, position(0, 0), L" Del ");
  if (pos_del.col < 0)
    return true;
  auto pos_bar = find_next_occurence(fb.content, pos_del, L'|');
  if (pos_bar.col < 0)
    return true;
  pos_del = find_next_occurence_reverse(fb.content, pos_bar, L" Del ");
  auto current_text = to_string(fb.content, pos_del, pos_bar);
  auto text = get_command_text(state, buffer_id, s);
  return text != current_text;
}

app_state update_command_text(app_state state, uint32_t buffer_id, const settings& s) {
  assert(this_is_a_command_window(state, buffer_id));
  auto text = get_command_text(state, buffer_id, s);
  auto& fb = state.buffers[buffer_id].buffer;
  auto pos_del = find_next_occurence(fb.content, position(0, 0), L" Del ");
  if (pos_del.col < 0) {
    auto pos_bar = find_next_occurence(fb.content, position(0, 0), L'|');
    if (pos_bar.col < 0) {
      fb.pos = get_last_position(fb);
      text.push_back(L'|');
      fb = insert(fb, text, convert(s), false);
      return state;
    }
    fb.pos = pos_bar;
    fb = insert(fb, text, convert(s), false);
    return state;
  }
  auto pos_bar = find_next_occurence(fb.content, pos_del, L'|');
  if (pos_bar.col < 0) {
    fb.pos = get_last_position(fb);
    text.push_back(L'|');
    fb = insert(fb, text, convert(s), false);
    return state;
  }
  pos_del = find_next_occurence_reverse(fb.content, pos_bar, L" Del ");
  
  fb.start_selection = pos_del;
  fb.pos = pos_bar;
  fb = erase(fb, convert(s), false);
  fb = insert(fb, text, convert(s), false);

  return state;
}

app_state add_error_window(app_state state, const settings& s)
  {
  state = *command_new_window(state, 0, s);
  int64_t buffer_id = state.buffers.size()-1;
  int64_t command_id = state.buffers.size()-2;
  state.buffers[buffer_id].buffer.name = "+Errors";
  state.buffers[command_id].buffer.name = "+Errors";
  state.buffers[command_id].buffer.content = to_text(make_command_text(state, command_id, s));
  return state;
  }
  
app_state add_error_text(app_state state, const std::string& errortext, const settings& s)
  {
  std::string error_filename("+Errors");
  int64_t buffer_id = -1;
  for (const auto& w : state.windows)
    {
    if (w.wt == wt_normal)
      {
      const auto& f = state.buffers[w.buffer_id].buffer;
      if (f.name == error_filename)
        buffer_id = w.buffer_id;
      }
    }

  if (buffer_id < 0)
    {
    state = add_error_window(state, s);
    buffer_id = state.buffers.size() - 1;
    }
  auto active = state.active_buffer;
  state.active_buffer = buffer_id;
  get_active_buffer(state).pos = get_last_position(get_active_buffer(state));

  get_active_buffer(state) = insert(get_active_buffer(state), errortext, convert(s));

  state.active_buffer = active;
  return state;

  }


std::optional<app_state> command_delete_window(app_state state, uint32_t buffer_id, const settings& s) {
  for (uint32_t i = 0; i < state.g.columns.size(); ++i)
    {
    auto& c = state.g.columns[i];
    for (uint32_t j = 0; j < c.items.size(); ++j)
      {
      auto ci = c.items[j];
      auto& wp = state.window_pairs[ci.window_pair_id];
      if (state.windows[wp.command_window_id].buffer_id == buffer_id || state.windows[wp.window_id].buffer_id == buffer_id)
        {
        auto& w = state.windows[wp.window_id];
        auto& f = state.buffers[w.buffer_id];
        if (f.buffer.modification_mask == 1)
          {
          f.buffer.modification_mask = 2;
          std::stringstream str;
          str << f.buffer.name << " modified";
          return add_error_text(state, str.str(), s);
          }
        else
          {
          //invalidate_column_item(state, i, j);
          c.items.erase(c.items.begin() + j);

          int64_t f1 = w.buffer_id;
          int64_t f2 = state.windows[wp.command_window_id].buffer_id;
          if (f1 > f2)
            std::swap(f1, f2);
            
          state = *command_kill(state, f1, s);
          state = *command_kill(state, f2, s);

          state.buffers.erase(state.buffers.begin() + f2);
          state.buffers.erase(state.buffers.begin() + f1);

          int64_t w1 = state.buffer_id_to_window_id[f1];
          int64_t w2 = state.buffer_id_to_window_id[f2];
          if (w1 > w2)
            std::swap(w1, w2);

          state.buffer_id_to_window_id.erase(state.buffer_id_to_window_id.begin() + f2);
          state.buffer_id_to_window_id.erase(state.buffer_id_to_window_id.begin() + f1);

          auto wp_id = ci.window_pair_id;
          state.window_pairs.erase(state.window_pairs.begin() + ci.window_pair_id);

          state.windows.erase(state.windows.begin() + w2);
          state.windows.erase(state.windows.begin() + w1);
          for (auto& buff : state.buffers)
            {
            if (buff.buffer_id > f2)
              --buff.buffer_id;
            if (buff.buffer_id > f1)
              --buff.buffer_id;
            }
            
          for (auto& win : state.windows)
            {
            if (win.buffer_id > f2)
              --win.buffer_id;
            if (win.buffer_id > f1)
              --win.buffer_id;
            }
          for (auto& fid2winid : state.buffer_id_to_window_id)
            {
            if (fid2winid > w2)
              --fid2winid;
            if (fid2winid > w1)
              --fid2winid;
            }
          for (auto& winp : state.window_pairs)
            {
            if (winp.command_window_id > w2)
              --winp.command_window_id;
            if (winp.command_window_id > w1)
              --winp.command_window_id;
            if (winp.window_id > w2)
              --winp.window_id;
            if (winp.window_id > w1)
              --winp.window_id;
            }
          if (state.g.topline_window_id > w2)
            --state.g.topline_window_id;
          if (state.g.topline_window_id > w1)
            --state.g.topline_window_id;
          for (auto& col : state.g.columns)
            {
            if (col.column_command_window_id > w2)
              --col.column_command_window_id;
            if (col.column_command_window_id > w1)
              --col.column_command_window_id;
            for (auto& colitem : col.items)
              {
              if (colitem.window_pair_id > wp_id)
                --colitem.window_pair_id;
              }
            }
          if (state.active_buffer == f1 || state.active_buffer == f2)
            {
            if (c.items.empty())
              state.active_buffer = 0;
            else
              {
              if (j >= c.items.size())
                j = c.items.size() - 1;
              state.active_buffer = state.windows[state.window_pairs[c.items[j].window_pair_id].window_id].buffer_id;
              }
            }
          else
            {
            if (state.active_buffer > f2)
              --state.active_buffer;
            if (state.active_buffer > f1)
              --state.active_buffer;
            }
          if (state.g.columns[i].items.empty())
            return state;
          //return optimize_column(state, state.windows[state.window_pairs[state.g.columns[i].items.back().window_pair_id].window_id].buffer_id);
          return resize_windows(state, s);
          }
        }
      }
    }
  return state;
}


std::optional<app_state> command_delete_column(app_state state, uint32_t buffer_id, const settings& s) {
  for (uint32_t i = 0; i < state.g.columns.size(); ++i)
    {
    auto& c = state.g.columns[i];
    if (state.windows[c.column_command_window_id].buffer_id == buffer_id)
      {
      std::stringstream str;
      bool show_error_window = false;
      for (const auto& ci : c.items)
        {
        auto& wp = state.window_pairs[ci.window_pair_id];
        auto& f = state.buffers[state.windows[wp.window_id].buffer_id].buffer;
        if (f.modification_mask==1)
          {
          show_error_window = true;
          str << f.name << " modified\n";
          f.modification_mask = 2;
          }
        }
      if (show_error_window)
        {
        return add_error_text(state, str.str(), s);
        }
      else
        {
        int sz = c.items.size();
        for (int j = sz - 1; j >= 0; --j)
          {
          auto ci = state.g.columns[i].items[j];
          int64_t id = state.windows[state.window_pairs[ci.window_pair_id].window_id].buffer_id;
          state = *command_delete_window(state, id, s);
          }
        double right = state.g.columns[i].right;
        if (i)
          state.g.columns[i - 1].right = right;
        else if (state.g.columns.size() > 1)
          state.g.columns[1].left = 0.0;
        state.g.columns.erase(state.g.columns.begin() + i);
        return resize_windows(state, s);
        }
      }
    }
  return state;
}
  
std::optional<app_state> command_new_column(app_state state, uint32_t, const settings& s) {
  int rows, cols;
  getmaxyx(stdscr, rows, cols);

  uint32_t id = (uint32_t)state.buffers.size();
  buffer_data bd = make_empty_buffer_data();
  bd.buffer_id = id;
  bd.buffer = insert(bd.buffer, "New Delcol", convert(s), false);
  state.buffers.push_back(bd);

  column c;
  c.left = 0.0;
  if (!state.g.columns.empty())
    {
    if (state.g.columns.back().right == 1.0)
      state.g.columns.back().right = (state.g.columns.back().right - state.g.columns.back().left) / 2.0 + state.g.columns.back().left;
    c.left = state.g.columns.back().right;
    }
  c.right = 1.0;
  window w = make_window(id, 0, 1, cols, 1, e_window_type::wt_column_command);
  uint32_t window_id = (uint32_t)state.windows.size();
  state.windows.push_back(w);
  state.buffer_id_to_window_id.push_back(window_id);

  c.column_command_window_id = window_id;
  state.g.columns.push_back(c);
  return resize_windows(state, s);
}

std::optional<app_state> command_new_window(app_state state, uint32_t buffer_id, const settings& s) {
  if (state.g.columns.empty())
    state = *command_new_column(state, buffer_id, s);
  
  uint32_t column_id = get_column_id(state, buffer_id);
  
  if (column_id == get_column_id(state, state.active_buffer)) // if the active file is in the column where we clicked on "New", then make new window below active window
    {
    buffer_id = state.active_buffer;
    }
  assert(column_id != 0xffffffff);
  assert(!state.g.columns.empty());
  
  int rows, cols;
  getmaxyx(stdscr, rows, cols);
  
  uint32_t command_id = (uint32_t)state.buffers.size();
  buffer_data bd = make_empty_buffer_data();
  bd.buffer_id = command_id;
  state.buffers.push_back(bd);
  window cw = make_window(command_id, 0, 1, cols, 1, e_window_type::wt_command);
  uint32_t command_window_id = (uint32_t)state.windows.size();
  state.windows.push_back(cw);
  state.buffer_id_to_window_id.push_back(command_window_id);
  
  uint32_t editor_id = (uint32_t)state.buffers.size();
  buffer_data bd2 = make_empty_buffer_data();
  bd2.buffer_id = editor_id;
  //bd2.buffer = insert(bd2.buffer, "Type here", convert(s), false);
  state.buffers.push_back(bd2);
  window w = make_window(editor_id, 0, 1, cols, 1, e_window_type::wt_normal);
  uint32_t window_id = (uint32_t)state.windows.size();
  state.windows.push_back(w);
  state.buffer_id_to_window_id.push_back(window_id);
  
  window_pair wp;
  wp.command_window_id = command_window_id;
  wp.window_id = window_id;
  state.window_pairs.push_back(wp);
  
  state.buffers[command_id].buffer = insert(state.buffers[command_id].buffer, make_command_text(state, command_id, s), convert(s), false);
  
  column_item ci;
  ci.column_id = column_id;
  ci.top_layer = 0.0;
  ci.bottom_layer = 1.0;
  ci.window_pair_id = (uint32_t)(state.window_pairs.size() - 1);
  if (!state.g.columns[column_id].items.empty())
    {
    auto pos = state.g.columns[column_id].items.size();
    for (size_t k = 0; k < state.g.columns[column_id].items.size(); ++k) // look for window with file_id == id == active_file
      {
      if (state.windows[state.window_pairs[state.g.columns[column_id].items[k].window_pair_id].window_id].buffer_id == buffer_id)
        {
        state.g.columns[column_id].items[k].bottom_layer = (state.g.columns[column_id].items[k].bottom_layer + state.g.columns[column_id].items[k].top_layer)*0.5;
        ci.top_layer = state.g.columns[column_id].items[k].bottom_layer;
        pos = k + 1;
        break;
        }
      }

    state.g.columns[column_id].items.insert(state.g.columns[column_id].items.begin() + pos, ci);
    }
  else
    state.g.columns[column_id].items.push_back(ci);
    
  state.active_buffer = editor_id;
    
  return resize_windows(state, s);
}

void get_active_window_rect_for_editing(int& offset_x, int& offset_y, int& rows, int& cols, const app_state& state, const settings& s)
  {
  auto window_id = state.buffer_id_to_window_id[state.buffers[state.active_buffer].buffer_id];
  get_window_edit_range(offset_x, offset_y, cols, rows, state.buffers[state.active_buffer].scroll_row, state.windows[window_id], s);
  }

void get_active_window_size_for_editing(int& rows, int& cols, const app_state& state, const settings& s)
  {
  int offset_x, offset_y;
  get_active_window_rect_for_editing(offset_x, offset_y, rows, cols, state, s);
  }

app_state check_scroll_position(app_state state, const settings& s)
  {
  int rows, cols;
  get_active_window_size_for_editing(rows, cols, state, s);
  if (get_active_scroll_row(state) > get_active_buffer(state).pos.row)
    get_active_scroll_row(state) = get_active_buffer(state).pos.row;
  else
    {
    if (s.wrap)
      {
      auto senv = convert(s);
      int64_t actual_rows = 0;
      int r = 0;
      for (; r < rows; ++r)
        {
        if (get_active_scroll_row(state) + r >= get_active_buffer(state).content.size())
          break;
        actual_rows += wrapped_line_rows(get_active_buffer(state).content[get_active_scroll_row(state) + r], cols, rows, senv);
        if (actual_rows >= rows)
          break;
        }
      int64_t my_row = 0;
      if (get_active_buffer(state).pos.row < get_active_buffer(state).content.size())
        my_row = wrapped_line_rows(get_active_buffer(state).content[get_active_buffer(state).pos.row], cols, rows, senv);
      if (get_active_scroll_row(state) + r < get_active_buffer(state).pos.row + my_row - 1)
        {
        get_active_scroll_row(state) = get_active_buffer(state).pos.row;
        r = 0;
        actual_rows = my_row;
        for (; r < rows; ++r)
          {
          if (get_active_scroll_row(state) == 0)
            break;
          actual_rows += wrapped_line_rows(get_active_buffer(state).content[get_active_scroll_row(state)- 1], cols, rows, senv);
          if (actual_rows <= rows)
            --get_active_scroll_row(state);
          else
            break;
          }
        }
      }
    else if (get_active_scroll_row(state) + rows <= get_active_buffer(state).pos.row)
      {
      get_active_scroll_row(state) = get_active_buffer(state).pos.row - rows + 1;
      }
    }
  return state;
  }
  
app_state check_operation_scroll_position(app_state state, const settings& s)
  {
  int64_t lastrow = (int64_t)state.operation_buffer.content.size() - 1;
  if (lastrow < 0)
    lastrow = 0;

  if (state.operation_scroll_row > lastrow)
    state.operation_scroll_row = lastrow ;
  if (state.operation_scroll_row < 0)
    state.operation_scroll_row = 0;
  return state;
  }
  
app_state check_operation_buffer(app_state state)
  {
  if (state.operation_buffer.content.size() > 1)
    state.operation_buffer.content = state.operation_buffer.content.take(1);
  state.operation_buffer.pos.row = 0;
  return state;
  }
  

app_state check_pipes(bool& modifications, app_state state, const settings& s)
  {
  modifications = false;
  if (state.buffers[state.active_buffer].bt != bt_piped)
    return state;
#ifdef _WIN32
  std::string text = jtk::read_from_pipe(state.buffers[state.active_buffer].process, 10);
#else
  std::string text = jtk::read_from_pipe(state.buffers[state.active_buffer].process.data(), 10);
#endif
  if (text.empty())
    return state;
  modifications = true;
  get_active_buffer(state).pos = get_last_position(get_active_buffer(state));
  get_active_buffer(state) = insert(get_active_buffer(state), text, convert(s));
  auto last_line = get_active_buffer(state).content.back();
  state.buffers[state.active_buffer].piped_prompt = std::wstring(last_line.begin(), last_line.end());
  return check_scroll_position(state, s);
  }

app_state cancel_selection(app_state state)
  {
  if (!keyb_data.selecting)
    {
    if (state.operation == op_editing)
      get_active_buffer(state) = clear_selection(get_active_buffer(state));
    else
      state.operation_buffer = clear_selection(state.operation_buffer);
    }
  return state;
  }
  
app_state move_left_editor(app_state state, const settings& s)
  {
  state = cancel_selection(state);
  get_active_buffer(state) = move_left(get_active_buffer(state), convert(s));
  return check_scroll_position(state, s);
  }

app_state move_left_operation(app_state state)
  {
  state = cancel_selection(state);
  if (state.operation_buffer.content.empty())
    return state;
  position actual = get_actual_position(state.operation_buffer);
  if (actual.col > 0)
    state.operation_buffer.pos.col = actual.col - 1;
  state.operation_buffer.pos.row = 0;
  return state;
  }

app_state move_left(app_state state, const settings& s)
  {
  if (state.operation == op_editing)
    return move_left_editor(state, s);
  else
    return move_left_operation(state);
  }

app_state move_right_editor(app_state state, const settings& s)
  {
  state = cancel_selection(state);
  get_active_buffer(state) = move_right(get_active_buffer(state), convert(s));
  return check_scroll_position(state, s);
  }

app_state move_right_operation(app_state state)
  {
  state = cancel_selection(state);
  if (state.operation_buffer.content.empty())
    return state;
  if (state.operation_buffer.pos.col < (int64_t)state.operation_buffer.content[0].size())
    ++state.operation_buffer.pos.col;
  state.operation_buffer.pos.row = 0;
  return state;
  }

app_state move_right(app_state state, const settings& s)
  {
  if (state.operation == op_editing)
    return move_right_editor(state, s);
  else
    return move_right_operation(state);
  }

app_state move_up_editor(app_state state, const settings& s)
  {
  state = cancel_selection(state);
  get_active_buffer(state) = move_up(get_active_buffer(state), convert(s));
  return check_scroll_position(state, s);
  }

app_state move_up_operation(app_state state)
  {
  state = cancel_selection(state);
  if (state.operation_scroll_row > 0)
    --state.operation_scroll_row;
  return state;
  }

app_state move_up(app_state state, const settings& s)
  {
  if (state.operation == op_editing)
    return move_up_editor(state, s);
  return state;
  }

app_state move_down_editor(app_state state, const settings& s)
  {
  state = cancel_selection(state);
  get_active_buffer(state) = move_down(get_active_buffer(state), convert(s));
  return check_scroll_position(state, s);
  }

app_state move_down_operation(app_state state, const settings& s)
  {
  state = cancel_selection(state);
  ++state.operation_scroll_row;
  return check_operation_scroll_position(state, s);
  }

app_state move_down(app_state state, const settings& s)
  {
  if (state.operation == op_editing)
    return move_down_editor(state, s);
  return state;
  }

app_state move_page_up_editor(app_state state, const settings& s)
  {
  state = cancel_selection(state);
  int rows, cols;
  get_active_window_size_for_editing(rows, cols, state, s);

  get_active_scroll_row(state) -= rows - 1;
  if (get_active_scroll_row(state) < 0)
    get_active_scroll_row(state) = 0;

  get_active_buffer(state) = move_page_up(get_active_buffer(state), rows - 1, convert(s));

  return check_scroll_position(state, s);
  }

app_state move_page_up(app_state state, const settings& s)
  {
  if (state.operation == op_editing)
    return move_page_up_editor(state, s);
  return state;
  }

app_state move_page_down_editor(app_state state, const settings& s)
  {
  state = cancel_selection(state);
  int rows, cols;
  get_active_window_size_for_editing(rows, cols, state, s);
  get_active_scroll_row(state) += rows - 1;
  if (get_active_scroll_row(state) + rows >= get_active_buffer(state).content.size())
    get_active_scroll_row(state)= (int64_t)get_active_buffer(state).content.size() - rows + 1;
  if (get_active_scroll_row(state) < 0)
    get_active_scroll_row(state) = 0;
  get_active_buffer(state) = move_page_down(get_active_buffer(state), rows - 1, convert(s));
  return check_scroll_position(state, s);
  }


app_state move_page_down(app_state state, const settings& s)
  {
  if (state.operation == op_editing)
    return move_page_down_editor(state, s);
  return state;
  }

app_state move_home_editor(app_state state, const settings& s)
  {
  state = cancel_selection(state);
  get_active_buffer(state) = move_home(get_active_buffer(state), convert(s));
  return state;
  }

app_state move_home_operation(app_state state, const settings& s)
  {
  state = cancel_selection(state);
  state.operation_buffer = move_home(state.operation_buffer, convert(s));
  return state;
  }

app_state move_home(app_state state, const settings& s)
  {
  if (state.operation == op_editing)
    return move_home_editor(state, s);
  else
    return move_home_operation(state, s);
  }

app_state move_end_editor(app_state state, const settings& s)
  {
  state = cancel_selection(state);
  get_active_buffer(state) = move_end(get_active_buffer(state), convert(s));
  return state;
  }

app_state move_end_operation(app_state state, const settings& s)
  {
  state = cancel_selection(state);
  if (state.operation_buffer.content.empty())
    return state;

  state.operation_buffer = move_end(state.operation_buffer, convert(s));
  return state;
  }

app_state move_end(app_state state, const settings& s)
  {
  if (state.operation == op_editing)
    return move_end_editor(state, s);
  else
    return move_end_operation(state, s);
  }

app_state text_input_standard(app_state state, const char* txt, const settings& s)
  {
  std::string t(txt);
  get_active_buffer(state) = insert(get_active_buffer(state), t, convert(s));
  return check_scroll_position(state, s);
  }

app_state text_input_operation(app_state state, const char* txt, settings& s)
  {
  std::string t(txt);
  state.operation_buffer = insert(state.operation_buffer, t, convert(s));
  if (state.operation == op_incremental_search)
    {
    if (get_active_buffer(state).start_selection != std::nullopt && *get_active_buffer(state).start_selection < get_active_buffer(state).pos)
      {
      get_active_buffer(state).pos = *get_active_buffer(state).start_selection;
      }
    get_active_buffer(state).start_selection = std::nullopt;
    get_active_buffer(state) = find_text(get_active_buffer(state), state.operation_buffer.content);
    s.last_find = to_string(state.operation_buffer.content);
    state = check_scroll_position(state, s);
    }
  return check_operation_buffer(state);
  }

app_state text_input(app_state state, const char* txt, settings& s)
  {
  if (state.operation == op_editing)
    return text_input_standard(state, txt, s);
  else
    return text_input_operation(state, txt, s);
  }

app_state backspace_editor(app_state state, const settings& s)
  {
  get_active_buffer(state) = erase(get_active_buffer(state), convert(s));
  return check_scroll_position(state, s);
  }

app_state backspace_operation(app_state state, const settings& s)
  {
  state.operation_buffer = erase(state.operation_buffer, convert(s));
  return check_operation_buffer(state);
  }

app_state backspace(app_state state, const settings& s)
  {
  if (state.operation == op_editing)
    return backspace_editor(state, s);
  else
    return backspace_operation(state, s);
  }

app_state tab_editor(app_state state, const settings& s)
  {
  std::string t("\t");
  get_active_buffer(state) = insert(get_active_buffer(state), t, convert(s));
  return check_scroll_position(state, s);
  }

app_state tab_operation(app_state state, const settings& s)
  {
  std::string t("\t");
  state.operation_buffer = insert(state.operation_buffer, t, convert(s));
  return state;
  }

app_state tab(app_state state, const settings& s)
  {
  if (state.operation == op_editing)
    return tab_editor(state, s);
  else
    return tab_operation(state, s);
  }

app_state spaced_tab_editor(app_state state, int tab_width, const settings &s)
  {
  std::string t;
  auto pos = get_actual_position(get_active_buffer(state));
  int nr_of_spaces = tab_width - (pos.col % tab_width);
  for (int i = 0; i < nr_of_spaces; ++i)
    t.push_back(' ');
  get_active_buffer(state) = insert(get_active_buffer(state), t, convert(s));
  return check_scroll_position(state, s);
  }

app_state spaced_tab_operation(app_state state, int tab_width, const settings& s)
  {
  std::string t;
  auto pos = get_actual_position(state.operation_buffer);
  int nr_of_spaces = tab_width - (pos.col % tab_width);
  for (int i = 0; i < nr_of_spaces; ++i)
    t.push_back(' ');
  state.operation_buffer = insert(state.operation_buffer, t, convert(s));
  return state;
  }

app_state spaced_tab(app_state state, int tab_width, const settings& s)
  {
  if (state.operation == op_editing)
    return spaced_tab_editor(state, tab_width, s);
  else
    return spaced_tab_operation(state, tab_width, s);
  }

app_state del_editor(app_state state, const settings& s)
  {
  get_active_buffer(state) = erase_right(get_active_buffer(state), convert(s));
  return check_scroll_position(state, s);
  }

app_state del_operation(app_state state, const settings& s)
  {
  state.operation_buffer = erase_right(state.operation_buffer, convert(s));
  return check_operation_buffer(state);
  }

app_state del(app_state state, const settings& s)
  {
  if (state.operation == op_editing)
    return del_editor(state, s);
  else
    return del_operation(state, s);
  }

app_state ret_editor(app_state state, settings& s)
  {
  if (state.buffers[state.active_buffer].bt == bt_piped && get_active_buffer(state).pos.row == (int64_t)get_active_buffer(state).content.size() - 1)
    {
    line ln = get_active_buffer(state).content[get_active_buffer(state).pos.row];
    std::wstring wcmd(ln.begin(), ln.end());
    size_t find_prompt = wcmd.find(state.buffers[state.active_buffer].piped_prompt);
    if (find_prompt != std::wstring::npos)
      {
      wcmd = wcmd.substr(find_prompt + state.buffers[state.active_buffer].piped_prompt.size());
      }
    wcmd.erase(std::remove(wcmd.begin(), wcmd.end(), '\r'), wcmd.end());
    std::string cmd = jtk::convert_wstring_to_string(wcmd);
    cmd.push_back('\n');
#ifdef _WIN32
    jtk::send_to_pipe(state.buffers[state.active_buffer].process, cmd.c_str());
#else
    jtk::send_to_pipe(state.buffers[state.active_buffer].process.data(), cmd.c_str());
#endif
    get_active_buffer(state).pos = get_last_position(get_active_buffer(state));
    get_active_buffer(state)= insert(get_active_buffer(state), "\n", convert(s));
    bool modifications;
    state = check_pipes(modifications, state, s);
    return check_scroll_position(state, s);
    }
  else
    {
    std::string indentation("\n");
    indentation.append(get_row_indentation_pattern(get_active_buffer(state), get_active_buffer(state).pos));
    return text_input(state, indentation.c_str(), s);
    }
  }

std::optional<app_state> ret_operation(app_state state, settings& s) {
  return state;
}

std::optional<app_state> ret(app_state state, settings& s)
  {
  if (state.operation == op_editing)
    return ret_editor(state, s);
  else
    return ret_operation(state, s);
  }
  
std::optional<app_state> stop_selection(app_state state)
  {
  if (keyb_data.selecting)
    {
    keyb_data.selecting = false;
    }
  return state;
  }


std::optional<app_state> move_editor_window_up_down(app_state state, int steps, const settings& s)
  {
  int rows, cols;
  get_active_window_size_for_editing(rows, cols, state, s);
  get_active_scroll_row(state) += steps;
  int64_t lastrow = (int64_t)get_active_buffer(state).content.size() - 1;
  if (lastrow < 0)
    {
    get_active_scroll_row(state) = 0;
    return state;
    }
  if (get_active_scroll_row(state) < 0)
    get_active_scroll_row(state) = 0;
  if (s.wrap)
    {
    auto senv = convert(s);
    int64_t actual_rows = 0;
    int r = 0;
    for (; r < rows; ++r)
      {
      if (get_active_scroll_row(state)+ r >= get_active_buffer(state).content.size())
        break;
      actual_rows += wrapped_line_rows(get_active_buffer(state).content[get_active_scroll_row(state) + r], cols, rows, senv);
      if (actual_rows >= rows)
        break;
      }
    if (get_active_scroll_row(state) + r > lastrow)
      {
      int64_t my_row = wrapped_line_rows(get_active_buffer(state).content[lastrow], cols, rows, senv);
      get_active_scroll_row(state) = lastrow;
      r = 0;
      actual_rows = my_row;
      for (; r < rows; ++r)
        {
        if (get_active_scroll_row(state) == 0)
          break;
        actual_rows += wrapped_line_rows(get_active_buffer(state).content[get_active_scroll_row(state) - 1], cols, rows, senv);
        if (actual_rows <= rows)
          --get_active_scroll_row(state);
        else
          break;
        }
      }
    }
  else
    {
    if (get_active_scroll_row(state) + rows > lastrow + 1)
      get_active_scroll_row(state) = lastrow - rows + 1;
    }
  if (get_active_scroll_row(state) < 0)
    get_active_scroll_row(state) = 0;
  return state;
  }
  
  
screen_ex_pixel find_mouse_text_pick(int x, int y)
  {
  int rows, cols;
  getmaxyx(stdscr, rows, cols);

  int x0 = x;
  screen_ex_pixel p = get_ex(y, x);
  while (x > 0 && (p.type != SET_TEXT_EDITOR && p.type != SET_TEXT_COMMAND))
    p = get_ex(y, --x);
  if (p.type != SET_TEXT_EDITOR && p.type != SET_TEXT_COMMAND)
    {
    x = x0;
    while (x < cols && (p.type != SET_TEXT_EDITOR && p.type != SET_TEXT_COMMAND))
      p = get_ex(y, ++x);
    }
  return p;
  }

screen_ex_pixel find_mouse_operation_pick(int x, int y)
  {
  int rows, cols;
  getmaxyx(stdscr, rows, cols);

  int y0 = y;
  screen_ex_pixel p = get_ex(y, x);
  while (y < rows && (p.type != SET_TEXT_OPERATION))
    p = get_ex(++y, x);
  if (p.type != SET_TEXT_OPERATION)
    {
    y = y0;
    while (y > 0 && (p.type != SET_TEXT_OPERATION))
      p = get_ex(--y, x);
    }
  return p;
  }

std::optional<app_state> mouse_motion(app_state state, int x, int y, const settings& s)
  {
  if (mouse.left_button_down)
    mouse.left_dragging = true;

  if (mouse.left_dragging)
    {
    if (mouse.left_drag_start.type == screen_ex_type::SET_PLUS) {
      auto window_id = state.buffer_id_to_window_id[mouse.left_drag_start.buffer_id];
      if (window_id == state.g.topline_window_id) {
        state.windows[window_id].rows = y;
        if (state.windows[window_id].rows <= 0)
          state.windows[window_id].rows = 1;
      } else if (state.windows[window_id].wt == e_window_type::wt_column_command || state.windows[window_id].wt == e_window_type::wt_command ) {
        int y0 = state.windows[window_id].y;
        state.windows[window_id].rows = y-y0;
        if (state.windows[window_id].rows <= 0)
          state.windows[window_id].rows = 1;
        auto column_id = get_column_id(state, mouse.left_drag_start.buffer_id);
        if (state.g.columns.size()>column_id+1) {
          int x0 = state.windows[window_id].x;
          int x1 = x;
          if (x1 - x0 < 1)
            x1 = x0+1;
          auto& c = state.g.columns[column_id];
          int rows, cols;
          getmaxyx(stdscr, rows, cols);
          c.right = (double)(x1+1) / (double)cols;
          state.g.columns[column_id+1].left = c.right;
        }
      }
      state = resize_windows(state, s);
    } else {
      auto p = find_mouse_text_pick(x, y);
      if (p.type == mouse.left_drag_start.type)
        {
        mouse.left_drag_end = p;
        if (mouse.left_drag_start.type == SET_TEXT_EDITOR || mouse.left_drag_start.type == SET_TEXT_COMMAND)
          {
          //state.buffer.pos = p.pos;
          get_active_buffer(state) = update_position(get_active_buffer(state), p.pos, convert(s));
          }
        }

      p = find_mouse_operation_pick(x, y);
      if (mouse.left_drag_start.type == SET_TEXT_OPERATION && mouse.left_drag_start.type == p.type)
        {
        //state.operation_buffer.pos.col = p.pos.col;
        state.operation_buffer = update_position(state.operation_buffer, position(0, p.pos.col), convert(s));
        }
      }
    }
  return state;
  }

bool valid_char_for_cpp_word_selection(wchar_t ch)
  {
  bool valid = false;
  valid |= (ch >= 48 && ch <= 57); // [0 : 9]
  valid |= (ch >= 97 && ch <= 122); // [a : z]
  valid |= (ch >= 65 && ch <= 90); // [A : Z]
  valid |= (ch == 95); // _  c++: naming
  return valid;
  }
  
bool valid_char_for_scheme_word_selection(wchar_t ch)
  {
  bool valid = false;
  valid |= (ch >= 48 && ch <= 57); // [0 : 9]
  valid |= (ch >= 97 && ch <= 122); // [a : z]
  valid |= (ch >= 65 && ch <= 90); // [A : Z]
  valid |= (ch == 33); // !  scheme: vector-set!
  valid |= (ch == 35); // #
  valid |= (ch == 37); // %
  valid |= (ch == 42); // *  scheme: let*
  valid |= (ch == 43); // +
  valid |= (ch == 45); // -  scheme: list-ref
  valid |= (ch == 47); // /
  valid |= (ch == 60); // <
  valid |= (ch == 62); // >
  valid |= (ch == 61); // =
  valid |= (ch == 63); // ?  scheme: eq?
  valid |= (ch == 95); // _  c++: naming
  return valid;
  }

bool valid_char_for_word_selection(wchar_t ch, bool scheme)
  {
  return scheme ? valid_char_for_scheme_word_selection(ch) : valid_char_for_cpp_word_selection(ch);
  }

std::pair<int64_t, int64_t> get_word_from_position(file_buffer fb, position pos)
  {
  auto ext = jtk::get_extension(fb.name);
  std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return (unsigned char)std::tolower(c); });
  bool scheme = ext == std::string("scm");
  std::pair<int64_t, int64_t> selection(-1, -1);
  if (pos.row >= fb.content.size())
    return selection;

  line ln = fb.content[pos.row];
  if (pos.col >= ln.size())
    return selection;

  const auto it0 = ln.begin();
  auto it = ln.begin() + pos.col;
  auto it2 = it;
  auto it_end = ln.end();
  if (it == it_end)
    --it;
  while (it > it0)
    {
    if (!valid_char_for_word_selection(*it, scheme))
      break;
    --it;
    }
  if (!valid_char_for_word_selection(*it, scheme))
    ++it;
  while (it2 < it_end)
    {
    if (!valid_char_for_word_selection(*it2, scheme))
      break;
    ++it2;
    }
  if (it2 <= it)
    return selection;
  int64_t p1 = (int64_t)std::distance(it0, it);
  int64_t p2 = (int64_t)std::distance(it0, it2);

  selection.first = p1;
  selection.second = p2 - 1;

  return selection;
  }

std::optional<app_state> select_word(app_state state, int x, int y, const settings& s)
  {
  std::pair<int64_t, int64_t> selection(-1, -1);
  auto p = get_ex(y, x);
  
  if (p.type == SET_TEXT_EDITOR || p.type == SET_TEXT_COMMAND)
    {
    selection = get_word_from_position(state.buffers[p.buffer_id].buffer, p.pos);
    }
  if (selection.first >= 0 && selection.second >= 0)
    {
    state.buffers[p.buffer_id].buffer.start_selection->row = p.pos.row;
    state.buffers[p.buffer_id].buffer.start_selection->col = selection.first;
    state.buffers[p.buffer_id].buffer.pos.row = p.pos.row;
    state.buffers[p.buffer_id].buffer.pos.col = selection.second;
    }
  else
    {
    p = find_mouse_text_pick(x, y);
    if (p.type == SET_TEXT_EDITOR || p.type == SET_TEXT_COMMAND)
      state.buffers[p.buffer_id].buffer = update_position(state.buffers[p.buffer_id].buffer, p.pos, convert(s));
    }
  return state;
  }
  
  

const auto executable_commands = std::map<std::wstring, std::function<std::optional<app_state>(app_state, int64_t, settings&)>>
  {
  {L"Exit", command_exit},
  {L"New", command_new_window},
  {L"Newcol", command_new_column},
  {L"Delcol", command_delete_column},
  {L"Del", command_delete_window}
  };

const auto executable_commands_with_parameters = std::map<std::wstring, std::function<std::optional<app_state>(app_state, std::wstring&, settings&)>>
  {
  //{L"Tab", command_tab},
  //{L"Win", command_piped_win}
  };


bool valid_command_char(wchar_t ch)
  {
  return (ch != L' ' && ch != L'\n' && ch != L'\r' && ch != L'\t');
  }

std::wstring clean_command(std::wstring command)
  {
  while (!command.empty() && (!valid_command_char(command.back())))
    command.pop_back();
  while (!command.empty() && (!valid_command_char(command.front())))
    command.erase(command.begin());
  return command;
  }

std::wstring find_command(file_buffer fb, position pos, const settings& s)
  {
  auto senv = convert(s);
  auto cursor = get_actual_position(fb);
  if (has_nontrivial_selection(fb, senv) && in_selection(fb, pos, cursor, fb.pos, fb.start_selection, fb.rectangular_selection, senv))
    {
    auto txt = get_selection(fb, senv);
    std::wstring out = to_wstring(txt);
    return clean_command(out);
    }
  if (fb.content.size() <= pos.row)
    return std::wstring();
  auto ln = fb.content[pos.row];
  if (ln.size() <= pos.col)
    return std::wstring();
  int x0 = pos.col;
  int x1 = pos.col;
  while (x0 > 0 && valid_command_char(ln[x0]))
    --x0;
  while (x1 < ln.size() && valid_command_char(ln[x1]))
    ++x1;
  if (!valid_command_char(ln[x0]))
    ++x0;
  if (x0 > x1)
    return std::wstring();
  std::wstring out(ln.begin() + x0, ln.begin() + x1);
  return clean_command(out);
  }
  
void split_command(std::wstring& first, std::wstring& remainder, const std::wstring& command)
  {
  first.clear();
  remainder.clear();
  auto pos = command.find_first_of(L' ');
  if (pos == std::wstring::npos)
    {
    first = command;
    return;
    }

  auto pos_quote = command.find_first_of(L'"');
  if (pos < pos_quote)
    {
    first = command.substr(0, pos);
    remainder = command.substr(pos);
    return;
    }

  auto pos_quote_2 = pos_quote + 1;
  while (pos_quote_2 < command.size() && command[pos_quote_2] != L'"')
    ++pos_quote_2;
  if (pos_quote_2 + 1 == command.size())
    {
    first = command;
    return;
    }
  first = command.substr(0, pos_quote_2 + 1);
  remainder = command.substr(pos_quote_2 + 1);
  }

char** alloc_arguments(const std::string& path, const std::vector<std::string>& parameters)
  {
  char** argv = new char*[parameters.size() + 2];
  argv[0] = const_cast<char*>(path.c_str());
  for (int j = 0; j < parameters.size(); ++j)
    argv[j + 1] = const_cast<char*>(parameters[j].c_str());
  argv[parameters.size() + 1] = nullptr;
  return argv;
  }

void free_arguments(char** argv)
  {
  delete[] argv;
  }

app_state execute_external(app_state state, const std::string& file_path, const std::vector<std::string>& parameters)
  {
  jtk::active_folder af(jtk::get_folder(get_active_buffer(state).name).c_str());

  char** argv = alloc_arguments(file_path, parameters);
#ifdef _WIN32
  void* process = nullptr;
#else
  pid_t process;
#endif
  int err = jtk::run_process(file_path.c_str(), argv, nullptr, &process);
  free_arguments(argv);
  if (err != 0)
    {
    std::string error_message = "Could not create child process";
    state.message = string_to_line(error_message);
    return state;
    }
  jtk::destroy_process(process, 0);
  return state;
  }

app_state execute_external_input(app_state state, const std::string& file_path, const std::vector<std::string>& parameters, const settings& s)
  {
  jtk::active_folder af(jtk::get_folder(get_active_buffer(state).name).c_str());

  char** argv = alloc_arguments(file_path, parameters);
#ifdef _WIN32
  void* process = nullptr;
  int err = jtk::create_pipe(file_path.c_str(), argv, nullptr, &process);
  free_arguments(argv);
  if (err != 0)
    {
    std::string error_message = "Could not create child process";
    state.message = string_to_line(error_message);
    return state;
    }
  std::string text = jtk::read_from_pipe(process, 100);
#else
  int pipefd[3];
  int err = jtk::create_pipe(file_path.c_str(), argv, nullptr, pipefd);
  free_arguments(argv);
  if (err != 0)
    {
    std::string error_message = "Could not create child process";
    state.message = string_to_line(error_message);
    return state;
    }
  std::string text = jtk::read_from_pipe(pipefd, 100);
#endif

  get_active_buffer(state) = insert(get_active_buffer(state), text, convert(s));

#ifdef _WIN32
  jtk::close_pipe(process);
#else
  jtk::close_pipe(pipefd);
#endif
  return state;
  }

app_state execute_external_output(app_state state, const std::string& file_path, const std::vector<std::string>& parameters, const settings& s)
  {
  auto woutput = to_wstring(get_selection(get_active_buffer(state), convert(s)));
  woutput.erase(std::remove(woutput.begin(), woutput.end(), '\r'), woutput.end());
  if (!woutput.empty() && woutput.back() != '\n')
    woutput.push_back('\n');
  auto output = jtk::convert_wstring_to_string(woutput);

  jtk::active_folder af(jtk::get_folder(get_active_buffer(state).name).c_str());

  char** argv = alloc_arguments(file_path, parameters);
#ifdef _WIN32
  void* process = nullptr;
  int err = jtk::create_pipe(file_path.c_str(), argv, nullptr, &process);
  free_arguments(argv);
  if (err != 0)
    {
    std::string error_message = "Could not create child process";
    state.message = string_to_line(error_message);
    return state;
    }
  int res = jtk::send_to_pipe(process, output.c_str());
  if (res != NO_ERROR)
    {
    state.message = string_to_line(std::string("Error writing to external process"));
    return state;
    }
  jtk::close_pipe(process);
#else
  int pipefd[3];
  int err = jtk::create_pipe(file_path.c_str(), argv, nullptr, pipefd);
  free_arguments(argv);
  if (err != 0)
    {
    std::string error_message = "Could not create child process";
    state.message = string_to_line(error_message);
    return state;
    }
  jtk::send_to_pipe(pipefd, output.c_str());
  jtk::close_pipe(pipefd);
#endif

  return state;
  }


app_state execute_external_input_output(app_state state, const std::string& file_path, const std::vector<std::string>& parameters, const settings& s)
  {
  auto woutput = to_wstring(get_selection(get_active_buffer(state), convert(s)));
  woutput.erase(std::remove(woutput.begin(), woutput.end(), '\r'), woutput.end());
  if (!woutput.empty() && woutput.back() != '\n')
    woutput.push_back('\n');
  auto output = jtk::convert_wstring_to_string(woutput);

  jtk::active_folder af(jtk::get_folder(get_active_buffer(state).name).c_str());

  char** argv = alloc_arguments(file_path, parameters);
#ifdef _WIN32
  void* process = nullptr;
  int err = jtk::create_pipe(file_path.c_str(), argv, nullptr, &process);
  free_arguments(argv);
  if (err != 0)
    {
    std::string error_message = "Could not create child process";
    state.message = string_to_line(error_message);
    return state;
    }
  int res = jtk::send_to_pipe(process, output.c_str());
  if (res != NO_ERROR)
    {
    state.message = string_to_line(std::string("Error writing to external process"));
    return state;
    }
  std::string text = jtk::read_from_pipe(process, 100);
#else
  int pipefd[3];
  int err = jtk::create_pipe(file_path.c_str(), argv, nullptr, pipefd);
  free_arguments(argv);
  if (err != 0)
    {
    std::string error_message = "Could not create child process";
    state.message = string_to_line(error_message);
    return state;
    }
  jtk::send_to_pipe(pipefd, output.c_str());
  std::string text = jtk::read_from_pipe(pipefd, 100);
#endif

  get_active_buffer(state) = insert(get_active_buffer(state), text, convert(s));

#ifdef _WIN32
  jtk::close_pipe(process);
#else
  jtk::close_pipe(pipefd);
#endif
  return state;
  }
  

std::optional<app_state> command_kill(app_state state, int64_t buffer_id, const settings& s)
  {
#ifdef _WIN32
  if (state.buffers[buffer_id].bt == bt_piped)
    {
    jtk::destroy_pipe(state.buffers[buffer_id].process, 9);
    state.buffers[buffer_id].process = nullptr;
    state.buffers[buffer_id].bt = bt_normal;
    }
#else
  if (state.buffers[buffer_id].bt == bt_piped)
    {
    jtk::destroy_pipe(state.buffers[buffer_id].process.data(), 9);
    state.buffers[buffer_id].process[0] = state.buffers[buffer_id].process[1] = state.buffers[buffer_id].process[2] = -1;
    state.buffers[buffer_id].bt = bt_normal;
    }
#endif
  if (!state.buffers[buffer_id].buffer.name.empty() && state.buffers[buffer_id].buffer.name[0] == '=')
    state.buffers[buffer_id].buffer.name.clear();
  return state;
  }
  
app_state start_pipe(app_state state, int64_t buffer_id, const std::string& inputfile, const std::vector<std::string>& parameters, settings& s)
  {
  state = *command_kill(state, buffer_id, s);
  //state.buffer = make_empty_buffer();
  state.buffers[buffer_id].buffer.name = "=" + inputfile;
  state.buffers[buffer_id].scroll_row = 0;
  state.operation = op_editing;
  state.buffers[buffer_id].bt = bt_piped;

  char** argv = alloc_arguments(inputfile, parameters);
#ifdef _WIN32
  state.buffers[buffer_id].process = nullptr;
  int err = jtk::create_pipe(inputfile.c_str(), argv, nullptr, &state.buffers[buffer_id].process);
  free_arguments(argv);
  if (err != 0)
    {
    std::string error_message = "Could not create child process";
    state.message = string_to_line(error_message);
    state.buffers[buffer_id].bt = bt_normal;
    return state;
    }
  std::string text = jtk::read_from_pipe(state.buffers[buffer_id].process, 100);
#else
  int err = jtk::create_pipe(inputfile.c_str(), argv, nullptr, state.buffers[buffer_id].process.data());
  free_arguments(argv);
  if (err != 0)
    {
    std::string error_message = "Could not create child process";
    state.message = string_to_line(error_message);
    state.buffers[buffer_id].bt = bt_normal;
    return state;
    }
  std::string text = jtk::read_from_pipe(state.buffers[buffer_id].process.data(), 100);
#endif

  state.buffers[buffer_id].buffer = insert(state.buffers[buffer_id].buffer, text, convert(s));
  if (!state.buffers[buffer_id].buffer.content.empty())
    {
    auto last_line = state.buffers[buffer_id].buffer.content.back();
    state.buffers[buffer_id].piped_prompt = std::wstring(last_line.begin(), last_line.end());
    }
  state.buffers[buffer_id].buffer.pos = get_last_position(state.buffers[buffer_id].buffer);
  state.active_buffer = buffer_id;
  return check_scroll_position(state, s);
  }

app_state start_pipe(app_state state, int64_t buffer_id, const std::string& inputfile, int argc, char** argv, settings& s)
  {
  std::vector<std::string> parameters;
  for (int j = 2; j < argc; ++j)
    parameters.emplace_back(argv[j]);
  return start_pipe(state, buffer_id, inputfile, parameters, s);
  }
  
std::optional<app_state> execute(app_state state, int64_t buffer_id, const std::wstring& command, settings& s)
  {
  auto it = executable_commands.find(command);
  if (it != executable_commands.end())
    {
    return it->second(state, buffer_id, s);
    }

  std::wstring cmd_id, cmd_remainder;
  split_command(cmd_id, cmd_remainder, command);
  char pipe_cmd = cmd_id[0];
  if (pipe_cmd == '!' || pipe_cmd == '<' || pipe_cmd == '>' || pipe_cmd == '|' || pipe_cmd == '=')
    {
    cmd_id.erase(cmd_id.begin());
    }
  else
    pipe_cmd = '!';
  remove_whitespace(cmd_id);
  remove_quotes(cmd_id);

  auto it2 = executable_commands_with_parameters.find(cmd_id);
  if (it2 != executable_commands_with_parameters.end())
    {
    return it2->second(state, cmd_remainder, s);
    }

  auto file_path = get_file_path(jtk::convert_wstring_to_string(cmd_id), get_active_buffer(state).name);

  if (file_path.empty())
    return state;

  std::vector<std::string> parameters;
  while (!cmd_remainder.empty())
    {
    cmd_remainder = clean_command(cmd_remainder);
    std::wstring first, rest;
    split_command(first, rest, cmd_remainder);
    bool has_quotes = remove_quotes(first);
    auto par_path = get_file_path(jtk::convert_wstring_to_string(first), get_active_buffer(state).name);
    if (par_path.empty())
      parameters.push_back(jtk::convert_wstring_to_string(first));
    else
      parameters.push_back(par_path);
#ifdef _WIN32
    if (has_quotes)
      {
      parameters.back().insert(parameters.back().begin(), '"');
      parameters.back().push_back('"');
      }
#endif
    cmd_remainder = clean_command(rest);
    }

  if (pipe_cmd == '!')
    return execute_external(state, file_path, parameters);
  else if (pipe_cmd == '|')
    return execute_external_input_output(state, file_path, parameters, s);
  else if (pipe_cmd == '<')
    return execute_external_input(state, file_path, parameters, s);
  else if (pipe_cmd == '>')
    return execute_external_output(state, file_path, parameters, s);
  else if (pipe_cmd == '=')
    return start_pipe(state, buffer_id, file_path, parameters, s);
  return state;
  }

std::optional<app_state> left_mouse_button_down(app_state state, int x, int y, bool double_click, const settings& s)
  {
  screen_ex_pixel p = get_ex(y, x);
  mouse.left_button_down = true;
  
  if (p.buffer_id == 0xffffffff)
    return state;

  state.active_buffer = p.buffer_id;

  if (p.type == SET_SCROLLBAR_EDITOR)
    {
    return state;
    }

  if (double_click)
    {
    mouse.left_button_down = false;
    return select_word(state, x, y, s);
    }

  mouse.left_drag_start = find_mouse_text_pick(x, y);
  if (mouse.left_drag_start.type == SET_TEXT_EDITOR || mouse.left_drag_start.type == SET_TEXT_COMMAND)
    {
    state.operation = op_editing;
    if (!keyb_data.selecting)
      {
      get_active_buffer(state).start_selection = mouse.left_drag_start.pos;
      get_active_buffer(state).rectangular_selection = alt_pressed();
      }
    get_active_buffer(state)= update_position(get_active_buffer(state), mouse.left_drag_start.pos, convert(s));
    //state.command_buffer = clear_selection(state.command_buffer);
    keyb_data.selecting = false;
    }
 
  mouse.left_drag_start = get_ex(y, x);
  if (mouse.left_drag_start.type == SET_TEXT_OPERATION)
    {
    if (!keyb_data.selecting)
      {
      state.operation_buffer.start_selection = mouse.left_drag_start.pos;
      state.operation_buffer.rectangular_selection = false;
      }
    state.operation_buffer = update_position(state.operation_buffer, mouse.left_drag_start.pos, convert(s));
    //state.buffer = clear_selection(state.buffer);
    keyb_data.selecting = false;
    }
  return state;
  }

std::optional<app_state> middle_mouse_button_down(app_state state, int x, int y, bool double_click, const settings& s)
  {
  mouse.middle_button_down = true;
  return state;
  }

std::optional<app_state> right_mouse_button_down(app_state state, int x, int y, bool double_click, const settings& s)
  {
  screen_ex_pixel p = get_ex(y, x);
  mouse.right_button_down = true;
  return state;
  }

std::optional<app_state> left_mouse_button_up(app_state state, int x, int y, const settings& s)
  {
  if (!mouse.left_button_down) // we come from a double click
    return state;

  bool was_dragging = mouse.left_dragging;

  mouse.left_dragging = false;
  mouse.left_button_down = false;

  auto p = get_ex(y, x);
  if (p.type == SET_SCROLLBAR_EDITOR && !was_dragging)
    {
    int offsetx, offsety, cols, rows;
    get_active_window_rect_for_editing(offsetx, offsety, rows, cols, state, s);
    double fraction = (double)(y - offsety) / (double)rows;
    int steps = (int)(fraction * rows);
    if (steps < 1)
      steps = 1;
    return move_editor_window_up_down(state, -steps, s);
    }

  //if (p.type == SET_TEXT_EDITOR)
  //  {
  //  state.buffer = update_position(state.buffer, p.pos, convert(s));
  //  }

  return state;
  }

std::optional<app_state> middle_mouse_button_up(app_state state, int x, int y, settings& s)
  {
  mouse.middle_button_down = false;

  screen_ex_pixel p = get_ex(y, x);
  
  if (p.buffer_id == 0xffffffff)
    return state;

  if (p.type == SET_SCROLLBAR_EDITOR)
    {
    int rows, cols;
    get_active_window_size_for_editing(rows, cols, state, s);
    get_active_scroll_row(state) = p.pos.row;
    int64_t lastrow = (int64_t)get_active_buffer(state).content.size() - 1;
    if (lastrow < 0)
      lastrow = 0;

    if (get_active_scroll_row(state) + rows > lastrow + 1)
      get_active_scroll_row(state) = lastrow - rows + 1;
    if (get_active_scroll_row(state) < 0)
      get_active_scroll_row(state) = 0;
    return state;
    }

  if (p.type == SET_TEXT_EDITOR || p.type == SET_TEXT_COMMAND)
    {
    // to add when implementing commands
    std::wstring command = find_command(state.buffers[p.buffer_id].buffer, p.pos, s);
    return execute(state, p.buffer_id, command, s);
    }

  if (p.type == SET_NONE)
    {
    // to add when implementing commands
    //std::wstring command = find_bottom_line_help_command(x, y);
    //return execute(state, command, s);
    }

  return state;
  }

std::optional<app_state> right_mouse_button_up(app_state state, int x, int y, settings& s)
  {
  mouse.right_button_down = false;

  screen_ex_pixel p = get_ex(y, x);
  
  if (p.buffer_id == 0xffffffff)
    return state;

  if (p.type == SET_SCROLLBAR_EDITOR)
    {
    int offsetx, offsety, cols, rows;
    get_active_window_rect_for_editing(offsetx, offsety, rows, cols, state, s);
    double fraction = (double)(y - offsety) / (double)rows;
    int steps = (int)(fraction * rows);
    if (steps < 1)
      steps = 1;
    return move_editor_window_up_down(state, steps, s);
    }

  if (p.type == SET_TEXT_EDITOR || p.type == SET_TEXT_COMMAND)
    {
    //std::wstring command = find_command(state.buffers[p.buffer_id].buffer, p.pos, s);
    //return load(state, command, s);
    }


  if (p.type == SET_NONE)
    {
    std::wstring command;
    if (y == 0) // clicking on title bar
      {
      //command = jtk::convert_string_to_wstring(jtk::get_folder(state.buffer.name));
      }
    //else
      //command = find_bottom_line_help_command(x, y);
    //return load(state, command, s);
    }
  return state;
  }

std::optional<app_state> process_input(app_state state, uint32_t buffer_id, settings& s) {
  SDL_Event event;
  auto tic = std::chrono::steady_clock::now();
  for (;;)
    {
    while (SDL_PollEvent(&event))
      {
      keyb.handle_event(event);
      switch (event.type)
        {
        case SDL_WINDOWEVENT:
        {
        if (event.window.event == SDL_WINDOWEVENT_RESIZED)
          {
          auto new_w = event.window.data1;
          auto new_h = event.window.data2;

          state.w = (new_w / font_width) * font_width;
          state.h = (new_h / font_height) * font_height;
          if (state.w != new_w || state.h != new_h)
            {
            auto flags = SDL_GetWindowFlags(pdc_window);
            if (flags & SDL_WINDOW_MAXIMIZED)
              {
              //int x, y;
              //SDL_GetWindowPosition(pdc_window, &x, &y);
              //SDL_RestoreWindow(pdc_window);
              //SDL_SetWindowPosition(pdc_window, x, y);
              state.w = new_w;
              state.h = new_h;
              }
            SDL_SetWindowSize(pdc_window, state.w, state.h);
            }
          resize_term(state.h / font_height, state.w / font_width);
          resize_term_ex(state.h / font_height, state.w / font_width);
          return resize_windows(state, s);
          }
        break;
        }
        case SDL_TEXTINPUT:
        {
        return text_input(state, event.text.text, s);
        }
        case SDL_KEYDOWN:
        {
        switch (event.key.keysym.sym)
          {
          case SDLK_LEFT: return move_left(state, s);
          case SDLK_RIGHT: return move_right(state, s);
          case SDLK_DOWN: return move_down(state, s);
          case SDLK_UP: return move_up(state, s);
          case SDLK_PAGEUP: return move_page_up(state, s);
          case SDLK_PAGEDOWN: return move_page_down(state, s);
          case SDLK_HOME: return move_home(state, s);
          case SDLK_END: return move_end(state, s);
          case SDLK_TAB: return s.use_spaces_for_tab ? spaced_tab(state, s.tab_space, s) : tab(state, s);
          case SDLK_KP_ENTER:
          case SDLK_RETURN: return ret(state, s);
          case SDLK_BACKSPACE: return backspace(state, s);
          case SDLK_DELETE:
          {
          if (shift_pressed()) // copy
            {
            //state = *command_copy_to_snarf_buffer(state, s);
            }
          return del(state, s);
          }
          }
        //return state;
        break;
        } // case SDLK_KEYUP:
        case SDL_KEYUP:
        {
        switch (event.key.keysym.sym)
          {
          case SDLK_LSHIFT:
          {
          if (keyb_data.selecting)
            return stop_selection(state);
          break;
          }
          case SDLK_RSHIFT:
          {
          if (keyb_data.selecting)
            return stop_selection(state);
          break;
          }
          }
        break;
        } // case SDLK_KEYUP:
        case SDL_MOUSEMOTION:
        {
        int x = event.motion.x / font_width;
        int y = event.motion.y / font_height;
        mouse.prev_mouse_x = mouse.mouse_x;
        mouse.prev_mouse_y = mouse.mouse_y;
        mouse.mouse_x = event.motion.x;
        mouse.mouse_y = event.motion.y;
        return mouse_motion(state, x, y, s);
        break;
        }
        case SDL_MOUSEBUTTONDOWN:
        {
        mouse.mouse_x_at_button_press = event.button.x;
        mouse.mouse_y_at_button_press = event.button.y;
        int x = event.button.x / font_width;
        int y = event.button.y / font_height;
        bool double_click = event.button.clicks > 1;
        if (event.button.button == 1)
          {
          if (ctrl_pressed())
            {
            mouse.left_button_down = false;
            mouse.right_button_down = false;
            mouse.left_dragging = false;
            return middle_mouse_button_down(state, x, y, false, s);
            }
          else
            return left_mouse_button_down(state, x, y, double_click, s);
          }
        else if (event.button.button == 2)
          return middle_mouse_button_down(state, x, y, double_click, s);
        else if (event.button.button == 3)
          {
          return right_mouse_button_down(state, x, y, double_click, s);
          }
        break;
        }
        case SDL_MOUSEBUTTONUP:
        {
        int x = event.button.x / font_width;
        int y = event.button.y / font_height;
        if (event.button.button == 1 && mouse.left_button_down)
          return left_mouse_button_up(state, x, y, s);
        else if (event.button.button == 2 && mouse.middle_button_down)
          return middle_mouse_button_up(state, x, y, s);
        else if (event.button.button == 3 && mouse.right_button_down)
          return right_mouse_button_up(state, x, y, s);
        else if (((event.button.button == 1) || (event.button.button == 3)) && mouse.middle_button_down)
          return middle_mouse_button_up(state, x, y, s);
        break;
        }
        case SDL_MOUSEWHEEL:
        {
        if (ctrl_pressed())
          {
          if (event.wheel.y > 0)
            ++pdc_font_size;
          else if (event.wheel.y < 0)
            --pdc_font_size;
          if (pdc_font_size < 1)
            pdc_font_size = 1;
          return resize_font(state, pdc_font_size, s);
          }
        else
          {
          int steps = s.mouse_scroll_steps;
          if (event.wheel.y > 0)
            steps = -steps;
          return move_editor_window_up_down(state, steps, s);
          }
        break;
        }
        case SDL_QUIT:
        {
        return command_exit(state, buffer_id, s);
        }
        } // switch (event.type)
      }
    std::this_thread::sleep_for(std::chrono::duration<double, std::milli>(5.0));
    auto toc = std::chrono::steady_clock::now();
    auto time_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(toc - tic).count();
    }
  }

app_state make_topline(app_state state, const settings& s) {
  uint32_t id = (uint32_t)state.buffers.size();
  buffer_data bd = make_empty_buffer_data();
  bd.buffer_id = id;
  bd.buffer = insert(bd.buffer, "Newcol Exit", convert(s), false);
  state.buffers.push_back(bd);
  int rows, cols;
  getmaxyx(stdscr, rows, cols);
  window w = make_window(id, 0, 0, cols, 1, e_window_type::wt_topline);
  uint32_t window_id = (uint32_t)state.windows.size();
  state.windows.push_back(w);
  state.buffer_id_to_window_id.push_back(window_id);
  state.g.topline_window_id = window_id;
  return state;
}

app_state check_update_active_command_text(app_state state, const settings& s) {
  auto buffer_id = state.active_buffer;
  if (buffer_id == 0xffffffff)
    return state;
  const auto& w = state.windows[state.buffer_id_to_window_id[buffer_id]];
  if (w.wt == e_window_type::wt_command || w.wt == e_window_type::wt_normal) {
    auto command_id = w.wt == e_window_type::wt_command ? buffer_id : buffer_id-1;
    if (should_update_command_text(state, command_id, s))
      state = update_command_text(state, command_id, s);
  }
  return state;
}
  
engine::engine(int argc, char** argv, const settings& input_settings) : s(input_settings)
  {
  pdc_font_size = s.font_size;
#ifdef _WIN32
  TTF_CloseFont(pdc_ttffont);
  pdc_ttffont = TTF_OpenFont("C:/Windows/Fonts/consola.ttf", pdc_font_size);
#elif defined(unix)
  TTF_CloseFont(pdc_ttffont);
  pdc_ttffont = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf", pdc_font_size);
#elif defined(__APPLE__)
  TTF_CloseFont(pdc_ttffont);
  pdc_ttffont = TTF_OpenFont("/System/Library/Fonts/Menlo.ttc", pdc_font_size);
#endif

  TTF_SizeText(pdc_ttffont, "W", &font_width, &font_height);
  pdc_fheight = font_height;
  pdc_fwidth = font_width;
  pdc_fthick = pdc_font_size / 20 + 1;

  state.w = s.w * font_width;
  state.h = s.h * font_height;


  s.show_line_numbers = true;

  nodelay(stdscr, TRUE);
  noecho();

  start_color();
  use_default_colors();
  init_colors(s);
  bkgd(COLOR_PAIR(default_color));


  SDL_ShowCursor(1);
  SDL_SetWindowSize(pdc_window, state.w, state.h);
  SDL_SetWindowPosition(pdc_window, s.x, s.y);
  
  resize_term(state.h / font_height, state.w / font_width);
  resize_term_ex(state.h / font_height, state.w / font_width);

  state.active_buffer = 0;
  state.operation = e_operation::op_editing;
  state = make_topline(state, s);
  state = *command_new_column(state, 0, s);
  state = *command_new_column(state, 0, s);
  state = *command_new_window(state, 1, s);

  }

engine::~engine()
  {

  }

void engine::run()
  {
  state = draw(state, s);
  SDL_UpdateWindowSurface(pdc_window);

  while (auto new_state = process_input(state, 0, s))
    {
    state = check_update_active_command_text(*new_state, s);
    state = draw(state, s);

    SDL_UpdateWindowSurface(pdc_window);
    }



  s.w = state.w / font_width;
  s.h = state.h / font_height;
  SDL_GetWindowPosition(pdc_window, &s.x, &s.y);
  }
