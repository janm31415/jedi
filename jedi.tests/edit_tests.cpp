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

void run_all_edit_tests() {
  parse_test_1();
  parse_test_2();
  parse_test_3();
  parse_test_4();
  parse_test_5();
}
