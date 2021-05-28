#include "draw.h"
#include "pdcex.h"
#include "colors.h"
#include "syntax_highlight.h"
#include "utils.h"
#include <SDL.h>
#include <SDL_syswm.h>
#include <curses.h>
#include <sstream>

#include "jtk/file_utils.h"

extern "C"
  {
#include <sdl2/pdcsdl.h>
  }


#define DEFAULT_COLOR (A_NORMAL | COLOR_PAIR(default_color))

#define COMMAND_COLOR (A_NORMAL | COLOR_PAIR(command_color))

#define TOPLINE_COMMAND_COLOR (A_NORMAL | COLOR_PAIR(topline_command_color))

#define COLUMN_COMMAND_COLOR (A_NORMAL | COLOR_PAIR(column_command_color))


const syntax_highlighter& get_syntax_highlighter()
  {
  static syntax_highlighter s;
  return s;
  }
  

file_buffer set_multiline_comments(file_buffer fb)
  {
  auto ext = jtk::get_extension(fb.name);
  auto filename = jtk::get_filename(fb.name);
  std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return (unsigned char)std::tolower(c); });
  std::transform(filename.begin(), filename.end(), filename.begin(), [](unsigned char c) { return (unsigned char)std::tolower(c); });
  const syntax_highlighter& shl = get_syntax_highlighter();
  comment_data cd;
  fb.syntax.should_highlight = false;
  if (shl.extension_or_filename_has_syntax_highlighter(ext))
    {
    cd = shl.get_syntax_highlighter(ext);
    fb.syntax.should_highlight = true;
    }
  else if (shl.extension_or_filename_has_syntax_highlighter(filename))
    {
    cd = shl.get_syntax_highlighter(filename);
    fb.syntax.should_highlight = true;
    }
  fb.syntax.multiline_begin = cd.multiline_begin;
  fb.syntax.multiline_end = cd.multiline_end;
  fb.syntax.multistring_begin = cd.multistring_begin;
  fb.syntax.multistring_end = cd.multistring_end;
  fb.syntax.single_line = cd.single_line;
  fb.syntax.uses_quotes_for_chars = cd.uses_quotes_for_chars;
  return fb;
  }


const keyword_data& get_keywords(const std::string& name)
  {
  auto ext = jtk::get_extension(name);
  auto filename = jtk::get_filename(name);
  std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return (unsigned char)std::tolower(c); });
  std::transform(filename.begin(), filename.end(), filename.begin(), [](unsigned char c) { return (unsigned char)std::tolower(c); });
  const syntax_highlighter& shl = get_syntax_highlighter();
  if (shl.extension_or_filename_has_keywords(ext))
    return shl.get_keywords(ext);
  if (shl.extension_or_filename_has_keywords(filename))
    return shl.get_keywords(filename);
  static keyword_data empty;
  return empty;
  }

uint16_t character_to_pdc_char(uint32_t character, uint32_t char_id, const settings& s)
  {
  if (character > 65535)
    return '?';
  switch (character)
    {
    case 9:
    {
    if (s.show_all_characters)
      {
      switch (char_id)
        {
        case 0: return 84; break;
        case 1: return 66; break;
        default: return 32; break;
        }
      }
    return 32; break;
    }
    case 10: {
    if (s.show_all_characters)
      return char_id == 0 ? 76 : 70;
    return 32; break;
    }
    case 13: {
    if (s.show_all_characters)
      return char_id == 0 ? 67 : 82;
    return 32; break;
    }
    case 32: return s.show_all_characters ? 46 : 32; break;
#ifdef _WIN32
    case 65440: return (uint16_t)' '; // This sign is a half-width space. It's used by cmd.exe on Windows for file sizes.
#endif
    default: return (uint16_t)character;
    }
  }
  
inline int number_of_digits(int64_t v)
  {
  return  1
    + (int)(v >= 10)
    + (int)(v >= 100)
    + (int)(v >= 1000)
    + (int)(v >= 10000)
    + (int)(v >= 100000)
    + (int)(v >= 1000000)
    + (int)(v >= 10000000)
    + (int)(v >= 100000000)
    + (int)(v >= 1000000000)
    + (int)(v >= 10000000000ull)
    + (int)(v >= 100000000000ull)
    + (int)(v >= 1000000000000ull)
    + (int)(v >= 10000000000000ull)
    + (int)(v >= 100000000000000ull)
    + (int)(v >= 1000000000000000ull)
    + (int)(v >= 10000000000000000ull)
    + (int)(v >= 100000000000000000ull)
    + (int)(v >= 1000000000000000000ull)
    + (int)(v >= 10000000000000000000ull);
  }
  
