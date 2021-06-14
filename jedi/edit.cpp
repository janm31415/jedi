#include "edit.h"

#include <functional>
#include <inttypes.h>
#include <stdio.h>
#include <sstream>
#include <regex>
#include <iterator>

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

std::string _treat_escape_characters(std::string s)
{
  auto pos = s.find_first_of('\\');
  if (pos == std::string::npos)
    return s;
  
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
  return str.str();
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
  address() : null_selection(false) {}
  position p1, p2;
  bool null_selection;
};

address find_regex_range(std::string re, file_buffer fb, bool reverse, position starting_pos)
{
  address r;
  r.null_selection = true;
  
  if (fb.content.empty()) {
    r.p1 = r.p2 = position(0,0);
    return r;
  }
  
  if (reverse)
  {
    r.p1 = r.p2 = position(0, 0);
    std::regex reg(re);
    for (int64_t row = starting_pos.row; row >= 0; --row) {
      std::string line = to_string(fb.content[row]);
      std::smatch sm;
      if (std::regex_search(line, sm, reg)) {
        for (int i = (int)sm.size()-1; i >= 0; --i) {
          int64_t p1 = sm.position(i);
          int64_t p2 = p1 + sm.length(i);
          if (row == starting_pos.row && p2 > starting_pos.col)
            continue;
          r.p1.row = row;
          r.p1.col = p1;
          r.p2.row = row;
          r.p2.col = p2-1;
          if (sm.length(i)==0) {
            r.p2.col = p2;
            r.null_selection = true;
          } else
            r.null_selection = false;
          return r;
        }
      }
    }
  } else
  {
    r.p1 = r.p2 = position(fb.content.size()-1, 0);
    if (!fb.content[r.p1.row].empty()) {
      r.p1.col = fb.content[r.p1.row].size()-1;
      r.p2.col = fb.content[r.p1.row].size()-1;
    }
    std::regex reg(re);
    for (int64_t row = starting_pos.row; row < fb.content.size(); ++row) {
      std::string line = to_string(fb.content[row]);
      std::smatch sm;
      if (std::regex_search(line, sm, reg)) {
        for (int i = 0; i < (int)sm.size(); ++i) {
          int64_t p1 = sm.position(i);
          int64_t p2 = p1 + sm.length(i);
          if (row == starting_pos.row && p1 < starting_pos.col)
            continue;
          r.p1.row = row;
          r.p1.col = p1;
          r.p2.row = row;
          r.p2.col = p2-1;
          if (sm.length(i)==0) {
            r.p2.col = p2;
            r.null_selection = true;
          } else
            r.null_selection = false;
          return r;
        }
      }
    }
  }  
  return r;
}

position recompute_position_after_erase(file_buffer fb, position pos, position erase_p1, position erase_p2) {
  if (pos < erase_p1)
    return pos;
  if (pos < erase_p2) {
    return erase_p1;
  }
  if (pos.row == erase_p2.row) {
    int64_t col_offset = pos.col - erase_p2.col;
    pos = erase_p1;
    pos.col += col_offset;
    return pos;
  }
  pos.row -= (erase_p2.row - erase_p1.row);
  return pos;
}

