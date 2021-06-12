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

void run_all_edit_tests() {
  parse_test_1();
  parse_test_2();
}
