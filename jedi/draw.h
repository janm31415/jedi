#pragma once

#include "engine.h"
#include "syntax_highlight.h"

const syntax_highlighter& get_syntax_highlighter();

file_buffer set_multiline_comments(file_buffer fb);
void get_window_edit_range(int& offset_x, int& offset_y, int& maxcol, int& maxrow, int64_t scroll_row, const window& w, const settings& s);
int64_t wrapped_line_rows(line ln, int maxcol, int maxrow, const env_settings& senv);

void draw(const app_state& state, const settings& s);
