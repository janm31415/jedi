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
  
std::optional<app_state> command_exit(app_state state, settings& s)
  {
  return std::nullopt;
  }
  
std::optional<app_state> process_input(app_state state, settings& s) {
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
          return state;
          }
        break;
        }
        case SDL_TEXTINPUT:
        {
        //return text_input(state, event.text.text, s);
        }
        case SDL_KEYDOWN:
        {
        return state;
        break;
        } // case SDLK_KEYUP:
        case SDL_QUIT:
        {
        return command_exit(state, s);
        }
        } // switch (event.type)
      }
    std::this_thread::sleep_for(std::chrono::duration<double, std::milli>(5.0));
    auto toc = std::chrono::steady_clock::now();
    auto time_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(toc - tic).count();
    }
  }
  
buffer_data make_empty_buffer_data() {
  buffer_data bd;
  bd.buffer = make_empty_buffer();
  bd.scroll_row = 0;
  bd.operation = e_operation::op_none;
#ifdef _WIN32
  bd.process = nullptr;
#else
  bd.process = {{-1,-1,-1}};
#endif
  bd.bt = bt_normal;
  return bd;
}

app_state make_topline(app_state state, const settings& s) {
  uint32_t id = (uint32_t)state.buffers.size();
  buffer_data bd = make_empty_buffer_data();
  bd.buffer_id = id;
  bd.buffer = insert(bd.buffer, "Newcol Exit", convert(s), false);
  state.buffers.push_back(bd);
  window w = make_window(id, 0, 0, state.w, 1, e_window_type::wt_topline);
  uint32_t window_id = (uint32_t)state.windows.size();
  state.windows.push_back(w);
  state.buffer_id_to_window_id.push_back(window_id);
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
 
  state = make_topline(state, s);
  state.active_buffer = 0;
  
  resize_term(state.h / font_height, state.w / font_width);
  resize_term_ex(state.h / font_height, state.w / font_width);

  }

engine::~engine()
  {

  }

void engine::run()
  {
  state = draw(state, s);
  SDL_UpdateWindowSurface(pdc_window);

  while (auto new_state = process_input(state, s))
    {
    state = *new_state;
    state = draw(state, s);

    SDL_UpdateWindowSurface(pdc_window);
    }



  s.w = state.w / font_width;
  s.h = state.h / font_height;
  SDL_GetWindowPosition(pdc_window, &s.x, &s.y);
  }