int columns_reserved_for_line_numbers(int64_t scroll_row, const settings& s)
  {
  if (s.show_line_numbers)
    {
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    scroll_row += rows - (4 + s.command_buffer_rows);
    return number_of_digits(scroll_row)+1;
    }
  return 0;
  }
  
bool is_command_window(e_window_type wt) {
  return wt != e_window_type::wt_normal;
}

bool line_can_be_wrapped(line ln, int maxcol, int maxrow, const env_settings& senv)
  {
  int64_t max_length_allowed = (maxcol-1)*(maxrow-1);
  int64_t full_len = line_length_up_to_column(ln, max_length_allowed + 1, senv);
  return (full_len < max_length_allowed);
  }
  
int64_t wrapped_line_rows(line ln, int maxcol, int maxrow, const env_settings& senv)
  {
  int64_t max_length_allowed = (maxcol-1)*(maxrow-1);
  int64_t full_len = line_length_up_to_column(ln, max_length_allowed + 1, senv);
  if (full_len < max_length_allowed)
    {
    return full_len / (maxcol-1) + 1;
    }
  else
    return 1;
  }

/*
Returns an x offset (let's call it multiline_offset_x) such that
  int x = (int)current.col + multiline_offset_x + wide_characters_offset;
equals the x position in the screen of where the next character should come.
This makes it possible to further fill the line with spaces after calling "draw_line".
 */
