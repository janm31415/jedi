#pragma once

#include "engine.h"

void get_window_edit_range(int& offset_x, int& offset_y, int& maxcol, int& maxrow, int64_t scroll_row, const window& w, const settings& s);
int64_t wrapped_line_rows(line ln, int maxcol, int maxrow, const env_settings& senv);

app_state draw(app_state state, const settings& s);
