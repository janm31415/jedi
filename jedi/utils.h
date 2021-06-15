#pragma once

#include <string>
#include <stdint.h>
#include <vector>

std::string get_file_in_executable_path(const std::string& filename);

uint16_t ascii_to_utf16(unsigned char ch);
uint16_t ascii437_to_utf16(unsigned char ch);

/*
Input is a filename and the filename of the window.
This method will look for filename in the folder defined by the window_filename, or the executable path.
Returns empty string if nothing was found, or returns the path of the file.
*/
std::string get_file_path(const std::string& filename, const std::string& buffer_filename);

bool remove_quotes(std::string& cmd);
bool remove_quotes(std::wstring& cmd);

void remove_whitespace(std::string& cmd);
void remove_whitespace(std::wstring& cmd);

std::vector<std::wstring> break_string(std::string in);

std::string complete_file_path(const std::string& incomplete_path, const std::string& buffer_filename);
