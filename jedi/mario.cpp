#include "mario.h"
#include <stdint.h>
#include <SDL.h>
#include <SDL_syswm.h>
#include <curses.h>

extern "C"
  {
#include <sdl2/pdcsdl.h>
  }

namespace
  {
  static const char Mario[] =
    //123456789ABCDEF
    //               0123456789ABCDEF
    "                      #####     "
    "      ######         #'''''###  "
    "     #''''''##      #'''''''''# "
    "    #'''''''''#     ###'.#.###  "
    "    ###..#.###     #..##.#....# "
    "   #..##.#....#    #..##..#...# "
    "   #..##..#...#     ##...#####  "
    "    ##...#####      ###.....#   "
    "     ##.....#     ##'''##''###  "
    "    #''##''#     #..''''##''#'# "
    "   #''''##''#    #..'''######'.#"
    "   #''''#####     #..####.##.#.#"
    "    #...##.##     .#########''# "
    "    #..'''###     #''######'''# "
    "     #'''''#      #'''#  #'''#  "
    "      #####        ###    ###   ";

  bool init = false;

  uint32_t mario_x = 0;
  uint32_t prev_mario_x = 0;

  uint32_t old_val[16 * 16];

  void set_pixel(uint32_t x, uint32_t y, uint32_t color)
    {
    uint32_t* pixels = (uint32_t*)pdc_screen->pixels;
    pixels[x + y * pdc_screen->pitch / 4] = color;
    }

  uint32_t get_pixel(uint32_t x, uint32_t y)
    {
    const uint32_t* pixels = (const uint32_t*)pdc_screen->pixels;
    return pixels[x + y * pdc_screen->pitch / 4];
    }

  uint32_t clr_hashtag = 0xff0000ff;
  uint32_t clr_dot = 0xffffdd00;
  uint32_t clr_quote = 0xffff0000;
  }

void draw_mario()
  {
  if (init)
    {
    for (int y = 0; y < 16; ++y)
      {
      for (int x = 0; x < 16; ++x)
        {
        char ch = Mario[y * 32 + x + (prev_mario_x % 2) * 16];
        uint32_t expected_color = 0xffffffff;
        if (ch == '#')
          expected_color = clr_hashtag;
        else if (ch == '.')
          expected_color = clr_dot;
        else if (ch == '\'')
          expected_color = clr_quote;
        uint32_t actual_color = get_pixel(prev_mario_x + x, y);
        if (expected_color != 0xffffffff && actual_color == expected_color)
          set_pixel(prev_mario_x + x, y, old_val[y*16+x]);
        }
      }
    }

  prev_mario_x = mario_x;

  for (int y = 0; y < 16; ++y)
    {
    for (int x = 0; x < 16; ++x)
      {
      old_val[y*16+x] = get_pixel(mario_x + x, y);
      char ch = Mario[y*32+x + (mario_x%2)*16];
      if (ch == '#')
        set_pixel(mario_x + x, y, clr_hashtag);
      else if (ch == '.')
        set_pixel(mario_x + x, y, clr_dot);
      else if (ch == '\'')
        set_pixel(mario_x + x, y, clr_quote);
      }
    }

  ++mario_x;
  if (mario_x > pdc_screen->w - 16)
    mario_x = 0;

  init = true;
  }