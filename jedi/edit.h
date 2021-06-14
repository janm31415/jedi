#pragma once

#include <string>
#include <variant>
#include <vector>

#include "buffer.h"

enum error_type
{
  bad_syntax,
  no_tokens,
  command_expected,
  address_expected,
  token_expected,
  invalid_address,
  invalid_regex,
  pipe_error,
  not_implemented
};

void throw_error(error_type t, std::string extra = std::string(""));

struct token
{
  enum e_type
  {
    T_BAD,
    T_NUMBER,
    T_DOT,
    T_PLUS,
    T_DOLLAR,
    T_MINUS,
    T_COMMAND,
    T_FILENAME,
    T_DELIMITER_SLASH,
    T_COMMA,
    T_HASHTAG,
    T_TEXT,
    T_EXTERNAL_COMMAND
  };
  
  e_type type;
  std::string value;
  
  token(e_type i_type, const std::string& v) : type(i_type), value(v) {}
};

std::vector<token> tokenize(const std::string& str);

struct LineNumber;
struct CharacterNumber;
struct Dot;
struct EndOfFile;
struct RegExp;

struct LineNumber
{
  int64_t value;
};

struct CharacterNumber
{
  int64_t value;
};

struct Dot
{
};

struct EndOfFile
{
};

struct RegExp
{
  std::string regexp;
};

typedef std::variant<CharacterNumber, Dot, EndOfFile, LineNumber, RegExp> SimpleAddress;

template<typename T>
class Precedence { public: std::vector<T> operands; std::vector<std::string> fops; };

typedef Precedence<SimpleAddress> AddressTerm;
typedef Precedence<AddressTerm> AddressRange;

struct Text
{
  std::string text;
};

struct Cmd_a // Append text after dot
{
  Text txt;
};

struct Cmd_c // Change text in dot
{
  Text txt;
};

struct Cmd_i // Insert text before dot
{
  Text txt;
};

struct Cmd_d // Delete text in dot
{
};

struct Cmd_s // Substitute text for match of regular expression in dot
{
  Text txt;
  RegExp regexp;
};

struct Cmd_m // Move text in dot after address
{
  AddressRange addr;
};

struct Cmd_t // Copy text in dot after address
{
  AddressRange addr;
};

struct Cmd_u // Undo last n (default 1) changes
{
  uint64_t value;
};

struct Cmd_e // Replace file with named disc file
{
  std::string filename;
};

struct Cmd_r // Replace dot by contents of named disc file
{
  std::string filename;
};

struct Cmd_w // Write file to named disc file
{
  std::string filename;
};

struct Cmd_x; // For each match of regexp, set dot and run command

struct Cmd_y; // Complement of Cmd_x: Between each match of regexp, set dot and run command

struct Cmd_g; // If dot contains match of regexp, run command

struct Cmd_v; // If dot does not contain a match of regexp, run command

struct Cmd_null
{
  
};

typedef std::variant<Cmd_a, Cmd_c, Cmd_d, Cmd_e, Cmd_g, Cmd_i, Cmd_m, Cmd_r, Cmd_s, Cmd_t, Cmd_u, Cmd_v, Cmd_w, Cmd_x, Cmd_y, Cmd_null> Command;

struct Cmd_g // If dot contains match of regexp, run command
{
  RegExp regexp;
  std::vector<Command> cmd;
};

struct Cmd_v // If dot does not contain a match of regexp, run command
{
  RegExp regexp;
  std::vector<Command> cmd;
};

struct Cmd_x // For each match of regexp, set dot and run command
{
  RegExp regexp;
  std::vector<Command> cmd;
};

struct Cmd_y // Complement of Cmd_x: Between each match of regexp, set dot and run command
  {
  RegExp regexp;
  std::vector<Command> cmd;
  };

typedef std::variant<AddressRange, Command> Expression;

std::vector<Expression> parse(std::vector<token> tokens);


file_buffer handle_command(file_buffer fb, std::string command, const env_settings& s);