position recompute_position_after_dot_change(file_buffer fb, position pos, position old_dot_p1, position old_dot_p2, position new_dot_p1, position new_dot_p2) {
  if (old_dot_p1 != new_dot_p1)
    return pos;
  if (pos <= old_dot_p1)
    return pos;
  if (pos <= old_dot_p2)
    return old_dot_p1;
  if (pos.row == old_dot_p2.row) {
    pos.row = new_dot_p2.row;
    int64_t col_diff = pos.col - old_dot_p2.col;
    pos.col = new_dot_p2.col + col_diff;
  } else {
  int64_t old_nr_rows = old_dot_p2.row - old_dot_p1.row;
  int64_t new_nr_rows = new_dot_p2.row - new_dot_p1.row;
  int64_t row_change = new_nr_rows - old_nr_rows;
  pos.row += row_change;
  }
  return pos;
}

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
      r.p1.row = (int64_t)f.content.size()-1;
    if (r.p2.row >= f.content.size())
      r.p2.row = (int64_t)f.content.size()-1;
    if (r.p1.row < 0)
      r.p1.row = 0;
    if (r.p2.row < 0)
      r.p2.row = 0;
    if (!f.content.empty()) {
      if (r.p1.col > f.content[r.p1.row].size())
        r.p1.col = (int64_t)f.content[r.p1.row].size();
      if (r.p2.col > f.content[r.p2.row].size())
        r.p2.col = (int64_t)f.content[r.p2.row].size();
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
    r.null_selection = true;
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
      while ((pos.row < (f.content.size()-1) || (pos.col) < f.content[pos.row].size()) && v>0) {
        if (v > f.content[pos.row].size()) {
          v -= (f.content[pos.row].size()-pos.col);
          pos.col = 0;
          ++pos.row;
          if (pos.row >= f.content.size())
            throw_error(invalid_address);
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
    r.null_selection = f.start_selection == std::nullopt;
    check_range(r);
    return r;
  }
  
  address operator()(const EndOfFile&)
  {
    address r;
    r.p1 = r.p2 = get_last_position(f);
    r.null_selection = true;
    return r;
  }
  
  address operator()(const LineNumber& ln)
  {
    address r;
    r.p1 = starting_pos;
    r.p2 = starting_pos;
    if (reverse) {
      r.p1.row -= (ln.value-1);
      r.p2.row -= (ln.value-1);
      if (r.p1.row < 0)
        throw_error(invalid_address);
    }
    else {
      r.p1.row += (ln.value-1);
      r.p2.row += (ln.value-1);
      if (r.p1.row >= f.content.size())
        throw_error(invalid_address);
    }
    check_range(r);
    r.p1.col = 0;
    r.p2.col = (int64_t)(f.content[r.p2.row].size())-1;
    check_range(r);
    r.null_selection = f.content[r.p2.row].empty();
    return r;
  }
  
  address operator()(const RegExp& re)
  {
    address ret;
    ret.p1 = ret.p2 = starting_pos;
    ret.null_selection = true;
    try
    {
      ret = find_regex_range(re.regexp, f, reverse, starting_pos);
    }
    catch (std::regex_error e)
    {
      throw_error(invalid_regex, e.what());
    }
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
      if (right.p2 < out.p1) {
        out.p2 = out.p1;
        out.null_selection = true;
      }
      else if (right.null_selection) {
        if (right.p2 != get_last_position(f))
          out.p2 = get_previous_position(f, right.p2);
        else
          out.p2 = right.p2;
        if (out.p2 < out.p1)
          out.p2 = out.p1;
        if (out.null_selection) {
          if (right.p2 > out.p1)
            out.null_selection = false;
        }
      } else {
        out.p2 = right.p2;
        out.null_selection = false;
      }
    }
    else
      throw_error(not_implemented, addr.fops[0]);
  }
  return out;
}

struct command_handler
{
  file_buffer fb;
  env_settings s;
  bool save_undo;
  
  command_handler(file_buffer i_fb, const env_settings& i_s) : fb(i_fb), s(i_s), save_undo(true) {}
  
  std::pair<position, position> get_dot() const {
    position p1 = fb.pos;
    position p2 = p1;
    if (fb.start_selection) {
      p2 = *fb.start_selection;
      if (p2 < p1)
        std::swap(p1, p2);
      }
    return std::pair<position, position>(p1, p2);
  }
  
  file_buffer operator() (const Cmd_a& cmd) {
    auto dot = get_dot();
    fb.pos = dot.second;
    auto init_pos = dot.second;
    fb.start_selection = std::nullopt;
    fb = insert(fb, _treat_escape_characters(cmd.txt.text), s, save_undo);
    fb.start_selection = init_pos;
    return fb;
  }
  
  file_buffer operator() (const Cmd_c& cmd) {
    auto dot = get_dot();
    auto init_pos = dot.first;
    if (fb.start_selection == std::nullopt) {
      fb.pos = dot.first;
      fb.start_selection = std::nullopt;
      fb = insert(fb, _treat_escape_characters(cmd.txt.text), s, save_undo);
    } else {
      fb.pos = dot.first;
      fb.start_selection = dot.second;
      bool save_undo_mem = save_undo;
      if (fb.pos == *fb.start_selection) {
        fb = erase_right(fb, s, save_undo_mem);
        save_undo = false;
      }
      fb = insert(fb, _treat_escape_characters(cmd.txt.text), s, save_undo);
      save_undo = save_undo_mem;
    }
    fb.start_selection = init_pos;
    return fb;
  }
  
  file_buffer operator() (const Cmd_d& cmd) {
    return erase_right(fb, s, save_undo);
  }
  
  file_buffer operator() (const Cmd_e& cmd) {
    file_buffer b = read_from_file(cmd.filename);
    fb.start_selection = position(0,0);
    fb.pos = get_last_position(fb);
    fb = erase_right(fb, s, save_undo);
    fb = insert(fb, b.content, s, false);
    fb.start_selection = position(0, 0);
    return fb;
  }
  
