#include "engine.h"
#include "clipboard.h"
#include "colors.h"
#include "keyboard.h"
#include "mouse.h"
#include "pdcex.h"
#include "syntax_highlight.h"
#include "utils.h"
#include "draw.h"
#include "serialize.h"
#include "plumber.h"
#include "hex.h"
#include "edit.h"

#include <jtk/file_utils.h>
#include <jtk/pipe.h>

#include <map>
#include <functional>
#include <sstream>
#include <cctype>
#include <fstream>

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

const plumber& get_plumber()
  {
  static plumber p;
  return p;
  }

env_settings convert(const settings& s)
  {
  env_settings out;
  out.tab_space = s.tab_space;
  out.show_all_characters = s.show_all_characters;
  out.perform_syntax_highlighting = s.syntax;
  return out;
  }

buffer_data make_empty_buffer_data() {
  buffer_data bd;
  bd.buffer = make_empty_buffer();
  bd.scroll_row = 0;
#ifdef _WIN32
  bd.process = nullptr;
#else
  bd.process = { {-1,-1,-1} };
#endif
  bd.bt = bt_normal;
  bd.buffer.name = "";
  return bd;
  }

bool can_be_saved(const std::string& name) {
  if (name.empty())
    return true;
  if (name == std::string("+Errors") || name.front() == '=')
    return false;
  return true;
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

void set_font(int font_size, settings& s) {
  pdc_font_size = font_size;
  s.font_size = font_size;

  TTF_CloseFont(pdc_ttffont);
  pdc_ttffont = TTF_OpenFont(s.font.c_str(), pdc_font_size);

  if (!pdc_ttffont) {
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
    }

  TTF_SizeText(pdc_ttffont, "W", &font_width, &font_height);
  pdc_fheight = font_height;
  pdc_fwidth = font_width;
  pdc_fthick = pdc_font_size / 20 + 1;
  }

app_state resize_font(app_state state, int font_size, settings& s)
  {
  set_font(font_size, s);

  state.w = (state.w / font_width) * font_width;
  state.h = (state.h / font_height) * font_height;

  SDL_SetWindowSize(pdc_window, state.w, state.h);

  resize_term(state.h / font_height, state.w / font_width);
  resize_term_ex(state.h / font_height, state.w / font_width);

  return state;
  }

uint32_t get_column_id(const app_state& state, uint32_t buffer_id)
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

file_buffer& get_last_active_editor_buffer(app_state& state) {
  if (state.last_active_editor_buffer != 0xffffffff)
    return state.buffers[state.last_active_editor_buffer].buffer;
  else
    return state.buffers[state.active_buffer].buffer;
  }

file_buffer& get_mouse_pointing_buffer(app_state& state) {
  if (state.mouse_pointing_buffer != 0xffffffff)
    return state.buffers[state.mouse_pointing_buffer].buffer;
  else
    return state.buffers[state.active_buffer].buffer;
  }

int64_t& get_active_scroll_row(app_state& state) {
  return state.buffers[state.active_buffer].scroll_row;
  }

std::optional<app_state> command_exit(app_state state, uint32_t, settings& s)
  {
  bool show_error_window = false;
  std::stringstream str;
  for (uint32_t i = 0; i < state.g.columns.size(); ++i)
    {
    auto& c = state.g.columns[i];

    for (const auto& ci : c.items)
      {
      auto& wp = state.window_pairs[ci.window_pair_id];
      auto& f = state.buffers[state.windows[wp.window_id].buffer_id].buffer;
      if (f.modification_mask == 1 && can_be_saved(f.name))
        {
        show_error_window = true;
        str << (f.name.empty() ? std::string("<unsaved file>") : f.name) << " modified\n";
        f.modification_mask |= 2;
        }
      }
    }
  if (show_error_window)
    {
    return add_error_text(state, str.str(), s);
    }

  return std::nullopt;
  }

app_state resize_windows(app_state state, const settings& s) {
  int rows, cols;
  getmaxyx(stdscr, rows, cols);
  rows -= 2; // make room for bottom operation stuff
  state.windows[state.g.topline_window_id].x = 0;
  state.windows[state.g.topline_window_id].y = 0;
  state.windows[state.g.topline_window_id].cols = cols;
  if (state.windows[state.g.topline_window_id].rows <= 0)
    state.windows[state.g.topline_window_id].rows = 1;
  if (state.windows[state.g.topline_window_id].rows > rows)
    state.windows[state.g.topline_window_id].rows = rows;
  int start_row = state.windows[state.g.topline_window_id].rows;

  for (auto& c : state.g.columns) {
    int left = (int)(c.left * cols);
    int right = (int)(c.right * cols);
    state.windows[c.column_command_window_id].x = left;
    state.windows[c.column_command_window_id].y = start_row;
    state.windows[c.column_command_window_id].cols = right - left;
    int available_rows = rows - state.windows[state.g.topline_window_id].rows;
    if (state.windows[c.column_command_window_id].rows <= 0)
      state.windows[c.column_command_window_id].rows = 1;
    if (state.windows[c.column_command_window_id].rows > available_rows)
      state.windows[c.column_command_window_id].rows = available_rows;
    available_rows = rows - state.windows[state.g.topline_window_id].rows - state.windows[c.column_command_window_id].rows;
    for (auto& ci : c.items) {
      int row_offset = start_row + state.windows[c.column_command_window_id].rows;
      int top = row_offset + (int)(available_rows * ci.top_layer);
      int bottom = row_offset + (int)(available_rows * ci.bottom_layer);
      auto wp = state.window_pairs[ci.window_pair_id];
      int available_rows_for_command = bottom - top;
      if (state.windows[wp.command_window_id].rows <= 0)
        state.windows[wp.command_window_id].rows = 1;
      if (state.windows[wp.command_window_id].rows > available_rows_for_command)
        state.windows[wp.command_window_id].rows = available_rows_for_command;
      state.windows[wp.command_window_id].cols = right - left;
      state.windows[wp.command_window_id].x = left;
      state.windows[wp.command_window_id].y = top;
      state.windows[wp.window_id].cols = right - left;
      state.windows[wp.window_id].x = left;
      state.windows[wp.window_id].y = top + state.windows[wp.command_window_id].rows;
      state.windows[wp.window_id].rows = available_rows_for_command - state.windows[wp.command_window_id].rows;
      }
    }
  return state;
  }


std::wstring make_command_text(const app_state& state, uint32_t buffer_id, const settings& s)
  {
  const auto& f = state.buffers[buffer_id].buffer;
  std::stringstream out;
  out << f.name << " Del | ";
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
  auto& textf = state.buffers[buffer_id + 1].buffer;

  bool put = textf.modification_mask != 0 && can_be_saved(f.name);
  bool undo = !textf.history.empty() && textf.undo_redo_index > 0;
  bool redo = !textf.history.empty() && textf.undo_redo_index < textf.history.size();

  bool get = true;//jtk::is_directory(f.name);
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
  remove_whitespace(current_text);
  remove_whitespace(text);
  return text != current_text;
  }

void set_updated_command_text_position(file_buffer& fb,
  position original_position,
  std::optional<position> original_start_selection,
  uint32_t original_first_row_length,
  bool active) {
  if (active) {
    fb.pos = original_position;
    fb.start_selection = original_start_selection;
    if (fb.pos > get_last_position(fb))
      fb.pos = get_last_position(fb);
    if (original_start_selection) {
      if (*fb.start_selection > get_last_position(fb))
        fb.start_selection = get_last_position(fb);
      }
    return;
    }
  uint32_t current_first_row_length = fb.content.empty() ? 0 : fb.content.front().size();
  if (original_start_selection) {
    position sel = *original_start_selection;
    if (sel.row == 0)
      sel.col = sel.col + (int64_t)current_first_row_length - (int64_t)original_first_row_length;
    if (sel.col < 0)
      sel.col = 0;
    //if (sel > get_last_position(fb))
    //  sel = get_last_position(fb);
    fb.start_selection = sel;
    }
  if (original_position.row == 0)
    fb.pos.col = original_position.col + (int64_t)current_first_row_length - (int64_t)original_first_row_length;
  else
    fb.pos = original_position;
  if (fb.pos.col < 0)
    fb.pos.col = 0;
  //if (fb.pos > get_last_position(fb))
  //  fb.pos = get_last_position(fb);
  }

app_state update_command_text(app_state state, uint32_t buffer_id, const settings& s) {
  assert(this_is_a_command_window(state, buffer_id));
  auto text = get_command_text(state, buffer_id, s);
  auto& fb = state.buffers[buffer_id].buffer;

  auto original_position = fb.pos;
  std::optional<position> original_start_selection = fb.start_selection;
  uint32_t original_first_row_length = fb.content.empty() ? 0 : fb.content.front().size();

  auto pos_del = find_next_occurence(fb.content, position(0, 0), L" Del ");
  if (pos_del.col < 0) {
    auto pos_bar = find_next_occurence(fb.content, position(0, 0), L'|');
    if (pos_bar.col < 0) {
      fb.pos = get_last_position(fb);
      text.push_back(L'|');
      fb = insert(fb, text, convert(s), false);
      set_updated_command_text_position(fb, original_position, original_start_selection, original_first_row_length, state.active_buffer == buffer_id);
      return state;
      }
    fb.pos = pos_bar;
    fb = insert(fb, text, convert(s), false);
    set_updated_command_text_position(fb, original_position, original_start_selection, original_first_row_length, state.active_buffer == buffer_id);
    return state;
    }
  auto pos_bar = find_next_occurence(fb.content, pos_del, L'|');
  if (pos_bar.col < 0) {
    fb.pos = get_last_position(fb);
    text.push_back(L'|');
    fb = insert(fb, text, convert(s), false);
    set_updated_command_text_position(fb, original_position, original_start_selection, original_first_row_length, state.active_buffer == buffer_id);
    return state;
    }
  pos_del = find_next_occurence_reverse(fb.content, pos_bar, L" Del ");

  fb.start_selection = pos_del;
  fb.pos = pos_bar;
  fb = erase(fb, convert(s), false);
  text.push_back(L'|');
  fb = insert(fb, text, convert(s), false);
  set_updated_command_text_position(fb, original_position, original_start_selection, original_first_row_length, state.active_buffer == buffer_id);
  return state;
  }

app_state update_filename(app_state state, uint32_t buffer_id, const settings& s) {
  assert(this_is_a_command_window(state, buffer_id));
  assert(!should_update_command_text(state, buffer_id, s));
  auto& fb = state.buffers[buffer_id].buffer;
  auto pos_del = find_next_occurence(fb.content, position(0, 0), L" Del ");
  auto pos_bar = find_next_occurence(fb.content, pos_del, L'|');
  pos_del = find_next_occurence_reverse(fb.content, pos_bar, L" Del ");

  auto name = to_string(fb.content, position(0, 0), pos_del);
  remove_whitespace(name);

  fb.name = name;
  if (state.buffers[buffer_id + 1].buffer.name != name) { // this is costly, so avoid if possible
    state.buffers[buffer_id + 1].buffer.name = name;
    state.buffers[buffer_id + 1].buffer = set_multiline_comments(state.buffers[buffer_id + 1].buffer);
    state.buffers[buffer_id + 1].buffer = init_lexer_status(state.buffers[buffer_id + 1].buffer, convert(s));
    }
  return state;
  }

std::string get_user_command_text(const file_buffer& fb) {
  auto pos_del = find_next_occurence(fb.content, position(0, 0), L" Del ");
  if (pos_del.col < 0)
    return std::string();
  auto pos_bar = find_next_occurence(fb.content, pos_del, L'|');
  if (pos_bar.col < 0)
    return std::string();
  pos_del = find_next_occurence_reverse(fb.content, pos_bar, L" Del ");
  return to_string(fb.content, pos_bar, get_last_position(fb));
  }

std::string get_user_command_text(const app_state& state, uint32_t buffer_id) {
  assert(this_is_a_command_window(state, buffer_id));
  const auto& fb = state.buffers[buffer_id].buffer;
  return get_user_command_text(fb);
  }

app_state set_filename(app_state state, uint32_t buffer_id, const settings& s) {
  assert(this_is_a_command_window(state, buffer_id));

  auto& fb = state.buffers[buffer_id].buffer;
  std::string name = fb.name;
  std::string new_text = name + get_command_text(state, buffer_id, s) + get_user_command_text(state, buffer_id);

  fb = make_empty_buffer();
  fb.name = name;
  fb = insert(fb, new_text, convert(s), false);

  return state;
  }

app_state add_error_window(app_state state, settings& s)
  {
  state = *command_new_window(state, 0xffffffff, s);
  uint32_t buffer_id = state.buffers.size() - 1;
  uint32_t command_id = state.buffers.size() - 2;
  state.buffers[buffer_id].buffer.name = "+Errors";
  state.buffers[command_id].buffer.name = "+Errors";
  state.buffers[command_id].buffer.content = to_text(make_command_text(state, command_id, s));
  return state;
  }

app_state add_error_text(app_state state, const std::string& errortext, settings& s)
  {
  std::string error_filename("+Errors");
  uint32_t buffer_id = 0xffffffff;
  for (const auto& w : state.windows)
    {
    if (w.wt == wt_normal)
      {
      const auto& f = state.buffers[w.buffer_id].buffer;
      if (f.name == error_filename)
        buffer_id = w.buffer_id;
      }
    }

  if (buffer_id == 0xffffffff)
    {
    state = add_error_window(state, s);
    buffer_id = state.buffers.size() - 1;
    }
  auto active = state.active_buffer;
  state.active_buffer = buffer_id;
  get_active_buffer(state).pos = get_last_position(get_active_buffer(state));

  if (get_active_buffer(state).pos.col > 0)
    get_active_buffer(state) = insert(get_active_buffer(state), "\n", convert(s));
  get_active_buffer(state) = insert(get_active_buffer(state), errortext, convert(s));

  state.active_buffer = active;
  return state;

  }

std::optional<app_state> command_delete_window(app_state state, uint32_t buffer_id, settings& s) {
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
        if (f.buffer.modification_mask == 1 && can_be_saved(f.buffer.name))
          {
          f.buffer.modification_mask |= 2;
          std::stringstream str;
          str << (f.buffer.name.empty() ? std::string("<unsaved file>") : f.buffer.name) << " modified\n";
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

          kill(state, f1);
          kill(state, f2);

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
          if (state.last_active_editor_buffer == f1 || state.last_active_editor_buffer == f2)
            {
            if (c.items.empty())
              state.last_active_editor_buffer = 0;
            else
              {
              if (j >= c.items.size())
                j = c.items.size() - 1;
              state.last_active_editor_buffer = state.windows[state.window_pairs[c.items[j].window_pair_id].window_id].buffer_id;
              }
            }
          else
            {
            if (state.last_active_editor_buffer > f2)
              --state.last_active_editor_buffer;
            if (state.last_active_editor_buffer > f1)
              --state.last_active_editor_buffer;
            }
          if (state.mouse_pointing_buffer == f1 || state.mouse_pointing_buffer == f2)
            {
            if (c.items.empty())
              state.mouse_pointing_buffer = 0;
            else
              {
              if (j >= c.items.size())
                j = c.items.size() - 1;
              state.mouse_pointing_buffer = state.windows[state.window_pairs[c.items[j].window_pair_id].window_id].buffer_id;
              }
            }
          else
            {
            if (state.mouse_pointing_buffer > f2)
              --state.mouse_pointing_buffer;
            if (state.mouse_pointing_buffer > f1)
              --state.mouse_pointing_buffer;
            }
          if (state.g.columns[i].items.empty())
            return state;
          return optimize_column(state, state.windows[state.window_pairs[state.g.columns[i].items.back().window_pair_id].window_id].buffer_id, s);
          //return resize_windows(state, s);
          }
        }
      }
    }
  // no window found to delete, try the last active editor buffer
  if (state.last_active_editor_buffer != 0xffffffff) {
    auto& w = state.windows[state.buffer_id_to_window_id[state.last_active_editor_buffer]];
    if (w.wt == e_window_type::wt_normal)
      return command_delete_window(state, state.last_active_editor_buffer, s);
    }
  return state;
  }


std::optional<app_state> command_delete_column(app_state state, uint32_t buffer_id, settings& s) {
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
        if (f.modification_mask == 1 && can_be_saved(f.name))
          {
          show_error_window = true;
          str << (f.name.empty() ? std::string("<unsaved file>") : f.name) << " modified\n";
          f.modification_mask |= 2;
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

        uint32_t window_id = state.g.columns[i].column_command_window_id;
        uint32_t buffer_id = state.windows[window_id].buffer_id;

        state.buffers.erase(state.buffers.begin() + buffer_id);

        //int64_t w = state.buffer_id_to_window_id[buffer_id];

        state.buffer_id_to_window_id.erase(state.buffer_id_to_window_id.begin() + buffer_id);

        state.windows.erase(state.windows.begin() + window_id);
        for (auto& buff : state.buffers)
          {
          if (buff.buffer_id > buffer_id)
            --buff.buffer_id;
          }

        for (auto& win : state.windows)
          {
          if (win.buffer_id > buffer_id)
            --win.buffer_id;
          }
        for (auto& fid2winid : state.buffer_id_to_window_id)
          {
          if (fid2winid > window_id)
            --fid2winid;
          }
        for (auto& winp : state.window_pairs)
          {
          if (winp.command_window_id > window_id)
            --winp.command_window_id;
          if (winp.window_id > window_id)
            --winp.window_id;
          }
        if (state.g.topline_window_id > window_id)
          --state.g.topline_window_id;
        for (auto& col : state.g.columns)
          {
          if (col.column_command_window_id > window_id)
            --col.column_command_window_id;
          }
        if (state.active_buffer == window_id)
          {
          state.active_buffer = 0;
          }
        else
          {
          if (state.active_buffer > window_id)
            --state.active_buffer;
          }
        if (state.last_active_editor_buffer == window_id)
          {
          state.last_active_editor_buffer = 0;
          }
        else
          {
          if (state.last_active_editor_buffer > window_id)
            --state.last_active_editor_buffer;
          }
        if (state.mouse_pointing_buffer == window_id)
          {
          state.mouse_pointing_buffer = 0;
          }
        else
          {
          if (state.mouse_pointing_buffer > window_id)
            --state.mouse_pointing_buffer;
          }

        state.g.columns.erase(state.g.columns.begin() + i);
        return resize_windows(state, s);
        }
      }
    }
  // no column found to delete, try the column of the current buffer_id
  if (buffer_id != 0xffffffff) {
    auto& w = state.windows[state.buffer_id_to_window_id[buffer_id]];
    if (w.wt == e_window_type::wt_normal || w.wt == e_window_type::wt_command) {
      auto& c = state.g.columns[get_column_id(state, buffer_id)];
      return command_delete_column(state, c.column_command_window_id, s);
      }
    }
  // no column found to delete, try the last active editor buffer
  if (state.last_active_editor_buffer != 0xffffffff) {
    auto& w = state.windows[state.buffer_id_to_window_id[state.last_active_editor_buffer]];
    if (w.wt == e_window_type::wt_normal) {
      auto& c = state.g.columns[get_column_id(state, state.last_active_editor_buffer)];
      return command_delete_column(state, c.column_command_window_id, s);
      }
    }
  return state;
  }

std::optional<app_state> command_new_column(app_state state, uint32_t, settings& s) {
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

/*
if buffer_id == 0 then will be added as first item
if buffer_id == 0xffffffff then will be added as last item
*/
std::optional<app_state> command_new_window(app_state state, uint32_t buffer_id, settings& s) {
  if (state.g.columns.empty())
    state = *command_new_column(state, buffer_id, s);

  uint32_t column_id;

  if (buffer_id == 0 || buffer_id == 0xffffffff)
    {
    if (buffer_id == 0)
      column_id = 0;
    if (buffer_id == 0xffffffff)
      column_id = state.g.columns.size() - 1;
    }
  else
    {
    column_id = get_column_id(state, buffer_id);
    if (column_id == get_column_id(state, state.active_buffer)) // if the active file is in the column where we clicked on "New", then make new window above active window
      {
      buffer_id = state.active_buffer;
      }
    }

  // if there is an empty column, then fill that column
  for (uint32_t c_id = 0; c_id < state.g.columns.size(); ++c_id)
    {
    if (state.g.columns[c_id].items.empty()) {
      column_id = c_id;
      break;
      }
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
        state.g.columns[column_id].items[k].bottom_layer = (state.g.columns[column_id].items[k].bottom_layer + state.g.columns[column_id].items[k].top_layer) * 0.5;
        ci.top_layer = state.g.columns[column_id].items[k].bottom_layer;
        pos = k + 1; // change to k if you want to insert before existing
        break;
        }
      }
    //add the next lines if you want to insert before existing
    //if (buffer_id == 0) // for buffer_id == 0, insert at the top
    //  pos = 0;
    state.g.columns[column_id].items.insert(state.g.columns[column_id].items.begin() + pos, ci);
    }
  else
    state.g.columns[column_id].items.push_back(ci);

  state.active_buffer = editor_id;

  return optimize_column(state, editor_id, s);
  }

