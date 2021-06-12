#include "edit.h"

#include <functional>
#include <inttypes.h>
#include <stdio.h>
#include <sstream>

void throw_error(error_type t, std::string extra)
{
  std::stringstream str;
  switch (t)
  {
    case bad_syntax:
      str << "Bad syntax";
      break;
    case no_tokens:
      str << "I expect more tokens in this command";
      break;
    case command_expected:
      str << "I expect a command";
      break;
    case address_expected:
      str << "I expect an address";
      break;
    case token_expected:
      str << "I expect a token";
      break;
    case invalid_regex:
      str << "Invalid regular expression";
      break;
    case pipe_error:
      str << "Pipe error";
      break;
    case invalid_address:
      str << "Invalid address";
      break;
    case not_implemented:
      str << "Not implemented";
      break;
  }
  if (!extra.empty())
    str << ": " << extra;
  throw std::runtime_error(str.str());
}

namespace
{
bool ignore_character(const char& ch)
{
  return (ch == ' ' || ch == '\n' || ch == '\t');
}

bool is_number(const char* s)
{
  if (s[0] == '\0')
    return false;
  while (*s != '\0')
  {
    if (isdigit((unsigned char)(*s)) == 0)
    {
      return false;
    }
    ++s;
  }
  return true;
}

void _treat_escape_characters(std::string& s)
{
  auto pos = s.find_first_of('\\');
  if (pos == std::string::npos)
    return;
  
  std::stringstream str;
  while (pos != std::string::npos)
  {
    str << s.substr(0, pos);
    if (pos + 1 < s.length())
    {
      switch (s[pos + 1])
      {
        case 'n': str << '\n'; break;
        case 'r': str << '\r'; break;
        case 't': str << '\t'; break;
        case '\\': str << '\\'; break;
        default: str << "\\" << s[pos + 1]; break;
      }
      s = s.substr(pos + 2);
      pos = s.find_first_of('\\');
    }
    else
      pos = std::string::npos;
  }
  str << s;
  s = str.str();
}

void _treat_escape_characters(std::vector<token>& tokens)
{
  for (auto& token : tokens)
  {
    if (token.type == token::T_TEXT)
      _treat_escape_characters(token.value);
  }
}

void _treat_number(std::string& number, std::vector<token>& tokens)
{
  if (!number.empty())
  {
    tokens.emplace_back(token::T_NUMBER, number);
  }
  number.clear();
}

bool is_filename_command(char ch)
{
  return ch == 'e' || ch == 'w' || ch == 'r' || ch == 'l';
}

bool is_external_command(char ch)
{
  return ch == '<' || ch == '>' || ch == '|' || ch == '!';
}

void _treat_buffer(std::string& buff, std::vector<token>& tokens, const char*& s, const char* s_end)
{
  if (!buff.empty())
  {
    std::string number;
    for (size_t i = 0; i < buff.length(); ++i)
    {
      if (!isdigit(buff[i]))
      {
        _treat_number(number, tokens);
        tokens.emplace_back(token::T_COMMAND, "");
        tokens.back().value.push_back(buff[i]);
        if (is_filename_command(buff[i]))
        {
          std::string filename = buff.substr(i + 1);
          if (filename.empty())
          {
            while (ignore_character(*s))
              ++s;
            if (s != s_end && *s == '"')
            {
              ++s;
              while (s < s_end && (*s != '"'))
              {
                filename.push_back(*s);
                ++s;
              }
              if (s < s_end)
                ++s;
            }
            else
            {
              while (s < s_end && !ignore_character(*s))
              {
                filename.push_back(*s);
                ++s;
              }
            }
          }
          tokens.emplace_back(token::T_FILENAME, filename);
          i = buff.length();
        }
        else if (is_external_command(buff[i]))
        {
          std::string command = buff.substr(i + 1);
          while (s < s_end)
          {
            command.push_back(*s);
            ++s;
          }
          tokens.emplace_back(token::T_EXTERNAL_COMMAND, command);
          i = buff.length();
        }
      }
      else
      {
        number.push_back(buff[i]);
      }
    }
    _treat_number(number, tokens);
  }
  buff.clear();
}
}

