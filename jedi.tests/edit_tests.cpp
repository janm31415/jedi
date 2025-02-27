#include "edit_tests.h"

#include "../jedi/edit.h"

#include "test_assert.h"

void parse_test_1() {
  std::string command = "3";
  auto tokens = tokenize(command);
  TEST_EQ(1, tokens.size());
  TEST_EQ(token::T_NUMBER, tokens.front().type);
  TEST_EQ(std::string("3"), tokens.front().value);
  auto expr = parse(tokens);
  TEST_EQ(1, expr.size());
  TEST_ASSERT(std::holds_alternative<AddressRange>(expr.front()));
  TEST_ASSERT(std::get<AddressRange>(expr.front()).fops.empty());
  TEST_ASSERT(std::holds_alternative<LineNumber>(std::get<AddressRange>(expr.front()).operands.front().operands.front()));
  TEST_EQ(3, std::get<LineNumber>(std::get<AddressRange>(expr.front()).operands.front().operands.front()).value);
}

void parse_test_2() {
  std::string command = ".";
  auto tokens = tokenize(command);
  TEST_EQ(1, tokens.size());
  TEST_EQ(token::T_DOT, tokens.front().type);
  auto expr = parse(tokens);
  TEST_EQ(1, expr.size());
  TEST_ASSERT(std::holds_alternative<AddressRange>(expr.front()));
  TEST_ASSERT(std::get<AddressRange>(expr.front()).fops.empty());
  TEST_ASSERT(std::holds_alternative<Dot>(std::get<AddressRange>(expr.front()).operands.front().operands.front()));
}

void parse_test_3() {
  std::string command = "$";
  auto tokens = tokenize(command);
  TEST_EQ(1, tokens.size());
  TEST_EQ(token::T_DOLLAR, tokens.front().type);
  auto expr = parse(tokens);
  TEST_EQ(1, expr.size());
  TEST_ASSERT(std::holds_alternative<AddressRange>(expr.front()));
  TEST_ASSERT(std::get<AddressRange>(expr.front()).fops.empty());
  TEST_ASSERT(std::holds_alternative<EndOfFile>(std::get<AddressRange>(expr.front()).operands.front().operands.front()));
}

void parse_test_4() {
  std::string command = "$-3";
  auto tokens = tokenize(command);
  TEST_EQ(3, tokens.size());
  TEST_EQ(token::T_DOLLAR, tokens[0].type);
  TEST_EQ(token::T_MINUS, tokens[1].type);
  TEST_EQ(token::T_NUMBER, tokens[2].type);
  auto expr = parse(tokens);
  TEST_EQ(1, expr.size());
  TEST_ASSERT(std::holds_alternative<AddressRange>(expr.front()));
  TEST_ASSERT(std::get<AddressRange>(expr.front()).fops.empty());
  TEST_ASSERT(std::holds_alternative<EndOfFile>(std::get<AddressRange>(expr.front()).operands.front().operands[0]));
  TEST_ASSERT(std::holds_alternative<LineNumber>(std::get<AddressRange>(expr.front()).operands.front().operands[1]));
  TEST_EQ(1, std::get<AddressRange>(expr.front()).operands.front().fops.size());
  TEST_ASSERT(std::get<AddressRange>(expr.front()).operands.front().fops[0]==std::string("-"));
}

void parse_test_5() {
  std::string command = "c/Jan/";
  auto tokens = tokenize(command);
  TEST_EQ(4, tokens.size());
  TEST_EQ(token::T_COMMAND, tokens[0].type);
  TEST_EQ(token::T_DELIMITER_SLASH, tokens[1].type);
  TEST_EQ(token::T_TEXT, tokens[2].type);
  TEST_EQ(token::T_DELIMITER_SLASH, tokens[3].type);
  auto expr = parse(tokens);
  TEST_EQ(1, expr.size());
  TEST_ASSERT(std::holds_alternative<Command>(expr.front()));
  TEST_ASSERT(std::holds_alternative<Cmd_c>(std::get<Command>(expr.front())));
  TEST_ASSERT(std::get<Cmd_c>(std::get<Command>(expr.front())).txt.text == std::string("Jan"));
  }
  
void parse_test_6() {
  std::string command = "c";
  auto tokens = tokenize(command);
  TEST_EQ(1, tokens.size());
  TEST_EQ(token::T_COMMAND, tokens[0].type);
  try {
  auto expr = parse(tokens);
  } catch (std::runtime_error& e) {
    TEST_EQ(std::string("I expect a token: /"), std::string(e.what()));
  }
}

