#include <SDL.h>
#include <SDL_syswm.h>
#include <curses.h>

#include <iostream>
#include <stdlib.h>

#define JTK_FILE_UTILS_IMPLEMENTATION
#include "jtk/file_utils.h"

#define JTK_PIPE_IMPLEMENTATION
#include "jtk/pipe.h"

#include "engine.h"
#include "utils.h"

extern "C"
  {
#include <sdl2/pdcsdl.h>
  }

#ifdef _WIN32
#include <windows.h>
#endif



int main(int argc, char** argv)
  {

  /* Initialize SDL */
  if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
    std::cout << "Could not initialize SDL" << std::endl;
    exit(1);
    }
  SDL_GL_SetSwapInterval(1);
  atexit(SDL_Quit);


  /* Initialize PDCurses */

  {
  initscr();
  }

  start_color();
  scrollok(stdscr, TRUE);

  PDC_set_title("jedi");

  settings s;
  s = read_settings(get_file_in_executable_path("jedi_settings.json").c_str());
  update_settings(s, get_file_in_executable_path("jedi_user_settings.json").c_str());



  engine e(argc, argv, s);
  e.run();

  settings s_latest = read_settings(get_file_in_executable_path("jedi_settings.json").c_str());
  update_settings(s_latest, get_file_in_executable_path("jedi_user_settings.json").c_str());

  update_settings_if_different(s_latest, e.s, s);
  write_settings(s_latest, get_file_in_executable_path("jedi_settings.json").c_str());

  endwin();

  return 0;
  }