  file_buffer operator() (const Cmd_g& cmd) {
    std::regex reg(cmd.regexp.regexp);
    auto dot = get_dot();
        
    for (int64_t row = dot.first.row; row <= dot.second.row; ++row) {
      std::string line = to_string(fb.content[row]);
      int64_t offset = 0;
      if (row == dot.first.row) {
        line = line.substr(dot.first.col);
        offset = dot.first.col;
        }
      if (row == dot.second.row) {
        const int64_t sz = dot.second.col - offset;
        if (sz <= 0)
          continue;
        line = line.substr(0, sz);
      }
      std::smatch sm;
      if (std::regex_search(line, sm, reg)) {
        fb = std::visit(*this, cmd.cmd.front());
        return fb;
      }
    }
  
    return fb;
  }
  
  file_buffer operator() (const Cmd_i& cmd) {
    if (fb.start_selection != std::nullopt) {
      if (*fb.start_selection < fb.pos)
        fb.pos = *fb.start_selection;
      fb.start_selection = std::nullopt;
    }
    auto init_pos = get_dot().first;
    fb = insert(fb, _treat_escape_characters(cmd.txt.text), s, save_undo);
    fb.start_selection = init_pos;
    return fb;
  }
  
  file_buffer operator() (const Cmd_m& cmd) {
    if (fb.start_selection == std::nullopt)
      return fb;
    text t = get_selection(fb, s);
    address addr = interpret_address_range(cmd.addr, fb);
    auto dot = get_dot();
    addr.p1 = recompute_position_after_erase(fb, addr.p1, dot.first, dot.second);
    addr.p2 = recompute_position_after_erase(fb, addr.p2, dot.first, dot.second);
    fb = erase_right(fb, s, save_undo);
    fb.pos = addr.p2;
    fb.start_selection = std::nullopt;
    fb = insert(fb, t, s, false);
    fb.start_selection = addr.p2;
    return fb;
  /*
    text t = get_selection(fb, s);
    address addr = interpret_address_range(cmd.addr, fb);
    position p1 = fb.pos;
    position p2 = p1;
    if (fb.start_selection)
      p2 = *fb.start_selection;
    if (p2 < p1)
      std::swap(p1, p2);
    addr.p1 = recompute_position_after_erase(fb, addr.p1, p1, p2);
    addr.p2 = recompute_position_after_erase(fb, addr.p2, p1, p2);
    fb = erase_right(fb, s, save_undo);
    fb.pos = addr.p2;
    fb.start_selection = std::nullopt;
    fb = insert(fb, t, s, false);
    fb.start_selection = addr.p2;
    return fb;
  */
    /*
    if (fb.start_selection == std::nullopt) {
      return fb;
    } else {
    text t = get_selection(fb, s);
    address addr = interpret_address_range(cmd.addr, fb);
    auto dot = get_dot();
    addr.p1 = recompute_position_after_erase(fb, addr.p1, dot.first, dot.second);
    addr.p2 = recompute_position_after_erase(fb, addr.p2, dot.first, dot.second);
    fb.pos = dot.first;
    fb.start_selection = dot.second;
    fb = erase_right(fb, s, save_undo);
    fb.pos = addr.p2;
    fb.start_selection = std::nullopt;
    fb = insert(fb, t, s, false);
    fb.start_selection = addr.p2;
    return fb;
     */
  }
  
  file_buffer operator() (const Cmd_p& cmd) {
    return fb;
  }
  
  file_buffer operator() (const Cmd_p_dot& cmd) {
    return fb;
  }
  
  file_buffer operator() (const Cmd_r& cmd) {
    file_buffer b = read_from_file(cmd.filename);
    auto init_pos = get_dot().first;
    fb = insert(fb, b.content, s, save_undo);
    fb.start_selection = init_pos;
    return fb;
  }
  
  file_buffer operator() (const Cmd_s& cmd) {
    std::regex reg(cmd.regexp.regexp);
    auto dot = get_dot();
        
    for (int64_t row = dot.first.row; row <= dot.second.row; ++row) {
      std::string line = to_string(fb.content[row]);
      int64_t offset = 0;
      if (row == dot.first.row) {
        line = line.substr(dot.first.col);
        offset = dot.first.col;
        }
      if (row == dot.second.row) {
        const int64_t sz = dot.second.col - offset;
        if (sz <= 0)
          continue;
        line = line.substr(0, sz);
      }
      std::smatch sm;
      if (std::regex_search(line, sm, reg)) {

        int64_t pos1 = sm.position(0);
        int64_t pos2 = pos1 + sm.length(0);
        if (pos2 > pos1)
          pos2 -= 1;
        fb.start_selection = position(row, pos1+offset);
        fb.pos = position(row, pos2+offset);
        auto init_pos = *fb.start_selection;
        if (sm.length(0) == 1) {
          fb = erase_right(fb, s, save_undo);
          fb = insert(fb, cmd.txt.text, s, false);
        } else {
          fb = insert(fb, cmd.txt.text, s, save_undo);
        }
        fb.start_selection = init_pos;
        fb.pos = get_previous_position(fb, fb.pos);
        return fb;
        
      }
    }
  
    return fb;
  }
  