void handle_command_test_1() {
  env_settings s;
  s.show_all_characters = false;
  s.tab_space = 8;
  file_buffer fb = make_empty_buffer();
  fb = insert(fb, "The quick brown fox jumps over the lazy dog", s);
  TEST_ASSERT(fb.start_selection == std::nullopt);
  TEST_ASSERT(fb.pos == get_last_position(fb));
  fb = handle_command(fb, "1", s);
  TEST_ASSERT(fb.start_selection == position(0, 0));
  TEST_ASSERT(fb.pos == position(0, fb.content[0].size()-1));
  fb = handle_command(fb, "#1", s);
  TEST_ASSERT(fb.start_selection == std::nullopt);
  TEST_ASSERT(fb.pos == position(0, 1));
  fb = handle_command(fb, "#0", s);
  TEST_ASSERT(fb.start_selection == std::nullopt);
  TEST_ASSERT(fb.pos == position(0, 0));
  fb = handle_command(fb, ",", s);
  TEST_ASSERT(fb.start_selection == position(0, 0));
  TEST_ASSERT(fb.pos == position(0, fb.content[0].size()));
  fb = make_empty_buffer();
  fb = handle_command(fb, "a/ABCDE/", s);
  fb = handle_command(fb, "/B/", s);
  TEST_ASSERT(fb.pos == position(0,1));
  TEST_ASSERT(*fb.start_selection == position(0,1));
  fb = handle_command(fb, "#0,#0", s);
  TEST_ASSERT(fb.start_selection == std::nullopt);
  TEST_ASSERT(fb.pos == position(0, 0));
  fb = handle_command(fb, "#0,#1", s);
  TEST_ASSERT(fb.start_selection == position(0, 0));
  TEST_ASSERT(fb.pos == position(0, 0));
  fb = handle_command(fb, ", c/A/", s);
  fb = handle_command(fb, "#0,#1", s);
  TEST_ASSERT(fb.start_selection == position(0, 0));
  TEST_ASSERT(fb.pos == position(0, 1));
}

void handle_command_test_2() {
  env_settings s;
  s.show_all_characters = false;
  s.tab_space = 8;
  file_buffer fb = make_empty_buffer();
  fb = handle_command(fb, "a/Hello world/", s);
  TEST_ASSERT(to_string(fb.content)==std::string("Hello world"));
  fb = handle_command(fb, "a/!/", s);
  TEST_ASSERT(to_string(fb.content)==std::string("Hello world!"));
  fb = handle_command(fb, "a/\\nHere is a newline./", s);
  TEST_ASSERT(to_string(fb.content)==std::string("Hello world!\nHere is a newline."));
  fb.pos = position(0, 0);
  fb.start_selection = position(0, 10);
  fb = handle_command(fb, "a/INS/", s);
  TEST_ASSERT(to_string(fb.content)==std::string("Hello worlINSd!\nHere is a newline."));
}

void handle_command_test_3() {
  env_settings s;
  s.show_all_characters = false;
  s.tab_space = 8;
  file_buffer fb = make_empty_buffer();
  fb = insert(fb, "The quick brown fox jumps over the lazy dog", s);
  fb = handle_command(fb, "/q.*k/", s);
  TEST_ASSERT(fb.start_selection == position(0, 4));
  TEST_ASSERT(fb.pos == position(0, 8));
  }
  
void handle_command_test_4() {
  env_settings s;
  s.show_all_characters = false;
  s.tab_space = 8;
  file_buffer fb = make_empty_buffer();
  fb = insert(fb, "The quick brown fox jumps over the lazy dog", s);
  fb = handle_command(fb, "c/AAA/", s);
  TEST_ASSERT(to_string(fb.content)==std::string("The quick brown fox jumps over the lazy dogAAA"));
  }
  
void handle_command_test_5() {
  env_settings s;
  s.show_all_characters = false;
  s.tab_space = 8;
  file_buffer fb = make_empty_buffer();
  fb = insert(fb, "The quick brown fox jumps over the lazy dog", s);
  fb = handle_command(fb, ",c/AAA/", s);
  TEST_ASSERT(to_string(fb.content)==std::string("AAA"));
  //printf("%s\n", to_string(fb.content).c_str());
  }
  
void handle_command_test_6() {
  env_settings s;
  s.show_all_characters = false;
  s.tab_space = 8;
  file_buffer fb = make_empty_buffer();
  fb = handle_command(fb, "a/Peter/", s);
  fb = handle_command(fb, "s/t/st/", s);
  TEST_ASSERT(to_string(fb.content)==std::string("Pester"));
  //printf("%s\n", to_string(fb.content).c_str());
  fb = handle_command(fb, "u", s);
  TEST_ASSERT(to_string(fb.content)==std::string("Peter"));
  }
  
void handle_command_test_7() {
  env_settings s;
  s.show_all_characters = false;
  s.tab_space = 8;
  file_buffer fb = make_empty_buffer();
  fb = handle_command(fb, "a/The quick brown fox jumps over the lazy dog/", s);
  fb = handle_command(fb, "s/ck/cker/", s);
  fb = handle_command(fb, "m #15", s);
  TEST_ASSERT(to_string(fb.content)==std::string("The qui browckern fox jumps over the lazy dog"));
  //printf("%s\n", to_string(fb.content).c_str());
}

void handle_command_test_8() {
  env_settings s;
  s.show_all_characters = false;
  s.tab_space = 8;
  file_buffer fb = make_empty_buffer();
  fb = handle_command(fb, "a/The quick brown fox jumps over the lazy dog/", s);
  fb = handle_command(fb, "/b/ c/B/", s);
  TEST_ASSERT(to_string(fb.content)==std::string("The quick Brown fox jumps over the lazy dog"));
  //printf("%s\n", to_string(fb.content).c_str());
}

