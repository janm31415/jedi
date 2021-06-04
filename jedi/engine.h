#pragma once

#include "buffer.h"
#include "settings.h"
#include "grid.h"
#include "window.h"
#include <array>
#include <string>
#include <vector>

enum e_operation
  {
  op_editing,
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

enum e_buffer_type
  {
  bt_normal,
  bt_piped
  };
  
struct buffer_data {
  uint32_t buffer_id;
  file_buffer buffer;
  int64_t scroll_row;
#ifdef _WIN32
  void* process;
#else
  std::array<int, 3> process;
#endif
  e_buffer_type bt;
  std::wstring piped_prompt;
};

struct app_state
  {
  std::vector<buffer_data> buffers;
  std::vector<uint32_t> buffer_id_to_window_id;
  std::vector<window_pair> window_pairs;
  std::vector<window> windows;
  grid g;
  text snarf_buffer;
  line message;
  int w, h;
  uint32_t active_buffer;
  e_operation operation;
  std::vector<e_operation> operation_stack;
  file_buffer operation_buffer;
  int64_t operation_scroll_row;
  };

std::optional<app_state> command_new_window(app_state state, uint32_t buffer_id, settings& s);
std::optional<app_state> command_kill(app_state state, uint32_t buffer_id, settings& s);
app_state optimize_column(app_state state, uint32_t buffer_id, settings& s);
void kill(app_state& state, uint32_t buffer_id);
std::string get_user_command_text(const file_buffer& fb);
std::optional<app_state> command_copy_to_snarf_buffer(app_state state, uint32_t, settings& s);
std::optional<app_state> command_paste_from_snarf_buffer(app_state state, uint32_t, settings& s);
std::optional<app_state> command_redo(app_state state, uint32_t buffer_id, settings& s);
std::optional<app_state> command_undo(app_state state, uint32_t buffer_id, settings& s);
std::optional<app_state> command_goto(app_state state, uint32_t buffer_id, settings& s);
std::optional<app_state> command_find(app_state state, uint32_t buffer_id, settings& s);
std::optional<app_state> command_replace(app_state state, uint32_t buffer_id, settings& s);
std::optional<app_state> command_select_all(app_state state, uint32_t buffer_id, settings& s);
std::optional<app_state> load_file(app_state state, uint32_t buffer_id, const std::string& filename, settings& s);
app_state add_error_text(app_state state, const std::string& errortext, settings& s);
app_state replace_all(app_state state, settings& s);
app_state replace_selection(app_state state, settings& s);
uint32_t get_editor_buffer_id(const app_state& state, uint32_t buffer_id);
bool can_be_saved(const std::string& name);

env_settings convert(const settings& s);

struct engine
  {
  app_state state;
  settings s;

  engine(int argc, char** argv, const settings& input_settings);
  ~engine();

  void run();

  };