int draw_line(int& wide_characters_offset, file_buffer fb, uint32_t buffer_id, position& current, position cursor, position buffer_pos, position underline, chtype base_color, int& r, int yoffset, int xoffset, int maxcol, int maxrow, std::optional<position> start_selection, bool rectangular, bool active, screen_ex_type set_type, const keyword_data& kd, bool wrap, const settings& s, const env_settings& senv, int wx, int wy)
  {
  int MULTILINEOFFSET = 10;
  auto tt = get_text_type(fb, current.row);

  line ln = fb.content[current.row];
  int multiline_tag = (int)multiline_tag_editor;
  if (set_type == SET_TEXT_COMMAND)
    multiline_tag = (int)multiline_tag_command;

  wide_characters_offset = 0;
  bool has_selection = (start_selection != std::nullopt) && (cursor.row >= 0) && (cursor.col >= 0);

  int64_t len = line_length_up_to_column(ln, maxcol - 1, senv);
  
  if (wrap && (len >= (maxcol - 1)))
    {
    if (!line_can_be_wrapped(ln, maxcol, maxrow, senv))
      wrap = false;
    }

  bool multiline = !wrap && (cursor.row == current.row) && (len >= (maxcol - 1));
  int64_t multiline_ref_col = cursor.col;

  if (!multiline && has_selection && !wrap)
    {
    if (!rectangular)
      {
      if (start_selection->row == current.row || cursor.row == current.row)
        {
        multiline_ref_col = ln.size();
        if (start_selection->row == current.row && start_selection->col < multiline_ref_col)
          multiline_ref_col = start_selection->col;
        if (cursor.row == current.row && cursor.col < multiline_ref_col)
          multiline_ref_col = cursor.col;
        if (multiline_ref_col < ln.size())
          {
          multiline = (len >= (maxcol - 1));
          }
        }
      else if ((start_selection->row > current.row && cursor.row < current.row) || (start_selection->row < current.row && cursor.row > current.row))
        {
        multiline_ref_col = 0;
        multiline = (len >= (maxcol - 1));
        }
      }
    else
      {
      int64_t min_col = start_selection->col;
      int64_t min_row = start_selection->row;
      int64_t max_col = buffer_pos.col;
      int64_t max_row = buffer_pos.row;
      if (max_col < min_col)
        std::swap(max_col, min_col);
      if (max_row < min_row)
        std::swap(max_row, min_row);
      if (current.row >= min_row && current.row <= max_row)
        {
        multiline_ref_col = min_col;
        if (multiline_ref_col >= (maxcol - 1) && (multiline_ref_col < ln.size()))
          multiline = true;
        }
      }
    }


  auto it = ln.begin();
  auto it_end = ln.end();

  int page = 0;

  if (multiline)
    {
    int pagewidth = maxcol - 2 - MULTILINEOFFSET;
    if (pagewidth <= 0) {
      MULTILINEOFFSET = (maxcol - 2)/2;
      pagewidth = maxcol - 2 - MULTILINEOFFSET;
    }
    int64_t len_to_cursor = line_length_up_to_column(ln, multiline_ref_col - 1, senv);
    page = len_to_cursor / pagewidth;
    if (page != 0)
      {
      if (len_to_cursor == multiline_ref_col - 1) // no characters wider than 1 so far.
        {
        int offset = page * pagewidth - MULTILINEOFFSET / 2;
        it += offset;
        current.col += offset;
        xoffset -= offset;
        }
      else
        {
        int offset = page * pagewidth - MULTILINEOFFSET / 2;
        current.col = get_col_from_line_length(ln, offset, senv);
        int64_t length_done = line_length_up_to_column(ln, current.col - 1, senv);
        it += current.col;
        wide_characters_offset = length_done - (current.col - 1);
        xoffset -= current.col + wide_characters_offset;
        }
      move((int)r + yoffset+wy, (int)current.col + xoffset + wide_characters_offset+wx);
      attron(COLOR_PAIR(multiline_tag));
      add_ex(position(), buffer_id, SET_NONE);
      addch('$');
      attron(base_color);
      ++xoffset;
      --maxcol;
      }
    --maxcol;
    }

  int drawn = 0;
  auto current_tt = tt.back();
  assert(current_tt.first == 0);
  tt.pop_back();

  int next_word_read_length_remaining = 0;
  bool keyword_type_1 = false;
  bool keyword_type_2 = false;

  for (; it != it_end; ++it)
    {
    if (next_word_read_length_remaining > 0)
      --next_word_read_length_remaining;
    if (!wrap && drawn >= maxcol)
      break;

    while (!tt.empty() && tt.back().first <= current.col)
      {
      current_tt = tt.back();
      tt.pop_back();
      }

    if (!(kd.keywords_1.empty() && kd.keywords_2.empty()) && current_tt.second == tt_normal && next_word_read_length_remaining == 0)
      {
      keyword_type_1 = false;
      keyword_type_2 = false;
      std::wstring next_word = read_next_word(it, it_end);
      next_word_read_length_remaining = next_word.length();
      auto it = std::lower_bound(kd.keywords_1.begin(), kd.keywords_1.end(), next_word);
      if (it != kd.keywords_1.end() && *it == next_word)
        keyword_type_1 = true;
      else
        {
        it = std::lower_bound(kd.keywords_2.begin(), kd.keywords_2.end(), next_word);
        if (it != kd.keywords_2.end() && *it == next_word)
          keyword_type_2 = true;
        }
      }

    switch (current_tt.second)
      {
      case tt_normal:
      {
      if (keyword_type_1)
        attron(COLOR_PAIR(keyword_color));
      else if (keyword_type_2)
        attron(COLOR_PAIR(keyword_2_color));
      else
        attron(base_color);
      break;
      }
      case tt_string: attron(COLOR_PAIR(string_color)); break;
      case tt_comment: attron(COLOR_PAIR(comment_color)); break;
      }

    if (active && in_selection(fb, current, cursor, buffer_pos, start_selection, rectangular, senv))
      attron(A_REVERSE);
    else
      attroff(A_REVERSE);

    if (!has_selection && (current == cursor))
      {
      attron(A_REVERSE);
      }

    attroff(A_UNDERLINE | A_ITALIC);
    if ((current == cursor) && valid_position(fb, underline))
      attron(A_UNDERLINE | A_ITALIC);
    if (current == underline)
      attron(A_UNDERLINE | A_ITALIC);

    move((int)r + yoffset+wy, (int)current.col + xoffset + wide_characters_offset+wx);
    auto character = *it;
    uint32_t cwidth = character_width(character, current.col + wide_characters_offset, senv);
    for (int32_t cnt = 0; cnt < cwidth; ++cnt)
      {
      add_ex(current, buffer_id, set_type);
      addch(character_to_pdc_char(character, cnt, s));
      ++drawn;
      if (drawn == maxcol)
        {
        if (wrap)
          {
          if (cnt == cwidth-1 && ((it+1) == it_end)) // last word done, don't make a newline
            break;
          drawn = 0;
          ++r;
          if (r >= maxrow)
            break; // test this
          wide_characters_offset = -(int)current.col - 1;
          move((int)r + yoffset+wy, (int)current.col + xoffset + wide_characters_offset+wx);
          wide_characters_offset -= cnt;
          }
        else
          break;
        }
      }
    wide_characters_offset += cwidth - 1;
    ++current.col;
    if (wrap && r >= maxrow)
      break;
    }
  attroff(A_UNDERLINE | A_ITALIC);

  if (!in_selection(fb, current, cursor, buffer_pos, start_selection, rectangular, senv))
    attroff(A_REVERSE);

  if (multiline && (it != it_end))
    {
    attroff(A_REVERSE);
    attron(COLOR_PAIR(multiline_tag));
    add_ex(position(), buffer_id, SET_NONE);
    addch('$');
    attron(base_color);
    ++xoffset;
    }

  return xoffset;
  }
  