void handle_command_test_9() {
  env_settings s;
  s.show_all_characters = false;
  s.tab_space = 8;
  file_buffer fb = make_empty_buffer();
  fb = handle_command(fb, "a/AAA/", s);
  fb = handle_command(fb, "s/B*/prr/", s);
  TEST_ASSERT(to_string(fb.content)==std::string("prrAAA"));
  //printf("%s\n", to_string(fb.content).c_str());
}

void handle_command_test_10() {
  env_settings s;
  s.show_all_characters = false;
  s.tab_space = 8;
  file_buffer fb = make_empty_buffer();
  fb = handle_command(fb, "a/AA/", s);
  fb = handle_command(fb, "#1 i/B/", s);
  TEST_ASSERT(to_string(fb.content)==std::string("ABA"));
  //printf("%s\n", to_string(fb.content).c_str());
  fb = handle_command(fb, "/C*/ i/C/", s);
  TEST_ASSERT(to_string(fb.content)==std::string("CABA"));
  //printf("%s\n", to_string(fb.content).c_str());
}

void handle_command_test_11() {
  env_settings s;
  s.show_all_characters = false;
  s.tab_space = 8;
  file_buffer fb = make_empty_buffer();
  fb = handle_command(fb, "a/ABCDE/", s);
  fb = handle_command(fb, "/B/ m #3", s);
  TEST_ASSERT(to_string(fb.content)==std::string("ACDBE"));
  //printf("%s\n", to_string(fb.content).c_str());
}

void handle_command_test_12() {
  env_settings s;
  s.show_all_characters = false;
  s.tab_space = 8;
  file_buffer fb = make_empty_buffer();
  fb = handle_command(fb, "a/The quick brown fox jumps over the lazy dog/", s);
  fb = handle_command(fb, ", c/AAA/", s);
  fb = handle_command(fb, "x/B*/ c/-/", s);
  TEST_ASSERT(to_string(fb.content)==std::string("-A-A-A-"));
  //printf("%s\n", to_string(fb.content).c_str());
}

void handle_command_test_13() {
  env_settings s;
  s.show_all_characters = false;
  s.tab_space = 8;
  file_buffer fb = make_empty_buffer();
  fb = handle_command(fb, "a/This text contains a newline\nYou see?\nWhat a text!/", s);
  fb = handle_command(fb, ", x/text/ d", s); // delete all occurences of text
  TEST_ASSERT(to_string(fb.content)==std::string("This  contains a newline\nYou see?\nWhat a !"));
  //printf("%s\n", to_string(fb.content).c_str());
}

void handle_command_test_14() {
  env_settings s;
  s.show_all_characters = false;
  s.tab_space = 8;
  file_buffer fb = make_empty_buffer();
  fb = handle_command(fb, "a/The quick brown fox jumps over the lazy dog/", s);
  std::string error;
  try {
  fb = handle_command(fb, "100", s);
  } catch (std::runtime_error e) {
    error = std::string(e.what());
  }
  TEST_ASSERT(error == std::string("Invalid address"));
  error = "";
  try {
  fb = handle_command(fb, "1", s);
  } catch (std::runtime_error e) {
    error = std::string(e.what());
  }
  TEST_ASSERT(error == std::string(""));
  error = "";
  try {
  fb = handle_command(fb, "#500", s);
  } catch (std::runtime_error e) {
    error = std::string(e.what());
  }
  TEST_ASSERT(error == std::string("Invalid address"));
}

void handle_command_test_15() {
  env_settings s;
  s.show_all_characters = false;
  s.tab_space = 8;
  file_buffer fb = make_empty_buffer();
  fb = handle_command(fb, "a/The quick brown fox jumps over the lazy dog/", s);
  fb = handle_command(fb, ", c/AAA/", s);
  fb = handle_command(fb, "y/A/ c/-/", s);
  TEST_ASSERT(to_string(fb.content) == std::string("-A-A-A-"));
  //printf("%s\n", to_string(fb.content).c_str());
  }

void handle_command_test_16() {
  env_settings s;
  s.show_all_characters = false;
  s.tab_space = 8;
  file_buffer fb = make_empty_buffer();
  fb = handle_command(fb, "a/The quick brown fox jumps over the lazy dog/", s);
  fb = handle_command(fb, "#10,$ c/AAA/", s);
  TEST_ASSERT(to_string(fb.content) == std::string("The quick AAA"));
  printf("%s\n", to_string(fb.content).c_str());
  }

void run_all_edit_tests() {
  parse_test_1();
  parse_test_2();
  parse_test_3();
  parse_test_4();
  parse_test_5();
  parse_test_6();
  handle_command_test_1();
  handle_command_test_2();
  handle_command_test_3();
  handle_command_test_4();
  handle_command_test_5();
  handle_command_test_6();
  handle_command_test_7();
  handle_command_test_8();
  handle_command_test_9();
  handle_command_test_10();
  handle_command_test_11();
  handle_command_test_12();
  handle_command_test_13();
  handle_command_test_14();
  handle_command_test_15();
  handle_command_test_16();
}