void get_window_rect_for_editing(int& offset_x, int& offset_y, int& rows, int& cols, uint32_t buffer_id, const app_state& state, const settings& s)
  {
  auto window_id = state.buffer_id_to_window_id[buffer_id];
  get_window_edit_range(offset_x, offset_y, cols, rows, state.buffers[buffer_id].scroll_row, state.windows[window_id], s);
  }

void get_window_size_for_editing(int& rows, int& cols, uint32_t buffer_id, const app_state& state, const settings& s)
  {
  int offset_x, offset_y;
  get_window_rect_for_editing(offset_x, offset_y, rows, cols, buffer_id, state, s);
  }

void get_active_window_rect_for_editing(int& offset_x, int& offset_y, int& rows, int& cols, const app_state& state, const settings& s)
  {
  //auto window_id = state.buffer_id_to_window_id[state.buffers[state.active_buffer].buffer_id];
  //get_window_edit_range(offset_x, offset_y, cols, rows, state.buffers[state.active_buffer].scroll_row, state.windows[window_id], s);
  get_window_rect_for_editing(offset_x, offset_y, rows, cols, state.active_buffer, state, s);
  }

void get_active_window_size_for_editing(int& rows, int& cols, const app_state& state, const settings& s)
  {
  int offset_x, offset_y;
  get_active_window_rect_for_editing(offset_x, offset_y, rows, cols, state, s);
  }

app_state check_scroll_position(app_state state, uint32_t buffer_id, const settings& s)
  {
  if (buffer_id == 0xffffffff)
    buffer_id = state.active_buffer;
  int rows, cols;
  get_window_size_for_editing(rows, cols, buffer_id, state, s);
  if (state.buffers[buffer_id].scroll_row > state.buffers[buffer_id].buffer.pos.row)
    state.buffers[buffer_id].scroll_row = state.buffers[buffer_id].buffer.pos.row;
  else
    {
    if (s.wrap)
      {
      auto senv = convert(s);
      int64_t actual_rows = 0;
      int r = 0;
      for (; r < rows; ++r)
        {
        if (state.buffers[buffer_id].scroll_row + r >= state.buffers[buffer_id].buffer.content.size())
          break;
        actual_rows += wrapped_line_rows(state.buffers[buffer_id].buffer.content[state.buffers[buffer_id].scroll_row + r], cols, rows, senv);
        if (actual_rows >= rows)
          break;
        }
      int64_t my_row = 0;
      if (state.buffers[buffer_id].buffer.pos.row < state.buffers[buffer_id].buffer.content.size())
        my_row = wrapped_line_rows(state.buffers[buffer_id].buffer.content[state.buffers[buffer_id].buffer.pos.row], cols, rows, senv);
      if (state.buffers[buffer_id].scroll_row + r < state.buffers[buffer_id].buffer.pos.row + my_row - 1)
        {
        state.buffers[buffer_id].scroll_row = state.buffers[buffer_id].buffer.pos.row;
        r = 0;
        actual_rows = my_row;
        for (; r < rows; ++r)
          {
          if (state.buffers[buffer_id].scroll_row == 0)
            break;
          actual_rows += wrapped_line_rows(state.buffers[buffer_id].buffer.content[state.buffers[buffer_id].scroll_row - 1], cols, rows, senv);
          if (actual_rows <= rows)
            --state.buffers[buffer_id].scroll_row;
          else
            break;
          }
        }
      }
    else if (state.buffers[buffer_id].scroll_row + rows <= state.buffers[buffer_id].buffer.pos.row)
      {
      state.buffers[buffer_id].scroll_row = state.buffers[buffer_id].buffer.pos.row - rows + 1;
      }
    }
  return state;
  }

app_state check_scroll_position(app_state state, const settings& s)
  {
  return check_scroll_position(state, state.active_buffer, s);
  /*
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
   */
  }

