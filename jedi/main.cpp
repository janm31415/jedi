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


#ifdef _WIN32
int CopyEventFilter(void* userdata, SDL_Event* event)
  {
  if (event->type == SDL_SYSWMEVENT)
    {
    if (event->syswm.msg->msg.win.msg == WM_COPYDATA)
      {
      COPYDATASTRUCT* p_copy_data = reinterpret_cast<COPYDATASTRUCT*>(event->syswm.msg->msg.win.lParam);
      if (!p_copy_data)
        return 0;
      if (p_copy_data->dwData == 0)
        {
        async_messages* p_messages = reinterpret_cast<async_messages*>(userdata);
        std::string message_data(static_cast<char*>(p_copy_data->lpData));
        async_message m;
        m.m = ASYNC_MESSAGE_LOAD;
        m.str = message_data;
        p_messages->push(m);
        }
      }
    }
  return 0;
  }
#endif

int main(int argc, char** argv)
  {
#ifdef _WIN32
  bool the_first_one = true;
  ::SetLastError(NO_ERROR);
  ::CreateMutex(NULL, false, "jediInstance");
  if (::GetLastError() == ERROR_ALREADY_EXISTS)
    the_first_one = false;

  if (!the_first_one)
    {
    HWND h_jedi = ::FindWindow(NULL, "jedi");
    for (int i = 0; !h_jedi && i < 5; ++i)
      {
      Sleep(100);
      h_jedi = ::FindWindow(NULL, "jedi");
      }

    if (h_jedi)
      {
      int sw = 0;

      if (::IsZoomed(h_jedi))
        sw = SW_MAXIMIZE;
      else if (::IsIconic(h_jedi))
        sw = SW_RESTORE;

      if (sw != 0)
        ::ShowWindow(h_jedi, sw);

      ::SetForegroundWindow(h_jedi);

      if (argc > 1)
        {
        HINSTANCE instance = GetModuleHandle(NULL);
        for (int i = 1; i < argc; ++i)
          {
          std::string text(argv[i]);
          COPYDATASTRUCT data_to_copy;
          data_to_copy.dwData = 0;
          data_to_copy.lpData = (void*)text.c_str();
          data_to_copy.cbData = (long)(text.length() + 1) * (sizeof(char));
          ::SendMessage(h_jedi, WM_COPYDATA, reinterpret_cast<WPARAM>(instance), reinterpret_cast<LPARAM>(&data_to_copy));
          Sleep(10000);
          }
        }

      return 0;
      }
    }
#endif

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

#ifdef _WIN32
  SDL_EventState(SDL_SYSWMEVENT, SDL_ENABLE);
  SDL_AddEventWatch(&CopyEventFilter, &e.messages);
#endif

  e.run();

  settings s_latest = read_settings(get_file_in_executable_path("jedi_settings.json").c_str());
  update_settings(s_latest, get_file_in_executable_path("jedi_user_settings.json").c_str());

  update_settings_if_different(s_latest, e.s, s);
  write_settings(s_latest, get_file_in_executable_path("jedi_settings.json").c_str());

  endwin();

  return 0;
  }
