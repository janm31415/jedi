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

namespace {

struct address {
  position p1, p2;
};



struct simple_address_handler
{
  file_buffer f;
  position starting_pos;
  bool reverse;
  
  simple_address_handler(file_buffer i_f, position i_starting_pos, bool i_reverse) : f(i_f),
  starting_pos(i_starting_pos), reverse(i_reverse) {}
  
  void check_range(address& r)
  {
    if (r.p2 < r.p1)
      std::swap(r.p1, r.p2);
    if (r.p1.row >= f.content.size())
      r.p1.row = (int64_t)f.content.size() - 1;
    if (r.p2.row >= f.content.size())
      r.p2.row = (int64_t)f.content.size() - 1;
    if (r.p1.row < 0)
      r.p1.row = 0;
    if (r.p2.row < 0)
      r.p2.row = 0;
    if (!f.content.empty()) {
      if (r.p1.col >= f.content[r.p1.row].size())
        r.p1.col = (int64_t)f.content[r.p1.row].size()-1;
      if (r.p2.col >= f.content[r.p2.row].size())
        r.p2.col = (int64_t)f.content[r.p2.row].size()-1;
    }
    if (r.p1.col < 0)
      r.p1.col = 0;
    if (r.p2.col < 0)
      r.p2.col = 0;
  }
  
  address operator()(const CharacterNumber& cn)
  {
    int64_t v = (int64_t)cn.value;
    address r;
    position pos = starting_pos;
    if (f.content.empty()) {
      r.p1 = r.p2 = pos;
      check_range(r);
      return r;
    }
    if (reverse)
    {
      while ((pos.row > 0 || pos.col > 0) && v>0) {
        if (v >= pos.col) {
          v -= pos.col;
          pos.col = 0;
          if (pos.row) {
            --pos.row;
            pos.col = f.content[pos.row].size();
          }
        } else {
          pos.col -= v;
          v = 0;
        }
      }
    }
    else
    {
      while ((pos.row < (f.content.size()-1) || (pos.col+1) < f.content[pos.row].size()) && v>0) {
        if (v >= f.content[pos.row].size()) {
          v -= (f.content[pos.row].size()-pos.col);
          pos.col = 0;
          ++pos.row;
          }
        else {
          pos.col += v;
          v = 0;
        }
      }
    }
    r.p1 = pos;
    r.p2 = pos;
    check_range(r);
    return r;
  }
  
  address operator()(const Dot&)
  {
    address r;
    r.p1 = f.pos;
    r.p2 = f.start_selection ? *f.start_selection : f.pos;
    check_range(r);
    return r;
  }
  
  address operator()(const EndOfFile&)
  {
    address r;
    r.p1 = r.p2 = get_last_position(f);
    return r;
  }
  
  address operator()(const LineNumber& ln)
  {
    address r;
    r.p1 = starting_pos;
    r.p2 = starting_pos;
    if (reverse) {
      r.p1.row -= ln.value;
      r.p2.row -= ln.value;
      }
    else {
      r.p1.row += ln.value;
      r.p2.row += ln.value;
    }
    check_range(r);
    r.p1.col = 0;
    r.p2.col = (int64_t)(f.content[r.p2.row].size())-1;
    check_range(r);
    return r;
  }
  
  address operator()(const RegExp& re)
  {
    address ret;
    ret.p1 = ret.p2 = starting_pos;
    /*
    try
    {
      ret = find_regex_range(re.regexp, f.content, reverse, starting_pos, f.enc);
    }
    catch (std::regex_error e)
    {
      throw_error(invalid_regex, e.what());
    }
    */
    return ret;
  }
  
};

address interpret_simple_address(const SimpleAddress& term, position starting_pos, bool reverse, file_buffer f)
{
  address out;
  simple_address_handler sah(f, starting_pos, reverse);
  out = std::visit(sah, term);
  return out;
}

address interpret_address_term(const AddressTerm& addr, file_buffer f)
{
  address out;
  if (addr.operands.size() > 2)
    throw_error(invalid_address);
  if (addr.operands.empty())
    throw_error(invalid_address);
  out = interpret_simple_address(addr.operands[0], position(0, 0), false, f);
  if (addr.operands.size() == 2)
  {
    if (addr.fops[0] == "+")
    {
      return interpret_simple_address(addr.operands[1], out.p2, false, f);
    }
    else if (addr.fops[0] == "-")
    {
      return interpret_simple_address(addr.operands[1], out.p1, true, f);
    }
    else
      throw_error(not_implemented, addr.fops[0]);
  }
  return out;
}

address interpret_address_range(const AddressRange& addr, file_buffer f)
{
  address out;
  if (addr.operands.size() > 2)
    throw_error(invalid_address);
  if (addr.operands.empty())
    throw_error(invalid_address);
  out = interpret_address_term(addr.operands[0], f);
  if (addr.operands.size() == 2)
  {
    address right = interpret_address_term(addr.operands[1], f);
    if (addr.fops[0] == ",")
    {
      out.p2 = right.p2;
    }
    else
      throw_error(not_implemented, addr.fops[0]);
  }
  return out;
}

struct expression_handler
{
  file_buffer fb;
  
  expression_handler(file_buffer i_fb) : fb(i_fb) {}
  
  file_buffer operator() (const AddressRange& addr)
  {
    address r = interpret_address_range(addr, fb);
    fb.pos = r.p2;
    fb.start_selection = r.p1;
    return fb;
  }
  
  file_buffer operator() (const Command& cmd)
  {
    //command_handler ch(state);
    //return std::visit(ch, cmd);
    return fb;
  }
};

}

file_buffer handle_command(file_buffer fb, std::string command) {
  auto tokens = tokenize(command);
  auto cmds = parse(tokens);
  for (const auto& cmd : cmds)
  {
    expression_handler eh(fb);
    fb = std::visit(eh, cmd);
  }
  return fb;
}