std::vector<token> tokenize(const std::string& str)
{
  std::vector<token> tokens;
  std::string buff;
  const char* s = str.c_str();
  const char* s_end = str.c_str() + str.length();
  while (s < s_end)
  {
    if (ignore_character(*s))
    {
      _treat_buffer(buff, tokens, s, s_end);
      while (ignore_character(*s))
        ++s;
    }
    
    
    switch (*s)
    {
      case ',':
      case '.':
      case '+':
      case '-':
      case '$':
      case '#':
      case '/':
        _treat_buffer(buff, tokens, s, s_end);
        break;
      default: break;
    }
    
    if (s >= s_end)
      break;
    
    const char* s_copy = s;
    switch (*s)
    {
      case ',':
        tokens.emplace_back(token::T_COMMA, ",");
        ++s;
        break;
      case '.':
        tokens.emplace_back(token::T_DOT, ".");
        ++s;
        break;
      case '+':
        tokens.emplace_back(token::T_PLUS, "+");
        ++s;
        break;
      case '-':
        tokens.emplace_back(token::T_MINUS, "-");
        ++s;
        break;
      case '$':
        tokens.emplace_back(token::T_DOLLAR, "$");
        ++s;
        break;
      case '#':
        tokens.emplace_back(token::T_HASHTAG, "#");
        ++s;
        break;
      case '/':
        bool expect_two_delimiting_texts = false;
        if (!tokens.empty() && tokens.back().type == token::T_COMMAND && tokens.back().value == "s")
          expect_two_delimiting_texts = true;
        tokens.emplace_back(token::T_DELIMITER_SLASH, "/");
        const char* t = s;
        const char* prev_t = s;
        ++t;
        std::string text;
        bool add_to_text = true;
        int nr_of_backslashes = 0;
        while (add_to_text)
        {
          if (*prev_t == '\\')
          {
            ++nr_of_backslashes;
          }
          else
            nr_of_backslashes = 0;
          if (*t == '\0')
          {
            tokens.emplace_back(token::T_TEXT, text);
            add_to_text = false;
            s = t;
          }
          else if (*t == '/')
          {
            if (nr_of_backslashes%2 == 1)
            {
              text.back() = '/';
            }
            else
            {
              tokens.emplace_back(token::T_TEXT, text);
              add_to_text = false;
              s = t;
              if (!expect_two_delimiting_texts)
              {
                tokens.emplace_back(token::T_DELIMITER_SLASH, "/");
                ++s;
              }
            }
          }
          else
          {
            text.push_back(*t);
          }
          prev_t = t;
          ++t;
        }
        break;
    }
    
    if (s_copy == s)
    {
      buff += *s;
      ++s;
    }
    
  }
  
  _treat_buffer(buff, tokens, s, s_end);
  _treat_escape_characters(tokens);
  return tokens;
}