void draw_scroll_bars(const window& w, const buffer_data& bd, const settings& s, const env_settings& senv, bool active) {
  const unsigned char scrollbar_ascii_sign = 219;
  int maxrow = w.rows;
  int maxcol = w.cols;
  int scroll1 = 0;
  int scroll2 = maxrow - 1;

  if (!bd.buffer.content.empty())
    {
    scroll1 = (int)((double)bd.scroll_row / (double)bd.buffer.content.size()*maxrow);
    scroll2 = (int)((double)(bd.scroll_row + maxrow) / (double)bd.buffer.content.size()*maxrow);
    }
  if (scroll1 >= maxrow)
    scroll1 = maxrow - 1;
  if (scroll2 >= maxrow)
    scroll2 = maxrow - 1;


  attron(COLOR_PAIR(scroll_bar_b_editor));

  for (int r = 0; r < maxrow; ++r)
    {
    move(r + w.y, w.x);

    if (r == scroll1)
      {
      attron(COLOR_PAIR(scroll_bar_f_editor));
      }

    int rowpos = 0;
    if (!bd.buffer.content.empty())
      {
      rowpos = (int)((double)r*(double)bd.buffer.content.size() / (double)maxrow);
      if (rowpos >= bd.buffer.content.size())
        rowpos = bd.buffer.content.size() - 1;
      }

    add_ex(position(rowpos, 0), bd.buffer_id, SET_SCROLLBAR_EDITOR);
    addch(ascii_to_utf16(scrollbar_ascii_sign));

    move(r + w.y, 1+w.x);
    add_ex(position(rowpos, 0), bd.buffer_id, SET_SCROLLBAR_EDITOR);
      

    if (r == scroll2)
      {
      attron(COLOR_PAIR(scroll_bar_b_editor));
      }
    }

}

void get_window_edit_range(int& offset_x, int& offset_y, int& maxcol, int& maxrow, int64_t scroll_row, const window& w, const settings& s) {
  int reserved = w.wt == e_window_type::wt_normal ? columns_reserved_for_line_numbers(scroll_row, s) : 0;
  offset_x = reserved + 2;
  offset_y = 0;
  maxcol = w.cols - offset_x;
  maxrow = w.rows;
}
  