app_state check_operation_scroll_position(app_state state, const settings& s)
  {
  int64_t lastrow = (int64_t)state.operation_buffer.content.size() - 1;
  if (lastrow < 0)
    lastrow = 0;

  if (state.operation_scroll_row > lastrow)
    state.operation_scroll_row = lastrow;
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


app_state check_pipes(bool& modifications, uint32_t buffer_id, app_state state, const settings& s)
  {
  modifications = false;
  if (state.buffers[buffer_id].bt != bt_piped)
    return state;
#ifdef _WIN32
  std::string text = jtk::read_from_pipe(state.buffers[buffer_id].process, 10);
#else
  std::string text = jtk::read_from_pipe(state.buffers[buffer_id].process.data(), 10);
#endif
  if (text.empty())
    return state;
  modifications = true;
  state.buffers[buffer_id].buffer.pos = get_last_position(state.buffers[buffer_id].buffer);
  state.buffers[buffer_id].buffer = insert(state.buffers[buffer_id].buffer, text, convert(s));
  auto last_line = state.buffers[buffer_id].buffer.content.back();
  state.buffers[buffer_id].piped_prompt = std::wstring(last_line.begin(), last_line.end());
  return check_scroll_position(state, buffer_id, s);
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
    get_active_scroll_row(state) = (int64_t)get_active_buffer(state).content.size() - rows + 1;
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
    get_active_buffer(state) = s.case_sensitive ? find_text(get_active_buffer(state), state.operation_buffer.content) : find_text_case_insensitive(get_active_buffer(state), state.operation_buffer.content);
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

app_state tab_editor(app_state state, int tab_width, std::string t, const settings& s)
  {
  auto s_env = convert(s);
  file_buffer& fb = get_active_buffer(state);
  if (has_multiline_selection(fb))
    {
    if (has_rectangular_selection(fb))
      {
      int64_t minrow, maxrow, minx, maxx;      
      minx = fb.pos.col;
      maxx = fb.start_selection->col;
      minrow = fb.pos.row;
      maxrow = fb.start_selection->row;
      if (maxx < minx)
        std::swap(maxx, minx);
      if (maxrow < minrow)
        std::swap(maxrow, minrow);
      auto pos = get_actual_position(fb);
      auto pos2 = *fb.start_selection;
      if (t.empty())
        {
        int nr_of_spaces = tab_width - (minx % tab_width);
        for (int i = 0; i < nr_of_spaces; ++i)
          t.push_back(' ');
        }
      fb = push_undo(fb);
      for (int r = minrow; r <= maxrow; ++r)
        {
        fb.pos.row = r;
        fb.pos.col = minx;
        fb.start_selection = std::nullopt;
        fb = insert(fb, t, s_env, false);
        }
      fb.pos = pos;
      fb.start_selection = pos2;
      fb.pos.col += t.length();
      fb.start_selection->col += t.length();
      }
    else
      {
      if (t.empty())
        {
        for (int i = 0; i < tab_width; ++i)
          t.push_back(' ');
        }
      auto pos = get_actual_position(fb);
      auto pos2 = *fb.start_selection;
      int r1 = pos.row;
      int r2 = pos2.row;
      if (r2 < r1)
        std::swap(r1, r2);
      fb = push_undo(fb);
      for (int r = r1; r <= r2; ++r)
        {
        fb.pos.row = r;
        fb.pos.col = 0;
        fb.start_selection = std::nullopt;
        fb = insert(fb, t, s_env, false);
        }
      fb.pos = pos;
      fb.start_selection = pos2;
      fb.pos.col += t.length();
      fb.start_selection->col += t.length();
      }
    }
  else
    {
    auto pos = get_actual_position(fb);
    if (t.empty())
      {
      int nr_of_spaces = tab_width - (pos.col % tab_width);
      for (int i = 0; i < nr_of_spaces; ++i)
        t.push_back(' ');
      }
    fb = insert(fb, t, s_env);
    }
  return check_scroll_position(state, s);
  }

app_state tab_operation(app_state state, int tab_width, std::string t, const settings& s)
  {
  auto pos = get_actual_position(state.operation_buffer);
  int nr_of_spaces = tab_width - (pos.col % tab_width);
  if (t.empty())
    {
    for (int i = 0; i < nr_of_spaces; ++i)
      t.push_back(' ');
    }
  state.operation_buffer = insert(state.operation_buffer, t, convert(s));
  return state;
  }

app_state tab(app_state state, int tab_width, std::string t, const settings& s)
  {
  if (state.operation == op_editing)
    return tab_editor(state, tab_width, t, s);
  else
    return tab_operation(state, tab_width, t, s);
  }

app_state inverse_tab_editor(app_state state, int tab_width, const settings& s)
  {
  auto s_env = convert(s);
  file_buffer& fb = get_active_buffer(state);
  if (has_multiline_selection(fb))
    {
    if (has_rectangular_selection(fb))
      {
      int64_t minrow, maxrow, minx, maxx;
      //get_rectangular_selection(minrow, maxrow, minx, maxx, fb, *fb.start_selection, fb.pos, s_env);
      minx = fb.pos.col;
      maxx = fb.start_selection->col;
      minrow = fb.pos.row;
      maxrow = fb.start_selection->row;
      if (maxx < minx)
        std::swap(maxx, minx);
      if (maxrow < minrow)
        std::swap(maxrow, minrow);
      auto pos = get_actual_position(fb);
      auto pos2 = *fb.start_selection;
      if (minx <= 0)
        return state;
      fb = push_undo(fb);
      fb.start_selection = std::nullopt;
      std::vector<int> removals(maxrow - minrow + 1, 0);
      for (int r = minrow; r <= maxrow; ++r)
        {        
        fb.pos.row = r;
        fb.pos.col = minx;        
        int64_t depth = line_length_up_to_column(fb.content[r], minx - 1, s_env);
        int64_t target_removals = depth % tab_width;
        if (target_removals == 0)
          target_removals = tab_width;
        int64_t target_depth = depth - target_removals;          
        if ((fb.content[r].size() >= fb.pos.col) &&fb.content[r][fb.pos.col - 1] == L'\t')
          {
          fb = erase(fb, s_env, false);
          ++removals[r - minrow];
          if (fb.pos.col > 0)
            {
            depth = line_length_up_to_column(fb.content[r], fb.pos.col - 1, s_env);
            if (depth > target_depth)
              {
              target_removals = depth % tab_width;
              while (target_removals && fb.pos.col > 0 && fb.content[fb.pos.row][fb.pos.col - 1] == L' ')
                {
                fb = erase(fb, s_env, false);
                ++removals[r - minrow];
                --target_removals;
                }
              }
            }
          }
        else
          {
          while (target_removals && fb.content[fb.pos.row][fb.pos.col - 1] == L' ')
            {
            fb = erase(fb, s_env, false);
            ++removals[r - minrow];
            --target_removals;
            }
          }
        } // for (int r = minrow; r <= maxrow; ++r)
      fb.pos = pos;
      fb.start_selection = pos2;
      fb.pos.col -= removals[pos.row - minrow];
      fb.start_selection->col -= removals[pos2.row - minrow];
      }
    else
      {
      auto pos = get_actual_position(fb);
      auto pos2 = *fb.start_selection;
      int r1 = pos.row;
      int r2 = pos2.row;
      if (r2 < r1)
        std::swap(r1, r2);
      fb = push_undo(fb);
      std::vector<int> removals(r2 - r1 + 1, 0);
      for (int r = r1; r <= r2; ++r)
        {
        fb.pos.row = r;
        fb.pos.col = 0;
        fb.start_selection = std::nullopt;
        if (fb.content[r].empty())
          continue;
        if (fb.content[r][0] == L'\t')
          {
          fb = erase_right(fb, s_env, false);
          removals[r - r1] = 1;
          }
        else
          {
          int cnt = tab_width;
          while (cnt && !fb.content[r].empty() && (fb.content[r][0] == L' ' || fb.content[r][0] == L'\t'))
            {
            fb = erase_right(fb, s_env, false);
            ++removals[r - r1];
            --cnt;
            }
          }
        }
      fb.pos = pos;
      fb.pos.col -= removals[pos.row - r1];
      fb.start_selection = pos2;
      fb.start_selection->col -= removals[pos2.row - r1];
      }
    }
  else
    {
    auto pos = get_actual_position(fb);
    if (pos.col <= 0)
      return state;
    int64_t depth = line_length_up_to_column(fb.content[pos.row], pos.col - 1, s_env);
    int64_t target_removals = depth % tab_width;
    if (target_removals == 0)
      target_removals = tab_width;
    int64_t target_depth = depth - target_removals;
    fb = push_undo(fb);
    if (fb.content[pos.row][pos.col - 1] == L'\t')
      {
      fb = erase(fb, s_env, false);
      if (fb.pos.col > 0)
        {
        depth = line_length_up_to_column(fb.content[pos.row], fb.pos.col - 1, s_env);
        if (depth > target_depth)
          {
          target_removals = depth % tab_width;
          while (target_removals && fb.pos.col > 0 && fb.content[fb.pos.row][fb.pos.col - 1] == L' ')
            {
            fb = erase(fb, s_env, false);
            --target_removals;
            }
          }
        }
      }
    else
      {
      while (target_removals && fb.content[fb.pos.row][pos.col - 1] == L' ')
        {
        fb = erase(fb, s_env, false);
        --target_removals;
        pos = fb.pos;
        }
      }
    }
  return check_scroll_position(state, s);
  }

app_state inverse_tab_operation(app_state state, int tab_width, const settings& s)
  {
  auto s_env = convert(s);
  file_buffer& fb = state.operation_buffer;
  auto pos = get_actual_position(fb);
  if (pos.col <= 0)
    return state;
  int64_t depth = line_length_up_to_column(fb.content[pos.row], pos.col - 1, s_env);
  int64_t target_removals = depth % tab_width;
  if (target_removals == 0)
    target_removals = tab_width;
  int64_t target_depth = depth - target_removals;
  fb = push_undo(fb);
  if (fb.content[pos.row][pos.col - 1] == L'\t')
    {
    fb = erase(fb, s_env, false);
    if (fb.pos.col > 0)
      {
      depth = line_length_up_to_column(fb.content[pos.row], fb.pos.col - 1, s_env);
      if (depth > target_depth)
        {
        target_removals = depth % tab_width;
        while (target_removals && fb.pos.col > 0 && fb.content[fb.pos.row][fb.pos.col - 1] == L' ')
          {
          fb = erase(fb, s_env, false);
          --target_removals;
          }
        }
      }
    }
  else
    {
    while (target_removals && fb.content[fb.pos.row][pos.col - 1] == L' ')
      {
      fb = erase(fb, s_env, false);
      --target_removals;
      pos = fb.pos;
      }
    }
  return state;
  }

app_state inverse_tab(app_state state, int tab_width, const settings& s)
  {
  if (state.operation == op_editing)
    return inverse_tab_editor(state, tab_width, s);
  else
    return inverse_tab_operation(state, tab_width, s);
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
    int res = jtk::send_to_pipe(state.buffers[state.active_buffer].process, cmd.c_str());
#else
    int res = jtk::send_to_pipe(state.buffers[state.active_buffer].process.data(), cmd.c_str());
#endif
    if (res != 0)
      kill(state, state.active_buffer);
    get_active_buffer(state).pos = get_last_position(get_active_buffer(state));
    get_active_buffer(state) = insert(get_active_buffer(state), "\n", convert(s));
    bool modifications;
    state = check_pipes(modifications, state.active_buffer, state, s);
    return check_scroll_position(state, s);
    }
  else
    {
    std::string indentation("\n");
    indentation.append(get_row_indentation_pattern(get_active_buffer(state), get_active_buffer(state).pos));
    return text_input(state, indentation.c_str(), s);
    }
  }

app_state clear_operation_buffer(app_state state)
  {
  state.operation_buffer.content = text();
  state.operation_buffer.lex = lexer_status();
  state.operation_buffer.history = immutable::vector<snapshot, false>();
  state.operation_buffer.undo_redo_index = 0;
  state.operation_buffer.start_selection = std::nullopt;
  state.operation_buffer.rectangular_selection = false;
  state.operation_buffer.pos.row = 0;
  state.operation_buffer.pos.col = 0;
  state.operation_scroll_row = 0;
  return state;
  }

app_state edit(app_state state, settings& s)
  {
  uint32_t buffer_id = state.active_buffer;
  std::string edit_command;
  if (!state.operation_buffer.content.empty())
    edit_command = jtk::convert_wstring_to_string(std::wstring(state.operation_buffer.content[0].begin(), state.operation_buffer.content[0].end()));
  try {
    state.buffers[buffer_id].buffer = handle_command(state.buffers[buffer_id].buffer, edit_command, convert(s));
    }
  catch (std::runtime_error e) {
    state = add_error_text(state, e.what(), s);
    }
  state.operation = op_editing;
  return check_scroll_position(state, buffer_id, s);
  }

app_state find(app_state state, settings& s)
  {
  uint32_t buffer_id = state.active_buffer;
  //state.message = string_to_line("[Find]");
  std::wstring search_string;
  if (!state.operation_buffer.content.empty())
    search_string = std::wstring(state.operation_buffer.content[0].begin(), state.operation_buffer.content[0].end());
  s.last_find = jtk::convert_wstring_to_string(search_string);
  state.buffers[buffer_id].buffer = s.case_sensitive ? find_text(state.buffers[buffer_id].buffer, search_string) : find_text_case_insensitive(state.buffers[buffer_id].buffer, search_string);
  state.operation = op_editing;
  return check_scroll_position(state, s);
  }

app_state replace(app_state state, settings& s)
  {
  uint32_t buffer_id = state.active_buffer;
  auto senv = convert(s);
  //state.message = string_to_line("[Replace]");
  state.operation = op_editing;
  std::wstring replace_string;
  state.buffers[buffer_id].buffer = s.case_sensitive ? find_text(state.buffers[buffer_id].buffer, s.last_find) : find_text_case_insensitive(state.buffers[buffer_id].buffer, s.last_find);
  state.buffers[buffer_id].buffer = erase_right(state.buffers[buffer_id].buffer, senv, true);
  if (!state.operation_buffer.content.empty())
    replace_string = std::wstring(state.operation_buffer.content[0].begin(), state.operation_buffer.content[0].end());
  s.last_replace = jtk::convert_wstring_to_string(replace_string);
  if (state.buffers[buffer_id].buffer.pos != get_last_position(state.buffers[buffer_id].buffer))
    state.buffers[buffer_id].buffer = insert(state.buffers[buffer_id].buffer, replace_string, senv, false);
  return check_scroll_position(state, buffer_id, s);
  }

app_state replace_all(app_state state, settings& s)
  {
  uint32_t buffer_id = state.active_buffer;
  //state.message = string_to_line("[Replace all]");
  state.operation = op_editing;
  std::wstring replace_string;
  if (!state.operation_buffer.content.empty())
    replace_string = std::wstring(state.operation_buffer.content[0].begin(), state.operation_buffer.content[0].end());
  s.last_replace = jtk::convert_wstring_to_string(replace_string);
  std::wstring find_string = jtk::convert_string_to_wstring(s.last_find);
  state.buffers[buffer_id].buffer.pos = position(0, 0); // go to start
  state.buffers[buffer_id].buffer.start_selection = std::nullopt;
  state.buffers[buffer_id].buffer.rectangular_selection = false;
  state.buffers[buffer_id].buffer = s.case_sensitive ? find_text(state.buffers[buffer_id].buffer, find_string) : find_text_case_insensitive(state.buffers[buffer_id].buffer, find_string);
  if (state.buffers[buffer_id].buffer.pos == get_last_position(state.buffers[buffer_id].buffer))
    return state;
  state.buffers[buffer_id].buffer = push_undo(state.buffers[buffer_id].buffer);
  auto senv = convert(s);
  while (state.buffers[buffer_id].buffer.pos != get_last_position(state.buffers[buffer_id].buffer))
    {
    state.buffers[buffer_id].buffer = erase_right(state.buffers[buffer_id].buffer, senv, false);
    state.buffers[buffer_id].buffer = insert(state.buffers[buffer_id].buffer, replace_string, senv, false);
    state.buffers[buffer_id].buffer = s.case_sensitive ? find_text(state.buffers[buffer_id].buffer, find_string) : find_text_case_insensitive(state.buffers[buffer_id].buffer, find_string);
    }
  return check_scroll_position(state, buffer_id, s);
  }

app_state replace_selection(app_state state, settings& s)
  {
  uint32_t buffer_id = state.active_buffer;
  //state.message = string_to_line("[Replace selection]");
  state.operation = op_editing;
  std::wstring replace_string;
  if (!state.operation_buffer.content.empty())
    replace_string = std::wstring(state.operation_buffer.content[0].begin(), state.operation_buffer.content[0].end());
  s.last_replace = jtk::convert_wstring_to_string(replace_string);
  std::wstring find_string = jtk::convert_string_to_wstring(s.last_find);

  if (!state.buffers[buffer_id].buffer.start_selection)
    return state;

  position start_pos = state.buffers[buffer_id].buffer.pos;
  position end_pos = *state.buffers[buffer_id].buffer.start_selection;
  if (end_pos < start_pos)
    std::swap(start_pos, end_pos);

  state.buffers[buffer_id].buffer.pos = start_pos;
  state.buffers[buffer_id].buffer = s.case_sensitive ? find_text(state.buffers[buffer_id].buffer, find_string) : find_text_case_insensitive(state.buffers[buffer_id].buffer, find_string);
  if (state.buffers[buffer_id].buffer.pos == get_last_position(state.buffers[buffer_id].buffer))
    return state;
  state.buffers[buffer_id].buffer = push_undo(state.buffers[buffer_id].buffer);
  auto senv = convert(s);

  while (state.buffers[buffer_id].buffer.pos <= end_pos)
    {
    state.buffers[buffer_id].buffer = erase_right(state.buffers[buffer_id].buffer, senv, false);
    state.buffers[buffer_id].buffer = insert(state.buffers[buffer_id].buffer, replace_string, senv, false);
    state.buffers[buffer_id].buffer = s.case_sensitive ? find_text(state.buffers[buffer_id].buffer, find_string) : find_text_case_insensitive(state.buffers[buffer_id].buffer, find_string);
    }
  return check_scroll_position(state, buffer_id, s);
  }

app_state replace_find(app_state state, settings& s)
  {
  std::wstring search_string;
  if (!state.operation_buffer.content.empty())
    search_string = std::wstring(state.operation_buffer.content[0].begin(), state.operation_buffer.content[0].end());
  s.last_find = jtk::convert_wstring_to_string(search_string);
  return state;
  }

app_state make_replace_buffer(app_state state, const settings& s)
  {
  state = clear_operation_buffer(state);
  state.operation = op_replace;
  state.operation_buffer = insert(state.operation_buffer, s.last_replace, convert(s), false);
  state.operation_buffer.start_selection = position(0, 0);
  state.operation_buffer = move_end(state.operation_buffer, convert(s));
  return check_scroll_position(state, s);
  }

std::optional<app_state> command_find_next(app_state state, uint32_t buffer_id, settings& s)
  {
  //state.message = string_to_line("[Find next]");
  state.operation = op_editing;
  state.buffers[buffer_id].buffer = s.case_sensitive ? find_text(state.buffers[buffer_id].buffer, s.last_find) : find_text_case_insensitive(state.buffers[buffer_id].buffer, s.last_find);
  return check_scroll_position(state, buffer_id, s);
  }

app_state gotoline(app_state state, const settings& s)
  {
  uint32_t buffer_id = state.active_buffer;
  state.operation = op_editing;
  std::stringstream messagestr;
  messagestr << "[Go to line ";
  if (!state.operation_buffer.content.empty())
    {
    int64_t r = -1;
    std::wstring line_nr = std::wstring(state.operation_buffer.content[0].begin(), state.operation_buffer.content[0].end());
    std::wstringstream str;
    str << line_nr;
    str >> r;
    messagestr << r << "]";
    if (r > 0)
      {
      state.buffers[buffer_id].buffer.pos.row = r - 1;
      state.buffers[buffer_id].buffer.pos.col = 0;
      state.buffers[buffer_id].buffer = clear_selection(state.buffers[buffer_id].buffer);
      if (state.buffers[buffer_id].buffer.pos.row >= state.buffers[buffer_id].buffer.content.size())
        {
        if (state.buffers[buffer_id].buffer.content.empty())
          state.buffers[buffer_id].buffer.pos.row = 0;
        else
          state.buffers[buffer_id].buffer.pos.row = state.buffers[buffer_id].buffer.content.size() - 1;
        }
      if (!state.buffers[buffer_id].buffer.content.empty())
        {
        state.buffers[buffer_id].buffer.start_selection = state.buffers[buffer_id].buffer.pos;
        state.buffers[buffer_id].buffer = move_end(state.buffers[buffer_id].buffer, convert(s));
        }
      }
    }
  state.operation = op_editing;

  //state.message = string_to_line(messagestr.str());
  return check_scroll_position(state, buffer_id, s);
  }

std::string clean_filename(std::string name)
  {
  remove_whitespace(name);
  remove_quotes(name);
  return name;
  }

app_state open_file(app_state state, settings& s)
  {
  uint32_t buffer_id = state.active_buffer;
  state.operation = op_editing;
  std::wstring wfilename;
  if (!state.operation_buffer.content.empty())
    wfilename = std::wstring(state.operation_buffer.content[0].begin(), state.operation_buffer.content[0].end());
  std::replace(wfilename.begin(), wfilename.end(), L'\\', L'/'); // replace all '\\' by '/'
  std::string filename = clean_filename(jtk::convert_wstring_to_string(wfilename));
  if (filename.find(' ') != std::string::npos)
    {
    filename.push_back('"');
    filename.insert(filename.begin(), '"');
    }
  if (!jtk::file_exists(filename))
    {
    if (filename.empty() || filename.back() != '"')
      {
      filename.push_back('"');
      filename.insert(filename.begin(), '"');
      }
    std::string error_message = "File " + filename + " not found\n";
    return add_error_text(state, error_message, s);
    }
  else
    {
    state = *load_file(state, buffer_id, filename, s);
    //state.buffer = read_from_file(filename);
    if (filename.empty() || filename.back() != '"')
      {
      filename.push_back('"');
      filename.insert(filename.begin(), '"');
      }
    std::string message = "Opened file " + filename;
    //state.message = string_to_line(message);
    state = add_error_text(state, message, s);
    }
  //state.buffer = set_multiline_comments(state.buffer);
  //state.buffer = init_lexer_status(state.buffer);
  return check_scroll_position(state, s);
  }


app_state finish_incremental_search(app_state state)
  {
  state.operation = op_editing;
  //state.message = string_to_line("[Incremental search]");
  return state;
  }

std::optional<app_state> ret_operation(app_state state, settings& s) {
  bool done = false;
  while (!done)
    {
    switch (state.operation)
      {
      case op_edit: state = edit(state, s); break;
      case op_find: state = find(state, s); break;
      case op_goto: state = gotoline(state, s); break;
      case op_open: state = open_file(state, s); break;
      case op_incremental_search: state = finish_incremental_search(state);  break;
        //case op_save: state = save_file(state); break;
        //case op_query_save: state = save_file(state); break;
      case op_replace_find: state = replace_find(state, s); break;
      case op_replace_to_find: state = make_replace_buffer(state, s); break;
      case op_replace: state = replace(state, s); break;
        //case op_new: state = make_new_buffer(state, s); break;
        //case op_get: state = get(state); break;
      case op_exit: return std::nullopt;
      default: break;
      }
    if (state.operation_stack.empty())
      {
      //state.operation = op_editing;
      done = true;
      }
    else
      {
      state.operation = state.operation_stack.back();
      state.operation_stack.pop_back();
      }
    }
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


std::optional<app_state> move_editor_window_up_down(app_state state, uint32_t buffer_id, int steps, settings& s)
  {
  int rows, cols;
  get_window_size_for_editing(rows, cols, buffer_id, state, s);
  state.buffers[buffer_id].scroll_row += steps;
  int64_t lastrow = (int64_t)state.buffers[buffer_id].buffer.content.size() - 1;
  if (lastrow < 0)
    {
    state.buffers[buffer_id].scroll_row = 0;
    return state;
    }
  if (state.buffers[buffer_id].scroll_row < 0)
    state.buffers[buffer_id].scroll_row = 0;
  if (s.wrap)
    {
    auto senv = convert(s);
    int64_t actual_rows = 0;
    int r = 0;
    for (; r < rows; ++r)
      {
      if (state.buffers[buffer_id].scroll_row + r >= state.buffers[buffer_id].buffer.content.size())
        break;
      actual_rows += wrapped_line_rows(state.buffers[buffer_id].buffer.content[state.buffers[buffer_id].scroll_row + r], cols, rows, senv);
      if (actual_rows >= rows)
        break;
      }
    if (state.buffers[buffer_id].scroll_row + r > lastrow)
      {
      int64_t my_row = wrapped_line_rows(state.buffers[buffer_id].buffer.content[lastrow], cols, rows, senv);
      state.buffers[buffer_id].scroll_row = lastrow;
      r = 0;
      actual_rows = my_row;
      for (; r < rows; ++r)
        {
        if (state.buffers[buffer_id].scroll_row == 0)
          break;
        actual_rows += wrapped_line_rows(state.buffers[buffer_id].buffer.content[state.buffers[buffer_id].scroll_row - 1], cols, rows, senv);
        if (actual_rows <= rows)
          --state.buffers[buffer_id].scroll_row;
        else
          break;
        }
      }
    }
  else
    {
    if (state.buffers[buffer_id].scroll_row + rows > lastrow + 1)
      state.buffers[buffer_id].scroll_row = lastrow - rows + 1;
    }
  if (state.buffers[buffer_id].scroll_row < 0)
    state.buffers[buffer_id].scroll_row = 0;
  return state;
  }

int compute_rows_necessary(const app_state& state, int number_of_column_cols, int number_of_rows_available, uint32_t window_pair_id)
  {
  uint32_t command_window_id = state.window_pairs[window_pair_id].command_window_id;
  uint32_t window_id = state.window_pairs[window_pair_id].window_id;
  uint32_t command_rows = state.windows[command_window_id].rows;
  const auto& w = state.windows[window_id];
  const auto& f = state.buffers[w.buffer_id].buffer.content;
  int row = 0;
  int col = 0;
  /*
   auto it = f.begin();
   auto it_end = f.end();
   for (; it != it_end; ++it)
   {
   if (*it == '\n')
   {
   col = 0;
   ++row;
   }
   else if (w.word_wrap)
   {
   ++col;
   if (col >= w.cols - 1)
   {
   col = 0;
   ++row;
   }
   }
   else
   ++col;
   if (row >= number_of_rows_available - command_rows)
   break;
   }
   if (col != 0)
   ++row;
   ++row;
   if (row < 4)
   row = 4;
   return row + command_rows;
   */
  return command_rows + 4;
  }

int get_available_rows(const app_state& state, uint32_t target_column) {
  int rows, cols;
  getmaxyx(stdscr, rows, cols);
  rows -= 2; // make room for bottom operation stuff
  rows -= state.windows[state.g.topline_window_id].rows; // subtract rows for topline window
  const auto& c = state.g.columns[target_column];
  rows -= state.windows[c.column_command_window_id].rows; // subtract rows for column command window
  return rows;
  }

int get_y_offset_from_top(const app_state& state, uint32_t target_column) {
  int offsety = 0;
  offsety += state.windows[state.g.topline_window_id].rows;
  const auto& c = state.g.columns[target_column];
  offsety += state.windows[c.column_command_window_id].rows;
  return offsety;
  }

app_state optimize_column(app_state state, uint32_t buffer_id, settings& s)
  {
  for (uint32_t i = 0; i < state.g.columns.size(); ++i)
    {
    auto& c = state.g.columns[i];
    for (const auto& ci : c.items)
      {
      auto& wp = state.window_pairs[ci.window_pair_id];
      if (state.windows[wp.window_id].buffer_id == buffer_id)
        {
        int rows, cols;
        getmaxyx(stdscr, rows, cols);
        rows = get_available_rows(state, i);
        int offsety = get_y_offset_from_top(state, i);

        int left = (int)std::round(c.left * cols);
        int right = (int)std::round(c.right * cols);

        std::vector<int> nr_of_rows_necessary;
        int total_rows = 0;
        for (int j = 0; j < c.items.size(); ++j)
          {
          int top = (int)std::round(c.items[j].top_layer * rows);
          int bottom = (int)std::round(c.items[j].bottom_layer * rows);
          int current_rows = bottom - top;
          if (current_rows == 0)
            current_rows = 1;
          nr_of_rows_necessary.push_back(compute_rows_necessary(state, right - left, rows, c.items[j].window_pair_id));
          if (nr_of_rows_necessary.back() > current_rows)
            nr_of_rows_necessary.back() = current_rows;
          total_rows += nr_of_rows_necessary.back();
          }
        if (total_rows > rows)
          {
          for (auto& r : nr_of_rows_necessary)
            {
            double d = (double)r / (double)total_rows * (double)rows;
            r = (int)std::floor(d);
            if (r <= 0)
              ++r;
            }
          }
        else // all windows can be visualized
          {
          int extra_per_item = (rows - total_rows) / c.items.size();
          int remainder = (rows - total_rows) % c.items.size();
          for (int j = 0; j < nr_of_rows_necessary.size(); ++j)
            {
            nr_of_rows_necessary[j] += extra_per_item;
            if (j < remainder)
              nr_of_rows_necessary[nr_of_rows_necessary.size() - j - 1] += 1;
            }
          for (int j = 0; j < c.items.size(); ++j)
            {
            auto wp = c.items[j].window_pair_id;
            auto& w = state.windows[state.window_pairs[wp].window_id];
            //w.file_pos = 0;
            //w.wordwrap_row = 0;
            }
          }
        c.items.front().top_layer = 0.0;
        for (int j = 0; j < c.items.size(); ++j)
          {
          c.items[j].bottom_layer = ((double)nr_of_rows_necessary[j] + c.items[j].top_layer * rows) / (double)rows;
          if (j + 1 < c.items.size())
            c.items[j + 1].top_layer = c.items[j].bottom_layer;
          }
        c.items.back().bottom_layer = 1.0;
        for (int j = 0; j < c.items.size(); ++j)
          {
          if (c.items[j].top_layer > 1)
            c.items[j].top_layer = 1;
          if (c.items[j].bottom_layer > 1)
            c.items[j].bottom_layer = 1;
          }
        return resize_windows(state, s);
        }
      }
    }
  return resize_windows(state, s);
  }

std::optional<app_state> load_file(app_state state, uint32_t buffer_id, const std::string& filename, settings& s)
  {
  if (jtk::is_directory(filename)) {
    state = *command_new_window(state, 0xffffffff, s);
    get_active_buffer(state) = read_from_file(filename);
    int64_t command_id = state.active_buffer - 1;
    state.buffers[command_id].buffer.name = get_active_buffer(state).name;
    get_active_buffer(state) = set_multiline_comments(get_active_buffer(state));
    get_active_buffer(state) = init_lexer_status(get_active_buffer(state), convert(s));
    state.buffers[command_id].buffer.content = to_text(make_command_text(state, command_id, s));
    return check_scroll_position(state, s);
    }
  else if (jtk::file_exists(filename))
    {
    state = *command_new_window(state, state.active_buffer, s);
    get_active_buffer(state) = read_from_file(filename);
    int64_t command_id = state.active_buffer - 1;
    state.buffers[command_id].buffer.name = get_active_buffer(state).name;
    get_active_buffer(state) = set_multiline_comments(get_active_buffer(state));
    get_active_buffer(state) = init_lexer_status(get_active_buffer(state), convert(s));
    state.buffers[command_id].buffer.content = to_text(make_command_text(state, command_id, s));
    return check_scroll_position(state, s);
    }
  else
    {
    std::stringstream str;
    str << "File " << filename << " does not exist\n";
    return add_error_text(state, str.str(), s);
    }
  return state;
  }

std::vector<std::string> split_folder(const std::string& folder)
  {
  std::wstring wfolder = jtk::convert_string_to_wstring(folder);
  std::vector<std::string> out;

  while (!wfolder.empty())
    {
    auto it = wfolder.find_first_of(L'/');
    auto it_backup = wfolder.find_first_of(L'\\');
    if (it_backup < it)
      it = it_backup;
    if (it == std::wstring::npos)
      {
      out.push_back(jtk::convert_wstring_to_string(wfolder));
      wfolder.clear();
      }
    else
      {
      std::wstring part = wfolder.substr(0, it);
      wfolder.erase(0, it + 1);
      out.push_back(jtk::convert_wstring_to_string(part));
      }
    }
  return out;
  }

std::vector<std::string> simplify_split_folder(const std::vector<std::string>& split)
  {
  std::vector<std::string> out = split;
  auto it = std::find(out.begin(), out.end(), std::string(".."));
  while (it != out.end() && it != out.begin())
    {
    out.erase(it - 1, it + 1);
    it = std::find(out.begin(), out.end(), std::string(".."));
    }
  it = std::find(out.begin(), out.end(), std::string("."));
  while (it != out.end() && it != out.begin())
    {
    out.erase(it);
    it = std::find(out.begin(), out.end(), std::string("."));
    }
  return out;
  }

std::string compose_folder_from_split(const std::vector<std::string>& split)
  {
  std::string out;
  for (const auto& s : split)
    {
    out.append(s);
    out.push_back('/');
    }
  return out;
  }


std::string simplify_folder(const std::string& folder)
  {
  auto split = split_folder(folder);
  split = simplify_split_folder(split);
  std::string simplified_folder_name = compose_folder_from_split(split);
  return simplified_folder_name;
  }

std::optional<app_state> load_folder(app_state state, uint32_t buffer_id, const std::string& folder, settings& s)
  {
  std::string simplified_folder_name = simplify_folder(folder);
  if (simplified_folder_name.empty())
    {
    std::stringstream str;
    str << "Folder " << folder << " is invalid\n";
    return add_error_text(state, str.str(), s);
    }
  if (jtk::is_directory(state.buffers[buffer_id].buffer.name) && (state.windows[state.buffer_id_to_window_id[buffer_id]].wt == e_window_type::wt_normal))
    {
    int64_t command_id = buffer_id - 1;
    state.buffers[buffer_id].buffer = read_from_file(simplified_folder_name);
    state.buffers[buffer_id].buffer = set_multiline_comments(state.buffers[buffer_id].buffer);
    state.buffers[buffer_id].buffer = init_lexer_status(state.buffers[buffer_id].buffer, convert(s));
    state.buffers[buffer_id].buffer.pos = position(0, 0);
    state.buffers[buffer_id].scroll_row = 0;
    auto original_position = state.buffers[command_id].buffer.pos;
    std::optional<position> original_start_selection = state.buffers[command_id].buffer.start_selection;
    uint32_t original_first_row_length = state.buffers[command_id].buffer.content.empty() ? 0 : state.buffers[command_id].buffer.content.front().size();
    std::string user_command_text = get_user_command_text(state, command_id);
    std::string command_text = get_command_text(state, command_id, s);
    std::string total_line = simplified_folder_name + command_text + user_command_text;
    state.buffers[command_id].buffer.content = to_text(total_line);
    set_updated_command_text_position(state.buffers[command_id].buffer, original_position, original_start_selection, original_first_row_length, false);
    return check_scroll_position(state, buffer_id, s);
    }
  else
    {
    return load_file(state, buffer_id, simplified_folder_name, s);
    }
  }

std::optional<app_state> find_text(app_state state, uint32_t buffer_id, const std::wstring& command, settings& s)
  {
  if (state.operation == op_editing)
    {
    s.last_find = jtk::convert_wstring_to_string(command);
    state.buffers[buffer_id].buffer = s.case_sensitive ? find_text(state.buffers[buffer_id].buffer, command) : find_text_case_insensitive(state.buffers[buffer_id].buffer, command);
    return check_scroll_position(state, s);
    }
  return state;
  }

std::optional<app_state> load(app_state state, uint32_t buffer_id, const std::wstring& command, settings& s)
  {
  if (command.empty())
    return state;
  std::string folder = jtk::get_folder(state.buffers[buffer_id].buffer.name);
  if (folder.empty())
    folder = jtk::get_folder(jtk::get_executable_path());
  if (folder.back() != '\\' && folder.back() != '/')
    folder.push_back('/');

  std::string cmd = jtk::convert_wstring_to_string(command);
  while (!cmd.empty() && cmd.front() == '/')
    cmd.erase(cmd.begin());
  if (!cmd.empty() && cmd.front() == '"')
    cmd.erase(cmd.begin());
  if (!cmd.empty() && cmd.back() == '"')
    cmd.pop_back();
  if (!cmd.empty() && cmd.front() == '<')
    cmd.erase(cmd.begin());
  if (!cmd.empty() && cmd.back() == '>')
    cmd.pop_back();
  std::string newfilename = folder + cmd;

  if (jtk::file_exists(newfilename))
    {
    if (!ctrl_pressed()) {
      auto exe = get_plumber().get_executable_from_extension(newfilename);
      if (!exe.empty()) {
        std::vector<std::string> parameters;
        parameters.push_back(newfilename);
        return execute_external(state, exe, parameters, s);
        }
      }
    return load_file(state, buffer_id, newfilename, s);
    }

  if (jtk::is_directory(newfilename))
    {
    return load_folder(state, buffer_id, newfilename, s);
    }

  if (jtk::file_exists(jtk::convert_wstring_to_string(command)))
    {
    if (!ctrl_pressed()) {
      auto exe = get_plumber().get_executable_from_extension(newfilename);
      if (!exe.empty()) {
        std::vector<std::string> parameters;
        parameters.push_back(newfilename);
        return execute_external(state, exe, parameters, s);
        }
      }
    return load_file(state, buffer_id, jtk::convert_wstring_to_string(command), s);
    }

  if (jtk::is_directory(jtk::convert_wstring_to_string(command)))
    {
    return load_folder(state, buffer_id, jtk::convert_wstring_to_string(command), s);
    }

  if (!ctrl_pressed()) {
    auto exe = get_plumber().get_executable_from_regex(jtk::convert_wstring_to_string(command));
    if (!exe.empty()) {
      std::vector<std::string> parameters;
      parameters.push_back(jtk::convert_wstring_to_string(command));
      return execute_external(state, exe, parameters, s);
      }
    }

  return find_text(state, buffer_id, command, s);
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

std::optional<app_state> mouse_motion(app_state state, int x, int y, settings& s)
  {
  if (mouse.left_button_down)
    mouse.left_dragging = true;

  if (mouse.left_dragging)
    {
    if (mouse.rearranging_windows) {
      move(mouse.rwd.y, mouse.rwd.x - 1);
      if (mouse.rwd.x - 1 > 0)
        addch(mouse.rwd.current_sign_left);
      move(mouse.rwd.y, mouse.rwd.x);
      addch(mouse.rwd.current_sign_mid);
      move(mouse.rwd.y, mouse.rwd.x + 1);
      mouse.rwd.x = x;
      mouse.rwd.y = y;
      mouse.rwd.current_sign_left = mvinch(y, x - 1);
      mouse.rwd.current_sign_mid = mvinch(y, x);

      auto pn = PAIR_NUMBER(mouse.rwd.current_sign_left);
      short fg, bg;
      PDC_pair_content(pn, &fg, &bg);
      bool modified = (state.buffers[mouse.rwd.rearranging_file_id + 1].buffer.modification_mask & 1) != 0 && can_be_saved(state.buffers[mouse.rwd.rearranging_file_id].buffer.name); // +1 because we want to check the editor window, not the command window for modification
      unsigned int color_pair = modified ? COLOR_PAIR(command_icon_modified) : COLOR_PAIR(command_icon);
      if (bg == jedi_colors::jedi_editor_bg) {
        color_pair = modified ? COLOR_PAIR(editor_icon_modified) : COLOR_PAIR(editor_icon);
        }
      else if (bg == jedi_colors::jedi_column_command_bg) {
        color_pair = modified ? COLOR_PAIR(column_command_icon_modified) : COLOR_PAIR(column_command_icon);
        }
      else if (bg == jedi_colors::jedi_topline_command_bg) {
        color_pair = modified ? COLOR_PAIR(topline_command_icon_modified) : COLOR_PAIR(topline_command_icon);
        }


      move(y, x - 1);
      if (mouse.rwd.x - 1 > 0)
        addch('>' | color_pair);
      else {
        move(y, x);
        addch('>' | color_pair);
        }
      /*
       move(y, x);
       //addch(mouse.rwd.icon_sign);
       addch('>' | color_pair);
       move(y, x + 1);
       if (mouse.rwd.x + 1 < SP->cols)
       addch('>' | color_pair);
       */
       //addch(mouse.rwd.icon_sign);
      refresh();
      SDL_UpdateWindowSurface(pdc_window);
      return state;
      }
    else if (mouse.left_drag_start.type == screen_ex_type::SET_PLUS) {
      auto window_id = state.buffer_id_to_window_id[mouse.left_drag_start.buffer_id];
      if (window_id == state.g.topline_window_id) {
        state.windows[window_id].rows = y;
        if (state.windows[window_id].rows <= 0)
          state.windows[window_id].rows = 1;
        }
      else if (state.windows[window_id].wt == e_window_type::wt_column_command || state.windows[window_id].wt == e_window_type::wt_command) {
        int y0 = state.windows[window_id].y;
        state.windows[window_id].rows = y - y0;
        if (state.windows[window_id].rows <= 0)
          state.windows[window_id].rows = 1;
        auto column_id = get_column_id(state, mouse.left_drag_start.buffer_id);
        if (state.g.columns.size() > column_id + 1) {
          int x0 = state.windows[window_id].x;
          int x1 = x;
          if (x1 - x0 < 1)
            x1 = x0 + 1;
          auto& c = state.g.columns[column_id];
          int rows, cols;
          getmaxyx(stdscr, rows, cols);
          c.right = (double)(x1 + 1) / (double)cols;
          state.g.columns[column_id + 1].left = c.right;
          }
        }
      state = resize_windows(state, s);
      }
    else {
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
  else { // not dragging
    auto p = get_ex(y, x);
    if (p.buffer_id != 0xffffffff) {
      /*
      auto& w = state.windows[state.buffer_id_to_window_id[p.buffer_id]];
      if (w.wt == e_window_type::wt_normal) {
        if (state.last_active_editor_buffer != 0xffffffff)
        state.active_buffer = state.last_active_editor_buffer;
        } else {
        state.active_buffer = p.buffer_id;
        }
        */
      state.mouse_pointing_buffer = p.buffer_id;
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

std::optional<app_state> select_word(app_state state, int x, int y, settings& s)
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

std::optional<app_state> command_cancel(app_state state, uint32_t buffer_id, settings& s)
  {
  if (state.operation == op_editing)
    {
    return command_exit(state, buffer_id, s);
    }
  else
    {
    //state.message = string_to_line("[Cancelled]");
    state.operation = op_editing;
    state.operation_stack.clear();
    }
  return state;
  }

std::optional<app_state> command_solarized_theme(app_state state, uint32_t, settings& s)
  {
  s.color_editor_text = 0xff625b47;
  s.color_editor_background = 0xffdcf5fd;
  s.color_editor_tag = 0xff869226;

  s.color_command_text = 0xff625b47;
  s.color_command_background = 0xffcce3ea;
  s.color_command_tag = 0xff869226;

  s.color_column_command_text = 0xff625b47;
  s.color_column_command_background = 0xffbcd3da;
  s.color_column_command_tag = 0xff869226;

  s.color_topline_command_text = 0xff625b47;
  s.color_topline_command_background = 0xffacc3ca;
  s.color_topline_command_tag = 0xff869226;

  s.color_line_numbers = 0xff909081;
  s.color_scrollbar = 0xff909081;//0xffb85959;//0xff0577a5;
  s.color_scrollbar_background = 0xffcce3ea;
  s.color_icon = 0xff869226;
  s.color_icon_modified = 0xff241bd1;
  s.color_plus = 0xff869226;

  s.color_comment = 0xff058a72;//0xff869226


  s.color_string = 0xff1237bd;
  s.color_keyword = 0xffc77621;
  s.color_keyword_2 = 0xff6f1bc6;

  init_colors(s);
  stdscr->_clear = TRUE;
  return state;
  }

std::optional<app_state> command_solarized_dark_theme(app_state state, uint32_t, settings& s)
  {
  s.color_editor_text = 0xff909081;
  s.color_editor_background = 0xff282002;
  s.color_editor_tag = 0xff869226;

  s.color_command_text = 0xff909081;
  s.color_command_background = 0xff32290a;
  s.color_command_tag = 0xff869226;

  s.color_column_command_text = 0xff909081;
  s.color_column_command_background = 0xff42391a;
  s.color_column_command_tag = 0xff869226;

  s.color_topline_command_text = 0xff909081;
  s.color_topline_command_background = 0xff52492a;
  s.color_topline_command_tag = 0xff869226;

  s.color_line_numbers = 0xff625b47;
  s.color_scrollbar = 0xff625b47;//0xffb85959;//0xff0577a5;
  s.color_scrollbar_background = 0xff32290a;
  s.color_icon = 0xff869226;
  s.color_icon_modified = 0xff241bd1;
  s.color_plus = 0xff869226;

  s.color_comment = 0xff058a72;//0xff869226


  s.color_string = 0xff1237bd;
  s.color_keyword = 0xffc77621;
  s.color_keyword_2 = 0xff6f1bc6;

  init_colors(s);
  stdscr->_clear = TRUE;
  return state;
  }

std::optional<app_state> command_tomorrow_night(app_state state, uint32_t, settings& s)
  {
  s.color_editor_text = 0xffc6c8c5;
  s.color_editor_background = 0xff211f1d;
  s.color_editor_tag = 0xff68bdb5;

  s.color_command_text = 0xffc6c8c5;
  s.color_command_background = 0xff2e2a28;
  s.color_command_tag = 0xff68bdb5;

  s.color_column_command_text = 0xffc6c8c5;
  s.color_column_command_background = 0xff413b37;
  s.color_column_command_tag = 0xff68bdb5;

  s.color_topline_command_text = 0xffc6c8c5;
  s.color_topline_command_background = 0xff514b47;
  s.color_topline_command_tag = 0xff68bdb5;

  s.color_line_numbers = 0xff969896;
  s.color_scrollbar = 0xffbb94b2;
  s.color_scrollbar_background = 0xff2e2a28;
  s.color_icon = 0xff68bdb5;
  s.color_icon_modified = 0xff6666cc;
  s.color_plus = 0xff68bdb5;

  s.color_comment = 0xff969896;
  s.color_string = 0xff5f93de;
  s.color_keyword = 0xffbb94b2;
  s.color_keyword_2 = 0xffb7be8a;

  init_colors(s);
  stdscr->_clear = TRUE;
  return state;
  }

std::optional<app_state> command_tomorrow(app_state state, uint32_t, settings& s)
  {
  s.color_editor_text = 0xff4c4d4d;
  s.color_editor_background = 0xffffffff;
  s.color_editor_tag = 0xff008c71;

  s.color_command_text = 0xff4c4d4d;
  s.color_command_background = 0xffefefef;
  s.color_command_tag = 0xff008c71;

  s.color_column_command_text = 0xff4c4d4d;
  s.color_column_command_background = 0xffd6d6d6;
  s.color_column_command_tag = 0xff008c71;

  s.color_topline_command_text = 0xff4c4d4d;
  s.color_topline_command_background = 0xffc6c6c6;
  s.color_topline_command_tag = 0xff008c71;

  s.color_line_numbers = 0xff8c908e;
  s.color_scrollbar = 0xff8c908e;//0xffa85989;
  s.color_scrollbar_background = 0xffefefef;
  s.color_icon = 0xff008c71;
  s.color_icon_modified = 0xff2928c8;
  s.color_plus = 0xff008c71;

  s.color_comment = 0xff8c908e;
  s.color_string = 0xff1f87f5;
  s.color_keyword = 0xffa85989;
  s.color_keyword_2 = 0xffae7142;

  init_colors(s);
  stdscr->_clear = TRUE;
  return state;
  }

std::optional<app_state> command_dracula_theme(app_state state, uint32_t, settings& s)
  {
  s.color_editor_text = 0xfff2f8f8;
  s.color_editor_background = 0xff362a28;
  s.color_editor_tag = 0xfffde98b;

  s.color_command_text = 0xfff2f8f8;
  s.color_command_background = 0xff5a4744;
  s.color_command_tag = 0xfffde98b;

  s.color_column_command_text = 0xfff2f8f8;
  s.color_column_command_background = 0xff6a5754;
  s.color_column_command_tag = 0xfffde98b;

  s.color_topline_command_text = 0xfff2f8f8;
  s.color_topline_command_background = 0xff7a6764;
  s.color_topline_command_tag = 0xfffde98b;

  s.color_line_numbers = 0xff5a4744;
  s.color_scrollbar = 0xffc679ff;
  s.color_scrollbar_background = 0xffa47262;
  s.color_icon = 0xfffde98b;
  s.color_icon_modified = 0xff5555ff;
  s.color_plus = 0xfffde98b;

  s.color_comment = 0xffa47262;
  s.color_string = 0xff8cfaf1;
  s.color_keyword = 0xffc679ff;
  s.color_keyword_2 = 0xfff993bd;

  init_colors(s);
  stdscr->_clear = TRUE;
  return state;
  }

std::optional<app_state> command_gruvbox_theme(app_state state, uint32_t, settings& s)
  {
  s.color_editor_text = 0xffb2dbeb;
  s.color_editor_background = 0xff282828;
  s.color_editor_tag = 0xff2fbdfa;

  s.color_command_text = 0xffb2dbeb;
  s.color_command_background = 0xff36383c;
  s.color_command_tag = 0xff2fbdfa;

  s.color_column_command_text = 0xffb2dbeb;
  s.color_column_command_background = 0xff454950;
  s.color_column_command_tag = 0xff2fbdfa;

  s.color_topline_command_text = 0xffb2dbeb;
  s.color_topline_command_background = 0xff545c66;
  s.color_topline_command_tag = 0xff2fbdfa;

  s.color_line_numbers = 0xff748392;
  s.color_scrollbar = 0xff748392;
  s.color_scrollbar_background = 0xff36383c;
  s.color_icon = 0xff2fbdfa;
  s.color_icon_modified = 0xff3449fb;
  s.color_plus = 0xff2fbdfa;

  s.color_comment = 0xff748392;
  s.color_string = 0xff26bbb8;
  s.color_keyword = 0xff3449fb;
  s.color_keyword_2 = 0xff7cc08e;

  init_colors(s);
  stdscr->_clear = TRUE;
  return state;
  }


std::optional<app_state> command_gruvbox_light_theme(app_state state, uint32_t, settings& s)
  {
  s.color_editor_text = 0xff36383c;
  s.color_editor_background = 0xffc7f1fb;
  s.color_editor_tag = 0xff2166d7;//0xff1476b5;

  s.color_command_text = 0xff36383c;
  s.color_command_background = 0xffb2dbeb;
  s.color_command_tag = 0xff2166d7;

  s.color_column_command_text = 0xff36383c;
  s.color_column_command_background = 0xffa1c4d5;
  s.color_column_command_tag = 0xff2166d7;

  s.color_topline_command_text = 0xff36383c;
  s.color_topline_command_background = 0xff93aebd;
  s.color_topline_command_tag = 0xff2166d7;

  s.color_line_numbers = 0xff748392;
  s.color_scrollbar = 0xff748392;
  s.color_scrollbar_background = 0xffb2dbeb;
  s.color_icon = 0xff2166d7;
  s.color_icon_modified = 0xff1d24cc;
  s.color_plus = 0xff2166d7;

  s.color_comment = 0xff748392;
  s.color_string = 0xff0e7479;
  s.color_keyword = 0xff06009d;//0xff1d24cc;
  s.color_keyword_2 = 0xff587b42;

  init_colors(s);
  stdscr->_clear = TRUE;
  return state;
  }

std::optional<app_state> command_acme_theme(app_state state, uint32_t, settings& s)
  {
  s.color_editor_text = 0xff000000;
  s.color_editor_background = 0xffe5ffff;
  s.color_editor_tag = 0xff4c9999;

  s.color_command_text = 0xff000000;
  s.color_command_background = 0xffffffe5;
  s.color_command_tag = 0xff94933a;

  s.color_column_command_text = 0xff000000;
  s.color_column_command_background = 0xffefefd5;
  s.color_column_command_tag = 0xff94933a;

  s.color_topline_command_text = 0xff000000;
  s.color_topline_command_background = 0xffdfdfc5;
  s.color_topline_command_tag = 0xff94933a;

  s.color_line_numbers = 0xff4c9999;
  s.color_scrollbar = 0xff4c9999;
  s.color_scrollbar_background = 0xffa5dddd;
  s.color_icon = 0xffc07275;
  s.color_icon_modified = 0xff1104ae;
  s.color_plus = 0xff94933a;

  s.color_comment = 0xff036206;
  s.color_string = 0xff1104ae;
  s.color_keyword = 0xffff0000;
  s.color_keyword_2 = 0xffff8080;

  init_colors(s);
  stdscr->_clear = TRUE;
  return state;
  }

std::optional<app_state> command_dark_theme(app_state state, uint32_t, settings& s)
  {
  s.color_editor_text = 0xffc0c0c0;
  s.color_editor_background = 0xff000000;
  s.color_editor_tag = 0xfff18255;

  s.color_command_text = 0xffc0c0c0;
  s.color_command_background = 0xff282828;
  s.color_command_tag = 0xfff18255;

  s.color_column_command_text = 0xffc0c0c0;
  s.color_column_command_background = 0xff383838;
  s.color_column_command_tag = 0xfff18255;

  s.color_topline_command_text = 0xffc0c0c0;
  s.color_topline_command_background = 0xff484848;
  s.color_topline_command_tag = 0xfff18255;

  s.color_line_numbers = 0xff505050;
  s.color_scrollbar = 0xfff18255;
  s.color_scrollbar_background = 0xff282828;
  s.color_icon = 0xff64c385;
  s.color_icon_modified = 0xff6464db;
  s.color_plus = 0xff64c385;

  s.color_comment = 0xff64c385;
  s.color_string = 0xff6464db;
  s.color_keyword = 0xffff8080;
  s.color_keyword_2 = 0xffffc0c0;

  init_colors(s);
  stdscr->_clear = TRUE;
  return state;
  }

std::optional<app_state> command_matrix_theme(app_state state, uint32_t, settings& s)
  {
  s.color_editor_text = 0xff5bed08;
  s.color_editor_background = 0xff000000;
  s.color_editor_tag = 0xff00ff00;
  s.color_line_numbers = 0xff005000;

  s.color_command_text = 0xff83ff83;
  s.color_command_background = 0xff002000;
  s.color_command_tag = 0xff00ff00;

  s.color_column_command_text = 0xff83ff83;
  s.color_column_command_background = 0xff103010;
  s.color_column_command_tag = 0xff00ff00;

  s.color_topline_command_text = 0xff83ff83;
  s.color_topline_command_background = 0xff204020;
  s.color_topline_command_tag = 0xff00ff00;

  s.color_line_numbers = 0xff204020;
  s.color_scrollbar = 0xff00de89;
  s.color_scrollbar_background = 0xff103010;
  s.color_icon = 0xff90ff46;
  s.color_icon_modified = 0xff00ff00;
  s.color_plus = 0xff90ff46;

  s.color_comment = 0xff006f00;
  s.color_string = 0xff00de89;
  s.color_keyword = 0xff63ac00;
  s.color_keyword_2 = 0xff90ff46;

  init_colors(s);
  stdscr->_clear = TRUE;
  return state;
  }

std::optional<app_state> command_light_theme(app_state state, uint32_t, settings& s)
  {
  s.color_editor_text = 0xff5e5146;
  s.color_editor_background = 0xfff2edeb;
  s.color_editor_tag = 0xff89733b;

  s.color_command_text = 0xff5e5146;
  s.color_command_background = 0xffe4dad4;
  s.color_command_tag = 0xff89733b;

  s.color_column_command_text = 0xff5e5146;
  s.color_column_command_background = 0xffd4cac4;
  s.color_column_command_tag = 0xff89733b;

  s.color_topline_command_text = 0xff5e5146;
  s.color_topline_command_background = 0xffc4bab4;
  s.color_topline_command_tag = 0xff89733b;

  s.color_line_numbers = 0xffb3ada6;
  s.color_scrollbar = 0xff89733b;
  s.color_scrollbar_background = 0xffe4dad4;
  s.color_icon = 0xff89733b;
  s.color_icon_modified = 0xff7168c4;
  s.color_plus = 0xff89733b;

  s.color_comment = 0xff87bda0;
  s.color_string = 0xff7168c4;
  s.color_keyword = 0xff6784d2;
  s.color_keyword_2 = 0xffb494ba;

  init_colors(s);
  stdscr->_clear = TRUE;
  return state;
  }

std::optional<app_state> command_consolas(app_state state, uint32_t, settings& s) {
  s.font = jtk::get_folder(jtk::get_executable_path()) + "fonts/consola.ttf";
  state = resize_font(state, s.font_size, s);
  return resize_windows(state, s);
  }

std::optional<app_state> command_hack(app_state state, uint32_t, settings& s) {
  s.font = jtk::get_folder(jtk::get_executable_path()) + "fonts/Hack-Regular.ttf";
  state = resize_font(state, s.font_size, s);
  return resize_windows(state, s);
  }

std::optional<app_state> command_menlo(app_state state, uint32_t, settings& s) {
  s.font = jtk::get_folder(jtk::get_executable_path()) + "fonts/Menlo-Regular.ttf";
  state = resize_font(state, s.font_size, s);
  return resize_windows(state, s);
  }

std::optional<app_state> command_comic(app_state state, uint32_t, settings& s) {
  s.font = jtk::get_folder(jtk::get_executable_path()) + "fonts/ComicMono.ttf";
  state = resize_font(state, s.font_size, s);
  return resize_windows(state, s);
  }

std::optional<app_state> command_fantasque(app_state state, uint32_t, settings& s) {
  s.font = jtk::get_folder(jtk::get_executable_path()) + "fonts/FantasqueSansMono-Regular.ttf";
  state = resize_font(state, s.font_size, s);
  return resize_windows(state, s);
  }

std::optional<app_state> command_victor(app_state state, uint32_t, settings& s) {
  s.font = jtk::get_folder(jtk::get_executable_path()) + "fonts/VictorMono-Regular.ttf";
  state = resize_font(state, s.font_size, s);
  return resize_windows(state, s);
  }

std::optional<app_state> command_dejavusansmono(app_state state, uint32_t, settings& s) {
  s.font = jtk::get_folder(jtk::get_executable_path()) + "fonts/DejaVuSansMono.ttf";
  state = resize_font(state, s.font_size, s);
  return resize_windows(state, s);
  }

std::optional<app_state> command_firacode(app_state state, uint32_t, settings& s) {
  s.font = jtk::get_folder(jtk::get_executable_path()) + "fonts/FiraCode-Regular.ttf";
  state = resize_font(state, s.font_size, s);
  return resize_windows(state, s);
  }

std::optional<app_state> command_monaco(app_state state, uint32_t, settings& s) {
  s.font = jtk::get_folder(jtk::get_executable_path()) + "fonts/Monaco-Linux.ttf";
  state = resize_font(state, s.font_size, s);
  return resize_windows(state, s);
  }

std::optional<app_state> command_noto(app_state state, uint32_t, settings& s) {
  s.font = jtk::get_folder(jtk::get_executable_path()) + "fonts/NotoMono-Regular.ttf";
  state = resize_font(state, s.font_size, s);
  return resize_windows(state, s);
  }

std::optional<app_state> command_inconsolata(app_state state, uint32_t, settings& s) {
  s.font = jtk::get_folder(jtk::get_executable_path()) + "fonts/Inconsolata-Regular.ttf";
  state = resize_font(state, s.font_size, s);
  return resize_windows(state, s);
  }

app_state get(app_state state, uint32_t buffer_id, const settings& s)
  {
  state.buffers[buffer_id].buffer = read_from_file(state.buffers[buffer_id].buffer.name);
  state.buffers[buffer_id].buffer = set_multiline_comments(state.buffers[buffer_id].buffer);
  state.buffers[buffer_id].buffer = init_lexer_status(state.buffers[buffer_id].buffer, convert(s));
  state.operation = op_editing;
  return state;
  }

uint32_t get_editor_buffer_id(const app_state& state, uint32_t buffer_id) {
  const auto& w = state.windows[state.buffer_id_to_window_id[buffer_id]];
  if (w.wt != e_window_type::wt_normal) {
    if (w.wt == e_window_type::wt_command)
      buffer_id = w.buffer_id + 1;
    else
      buffer_id = state.active_buffer;
    }
  if (state.windows[state.buffer_id_to_window_id[buffer_id]].wt != e_window_type::wt_normal)
    return 0xffffffff;
  return buffer_id;
  }

std::optional<app_state> command_get(app_state state, uint32_t buffer_id, settings& s)
  {
  buffer_id = get_editor_buffer_id(state, buffer_id);
  if (buffer_id == 0xffffffff)
    return state;
  auto& f = state.buffers[buffer_id];
  if (f.buffer.modification_mask == 1 && can_be_saved(f.buffer.name))
    {
    f.buffer.modification_mask |= 2;
    std::stringstream str;
    str << (f.buffer.name.empty() ? std::string("<unsaved file>") : f.buffer.name) << " modified\n";
    return add_error_text(state, str.str(), s);
    }
  return get(state, buffer_id, s);
  }

std::optional<app_state> command_show_all_characters(app_state state, uint32_t, settings& s)
  {
  s.show_all_characters = !s.show_all_characters;
  return state;
  }

std::optional<app_state> command_line_numbers(app_state state, uint32_t, settings& s)
  {
  s.show_line_numbers = !s.show_line_numbers;
  return state;
  }

std::optional<app_state> command_wrap(app_state state, uint32_t, settings& s)
  {
  s.wrap = !s.wrap;
  return state;
  }

std::optional<app_state> command_case_sensitive(app_state state, uint32_t, settings& s)
  {
  s.case_sensitive = !s.case_sensitive;
  return state;
  }

std::optional<app_state> command_syntax_highlighting(app_state state, uint32_t buffer_id, settings& s)
  {
  /*
  buffer_id = get_editor_buffer_id(state, buffer_id);
  if (buffer_id == 0xffffffff)
    return state;
  auto& f = state.buffers[buffer_id];
  if (f.buffer.syntax.should_highlight) {
    f.buffer.syntax.should_highlight = false;
    f.buffer = init_lexer_status(f.buffer, convert(s));
  } else {
    auto ext = jtk::get_extension(f.buffer.name);
    auto filename = jtk::get_filename(f.buffer.name);
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return (unsigned char)std::tolower(c); });
    std::transform(filename.begin(), filename.end(), filename.begin(), [](unsigned char c) { return (unsigned char)std::tolower(c); });
    const syntax_highlighter& shl = get_syntax_highlighter();
    if (shl.extension_or_filename_has_syntax_highlighter(ext) || shl.extension_or_filename_has_syntax_highlighter(filename)) {
      f.buffer.syntax.should_highlight = true;
      f.buffer = init_lexer_status(f.buffer, convert(s));
    }
  }
  */
  s.syntax = !s.syntax;
  if (s.syntax) {
    for (auto& b : state.buffers) {
      b.buffer = init_lexer_status(b.buffer, convert(s));
      }
    }
  return state;
  }

std::optional<app_state> command_edit_with_parameters(app_state state, uint32_t buffer_id, std::wstring& sz, settings& s)
  {
  buffer_id = get_editor_buffer_id(state, buffer_id);
  if (buffer_id == 0xffffffff)
    return state;
  std::string edit_command = jtk::convert_wstring_to_string(sz);
  try {
    state.buffers[buffer_id].buffer = handle_command(state.buffers[buffer_id].buffer, edit_command, convert(s));
    }
  catch (std::runtime_error e) {
    state = add_error_text(state, e.what(), s);
    }
  return check_scroll_position(state, buffer_id, s);
  }

std::optional<app_state> command_tab(app_state state, uint32_t, std::wstring& sz, settings& s)
  {
  int save_tab_space = s.tab_space;
  std::wstringstream str;
  str << sz;
  str >> s.tab_space;
  if (s.tab_space < 0 || s.tab_space > 100)
    s.tab_space = save_tab_space;
  return state;
  }

std::optional<app_state> command_tab_spaces(app_state state, uint32_t, settings& s)
  {
  s.use_spaces_for_tab = !s.use_spaces_for_tab;
  return state;
  }

std::optional<app_state> command_all(app_state state, uint32_t buffer_id, settings& s)
  {
  switch (state.operation)
    {
    case op_replace: return replace_all(state, s);
    default: return state;
    }
  }

std::optional<app_state> command_select(app_state state, uint32_t buffer_id, settings& s)
  {
  switch (state.operation)
    {
    case op_replace: return replace_selection(state, s);
    default: return state;
    }
  }

std::optional<app_state> command_put(app_state state, uint32_t buffer_id, settings& s)
  {
  buffer_id = get_editor_buffer_id(state, buffer_id);
  if (buffer_id == 0xffffffff)
    return state;

  if (state.buffers[buffer_id].buffer.name.empty())
    {
    std::string error_message = "Error saving nameless file\n";
    return add_error_text(state, error_message, s);
    }
  if (state.buffers[buffer_id].buffer.name.back() == '/')
    {
    std::string error_message = "Error saving folder " + state.buffers[buffer_id].buffer.name + " as file\n";
    return add_error_text(state, error_message, s);
    }
  if (!can_be_saved(state.buffers[buffer_id].buffer.name))
    {
    std::string error_message = "The name " + state.buffers[buffer_id].buffer.name + " is invalid for saving\n";
    return add_error_text(state, error_message, s);
    }
  bool success = false;
  state.buffers[buffer_id].buffer = save_to_file(success, state.buffers[buffer_id].buffer, state.buffers[buffer_id].buffer.name);
  if (success)
    {
    std::string message = "Saved file " + state.buffers[buffer_id].buffer.name;
    //state.message = string_to_line(message);
    state = add_error_text(state, message, s);
    }
  else
    {
    std::string error_message = "Error saving file " + state.buffers[buffer_id].buffer.name + "\n";
    return add_error_text(state, error_message, s);
    }
  return state;
  }

std::optional<app_state> command_putall(app_state state, uint32_t buffer_id, settings& s)
  {
  for (uint32_t buffer_id = 0; buffer_id < (uint32_t)state.buffers.size(); ++buffer_id) {
    const auto& w = state.windows[state.buffer_id_to_window_id[buffer_id]];
    if (w.wt == e_window_type::wt_normal) {
      if (can_be_saved(state.buffers[buffer_id].buffer.name) && state.buffers[buffer_id].bt == e_buffer_type::bt_normal) {
        if (state.buffers[buffer_id].buffer.modification_mask & 1)
          state = *command_put(state, buffer_id, s);
        }
      }
    }
  return state;
  }

std::optional<app_state> command_help(app_state state, uint32_t buffer_id, settings& s)
  {
  std::string helppath = jtk::get_folder(jtk::get_executable_path()) + "Help.txt";
  return load_file(state, buffer_id, helppath, s);
  }

std::optional<app_state> command_open(app_state state, uint32_t buffer_id, settings& s)
  {
  state.operation = op_open;
  return clear_operation_buffer(state);
  }

std::optional<app_state> command_incremental_search(app_state state, uint32_t buffer_id, settings& s)
  {
  state.operation = op_incremental_search;
  state = clear_operation_buffer(state);
  return state;
  }

std::optional<app_state> command_piped_win(app_state state, uint32_t buffer_id, std::wstring& parameters, settings& s)
  {
  auto active_buffer = state.active_buffer;
  jtk::active_folder af(jtk::get_folder(get_active_buffer(state).name).c_str());

  state = *command_new_window(state, buffer_id, s);
  buffer_id = (uint32_t)(state.buffers.size() - 1);
  parameters = clean_command(parameters);
  if (parameters.empty()) {
    parameters = to_wstring(get_selection(state.buffers[active_buffer].buffer, convert(s)));
    parameters = clean_command(parameters);
    }
  parameters = L"=" + parameters;
  state.active_buffer = active_buffer;
  return execute(state, buffer_id, parameters, s);
  }

std::optional<app_state> command_hex(app_state state, uint32_t buffer_id, std::wstring& parameters, settings& s)
  {
  auto active_buffer = parameters.empty() ? state.last_active_editor_buffer : state.active_buffer;
  if (active_buffer == 0xffffffff)
    active_buffer = state.active_buffer;
  state = *command_new_window(state, buffer_id, s);
  buffer_id = (uint32_t)(state.buffers.size() - 1);
  parameters = clean_command(parameters);
  if (parameters.empty()) {
    parameters = to_wstring(get_selection(state.buffers[active_buffer].buffer, convert(s)));
    parameters = clean_command(parameters);
    }
  auto file_path = get_file_path(jtk::convert_wstring_to_string(parameters), state.buffers[active_buffer].buffer.name);
  if (!file_path.empty())
    parameters = jtk::convert_string_to_wstring(file_path);
  state.buffers[buffer_id].buffer.content = to_text(to_hex(jtk::convert_wstring_to_string(parameters)));//read_from_file(jtk::convert_wstring_to_string(parameters));
  state.buffers[buffer_id].buffer = set_multiline_comments(state.buffers[buffer_id].buffer);
  state.buffers[buffer_id].buffer = init_lexer_status(state.buffers[buffer_id].buffer, convert(s));
  int64_t command_id = state.active_buffer - 1;
  //state.buffers[command_id].buffer.name = state.buffers[buffer_id].buffer.name;
  state.buffers[command_id].buffer.content = to_text(make_command_text(state, command_id, s));
  return state;
  }

std::optional<app_state> command_dump(app_state state, uint32_t, settings& s)
  {
  std::stringstream str;
  save_to_stream(str, state);
  auto pos = get_actual_position(get_last_active_editor_buffer(state));
  get_last_active_editor_buffer(state) = insert(get_last_active_editor_buffer(state), str.str(), convert(s));
  get_last_active_editor_buffer(state).start_selection = pos;

  return check_scroll_position(state, state.last_active_editor_buffer, s);
  }

app_state load_dump(app_state last_state, std::istream& str, settings& s) {
  app_state state;
  try {
    state = load_from_stream(str, s);
    }
  catch (nlohmann::detail::exception e)
    {
    last_state = add_error_text(last_state, e.what(), s);
    return last_state;
    }
  uint32_t sz = (uint32_t)state.windows.size();
  auto active_buffer = state.active_buffer;
  for (uint32_t j = 0; j < sz; ++j) {
    if (state.windows[j].wt == e_window_type::wt_normal && state.buffers[state.windows[j].buffer_id].bt != e_buffer_type::bt_piped) {
      uint32_t buffer_id = state.windows[j].buffer_id;
      std::string filename = state.buffers[buffer_id].buffer.name;
      if (jtk::file_exists(filename)) {
        state.buffers[buffer_id].buffer = read_from_file(filename);
        state.buffers[buffer_id].buffer = set_multiline_comments(state.buffers[buffer_id].buffer);
        state.buffers[buffer_id].buffer = init_lexer_status(state.buffers[buffer_id].buffer, convert(s));
        }
      else if (jtk::is_directory(filename)) {
        state.buffers[buffer_id].buffer = read_from_file(filename);
        state.buffers[buffer_id].buffer = set_multiline_comments(state.buffers[buffer_id].buffer);
        state.buffers[buffer_id].buffer = init_lexer_status(state.buffers[buffer_id].buffer, convert(s));
        }
      if (state.buffers[buffer_id].scroll_row > get_last_position(state.buffers[buffer_id].buffer).row)
        state.buffers[buffer_id].scroll_row = get_last_position(state.buffers[buffer_id].buffer).row;
      }
    if (state.buffers[state.windows[j].buffer_id].bt == e_buffer_type::bt_piped) {
      uint32_t buffer_id = state.windows[j].buffer_id;
      std::string pipe_command = state.buffers[buffer_id].buffer.name;
      if (pipe_command != std::string("+Errors")) {
        state = *execute(state, buffer_id, jtk::convert_string_to_wstring(pipe_command), s);
        }
      if (state.buffers[buffer_id].scroll_row > get_last_position(state.buffers[buffer_id].buffer).row)
        state.buffers[buffer_id].scroll_row = get_last_position(state.buffers[buffer_id].buffer).row;
      }
    }
  state.active_buffer = active_buffer;
  return state;
  }

std::optional<app_state> command_load(app_state state, uint32_t, settings& s)
  {
  auto& fb = get_last_active_editor_buffer(state);
  if (has_selection(fb))
    {
    std::string string_to_load = to_string(get_selection(fb, convert(s)));
    std::string filepath = get_file_path(string_to_load, fb.name);
    if (filepath.empty()) {
      std::stringstream str;
      str << string_to_load;
      state = load_dump(state, str, s);
      }
    else {
      std::ifstream f(filepath);
      if (f.is_open())
        {
        state = load_dump(state, f, s);
        f.close();
        }
      }
    SDL_ShowCursor(1);
    SDL_SetWindowSize(pdc_window, state.w, state.h);
    SDL_SetWindowPosition(pdc_window, s.x, s.y);

    resize_term(state.h / font_height, state.w / font_width);
    resize_term_ex(state.h / font_height, state.w / font_width);

    //state.active_buffer = 0;
    state.operation = e_operation::op_editing;
    return state;
    }
  return state;
  }

std::optional<app_state> command_complete(app_state state, uint32_t buffer_id, settings& s)
  {
  std::wstring command = to_wstring(get_selection(state.buffers[buffer_id].buffer, convert(s)));
  remove_whitespace(command);
  if (command.empty()) {
    auto command_end_pos = get_previous_position(state.buffers[buffer_id].buffer, state.buffers[buffer_id].buffer.pos);
    command = find_command(state.buffers[buffer_id].buffer, command_end_pos, s);
    if (command.empty())
      return state;
    auto command_begin_pos = command_end_pos;
    command_begin_pos.col -= (int64_t)(command.length() - 1);
    std::string suggestion = complete_file_path(jtk::convert_wstring_to_string(command), state.buffers[buffer_id].buffer.name);
    if (!suggestion.empty()) {
      state.buffers[buffer_id].buffer.start_selection = command_begin_pos;
      state.buffers[buffer_id].buffer.pos = command_end_pos;
      state.buffers[buffer_id].buffer = insert(state.buffers[buffer_id].buffer, suggestion, convert(s));
      }
    return state;
    }
  std::string suggestion = complete_file_path(jtk::convert_wstring_to_string(command), state.buffers[buffer_id].buffer.name);
  if (!suggestion.empty()) {
    state.buffers[buffer_id].buffer = insert(state.buffers[buffer_id].buffer, suggestion, convert(s));
    }
  return state;
  }

std::optional<app_state> command_undo_mouseclick(app_state state, uint32_t buffer_id, settings& s)
  {
  buffer_id = get_editor_buffer_id(state, buffer_id);
  return command_undo(state, buffer_id, s);
  }

std::optional<app_state> command_redo_mouseclick(app_state state, uint32_t buffer_id, settings& s)
  {
  buffer_id = get_editor_buffer_id(state, buffer_id);
  return command_redo(state, buffer_id, s);
  }

const auto executable_commands = std::map<std::wstring, std::function<std::optional<app_state>(app_state, uint32_t, settings&)>>
  {
    {L"AcmeTheme", command_acme_theme},
    {L"All", command_all},
    {L"AllChars", command_show_all_characters},
    {L"Back", command_cancel},
    {L"Cancel", command_cancel},
    {L"Case", command_case_sensitive},
    {L"Comic", command_comic},
    {L"Complet", command_complete},
    {L"Consolas", command_consolas},
    {L"Copy", command_copy_to_snarf_buffer},
    {L"DarkTheme", command_dark_theme},
    {L"DejaVu", command_dejavusansmono},
    {L"Delcol", command_delete_column},
    {L"Del", command_delete_window},
    {L"DraculaTheme", command_dracula_theme},
    {L"Dump", command_dump},
    {L"Edit", command_edit},
    {L"Execute", command_run},
    {L"Exit", command_exit},
    {L"Fantasque", command_fantasque},
    {L"Find", command_find},
    {L"FindNxt", command_find_next},
    {L"FiraCode", command_firacode},
    {L"Get", command_get},
    {L"Goto", command_goto},
    {L"GruvboxTheme", command_gruvbox_theme},
    {L"GruvboxLight", command_gruvbox_light_theme},
    {L"Hack", command_hack},
    {L"Help", command_help},
    {L"Inconsolata", command_inconsolata},
    {L"Incr", command_incremental_search},
    {L"Kill", command_kill},
    {L"LightTheme", command_light_theme},
    {L"LineNumbers", command_line_numbers},
    {L"Load", command_load},
    {L"MatrixTheme", command_matrix_theme},
    {L"Menlo", command_menlo},
    {L"Monaco", command_monaco},
    {L"New", command_new_window},
    {L"Newcol", command_new_column},
    {L"Noto", command_noto},
    {L"Open", command_open},
    {L"Paste", command_paste_from_snarf_buffer},
    {L"Put", command_put},
    {L"Putall", command_putall},
    {L"Redo", command_redo_mouseclick},
    {L"Replace", command_replace},
    {L"Select", command_select},
    {L"Sel/all", command_select_all},
    {L"SolarizedTheme", command_solarized_theme},
    {L"SolDarkTheme", command_solarized_dark_theme},
    {L"Syntax", command_syntax_highlighting},
    {L"TabSpaces", command_tab_spaces},
    {L"TomorrowDark", command_tomorrow_night},
    {L"TomorrowTheme", command_tomorrow},
    {L"Undo", command_undo_mouseclick},
    {L"Victor", command_victor},
    {L"Wrap", command_wrap}
  };

const auto executable_commands_with_parameters = std::map<std::wstring, std::function<std::optional<app_state>(app_state, uint32_t, std::wstring&, settings&)>>
  {
    {L"Edit", command_edit_with_parameters},
    {L"Tab", command_tab},
    {L"Win", command_piped_win},
    {L"Hex", command_hex}
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
  if (pos.col < 0 || pos.row < 0)
    return std::wstring();
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

std::wstring find_bottom_line_help_command(int x, int y)
  {
  std::wstring command;
  int x_start = (x / 10) * 10 + 2;
  int x_end = x_start + 8;
  int rows, cols;
  getmaxyx(stdscr, rows, cols);
  for (int i = x_start; i < x_end && i < cols; ++i)
    {
    move(y, i);
    chtype ch[2];
    winchnstr(stdscr, ch, 1);
    command.push_back((wchar_t)(ch[0] & A_CHARTEXT));
    }
  return clean_command(command);
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
  if (pos_quote_2 + 1 >= command.size())
    {
    first = command;
    return;
    }
  first = command.substr(0, pos_quote_2 + 1);
  remainder = command.substr(pos_quote_2 + 1);
  }

char* alloc_argument(const std::string& s) {
  char* ch = new char[s.length() + 1];
  size_t i = 0;
  for (i = 0; i < s.length(); ++i)
    ch[i] = s[i];
  ch[i] = 0;
  return ch;
  }

char** alloc_arguments(const std::string& path, const std::vector<std::string>& parameters)
  {
  char** argv = new char* [parameters.size() + 2];
  //argv[0] = const_cast<char*>(path.c_str());
  argv[0] = alloc_argument(path);
  for (int j = 0; j < parameters.size(); ++j)
    //argv[j + 1] = const_cast<char*>(parameters[j].c_str());
    argv[j + 1] = alloc_argument(parameters[j]);
  argv[parameters.size() + 1] = nullptr;
  return argv;
  }

void free_arguments(char** argv)
  {
  size_t i = 0;
  while (argv[i]) {
    delete[] argv[i];
    ++i;
    }
  delete[] argv;
  }

void kill(app_state& state, uint32_t buffer_id) {
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
  }

app_state execute_external_old(app_state state, const std::string& file_path, const std::vector<std::string>& parameters, settings& s)
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
    std::string error_message = "Could not create child process\n";
    return add_error_text(state, error_message, s);
    }
  jtk::destroy_process(process, 0);
  return state;
  }

app_state execute_external(app_state state, const std::string& file_path, const std::vector<std::string>& parameters, settings& s) {
  jtk::active_folder af(jtk::get_folder(get_active_buffer(state).name).c_str());
  std::string error_filename("+Errors");
  uint32_t buffer_id = 0xffffffff;
  for (const auto& w : state.windows)
    {
    if (w.wt == wt_normal)
      {
      const auto& f = state.buffers[w.buffer_id].buffer;
      if (f.name == error_filename)
        buffer_id = w.buffer_id;
      }
    }

  if (buffer_id == 0xffffffff)
    {
    state = add_error_window(state, s);
    buffer_id = state.buffers.size() - 1;
    }
  auto active = state.active_buffer;
  state.active_buffer = buffer_id;
  get_active_buffer(state).pos = get_last_position(get_active_buffer(state));


  state = *command_kill(state, buffer_id, s);

  state.operation = op_editing;
  state.buffers[buffer_id].bt = bt_piped;

  char** argv = alloc_arguments(file_path, parameters);
#ifdef _WIN32
  state.buffers[buffer_id].process = nullptr;
  int err = jtk::create_pipe(file_path.c_str(), argv, nullptr, &state.buffers[buffer_id].process);
  free_arguments(argv);
  if (err != 0)
    {
    std::string error_message = "Could not create child process\n";
    state.buffers[buffer_id].bt = bt_normal;
    return add_error_text(state, error_message, s);
    }
  std::string text = jtk::read_from_pipe(state.buffers[buffer_id].process, 100);
#else
  int err = jtk::create_pipe(file_path.c_str(), argv, nullptr, state.buffers[buffer_id].process.data());
  free_arguments(argv);
  if (err != 0)
    {
    std::string error_message = "Could not create child process\n";
    state.buffers[buffer_id].bt = bt_normal;
    return add_error_text(state, error_message, s);
    }
  std::string text = jtk::read_from_pipe(state.buffers[buffer_id].process.data(), 100);
#endif
  if (get_active_buffer(state).pos.col > 0)
    text.insert(text.begin(), '\n');
  state.buffers[buffer_id].buffer = insert(state.buffers[buffer_id].buffer, text, convert(s));
  if (!state.buffers[buffer_id].buffer.content.empty())
    {
    auto last_line = state.buffers[buffer_id].buffer.content.back();
    state.buffers[buffer_id].piped_prompt = std::wstring(last_line.begin(), last_line.end());
    }
  state.buffers[buffer_id].buffer.pos = get_last_position(state.buffers[buffer_id].buffer);
  state.active_buffer = active;
  return check_scroll_position(state, buffer_id, s);
  }

app_state execute_external_input(app_state state, const std::string& file_path, const std::vector<std::string>& parameters, settings& s)
  {
  jtk::active_folder af(jtk::get_folder(get_last_active_editor_buffer(state).name).c_str());

  char** argv = alloc_arguments(file_path, parameters);
#ifdef _WIN32
  void* process = nullptr;
  int err = jtk::create_pipe(file_path.c_str(), argv, nullptr, &process);
  free_arguments(argv);
  if (err != 0)
    {
    std::string error_message = "Could not create child process\n";
    return add_error_text(state, error_message, s);
    }
  std::string text = jtk::read_from_pipe(process, 100);
#else
  int pipefd[3];
  int err = jtk::create_pipe(file_path.c_str(), argv, nullptr, pipefd);
  free_arguments(argv);
  if (err != 0)
    {
    std::string error_message = "Could not create child process\n";
    return add_error_text(state, error_message, s);
    }
  std::string text = jtk::read_from_pipe(pipefd, 100);
#endif

  get_last_active_editor_buffer(state) = insert(get_last_active_editor_buffer(state), text, convert(s));

#ifdef _WIN32
  jtk::close_pipe(process);
#else
  jtk::close_pipe(pipefd);
#endif
  return state;
  }

app_state execute_external_output(app_state state, const std::string& file_path, const std::vector<std::string>& parameters, settings& s)
  {
  auto woutput = to_wstring(get_selection(get_last_active_editor_buffer(state), convert(s)));
  woutput.erase(std::remove(woutput.begin(), woutput.end(), '\r'), woutput.end());
  if (!woutput.empty() && woutput.back() != '\n')
    woutput.push_back('\n');
  auto output = jtk::convert_wstring_to_string(woutput);

  jtk::active_folder af(jtk::get_folder(get_last_active_editor_buffer(state).name).c_str());

  char** argv = alloc_arguments(file_path, parameters);
#ifdef _WIN32
  void* process = nullptr;
  int err = jtk::create_pipe(file_path.c_str(), argv, nullptr, &process);
  free_arguments(argv);
  if (err != 0)
    {
    std::string error_message = "Could not create child process\n";
    return add_error_text(state, error_message, s);
    }
  int res = jtk::send_to_pipe(process, output.c_str());
  if (res != NO_ERROR)
    {
    std::string error_message = "Error writing to external process\n";
    return add_error_text(state, error_message, s);
    }
  jtk::close_pipe(process);
#else
  int pipefd[3];
  int err = jtk::create_pipe(file_path.c_str(), argv, nullptr, pipefd);
  free_arguments(argv);
  if (err != 0)
    {
    std::string error_message = "Could not create child process\n";
    return add_error_text(state, error_message, s);
    }
  jtk::send_to_pipe(pipefd, output.c_str());
  jtk::close_pipe(pipefd);
#endif

  return state;
  }


app_state execute_external_input_output(app_state state, const std::string& file_path, const std::vector<std::string>& parameters, settings& s)
  {
  auto woutput = to_wstring(get_selection(get_last_active_editor_buffer(state), convert(s)));
  woutput.erase(std::remove(woutput.begin(), woutput.end(), '\r'), woutput.end());
  if (!woutput.empty() && woutput.back() != '\n')
    woutput.push_back('\n');
  auto output = jtk::convert_wstring_to_string(woutput);

  jtk::active_folder af(jtk::get_folder(get_last_active_editor_buffer(state).name).c_str());

  char** argv = alloc_arguments(file_path, parameters);
#ifdef _WIN32
  void* process = nullptr;
  int err = jtk::create_pipe(file_path.c_str(), argv, nullptr, &process);
  free_arguments(argv);
  if (err != 0)
    {
    std::string error_message = "Could not create child process\n";
    return add_error_text(state, error_message, s);
    }
  int res = jtk::send_to_pipe(process, output.c_str());
  if (res != NO_ERROR)
    {
    std::string error_message = "Error writing to external process\n";
    return add_error_text(state, error_message, s);
    }
  std::string text = jtk::read_from_pipe(process, 100);
#else
  int pipefd[3];
  int err = jtk::create_pipe(file_path.c_str(), argv, nullptr, pipefd);
  free_arguments(argv);
  if (err != 0)
    {
    std::string error_message = "Could not create child process\n";
    return add_error_text(state, error_message, s);
    }
  jtk::send_to_pipe(pipefd, output.c_str());
  std::string text = jtk::read_from_pipe(pipefd, 100);
#endif

  get_last_active_editor_buffer(state) = insert(get_last_active_editor_buffer(state), text, convert(s));

#ifdef _WIN32
  jtk::close_pipe(process);
#else
  jtk::close_pipe(pipefd);
#endif
  return state;
  }

std::optional<app_state> command_kill(app_state state, uint32_t, settings& s)
  {
  kill(state, state.last_active_editor_buffer);
  return state;
  }

app_state start_pipe(app_state state, uint32_t buffer_id, const std::string& inputfile, const std::vector<std::string>& parameters, settings& s)
  {
  state = *command_kill(state, buffer_id, s);
  //state.buffer = make_empty_buffer();
  state.buffers[buffer_id].buffer.name = std::string("=") + std::string("\"") + inputfile + std::string("\"");

  for (const auto& p : parameters) {
    state.buffers[buffer_id].buffer.name += std::string(" ") + std::string("\"") + p + std::string("\"");
    }

  state.buffers[buffer_id - 1].buffer.name = state.buffers[buffer_id].buffer.name;

  //state.buffers[buffer_id-1].buffer.pos = position(0, 0);
  //state.buffers[buffer_id-1].buffer = insert(state.buffers[buffer_id-1].buffer, state.buffers[buffer_id-1].buffer.name, convert(s), false);
  state = set_filename(state, buffer_id - 1, s);

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
    std::string error_message = "Could not create child process\n";
    state.buffers[buffer_id].bt = bt_normal;
    return add_error_text(state, error_message, s);
    }
  std::string text = jtk::read_from_pipe(state.buffers[buffer_id].process, 100);
#else
  int err = jtk::create_pipe(inputfile.c_str(), argv, nullptr, state.buffers[buffer_id].process.data());
  free_arguments(argv);
  if (err != 0)
    {
    std::string error_message = "Could not create child process\n";
    state.buffers[buffer_id].bt = bt_normal;
    return add_error_text(state, error_message, s);
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

app_state start_pipe(app_state state, uint32_t buffer_id, const std::string& inputfile, int argc, char** argv, settings& s)
  {
  std::vector<std::string> parameters;
  for (int j = 2; j < argc; ++j)
    parameters.emplace_back(argv[j]);
  return start_pipe(state, buffer_id, inputfile, parameters, s);
  }

std::optional<app_state> execute(app_state state, uint32_t buffer_id, const std::wstring& command, const std::wstring& optional_parameters, settings& s)
  {
  auto it = executable_commands.find(command);
  if (it != executable_commands.end())
    {
    return it->second(state, buffer_id, s);
    }

  std::wstring cmd_id, cmd_remainder;
  split_command(cmd_id, cmd_remainder, command);

  if (!optional_parameters.empty()) {
    if (cmd_remainder.empty())
      cmd_remainder = optional_parameters;
    else
      cmd_remainder += L" " + optional_parameters;
    }

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
    return it2->second(state, buffer_id, cmd_remainder, s);
    }

  auto file_path = get_file_path(jtk::convert_wstring_to_string(cmd_id), get_active_buffer(state).name);

  if (file_path.empty())
    {
    std::stringstream error_text;
    error_text << "invalid path: " << jtk::convert_wstring_to_string(cmd_id) << "\n";
    state = add_error_text(state, error_text.str(), s);
    return state;
    }

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
    else {
      parameters.push_back(par_path);
#ifdef _WIN32
      if (parameters.back().find(' ') != std::string::npos) {
        parameters.back().insert(parameters.back().begin(), '"');
        parameters.back().push_back('"');
        has_quotes = false; // set to false as we've already added quotes
        }
#endif
      }
#ifdef _WIN32
    if (has_quotes)
      {
      parameters.back().insert(parameters.back().begin(), '"');
      parameters.back().push_back('"');
      }
#endif
    cmd_remainder = clean_command(rest);
    }

  std::stringstream error_text;
  error_text << "executing " << pipe_cmd << file_path;
  for (const auto& p : parameters)
    error_text << " " << p;
  error_text << "\n";
  state = add_error_text(state, error_text.str(), s);
  if (pipe_cmd == '!')
    return execute_external(state, file_path, parameters, s);
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


std::optional<app_state> execute(app_state state, uint32_t buffer_id, const std::wstring& command, settings& s)
  {
  std::wstring optional_parameters;
  return execute(state, buffer_id, command, optional_parameters, s);
  }

app_state move_column(app_state state, uint64_t c, int x, int y, const settings& s)
  {
  int rows, cols;
  getmaxyx(stdscr, rows, cols);

  auto& column = state.g.columns[c];

  int left = (int)std::round(column.left * cols);
  int right = (int)std::round(column.right * cols);

  if (left < x)
    {
    if (!c)
      return state;
    while (x >= right)
      --x;
    if (x < right)
      {
      while (right - x < 2) // leave at least column width 2 for collapsing column
        --x;
      column.left = x / double(cols);
      state.g.columns[c - 1].right = column.left;
      return resize_windows(state, s);
      }
    }
  else // left > x
    {
    if (!c)
      return state;
    int leftleft = (int)(state.g.columns[c - 1].left * cols);
    while (x <= leftleft)
      ++x;
    if (leftleft < x)
      {
      while (x - leftleft < 2) // leave at least column width 2 for collapsing column
        ++x;
      column.left = x / double(cols);
      state.g.columns[c - 1].right = column.left;
      return resize_windows(state, s);
      }
    }
  return state;
  }

app_state move_window_to_other_column(app_state state, uint64_t c, uint64_t ci, int x, int y, const settings& s)
  {
  int rows, cols;
  getmaxyx(stdscr, rows, cols);

  uint64_t target_c = 0;
  for (; target_c < state.g.columns.size(); ++target_c)
    {
    auto& local_col = state.g.columns[target_c];

    int left = (int)std::round(local_col.left * cols);
    int right = (int)std::round(local_col.right * cols);

    if (x >= left && x <= right)
      break;
    }

  if (c == target_c)
    return state;

  auto& source_column = state.g.columns[c];
  auto& target_column = state.g.columns[target_c];

  auto item = source_column.items[ci];

  if (ci)
    source_column.items[ci - 1].bottom_layer = item.bottom_layer;
  else if (ci + 1 < source_column.items.size())
    source_column.items[ci + 1].top_layer = item.top_layer;

  source_column.items.erase(source_column.items.begin() + ci);

  int left = (int)(target_column.left * cols);
  int right = (int)(target_column.right * cols);

  int irows = get_available_rows(state, target_c);

  y -= get_y_offset_from_top(state, target_c);
  if (y < 0)
    y = 0;

  uint64_t new_pos = 0;
  for (; new_pos < target_column.items.size(); ++new_pos)
    {
    int top = (int)std::round(target_column.items[new_pos].top_layer * irows);
    if (top > y)
      break;
    }

  target_column.items.insert(target_column.items.begin() + new_pos, item);
  if (new_pos > 0)
    {
    double new_bot = (y) / double(irows);
    target_column.items[new_pos].bottom_layer = target_column.items[new_pos - 1].bottom_layer;
    target_column.items[new_pos - 1].bottom_layer = new_bot;
    target_column.items[new_pos].top_layer = new_bot;
    }
  else if (new_pos + 1 < target_column.items.size())
    {
    if (target_column.items[new_pos + 1].top_layer != 0.0)
      {
      target_column.items[new_pos].bottom_layer = target_column.items[new_pos + 1].top_layer;
      target_column.items[new_pos].top_layer = 0.0;
      }
    else
      {
      double new_top = (target_column.items[new_pos + 1].top_layer + target_column.items[new_pos + 1].bottom_layer) * 0.5;
      target_column.items[new_pos + 1].top_layer = new_top;
      target_column.items[new_pos].bottom_layer = new_top;
      target_column.items[new_pos].top_layer = 0.0;
      }
    }
  else
    {
    target_column.items[new_pos].top_layer = 0.0;
    target_column.items[new_pos].bottom_layer = 1.0;
    }

  double last_top_layer = target_column.items[new_pos].top_layer;
  for (int other_ci = new_pos - 1; other_ci >= 0; --other_ci)
    {
    auto& other_col_item = target_column.items[other_ci];
    other_col_item.bottom_layer = last_top_layer;
    if (other_col_item.top_layer >= other_col_item.bottom_layer)
      {
      other_col_item.top_layer = ((int)std::round(other_col_item.bottom_layer * irows) - 1) / double(irows);
      }
    last_top_layer = other_col_item.top_layer;
    }
  return resize_windows(state, s);
  }

app_state move_window_to_top(app_state state, uint64_t c, int64_t ci, const settings& s)
  {
  auto& column = state.g.columns[c];
  auto col_item = column.items[ci];
  for (int i = ci - 1; i >= 0; --i)
    column.items[i + 1] = column.items[i];
  column.items[0] = col_item;
  column.items[0].top_layer = 0.0;
  column.items[0].bottom_layer = column.items[1].top_layer;
  for (int i = 1; i < column.items.size() - 1; ++i)
    column.items[i].bottom_layer = column.items[i + 1].top_layer;
  column.items.back().bottom_layer = 1.0;
  return resize_windows(state, s);
  //return *optimize_column(state, state.windows[state.window_pairs[col_item.window_pair_id].window_id].file_id);
  }

int get_minimum_number_of_rows(const column_item& ci, const app_state& state) {
  int rows = state.windows[state.window_pairs[ci.window_pair_id].command_window_id].rows;
  return rows <= 0 ? 1 : rows;
  }

app_state move_window_up_down(app_state state, uint64_t c, int64_t ci, int x, int y, const settings& s)
  {
  int rows, cols;
  getmaxyx(stdscr, rows, cols);

  auto& column = state.g.columns[c];

  auto& col_item = column.items[ci];

  int left = (int)std::round(column.left * cols);
  int right = (int)std::round(column.right * cols);

  int irows = get_available_rows(state, c);

  int top = (int)std::round(col_item.top_layer * irows);
  int bottom = (int)std::round(col_item.bottom_layer * irows);

  auto y_offset = get_y_offset_from_top(state, c);

  y -= y_offset;

  if (y < top)
    {
    // First we check whether we want to move this item to the top.
    if (ci) // if ci == 0, then it is already at the top
      {
      int top_top = (int)std::round(column.items[0].top_layer * irows);
      if (y < top_top)
        return move_window_to_top(state, c, ci, s);
      }

    int minimum_size_for_higher_items = 0;
    for (int64_t other = (int64_t)ci - 1; other >= 0; --other)
      minimum_size_for_higher_items += get_minimum_number_of_rows(column.items[other], state);
    if (y < minimum_size_for_higher_items)
      y = minimum_size_for_higher_items;

    col_item.top_layer = (y) / double(irows);
    double last_top_layer = col_item.top_layer;
    for (int other_ci = ci - 1; other_ci >= 0; --other_ci)
      {
      auto& other_col_item = column.items[other_ci];
      other_col_item.bottom_layer = last_top_layer;
      if (other_col_item.top_layer >= other_col_item.bottom_layer)
        {
        other_col_item.top_layer = ((int)std::round(other_col_item.bottom_layer * irows) - 1) / double(irows);
        }
      last_top_layer = other_col_item.top_layer;
      }
    }
  else if (y >= top && y < bottom)
    {
    int minimum_nr_rows = get_minimum_number_of_rows(col_item, state);

    // y + minimum_nr_rows > bottom
    while (bottom < (y + minimum_nr_rows))
      --y;
    col_item.top_layer = (y) / double(irows);
    if (ci > 0)
      column.items[ci - 1].bottom_layer = col_item.top_layer;
    else
      {
      invalidate_range(left, y_offset, right - left, bottom - top);
      }
    }
  else
    {
    //int minimum_size_for_lower_items = column.items.size() - ci;
    int minimum_size_for_lower_items = 0;
    for (int64_t other = (int64_t)ci; other < column.items.size(); ++other)
      {
      minimum_size_for_lower_items += get_minimum_number_of_rows(column.items[other], state);
      }
    if (irows - minimum_size_for_lower_items < y)
      y = irows - minimum_size_for_lower_items;

    col_item.top_layer = (y) / double(irows);
    if (col_item.bottom_layer <= col_item.top_layer)
      col_item.bottom_layer = ((int)std::round(col_item.top_layer * irows) + get_minimum_number_of_rows(col_item, state)) / (double)(irows);
    if (ci > 0)
      {
      column.items[ci - 1].bottom_layer = col_item.top_layer;
      }
    else
      {
      invalidate_range(left, y_offset, right - left, bottom - top);
      }

    double last_bottom_layer = col_item.bottom_layer;
    for (int64_t other = (int64_t)ci + 1; other < column.items.size(); ++other)
      {
      auto& other_col_item = column.items[other];
      other_col_item.top_layer = last_bottom_layer;
      if (other_col_item.bottom_layer <= other_col_item.top_layer)
        other_col_item.bottom_layer = ((int)std::round(other_col_item.top_layer * irows) + +get_minimum_number_of_rows(other_col_item, state)) / (double)(irows);
      last_bottom_layer = other_col_item.bottom_layer;
      }
    /*
     int minimum_nr_rows = get_minimum_number_of_rows(col_item, right - left, state);
     col_item.top_layer = ((int)std::round(col_item.bottom_layer*irows) - minimum_nr_rows) / double(irows);
     if (ci > 0)
     column.items[ci - 1].bottom_layer = col_item.top_layer;
     else
     {
     invalidate_range(left, y_offset, right - left, bottom - top);
     }
     */
    }
  return resize_windows(state, s);
  }

app_state enlarge_window_as_much_as_possible(app_state state, int64_t buffer_id, const settings& s)
  {
  int64_t win_id = state.buffer_id_to_window_id[buffer_id];
  for (uint64_t c = 0; c < state.g.columns.size(); ++c)
    {
    auto& column = state.g.columns[c];
    for (uint64_t ci = 0; ci < column.items.size(); ++ci)
      {
      uint32_t win_pair = column.items[ci].window_pair_id;
      if (state.window_pairs[win_pair].window_id == win_id || state.window_pairs[win_pair].command_window_id == win_id)
        {
        column.contains_maximized_item = false;
        //int icols = get_cols();
        auto& col_item = column.items[ci];

        int rows, cols;
        getmaxyx(stdscr, rows, cols);

        int left = (int)std::round(column.left * cols);
        int right = (int)std::round(column.right * cols);

        int irows = get_available_rows(state, c);

        int minimum_size_for_higher_items = 0;
        for (int64_t other = (int64_t)ci - 1; other >= 0; --other)
          minimum_size_for_higher_items += get_minimum_number_of_rows(column.items[other], state);

        int top = minimum_size_for_higher_items;

        int minimum_size_for_lower_items = 0;
        for (uint64_t other = ci + 1; other < column.items.size(); ++other)
          minimum_size_for_lower_items += get_minimum_number_of_rows(column.items[other], state);


        int bottom = irows - minimum_size_for_lower_items;

        double new_top = top / (double)irows;
        double new_bottom = bottom / (double)irows;

        col_item.top_layer = new_top;
        col_item.bottom_layer = new_bottom;

        for (uint64_t other = ci + 1; other < column.items.size(); ++other)
          {
          column.items[other].top_layer = new_bottom;
          bottom += get_minimum_number_of_rows(column.items[other], state);
          new_bottom = bottom / (double)irows;
          column.items[other].bottom_layer = new_bottom;
          }
        for (int64_t other = (int64_t)ci - 1; other >= 0; --other)
          {
          column.items[other].bottom_layer = new_top;
          top -= get_minimum_number_of_rows(column.items[other], state);
          new_top = top / (double)irows;
          column.items[other].top_layer = new_top;
          }

        return resize_windows(state, s);
        }
      }
    }
  return state;
  }


app_state enlarge_window(app_state state, int64_t buffer_id, const settings& s)
  {
  int64_t win_id = state.buffer_id_to_window_id[buffer_id];
  for (uint64_t c = 0; c < state.g.columns.size(); ++c)
    {
    auto& column = state.g.columns[c];
    for (uint64_t ci = 0; ci < column.items.size(); ++ci)
      {
      uint32_t win_pair = column.items[ci].window_pair_id;
      if (state.window_pairs[win_pair].window_id == win_id || state.window_pairs[win_pair].command_window_id == win_id)
        {
        if (column.contains_maximized_item)
          {
          column.contains_maximized_item = false;
          return enlarge_window_as_much_as_possible(state, buffer_id, s);
          }
        int rows, cols;
        getmaxyx(stdscr, rows, cols);

        auto& col_item = column.items[ci];

        int left = (int)std::round(column.left * cols);
        int right = (int)std::round(column.right * cols);

        int irows = get_available_rows(state, c);

        int bottom = (int)std::round(col_item.bottom_layer * irows);

        int minimum_size_for_lower_items = 0;
        for (uint64_t other = ci + 1; other < column.items.size(); ++other)
          minimum_size_for_lower_items += get_minimum_number_of_rows(column.items[other], state);

        ++bottom;
        while (bottom > irows - minimum_size_for_lower_items)
          --bottom;


        if (bottom == (int)std::round(col_item.bottom_layer * irows)) // could not enlarge at the bottom, try the top
          {
          int top = (int)std::round(col_item.top_layer * irows);

          int minimum_size_for_upper_items = 0;
          for (uint64_t other = 0; other < ci; ++other)
            minimum_size_for_upper_items += get_minimum_number_of_rows(column.items[other], state);

          --top;
          while (top < minimum_size_for_upper_items)
            ++top;

          double new_top = top / (double)irows;
          col_item.top_layer = new_top;
          for (int64_t other = ci - 1; other >= 0; --other)
            {
            column.items[other].bottom_layer = new_top;
            int bottom = top;
            top = (int)std::round(column.items[other].top_layer * irows);
            new_top = column.items[other].top_layer;
            if (bottom - top < get_minimum_number_of_rows(column.items[other], state))
              {
              top = bottom - get_minimum_number_of_rows(column.items[other], state);
              new_top = top / (double)irows;
              }
            column.items[other].top_layer = new_top;
            }
          }
        else
          {
          double new_bottom = bottom / (double)irows;

          col_item.bottom_layer = new_bottom;

          for (uint64_t other = ci + 1; other < column.items.size(); ++other)
            {
            column.items[other].top_layer = new_bottom;
            int top = bottom;
            bottom = (int)std::round(column.items[other].bottom_layer * irows);
            new_bottom = column.items[other].bottom_layer;
            if (bottom - top < get_minimum_number_of_rows(column.items[other], state))
              {
              bottom = top + get_minimum_number_of_rows(column.items[other], state);
              new_bottom = bottom / (double)irows;
              }
            column.items[other].bottom_layer = new_bottom;
            }
          }

        int top = (int)std::round(col_item.top_layer * irows) + get_y_offset_from_top(state, c);
        SDL_WarpMouseInWindow(pdc_window, left * font_width + font_width / 2.0, top * font_height + font_height / 2.0); // move mouse on icon, so that you can keep clicking

        return resize_windows(state, s);
        }
      }
    }
  return state;
  }

app_state maximize_window(app_state state, int64_t buffer_id, const settings& s)
  {
  int64_t win_id = state.buffer_id_to_window_id[buffer_id];
  for (uint64_t c = 0; c < state.g.columns.size(); ++c)
    {
    auto& column = state.g.columns[c];
    for (uint64_t ci = 0; ci < column.items.size(); ++ci)
      {
      uint32_t win_pair = column.items[ci].window_pair_id;
      if (state.window_pairs[win_pair].window_id == win_id || state.window_pairs[win_pair].command_window_id == win_id)
        {
        column.contains_maximized_item = true;
        //int icols = get_cols();
        auto& col_item = column.items[ci];

        col_item.top_layer = 0.0;
        col_item.bottom_layer = 1.0;

        for (uint64_t other = ci + 1; other < column.items.size(); ++other)
          {
          column.items[other].top_layer = 1.0;
          column.items[other].bottom_layer = 1.0;
          }
        for (int64_t other = (int64_t)ci - 1; other >= 0; --other)
          {
          column.items[other].top_layer = 0.0;
          column.items[other].bottom_layer = 0.0;
          }

        return resize_windows(state, s);
        }
      }
    }
  return state;
  }

app_state adapt_grid(app_state state, int x, int y, const settings& s)
  {
  //int icols = get_cols();
  //int irows = get_lines();
  int rows, cols;
  getmaxyx(stdscr, rows, cols);

  int64_t win_id = state.buffer_id_to_window_id[mouse.rwd.rearranging_file_id];
  for (uint64_t c = 0; c < state.g.columns.size(); ++c)
    {
    if (state.g.columns[c].column_command_window_id == win_id)
      return move_column(state, c, x, y, s);

    auto& column = state.g.columns[c];

    int left = (int)std::round(column.left * cols);
    int right = (int)std::round(column.right * cols);

    for (uint64_t ci = 0; ci < column.items.size(); ++ci)
      {
      uint32_t win_pair = column.items[ci].window_pair_id;
      if (state.window_pairs[win_pair].window_id == win_id || state.window_pairs[win_pair].command_window_id == win_id)
        {
        if (x >= left - 2 && x <= right + 2)
          return move_window_up_down(state, c, ci, x, y, s);
        else
          return move_window_to_other_column(state, c, ci, x, y, s);
        }
      }
    }
  return state;
  }

std::optional<app_state> left_mouse_button_down(app_state state, int x, int y, bool double_click, settings& s)
  {
  screen_ex_pixel p = get_ex(y, x);
  mouse.left_button_down = true;

  if (p.buffer_id == 0xffffffff)
    return state;

  //bool swap_window = state.active_buffer != p.buffer_id;
  const bool new_active_buffer = state.active_buffer != p.buffer_id;
  state.active_buffer = p.buffer_id;
  auto& w = state.windows[state.buffer_id_to_window_id[p.buffer_id]];
  if (w.wt == e_window_type::wt_normal) {
    state.last_active_editor_buffer = p.buffer_id;
    }

  if (p.type == SET_SCROLLBAR_EDITOR)
    {
    return state;
    }

  if (p.type == SET_COMMAND_ICON) {
    mouse.rearranging_windows = true;
    mouse.rwd.rearranging_file_id = p.buffer_id;
    mouse.rwd.x = x;
    mouse.rwd.y = y;
    mouse.rwd.current_sign_left = mvinch(y, x - 1);
    mouse.rwd.current_sign_mid = mvinch(y, x);
    return state;
    }

  if (double_click)
    {
    mouse.left_button_down = false;
    return select_word(state, x, y, s);
    }

  if (new_active_buffer && has_nontrivial_selection(get_active_buffer(state), convert(s)))
    return state;
  mouse.left_drag_start = find_mouse_text_pick(x, y);
  if (mouse.left_drag_start.type == SET_TEXT_EDITOR || mouse.left_drag_start.type == SET_TEXT_COMMAND)
    {
    state.operation = op_editing;

    if (!keyb_data.selecting)
      {
      get_active_buffer(state).start_selection = mouse.left_drag_start.pos;
      get_active_buffer(state).rectangular_selection = alt_pressed();
      }
    get_active_buffer(state) = update_position(get_active_buffer(state), mouse.left_drag_start.pos, convert(s));
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

std::optional<app_state> middle_mouse_button_down(app_state state, int x, int y, bool double_click, settings& s)
  {
  mouse.middle_button_down = true;
  return state;
  }

std::optional<app_state> right_mouse_button_down(app_state state, int x, int y, bool double_click, settings& s)
  {
  screen_ex_pixel p = get_ex(y, x);
  mouse.right_button_down = true;
  return state;
  }

std::optional<app_state> left_mouse_button_up(app_state state, int x, int y, settings& s)
  {
  if (!mouse.left_button_down) // we come from a double click
    return state;

  bool was_dragging = mouse.left_dragging;

  mouse.left_dragging = false;
  mouse.left_button_down = false;

  auto p = get_ex(y, x);

  if (mouse.rearranging_windows) {
    mouse.rearranging_windows = false;
    if (p.buffer_id == mouse.rwd.rearranging_file_id && p.type == SET_COMMAND_ICON)
      {
      return enlarge_window(state, p.buffer_id, s);
      }
    return adapt_grid(state, x, y, s);
    }

  if (p.type == SET_SCROLLBAR_EDITOR && !was_dragging)
    {
    int offsetx, offsety, cols, rows;
    get_active_window_rect_for_editing(offsetx, offsety, rows, cols, state, s);
    double fraction = (double)(y - offsety) / (double)rows;
    int steps = (int)(fraction * rows);
    if (steps < 1)
      steps = 1;
    return move_editor_window_up_down(state, p.buffer_id, -steps, s);
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

  if (p.buffer_id == 0xffffffff) {
    if (p.type == SET_NONE)
      {
      std::wstring command = find_bottom_line_help_command(x, y);

      return execute(state, state.active_buffer, command, s);
      }

    return state;
    }

  if (p.type == SET_SCROLLBAR_EDITOR)
    {
    state.active_buffer = p.buffer_id;
    state.last_active_editor_buffer = p.buffer_id;
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
    std::wstring optional_parameters;
    if (state.active_buffer != 0xffffffff) {
      if (has_nontrivial_selection(state.buffers[state.active_buffer].buffer, convert(s)))
        optional_parameters = to_wstring(get_selection(state.buffers[state.active_buffer].buffer, convert(s)));
      remove_whitespace(optional_parameters);
      if (optional_parameters == command)
        optional_parameters.clear();
      }
    return execute(state, p.buffer_id, command, optional_parameters, s);
    }

  if (p.type == SET_COMMAND_ICON)
    {
    return enlarge_window_as_much_as_possible(state, p.buffer_id, s);
    }

  if (p.type == SET_NONE)
    {
    // to add when implementing commands
    std::wstring command = find_bottom_line_help_command(x, y);
    return execute(state, state.active_buffer, command, s);
    }

  return state;
  }

std::optional<app_state> right_mouse_button_up(app_state state, int x, int y, settings& s)
  {
  mouse.right_button_down = false;

  screen_ex_pixel p = get_ex(y, x);

  if (p.buffer_id == 0xffffffff) {
    if (p.type == SET_NONE)
      {
      std::wstring command = find_bottom_line_help_command(x, y);
      return load(state, state.active_buffer, command, s);
      }
    return state;
    }

  if (p.type == SET_SCROLLBAR_EDITOR)
    {
    int offsetx, offsety, cols, rows;
    get_active_window_rect_for_editing(offsetx, offsety, rows, cols, state, s);
    double fraction = (double)(y - offsety) / (double)rows;
    int steps = (int)(fraction * rows);
    if (steps < 1)
      steps = 1;
    return move_editor_window_up_down(state, p.buffer_id, steps, s);
    }

  if (p.type == SET_TEXT_EDITOR || p.type == SET_TEXT_COMMAND)
    {
    std::wstring command = find_command(state.buffers[p.buffer_id].buffer, p.pos, s);
    return load(state, p.buffer_id, command, s);
    }

  if (p.type == SET_COMMAND_ICON)
    {
    return maximize_window(state, p.buffer_id, s);
    }

  return state;
  }


std::optional<app_state> command_undo(app_state state, uint32_t buffer_id, settings& s)
  {
  //state.message = string_to_line("[Undo]");
  if (state.operation == op_editing)
    state.buffers[buffer_id].buffer = undo(state.buffers[buffer_id].buffer, convert(s));
  else
    state.operation_buffer = undo(state.operation_buffer, convert(s));
  return check_scroll_position(state, buffer_id, s);
  }

std::optional<app_state> command_redo(app_state state, uint32_t buffer_id, settings& s)
  {
  //state.message = string_to_line("[Redo]");
  if (state.operation == op_editing)
    state.buffers[buffer_id].buffer = redo(state.buffers[buffer_id].buffer, convert(s));
  else
    state.operation_buffer = redo(state.operation_buffer, convert(s));
  return check_scroll_position(state, buffer_id, s);
  }

#ifndef _WIN32
std::string pbpaste()
  {
#if defined(__APPLE__)
  FILE* pipe = popen("pbpaste", "r");
#else
  FILE* pipe = popen("xclip -o", "r");
#endif
  if (!pipe) return "ERROR";
  char buffer[128];
  std::string result = "";
  while (!feof(pipe))
    {
    if (fgets(buffer, 128, pipe) != NULL)
      {
      result += buffer;
      }
    }
  pclose(pipe);
  return result;
  }
#endif

std::optional<app_state> command_copy_to_snarf_buffer(app_state state, uint32_t, settings& s)
  {
  if (state.operation == op_editing)
    state.snarf_buffer = get_selection(get_active_buffer(state), convert(s));
  else
    state.snarf_buffer = get_selection(state.operation_buffer, convert(s));
  //state.message = string_to_line("[Copy]");
#ifdef _WIN32
  std::wstring txt = to_wstring(state.snarf_buffer);
  copy_to_windows_clipboard(jtk::convert_wstring_to_string(txt));
#else
  std::string txt = to_string(state.snarf_buffer);
  int pipefd[3];
#if defined(__APPLE__)
  std::string pbcopy = get_file_path("pbcopy", "");
#else
  std::string pbcopy = get_file_path("xclip", "");
#endif
  char** argv = alloc_arguments(pbcopy, std::vector<std::string>());
  int err = jtk::create_pipe(pbcopy.c_str(), argv, nullptr, pipefd);
  free_arguments(argv);
  if (err != 0)
    {
    std::string error_message = "Could not create child process\n";
    state = add_error_text(state, error_message, s);
    }
  if (jtk::send_to_pipe(pipefd, txt.c_str()) != 0) {
    std::string error_message = "Could not send copy buffer to pipe\n";
    state = add_error_text(state, error_message, s);
    }
  jtk::close_pipe(pipefd);
#endif
  return state;
  }

std::optional<app_state> command_paste_from_snarf_buffer(app_state state, uint32_t, settings& s)
  {
#if defined(_WIN32)
  auto txt = get_text_from_windows_clipboard();
  if (state.operation == op_editing)
    {
    bool has_non_trivial_selection = has_nontrivial_selection(get_active_buffer(state), convert(s));
    auto init_pos = get_active_buffer(state).pos;
    if (get_active_buffer(state).start_selection != std::nullopt) {
      if (*get_active_buffer(state).start_selection < init_pos)
        init_pos = *get_active_buffer(state).start_selection;
      }
    get_active_buffer(state) = insert(get_active_buffer(state), txt, convert(s));
    if (has_non_trivial_selection)
      {
      get_active_buffer(state).start_selection = init_pos;
      get_active_buffer(state).pos = get_previous_position(get_active_buffer(state), get_active_buffer(state).pos);
      }
    return check_scroll_position(state, s);
    }
  else
    {
    bool has_non_trivial_selection = has_nontrivial_selection(state.operation_buffer, convert(s));
    auto init_pos = state.operation_buffer.pos;
    if (state.operation_buffer.start_selection != std::nullopt) {
      if (*state.operation_buffer.start_selection < init_pos)
        init_pos = *state.operation_buffer.start_selection;
      }
    state.operation_buffer = insert(state.operation_buffer, txt, convert(s));
    if (has_non_trivial_selection)
      {
      state.operation_buffer.start_selection = init_pos;
      state.operation_buffer.pos = get_previous_position(state.operation_buffer, state.operation_buffer.pos);
      }
    }
#else
  std::string txt = pbpaste();
  if (state.operation == op_editing)
    {
    bool has_non_trivial_selection = has_nontrivial_selection(get_active_buffer(state), convert(s));
    auto init_pos = get_active_buffer(state).pos;
    if (get_active_buffer(state).start_selection != std::nullopt) {
      if (*get_active_buffer(state).start_selection < init_pos)
        init_pos = *get_active_buffer(state).start_selection;
      }
    get_active_buffer(state) = insert(get_active_buffer(state), txt, convert(s));
    if (has_non_trivial_selection)
      {
      get_active_buffer(state).start_selection = init_pos;
      get_active_buffer(state).pos = get_previous_position(get_active_buffer(state), get_active_buffer(state).pos);
      }
    return check_scroll_position(state, s);
    }
  else
    {
    bool has_non_trivial_selection = has_nontrivial_selection(state.operation_buffer, convert(s));
    auto init_pos = state.operation_buffer.pos;
    if (state.operation_buffer.start_selection != std::nullopt) {
      if (*state.operation_buffer.start_selection < init_pos)
        init_pos = *state.operation_buffer.start_selection;
      }
    state.operation_buffer = insert(state.operation_buffer, txt, convert(s));
    if (has_non_trivial_selection)
      {
      state.operation_buffer.start_selection = init_pos;
      state.operation_buffer.pos = get_previous_position(state.operation_buffer, state.operation_buffer.pos);
      }
    }
#endif
  return state;
  }

std::optional<app_state> make_goto_buffer(app_state state, uint32_t buffer_id, settings& s)
  {
  state = clear_operation_buffer(state);
  std::stringstream str;
  str << state.buffers[buffer_id].buffer.pos.row + 1;
  state.operation_buffer = insert(state.operation_buffer, str.str(), convert(s), false);
  state.operation_buffer.start_selection = position(0, 0);
  state.operation_buffer = move_end(state.operation_buffer, convert(s));
  return state;
  }

std::optional<app_state> make_edit_buffer(app_state state, uint32_t buffer_id, settings& s)
  {
  state = clear_operation_buffer(state);
  //std::stringstream str;
  //str << state.buffers[buffer_id].buffer.pos.row + 1;
  //state.operation_buffer = insert(state.operation_buffer, str.str(), convert(s), false);
  state.operation_buffer.start_selection = position(0, 0);
  state.operation_buffer = move_end(state.operation_buffer, convert(s));
  return state;
  }

std::optional<app_state> make_find_buffer(app_state state, uint32_t buffer_id, settings& s)
  {
  auto senv = convert(s);
  std::string find_text = s.last_find;
  if (has_selection(state.buffers[buffer_id].buffer))
    {
    std::string line = to_string(get_selection(state.buffers[buffer_id].buffer, senv));
    auto pos = line.find('\n');
    if (pos == std::string::npos)
      find_text = line;
    }
  state = clear_operation_buffer(state);
  state.operation_buffer = insert(state.operation_buffer, find_text, senv, false);
  state.operation_buffer.start_selection = position(0, 0);
  state.operation_buffer = move_end(state.operation_buffer, convert(s));
  return state;
  }

uint32_t get_command_buffer_id(app_state state, uint32_t buffer_id) {
  const auto& w = state.windows[state.buffer_id_to_window_id[buffer_id]];
  if (w.wt != e_window_type::wt_normal) {
    if (w.wt != e_window_type::wt_command) {
      if (state.last_active_editor_buffer != 0xffffffff) {
        buffer_id = state.last_active_editor_buffer - 1;
        }
      else {
        buffer_id = state.active_buffer;
        }
      }
    }
  else {
    buffer_id = buffer_id - 1;
    }
  return buffer_id;
  }

std::optional<app_state> command_run(app_state state, uint32_t buffer_id, settings& s)
  {
  buffer_id = get_command_buffer_id(state, buffer_id);
  if (buffer_id == 0xffffffff)
    return state;
  auto command = get_selection(state.buffers[buffer_id].buffer, convert(s));
  return execute(state, buffer_id, to_wstring(command), s);
  }

std::optional<app_state> command_goto(app_state state, uint32_t buffer_id, settings& s)
  {
  state.operation = op_goto;
  return make_goto_buffer(state, buffer_id, s);
  }

std::optional<app_state> command_edit(app_state state, uint32_t buffer_id, settings& s)
  {
  state.operation = op_edit;
  return make_edit_buffer(state, buffer_id, s);
  }

std::optional<app_state> command_find(app_state state, uint32_t buffer_id, settings& s)
  {
  state.operation = op_find;
  return make_find_buffer(state, buffer_id, s);
  }

std::optional<app_state> command_replace(app_state state, uint32_t buffer_id, settings& s)
  {
  state.operation = op_replace_find;
  state.operation_stack.push_back(op_replace_to_find);
  return make_find_buffer(state, buffer_id, s);
  }

std::optional<app_state> command_select_all(app_state state, uint32_t buffer_id, settings& s)
  {
  //state.message = string_to_line("[Select all]");
  if (state.operation == op_editing)
    {
    state.buffers[buffer_id].buffer = select_all(state.buffers[buffer_id].buffer, convert(s));
    return check_scroll_position(state, buffer_id, s);
    }
  else
    state.operation_buffer = select_all(state.operation_buffer, convert(s));
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
        case SDL_SYSWMEVENT:
        {
#ifdef _WIN32
        if (event.syswm.msg->msg.win.msg == WM_COPYDATA)
          {
          return state; // return so that we can process the messages queue
          }
#endif
        break;
        }
        case SDL_DROPFILE:
        {
        auto dropped_filedir = event.drop.file;
        int x, y;
        SDL_GetMouseState(&x, &y);
        x /= font_width;
        y /= font_height;
        auto p = find_mouse_text_pick(x, y);
        std::string path(dropped_filedir);
        SDL_free(dropped_filedir);    // Free dropped_filedir memory
        return load_file(state, p.buffer_id, path, s);
        break;
        }
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
          case SDLK_TAB:
          {
          if (shift_pressed())
            return inverse_tab(state, s.tab_space, s);
          else
            return s.use_spaces_for_tab ? tab(state, s.tab_space, std::string(""), s) : tab(state, s.tab_space, std::string("\t"), s);
          }
          case SDLK_KP_ENTER:
          case SDLK_RETURN: return ret(state, s);
          case SDLK_BACKSPACE: return backspace(state, s);
          case SDLK_DELETE:
          {
          if (shift_pressed()) // copy
            {
            state = *command_copy_to_snarf_buffer(state, buffer_id, s);
            }
          return del(state, s);
          }
          case SDLK_LALT:
          case SDLK_RALT:
          {
          if (state.operation == op_editing && get_active_buffer(state).start_selection != std::nullopt)
            get_active_buffer(state).rectangular_selection = true;
          return state;
          }
          case SDLK_LSHIFT:
          case SDLK_RSHIFT:
          {
          if (keyb_data.selecting)
            break;
          keyb_data.selecting = true;
          if (state.operation == op_editing)
            {
            if (get_active_buffer(state).start_selection == std::nullopt)
              {
              get_active_buffer(state).start_selection = get_actual_position(get_active_buffer(state));
              if (!get_active_buffer(state).rectangular_selection)
                get_active_buffer(state).rectangular_selection = alt_pressed();
              }
            }
          else
            {
            if (state.operation_buffer.start_selection == std::nullopt)
              state.operation_buffer.start_selection = get_actual_position(state.operation_buffer);
            }
          return state;
          }
          case SDLK_F1:
          {
          return command_help(state, buffer_id, s);
          }
          case SDLK_F2:
          {
          return command_complete(state, buffer_id, s);
          }
          case SDLK_F3:
          {
          if (ctrl_pressed())
            {
            auto fb = state.operation == op_editing ? state.buffers[buffer_id].buffer : state.operation_buffer;
            if (has_selection(fb))
              {
              s.last_find = to_string(get_selection(fb, convert(s)));
              }
            }
          return command_find_next(state, buffer_id, s);
          }
          case SDLK_F4:
          {
          return command_get(state, buffer_id, s);
          }
          case SDLK_F5:
          {
          return command_run(state, buffer_id, s);
          }
          case SDLK_a:
          {
          if (ctrl_pressed())
            {
            switch (state.operation)
              {
              case op_replace: return replace_all(state, s);
              default: return command_select_all(state, buffer_id, s);
              }
            }
          break;
          }
          case SDLK_c:
          {
          if (ctrl_pressed())
            {
            /*
            if (state.mouse_pointing_buffer != 0xffffffff && has_nontrivial_selection(state.buffers[state.mouse_pointing_buffer].buffer, convert(s)))
              {
              uint32_t active_buffer = state.active_buffer;
              state.active_buffer = state.mouse_pointing_buffer;
              state = *command_copy_to_snarf_buffer(state, buffer_id, s);
              state.active_buffer = active_buffer;
              return state;
              }
            else
              return command_copy_to_snarf_buffer(state, buffer_id, s);
            */
            return command_copy_to_snarf_buffer(state, buffer_id, s);
            }
          break;
          }
          case SDLK_e:
          {
          if (ctrl_pressed())
            {
            return command_edit(state, buffer_id, s);
            }
          break;
          }
          case SDLK_f:
          {
          if (ctrl_pressed())
            {
            return command_find(state, buffer_id, s);
            }
          break;
          }
          case SDLK_g:
          {
          if (ctrl_pressed())
            {
            return command_goto(state, buffer_id, s);
            }
          break;
          }
          case SDLK_h:
          {
          if (ctrl_pressed())
            {
            return command_replace(state, buffer_id, s);
            }
          break;
          }
          case SDLK_i:
          {
          if (ctrl_pressed())
            {
            return command_incremental_search(state, buffer_id, s);
            }
          break;
          }
          case SDLK_n:
          {
          if (ctrl_pressed())
            {
            return command_new_window(state, buffer_id, s);
            }
          break;
          }
          case SDLK_o:
          {
          if (ctrl_pressed())
            {
            return command_open(state, buffer_id, s);
            }
          break;
          }
          case SDLK_s:
          {
          if (ctrl_pressed())
            {
            switch (state.operation)
              {
              case op_replace: return replace_selection(state, s);
              default: return command_put(state, buffer_id, s);
              }
            }
          break;
          }
          case SDLK_v:
          {
          if (ctrl_pressed())
            {
            /*
            if (state.mouse_pointing_buffer != 0xffffffff
              && has_nontrivial_selection(state.buffers[state.mouse_pointing_buffer].buffer, convert(s))
              && !has_nontrivial_selection(state.buffers[state.active_buffer].buffer, convert(s)))
              {
              //uint32_t active_buffer = state.active_buffer;
              state.active_buffer = state.mouse_pointing_buffer;
              state = *command_paste_from_snarf_buffer(state, buffer_id, s);
              //state.active_buffer = active_buffer;
              return state;
              }
            else
              return command_paste_from_snarf_buffer(state, buffer_id, s);
            */
            return command_paste_from_snarf_buffer(state, buffer_id, s);
            }
          break;
          }
          case SDLK_w:
          {
          if (ctrl_pressed())
            {
            return command_delete_window(state, buffer_id, s);
            }
          break;
          }
          case SDLK_x:
          {
          if (ctrl_pressed())
            {
            return command_cancel(state, buffer_id, s);
            }
          break;
          }
          case SDLK_y:
          {
          if (ctrl_pressed())
            {
            return command_redo(state, buffer_id, s);
            }
          break;
          }
          case SDLK_z:
          {
          if (ctrl_pressed())
            {
            return command_undo(state, buffer_id, s);
            }
          break;
          }
          case SDLK_ESCAPE:
          {
          if (state.operation != op_editing)
            return command_cancel(state, buffer_id, s);
          break;
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
          state = resize_font(state, pdc_font_size, s);
          return resize_windows(state, s);
          }
        else
          {
          int steps = s.mouse_scroll_steps;
          if (event.wheel.y > 0)
            steps = -steps;
          int x = mouse.mouse_x / font_width;
          int y = mouse.mouse_y / font_height;
          screen_ex_pixel p = get_ex(y, x);
          uint32_t b_id = p.buffer_id != 0xffffffff ? p.buffer_id : state.active_buffer;
          return move_editor_window_up_down(state, b_id, steps, s);
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
    if (time_elapsed > 1000) {
      bool modifications = false;
      for (uint32_t b = 0; b < state.buffers.size(); ++b) {
        if (state.buffers[b].bt == e_buffer_type::bt_piped) {
          bool this_buffer_modified = false;
          state = check_pipes(this_buffer_modified, b, state, s);
          modifications |= this_buffer_modified;
          }
        }
      if (modifications)
        return state;
      tic = std::chrono::steady_clock::now();
      }
    }
  }

app_state make_topline(app_state state, const settings& s) {
  uint32_t id = (uint32_t)state.buffers.size();
  buffer_data bd = make_empty_buffer_data();
  bd.buffer_id = id;
  bd.buffer = insert(bd.buffer, "Newcol Kill Putall Dump Load Exit", convert(s), false);
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
    auto command_id = w.wt == e_window_type::wt_command ? buffer_id : buffer_id - 1;
    if (should_update_command_text(state, command_id, s))
      state = update_command_text(state, command_id, s);
    if (buffer_id == command_id) {
      state = update_filename(state, buffer_id, s);
      }
    }
  return state;
  }

app_state make_empty_state(settings& s) {
  app_state state;
  state.active_buffer = 0;
  state.last_active_editor_buffer = 0;
  state.mouse_pointing_buffer = 0;
  state.operation = e_operation::op_editing;
  state = make_topline(state, s);
  state = *command_new_column(state, 0, s);

  s.w = 80;
  s.h = 25;
  s.x = 100;
  s.y = 100;

  state.w = s.w * font_width;
  state.h = s.h * font_height;

  return state;
  }

bool file_already_opened(const app_state& state, const std::string& filename)
  {
  for (const auto& b : state.buffers)
    {
    if (b.buffer.name == filename)
      return true;
    }
  return false;
  }

engine::engine(int argc, char** argv, const settings& input_settings) : s(input_settings)
  {
  set_font(s.font_size, s);

  state.w = s.w * font_width;
  state.h = s.h * font_height;
  state.last_active_editor_buffer = 0xffffffff;

  nodelay(stdscr, TRUE);
  noecho();

  start_color();
  use_default_colors();
  init_colors(s);
  bkgd(COLOR_PAIR(default_color));

  app_state result = state;
  std::ifstream f(get_file_in_executable_path("temp.json"));
  if (f.is_open())
    {
    result = load_dump(state, f, s);
    f.close();
    state = result;
    }


  //state.active_buffer = 0;
  state.operation = e_operation::op_editing;
  //state = make_topline(state, s);
  //state = *command_new_column(state, 0, s);
  //state = *command_new_column(state, 0, s);
  //state = *command_new_window(state, 1, s);

  if (state.buffers.empty()) // if temp.json was an invalid file then initialise
    {
    state = make_empty_state(s);
    }

  SDL_ShowCursor(1);
  SDL_SetWindowSize(pdc_window, state.w, state.h);
  SDL_SetWindowPosition(pdc_window, s.x, s.y);

  resize_term(state.h / font_height, state.w / font_width);
  resize_term_ex(state.h / font_height, state.w / font_width);

  uint32_t active_buffer = state.active_buffer;
  state.active_buffer = 0;
  // put active buffer to 0 so that new folders are added at the top left

  for (int j = 1; j < argc; ++j) {
    std::string input(argv[j]);
    bool piped = input[0] == '=';
    if (piped)
      input.erase(input.begin());
    if (input[0] == '-') // options
      continue;
    remove_quotes(input);
    if (jtk::is_directory(input)) {
      auto inputfolder = jtk::get_cwd();
      if (!inputfolder.empty() && inputfolder.back() != '/')
        inputfolder.push_back('/');
      if (input.front() == '/')
        inputfolder.pop_back();
      inputfolder.append(input);
      if (jtk::is_directory(inputfolder))
        input.swap(inputfolder);
      input = simplify_folder(input);
      if (!file_already_opened(state, input))
        state = *load_file(state, 0, input, s);
      }
    else
      {
      std::string inputfile = get_file_path(input, std::string());
      if (inputfile.empty())
        {
        inputfile = jtk::get_cwd();
        if (inputfile.back() != '\\' && inputfile.back() != '/')
          inputfile.push_back('/');

        inputfile.append(input);
        }
      if (piped)
        {
        state = *command_new_window(state, 0, s);
        uint32_t buffer_id = (uint32_t)(state.buffers.size() - 1);
        std::stringstream str;
        for (; j < argc; ++j)
          str << argv[j] << (j + 1 < argc ? " " : "");
        std::wstring parameters = jtk::convert_string_to_wstring(str.str());
        state = *execute(state, buffer_id, parameters, s);
        }
      else {
        if (!file_already_opened(state, input))
          {
          if (jtk::file_exists(input))
            state = *load_file(state, 0, input, s);
          else
            {
            state = *command_new_window(state, 0, s);
            uint32_t buffer_id = (uint32_t)(state.buffers.size() - 1);
            int64_t command_id = buffer_id - 1;
            state.buffers[buffer_id].buffer.name = input;
            state.buffers[command_id].buffer.name = get_active_buffer(state).name;
            get_active_buffer(state) = set_multiline_comments(get_active_buffer(state));
            get_active_buffer(state) = init_lexer_status(get_active_buffer(state), convert(s));
            state.buffers[command_id].buffer.content = to_text(make_command_text(state, command_id, s));
            }
          }
        }
      }
    }

  // restore active_buffer
  if (state.active_buffer == 0)
    state.active_buffer = active_buffer;
  }

engine::~engine()
  {
  save_to_file(get_file_in_executable_path("temp.json"), state);
  for (uint32_t buffer_id = 0; buffer_id < (uint32_t)state.buffers.size(); ++buffer_id)
    kill(state, buffer_id);
  }

void engine::run()
  {
  draw(state, s);
  SDL_UpdateWindowSurface(pdc_window);

  while (auto new_state = process_input(state, state.active_buffer, s))
    {
    while (!messages.empty())
      {
      auto m = messages.pop();
      if (m.m == ASYNC_MESSAGE_LOAD && jtk::file_exists(m.str))
        {
        new_state = load_file(*new_state, new_state->active_buffer, m.str, s);
        }
      }
    state = check_update_active_command_text(*new_state, s);
    if (!mouse.rearranging_windows)
      draw(state, s);

    SDL_UpdateWindowSurface(pdc_window);
    }



  s.w = state.w / font_width;
  s.h = state.h / font_height;
  SDL_GetWindowPosition(pdc_window, &s.x, &s.y);
  }