namespace
{
token::e_type current_type(const std::vector<token>& tokens)
{
  return tokens.empty() ? token::T_BAD : tokens.back().type;
}

std::string current(const std::vector<token>& tokens)
{
  return tokens.empty() ? std::string() : tokens.back().value;
}

token take(std::vector<token>& tokens)
{
  if (tokens.empty())
  {
    throw_error(no_tokens);
  }
  token t = tokens.back();
  tokens.pop_back();
  return t;
}

void require(std::vector<token>& tokens, std::string required)
{
  if (tokens.empty())
  {
    throw_error(token_expected, required);
  }
  auto t = take(tokens);
  if (t.value != required)
    throw_error(token_expected, required);
}

void require_type(std::vector<token>& tokens, token::e_type required_type, std::string required)
{
  if (tokens.empty())
  {
    throw_error(token_expected, required);
  }
  auto t = take(tokens);
  if (t.type != required_type)
    throw_error(token_expected, required);
}

uint64_t s64(const char *s)
{
  uint64_t i;
  char c;
  sscanf(s, "%" SCNu64 "%c", &i, &c);
  return i;
}

SimpleAddress make_simple_address(std::vector<token>& tokens, AddressTerm& node)
{
  if (tokens.empty())
  {
    if (node.fops.empty())
      return EndOfFile();
    LineNumber ln;
    ln.value = 1;
    return ln;
  }
  
  auto t = take(tokens);
  switch (t.type)
  {
    case token::T_NUMBER:
    {
      LineNumber ln;
      ln.value = s64(t.value.c_str());
      return ln;
    }
    case token::T_HASHTAG:
    {
      CharacterNumber cn;
      cn.value = s64(current(tokens).c_str());
      require_type(tokens, token::T_NUMBER, "number");
      return cn;
    }
    case token::T_DELIMITER_SLASH:
    {
      RegExp re;
      re.regexp = current(tokens);
      require_type(tokens, token::T_TEXT, "regexp");
      require_type(tokens, token::T_DELIMITER_SLASH, "/");
      return re;
    }
    case token::T_DOLLAR:
    {
      return EndOfFile();
    }
    case token::T_DOT:
    {
      return Dot();
    }
    case token::T_COMMA:
    {
      tokens.push_back(t);
      LineNumber ln;
      ln.value = 0;
      return ln;
    }
    case token::T_PLUS:
    {
      tokens.push_back(t);
      return Dot();
    }
    case token::T_MINUS:
    {
      tokens.push_back(t);
      return Dot();
    }
    case token::T_COMMAND:
    {
      tokens.push_back(t);
      if (node.fops.empty())
        return EndOfFile();
      LineNumber ln;
      ln.value = 1;
      return ln;
    }
  default: break;
  }
  throw_error(address_expected);
  return Dot();
}


template <typename T, typename U>
T parse_multiop(std::vector<token>& tokens, std::function<U(std::vector<token>&, T&)> make, std::vector<std::string> ops)
{
  T node;
  node.operands.push_back(make(tokens, node));
  while (1) {
    std::string op = current(tokens);
    std::vector<std::string>::iterator opit = std::find(ops.begin(), ops.end(), op);
    if (opit == ops.end())
      break;
    tokens.pop_back();
    node.fops.push_back(op);
    node.operands.push_back(make(tokens, node));
  }
  return node;
}

AddressTerm make_address_term(std::vector<token>& tokens, AddressRange&) { return parse_multiop<AddressTerm, SimpleAddress>(tokens, make_simple_address, { "+", "-" }); }

AddressRange make_address_range(std::vector<token>& tokens) { return parse_multiop<AddressRange, AddressTerm>(tokens, make_address_term, { "," }); }

Command make_command(std::vector<token>& tokens)
{
  if (current_type(tokens) != token::T_COMMAND)
    throw_error(command_expected);
  auto t = take(tokens);
  switch (t.value[0])
  {
    case 'a':
    {
      Cmd_a cmd;
      require_type(tokens, token::T_DELIMITER_SLASH, "/");
      cmd.txt.text = current(tokens);
      require_type(tokens, token::T_TEXT, "text");
      require_type(tokens, token::T_DELIMITER_SLASH, "/");
      return cmd;
    }
    case 'c':
    {
      Cmd_c cmd;
      require_type(tokens, token::T_DELIMITER_SLASH, "/");
      cmd.txt.text = current(tokens);
      require_type(tokens, token::T_TEXT, "text");
      require_type(tokens, token::T_DELIMITER_SLASH, "/");
      return cmd;
    }
    case 'd':
    {
      return Cmd_d();
    }
    case 'e':
    {
      Cmd_e cmd;
      cmd.filename = current(tokens);
      require_type(tokens, token::T_FILENAME, "filename");
      return cmd;
    }
    case 'g':
    {
      Cmd_g cmd;
      require_type(tokens, token::T_DELIMITER_SLASH, "/");
      cmd.regexp.regexp = current(tokens);
      require_type(tokens, token::T_TEXT, "regexp");
      require_type(tokens, token::T_DELIMITER_SLASH, "/");
      cmd.cmd.push_back(make_command(tokens));
      return cmd;
    }
    case 'i':
    {
      Cmd_i cmd;
      require_type(tokens, token::T_DELIMITER_SLASH, "/");
      cmd.txt.text = current(tokens);
      require_type(tokens, token::T_TEXT, "text");
      require_type(tokens, token::T_DELIMITER_SLASH, "/");
      return cmd;
    }
    case 'm':
    {
      Cmd_m cmd;
      cmd.addr = make_address_range(tokens);
      return cmd;
    }
    case 'p':
    {
      return Cmd_p();
    }
    case 'r':
    {
      Cmd_r cmd;
      cmd.filename = current(tokens);
      require_type(tokens, token::T_FILENAME, "filename");
      return cmd;
    }
    case 's':
    {
      Cmd_s cmd;
      require_type(tokens, token::T_DELIMITER_SLASH, "/");
      cmd.regexp.regexp = current(tokens);
      require_type(tokens, token::T_TEXT, "regexp");
      require_type(tokens, token::T_DELIMITER_SLASH, "/");
      cmd.txt.text = current(tokens);
      require_type(tokens, token::T_TEXT, "text");
      require_type(tokens, token::T_DELIMITER_SLASH, "/");
      return cmd;
    }
    case 't':
    {
      Cmd_t cmd;
      cmd.addr = make_address_range(tokens);
      return cmd;
    }
    case 'u':
    {
      Cmd_u cmd;
      cmd.value = 1;
      if (current_type(tokens) == token::T_NUMBER)
      {
        cmd.value = s64(current(tokens).c_str());
        tokens.pop_back();
      }
      return cmd;
    }
    case 'v':
    {
      Cmd_v cmd;
      require_type(tokens, token::T_DELIMITER_SLASH, "/");
      cmd.regexp.regexp = current(tokens);
      require_type(tokens, token::T_TEXT, "regexp");
      require_type(tokens, token::T_DELIMITER_SLASH, "/");
      cmd.cmd.push_back(make_command(tokens));
      return cmd;
    }
    case 'w':
    {
      Cmd_w cmd;
      cmd.filename = current(tokens);
      require_type(tokens, token::T_FILENAME, "filename");
      return cmd;
    }
    case 'x':
    {
      Cmd_x cmd;
      require_type(tokens, token::T_DELIMITER_SLASH, "/");
      cmd.regexp.regexp = current(tokens);
      require_type(tokens, token::T_TEXT, "regexp");
      require_type(tokens, token::T_DELIMITER_SLASH, "/");
      cmd.cmd.push_back(make_command(tokens));
      return cmd;
    }
    case '=':
    {
      return Cmd_p_dot();
    }
  }
  throw_error(command_expected);
  return Cmd_null();
}

Expression make_expression(std::vector<token>& tokens)
{
  if (tokens.empty())
    throw_error(no_tokens);
  switch (current_type(tokens))
  {
    case token::T_COMMAND: return make_command(tokens);
    default: return make_address_range(tokens);
  }
}
}

std::vector<Expression> parse(std::vector<token> tokens)
{
  std::reverse(tokens.begin(), tokens.end());
  std::vector<Expression> out;
  
  while (!tokens.empty())
  {
    out.push_back(make_expression(tokens));
  }
  return out;
}
