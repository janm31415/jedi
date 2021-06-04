#include "hex.h"
#include "utils.h"
#include "jtk/file_utils.h"
#include <sstream>
#include <fstream>
#include <vector>

namespace {
int64_t addr = 0;

bool flip_bits = false;

bool is_little_endian()
  {
  short int number = 0x1;
  char *num_ptr = (char*)&number;
  return (num_ptr[0] == 1);
  }
  
std::wstring int_to_hex(uint8_t i)
  {
  std::wstring hex;
  int h1 = (i >> 4) & 0x0f;
  if (h1 < 10)
    hex += L'0' + h1;
  else
    hex += L'A' + h1 - 10;
  int h2 = (i) & 0x0f;
  if (h2 < 10)
    hex += L'0' + h2;
  else
    hex += L'A' + h2 - 10;
  return hex;
  }

std::wstring int_to_hex(uint16_t i)
  {
  std::wstring hex;
  uint8_t h1 = (i >> 8) & 0x00ff;
  uint8_t h2 = i & 0x00ff;
  return int_to_hex(h1) + int_to_hex(h2);
  }

std::wstring int_to_hex(uint32_t i)
  {
  std::wstring hex;
  uint16_t h1 = (i >> 16) & 0x0000ffff;
  uint16_t h2 = i & 0x0000ffff;
  return int_to_hex(h1) + int_to_hex(h2);
  }

std::wstring int_to_hex(char ch)
  {
  uint8_t* c = reinterpret_cast<uint8_t*>(&ch);
  return int_to_hex(*c);
  }

wchar_t to_str(char c)
  {
  //if (c == '\n' || c == '\r' || c == '\t')
  //  return L'.';
  uint16_t u16 = ascii437_to_utf16((unsigned char)c);
  return (wchar_t)u16;
  }

char flip_bits_of_char(char b)
  {
  b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
  b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
  b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
  return b;
  }
  
void dump(std::fstream& _file, std::wostream& ostr)
  {
  auto address = _file.tellg();
  char buffer[256];
  _file.read(buffer, 256);
  std::streamsize chars_read = 256;
  if (!_file)
    {
    chars_read = _file.gcount();
    }
  if (!chars_read)
    return;
  if (flip_bits)
    {
    for (auto i = 0; i < chars_read; ++i)
      buffer[i] = flip_bits_of_char(buffer[i]);
    }
  std::vector<char> characters;
  ostr << int_to_hex(uint32_t(address)) << ": ";
  for (auto i = 0; i < chars_read;++i)
    {
    ostr << int_to_hex(buffer[i]) << " ";
    characters.push_back(buffer[i]);
    if ((i + 1) % 16 == 0)
      {
      ostr << "| ";
      for (auto c : characters)
        {
        ostr << to_str(c);
        }
      characters.clear();
      ostr << std::endl;
      if (i != chars_read - 1)
        {
        address += 16;
        ostr << int_to_hex(uint32_t(address)) << ": ";
        }
      }
    }
  if (chars_read % 16)
    {
    for (int i = 0; i < (16 - (chars_read % 16)); ++i)
      ostr << "   ";
    ostr << "| ";
    for (auto c : characters)
      ostr << to_str(c);
    ostr << std::endl;
    }
  }

void dump(std::fstream& _file, std::streampos pos, std::wostream& ostr)
  {
  _file.seekg(pos);
  dump(_file, ostr);
  }

void dump_full(std::fstream& _file, std::wostream& ostr)
  {
  _file.seekg(0);
  while (_file)
    dump(_file, ostr);
  }
} // namespace
  
std::wstring to_hex(const std::string& filename) {
  std::wstringstream str;
  str << L"Hex dump of file " << jtk::convert_string_to_wstring(filename) << std::endl;
  
  std::fstream _file(filename, std::fstream::in | std::fstream::out | std::fstream::binary);
  if (!_file.is_open()) {
    str << L"Could not open file\n";
  } else {
    flip_bits = !is_little_endian();
    dump_full(_file, str);
    _file.close();
  }
  return str.str();
}