void draw_window(const window& w, const buffer_data& bd, const settings& s, const env_settings& senv, bool active) {
  //int reserved = w.wt == e_window_type::wt_normal ? columns_reserved_for_line_numbers(bd.scroll_row, s) : 0;
  //int offset_x = reserved + 2;
  //int offset_y = 0;
  //int maxcol = w.cols - offset_x;get_window_edit_range(offset_x, offset_y, maxcol, maxrow, bd.scroll_row, w, s);
  //if (is_command_window(w.wt))
  //  --maxcol; // one free spot at the end for the plus sign for dragging
  //int maxrow = w.rows;
  int offset_x, offset_y, maxcol, maxrow;
  get_window_edit_range(offset_x, offset_y, maxcol, maxrow, bd.scroll_row, w, s);
  position current;
  current.row = bd.scroll_row;

  position cursor;
  cursor.row = cursor.col = -100;

  if (active)
    cursor = get_actual_position(bd.buffer);

  bool has_nontrivial_selection = (bd.buffer.start_selection != std::nullopt) && (bd.buffer.start_selection != bd.buffer.pos);

  position underline(-1, -1);
  if (active && !has_nontrivial_selection)
    {
    underline = find_corresponding_token(bd.buffer, cursor, current.row, current.row + maxrow - 1);
    }

  const keyword_data& kd = get_keywords(bd.buffer.name);
  
  screen_ex_type set_type = SET_TEXT_EDITOR;
  if (is_command_window(w.wt))
    set_type = SET_TEXT_COMMAND;

  auto main_color = DEFAULT_COLOR;

  if (w.wt == e_window_type::wt_command)
    main_color = COMMAND_COLOR;
  else if (w.wt == e_window_type::wt_topline)
    main_color = TOPLINE_COMMAND_COLOR;
  else if (w.wt == e_window_type::wt_column_command)
    main_color = COLUMN_COMMAND_COLOR;
  
  attrset(main_color);

  int r = 0;
  for (; r < maxrow; ++r)
    {
    if (is_command_window(w.wt)) {
      for (int x = 0; x < offset_x; ++x)
        {
        move((int)r + offset_y + w.y, (int)x + w.x);
        add_ex(current, bd.buffer_id, set_type);
        addch(' ');
        }
      }
    else if (s.show_line_numbers)
      {
      attrset(A_NORMAL | COLOR_PAIR(linenumbers_color));
      const int64_t line_nr = current.row + 1;
      move((int)r + offset_y + w.y, offset_x - number_of_digits(line_nr) - 1 + w.x);
      std::stringstream str;
      str << line_nr;
      std::string line_nr_str;
      str >> line_nr_str;
      for (int p = 0; p < line_nr_str.length(); ++p)
        {
        addch(line_nr_str[p]);
        }
      for (int p = 2; p < offset_x; ++p)
        {
        move((int)r + offset_y + w.y, p + w.x);
        add_ex(position(line_nr - 1, 0), bd.buffer_id, SET_LINENUMBER);
        }
      attrset(DEFAULT_COLOR);
      }
      
    //if (is_command_window(w.wt))
    //  attrset(COMMAND_COLOR);
      
    current.col = 0;
    if (current.row >= bd.buffer.content.size())
      {
      int x = 0;
      if (bd.buffer.content.empty() && active) // file is empty, draw cursor
        {
        move((int)r + offset_y + w.y, (int)current.col + offset_x + w.x);
        attron(A_REVERSE);
        add_ex(position(0, 0), bd.buffer_id, set_type);
        addch(' ');
        attroff(A_REVERSE);
        ++x;
        }
      auto last_pos = get_last_position(bd.buffer);
      for (; x < maxcol; ++x)
        {
        move((int)r + offset_y + w.y, (int)x + offset_x + w.x);
        add_ex(last_pos, bd.buffer_id, set_type);
        addch(' ');
        }
      ++current.row;
      continue;
      //break;
      }

    int wide_characters_offset = 0;
    int multiline_offset_x = draw_line(wide_characters_offset, bd.buffer, bd.buffer_id, current, cursor, bd.buffer.pos, underline,
    main_color, r, offset_y, offset_x, maxcol, maxrow, bd.buffer.start_selection,
    bd.buffer.rectangular_selection, active, set_type, kd, s.wrap, s, senv, w.x, w.y);

    int x = (int)current.col + multiline_offset_x + wide_characters_offset;
    if (!has_nontrivial_selection && (current == cursor))
      {
      move((int)r + offset_y+w.y, x+w.x);
      assert(current.row == bd.buffer.content.size() - 1);
      assert(current.col == bd.buffer.content.back().size());
      attron(A_REVERSE);
      add_ex(current, bd.buffer_id, set_type);
      addch(' ');
      ++x;
      ++current.col;
      }
    attroff(A_REVERSE);
    while (x < offset_x+maxcol)
      {
      move((int)r + offset_y+w.y, (int)x+w.x);
      add_ex(current, bd.buffer_id, set_type);
      addch(' ');
      ++current.col;
      ++x;
      }

    ++current.row;
    }

  auto last_pos = get_last_position(bd.buffer);
  for (; r < maxrow; ++r)
    {
    move((int)r + offset_y+w.y, offset_x+w.x);
    add_ex(last_pos, bd.buffer_id, set_type);
    }
    
  if (is_command_window(w.wt)) {
    if (w.wt == e_window_type::wt_command)
      attron(COLOR_PAIR(command_plus));
    else if (w.wt == e_window_type::wt_topline)
      attron(COLOR_PAIR(topline_command_plus));
    else if (w.wt == e_window_type::wt_column_command)
      attron(COLOR_PAIR(column_command_plus));
    move((int)(maxrow-1) + offset_y+w.y, offset_x+maxcol-1+w.x);
    add_ex(last_pos, bd.buffer_id, SET_PLUS);
    addch('+');
  } else {
    draw_scroll_bars(w, bd, s, senv, active);
  }
  
}

app_state draw(app_state state, const settings& s) {
  erase();

  invalidate_ex();
  
  auto senv = convert(s);
  
  for (const auto& w : state.windows) {
    bool active = w.buffer_id == state.active_buffer;
    draw_window(w, state.buffers[w.buffer_id], s, senv, active);
  }
  
  
  curs_set(0);
  refresh();

  return state;
}
