#pragma once

#include "buffer.h"
#include "settings.h"
#include <array>
#include <string>
#include <vector>

enum e_operation
  {
  op_editing,
  op_command_editing,
  op_exit,
  op_find,
  op_goto,
  op_get,
  op_incremental_search,
  op_open,
  op_replace,
  op_replace_find,
  op_replace_to_find,
  op_save,
  op_query_save,
  op_new
  };

enum e_window_type
  {
  wt_normal,
  wt_piped
  };

struct app_state
  {
  file_buffer buffer;
  file_buffer operation_buffer;
  file_buffer command_buffer;
  text snarf_buffer;
  line message;
  int64_t scroll_row, operation_scroll_row, command_scroll_row;    
  e_operation operation;  
  std::vector<e_operation> operation_stack;
  std::wstring piped_prompt;
#ifdef _WIN32
  void* process;
#else
  std::array<int, 3> process;
#endif  
  int w, h;
  e_window_type wt;
  };


struct engine
  {
  app_state state;
  settings s;

  engine(int argc, char** argv, const settings& input_settings);
  ~engine();

  void run();

  };

