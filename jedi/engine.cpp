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
  return bd;
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
  state.windows[state.g.topline_window_id].x = 0;
  state.windows[state.g.topline_window_id].y = 0;
  state.windows[state.g.topline_window_id].cols = cols;
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
    if (state.windows[c.column_command_window_id].rows > available_rows)
      state.windows[c.column_command_window_id].rows = available_rows;
    available_rows = rows - state.windows[state.g.topline_window_id].rows - state.windows[c.column_command_window_id].rows;
    for (auto& ci : c.items) {
      int row_offset = start_row + state.windows[c.column_command_window_id].rows;
      int top = row_offset + (int)(available_rows*ci.top_layer);
      int bottom = row_offset + (int)(available_rows*ci.bottom_layer);
      auto wp = state.window_pairs[ci.window_pair_id];
      int available_rows_for_command = bottom-top;
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
  
std::optional<app_state> new_column_command(app_state state, uint32_t, const settings& s) {
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

std::optional<app_state> new_window(app_state state, uint32_t buffer_id, const settings& s) {
  if (state.g.columns.empty())
    state = *new_column_command(state, buffer_id, s);
  
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
  bd.buffer = insert(bd.buffer, "Del", convert(s), false);
  state.buffers.push_back(bd);
  window cw = make_window(command_id, 0, 1, cols, 1, e_window_type::wt_command);
  uint32_t command_window_id = (uint32_t)state.windows.size();
  state.windows.push_back(cw);
  state.buffer_id_to_window_id.push_back(command_window_id);
  
  uint32_t editor_id = (uint32_t)state.buffers.size();
  buffer_data bd2 = make_empty_buffer_data();
  bd2.buffer_id = editor_id;
  bd2.buffer = insert(bd2.buffer, "Type here", convert(s), false);
  state.buffers.push_back(bd2);
  window w = make_window(editor_id, 0, 1, cols, 1, e_window_type::wt_normal);
  uint32_t window_id = (uint32_t)state.windows.size();
  state.windows.push_back(w);
  state.buffer_id_to_window_id.push_back(window_id);
  
  window_pair wp;
  wp.command_window_id = command_window_id;
  wp.window_id = window_id;
  state.window_pairs.push_back(wp);
  
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

void get_active_window_size_for_editing(int& rows, int& cols, const app_state& state, const settings& s)
  {
  auto window_id = state.buffer_id_to_window_id[state.buffers[state.active_buffer].buffer_id];
  int offset_x, offset_y;
  get_window_edit_range(offset_x, offset_y, cols, rows, state.buffers[state.active_buffer].scroll_row, state.windows[window_id], s);
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
  
app_state check_operation_buffer(app_state state)
  {
  if (state.operation_buffer.content.size() > 1)
    state.operation_buffer.content = state.operation_buffer.content.take(1);
  state.operation_buffer.pos.row = 0;
  return state;
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
        return state;
        break;
        } // case SDLK_KEYUP:
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
  state = *new_column_command(state, 0, s);
  state = *new_column_command(state, 0, s);
  state = *new_window(state, 1, s);

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
    state = *new_state;
    state = draw(state, s);

    SDL_UpdateWindowSurface(pdc_window);
    }



  s.w = state.w / font_width;
  s.h = state.h / font_height;
  SDL_GetWindowPosition(pdc_window, &s.x, &s.y);
  }