  file_buffer operator() (const Cmd_t& cmd) {
    text t = get_selection(fb, s);
    address addr = interpret_address_range(cmd.addr, fb);
    fb.pos = addr.p2;
    fb.start_selection = std::nullopt;
    fb = insert(fb, t, s, save_undo);
    fb.start_selection = addr.p2;
    return fb;
  }
  
  file_buffer operator() (const Cmd_u& cmd) {
    for (int64_t i = 0; i < cmd.value; ++i)
    fb = undo(fb, s);
    return fb;
  }
  
  file_buffer operator() (const Cmd_v& cmd) {
    std::regex reg(cmd.regexp.regexp);
    auto dot = get_dot();
        
    for (int64_t row = dot.first.row; row <= dot.second.row; ++row) {
      std::string line = to_string(fb.content[row]);
      int64_t offset = 0;
      if (row == dot.first.row) {
        line = line.substr(dot.first.col);
        offset = dot.first.col;
        }
      if (row == dot.second.row) {
        const int64_t sz = dot.second.col - offset;
        if (sz <= 0)
          continue;
        line = line.substr(0, sz);
      }
      std::smatch sm;
      if (std::regex_search(line, sm, reg)) {
        return fb;
      }
    }
    fb = std::visit(*this, cmd.cmd.front());
    return fb;
  }
  
  file_buffer operator() (const Cmd_w& cmd) {
    bool success;
    save_to_file(success, fb, cmd.filename);
    return fb;
  }
  
  file_buffer operator() (const Cmd_x& cmd) {
  
    if (save_undo)
      fb = push_undo(fb);
  
    bool save_undo_backup = save_undo;
    save_undo = false;
    
    std::regex reg(cmd.regexp.regexp);
    auto dot = get_dot();
        
        
    for (int64_t row = dot.first.row; row <= dot.second.row; ++row) {
      std::string line = to_string(fb.content[row]);
      int64_t offset = 0;
      if (row == dot.first.row) {
        line = line.substr(dot.first.col);
        offset = dot.first.col;
        }
      if (row == dot.second.row) {
        const int64_t sz = dot.second.col - offset;
        if (sz <= 0)
          continue;
        line = line.substr(0, sz);
      }
      if (line.empty())
        continue;
      std::smatch sm;
      if (std::regex_search(line, sm, reg)) {
        int64_t pos1 = sm.position(0);
        int64_t pos2 = pos1 + sm.length(0);
        if (pos2 > pos1)
          pos2 -= 1;
        fb.start_selection = position(row, pos1+offset);
        fb.pos = position(row, pos2+offset);
        auto init_pos = *fb.start_selection;
        if (sm.length(0) == 0)
          fb.start_selection = std::nullopt;
        
        fb = std::visit(*this, cmd.cmd.front());

        position new_p1 = fb.pos;
        position new_p2 = fb.pos;
        if (fb.start_selection)
          new_p2 = *fb.start_selection;
        if (new_p2 < new_p1)
          std::swap(new_p1, new_p2);
        position end = recompute_position_after_dot_change(fb, dot.second, position(row, pos1+offset), position(row, pos2+offset), new_p1, new_p2);
        
        dot.first = fb.pos;
        dot.second = end;
        
        if (sm.length(0)==0)
          dot.first = get_next_position(fb, dot.first);
        
        row = dot.first.row-1;
      }
    }
    
    save_undo = save_undo_backup;
    return fb;
  }
  
  file_buffer operator() (const Cmd_null& cmd) {
    return fb;
  }
  
};

struct expression_handler
{
  file_buffer fb;
  env_settings s;
  expression_handler(file_buffer i_fb, const env_settings& i_s) : fb(i_fb), s(i_s) {}
  
  file_buffer operator() (const AddressRange& addr)
  {
    address r = interpret_address_range(addr, fb);
    if (r.null_selection) {
      fb.pos = r.p1;
      fb.start_selection = std::nullopt;
      } else {
      fb.pos = r.p2;
      fb.start_selection = r.p1;
      }
    return fb;
  }
  
  file_buffer operator() (const Command& cmd)
  {
    command_handler ch(fb, s);
    return std::visit(ch, cmd);
  }
};

}

file_buffer handle_command(file_buffer fb, std::string command, const env_settings& s) {
  auto tokens = tokenize(command);
  auto cmds = parse(tokens);
  for (const auto& cmd : cmds)
  {
    expression_handler eh(fb, s);
    fb = std::visit(eh, cmd);
  }
  return fb;
}
