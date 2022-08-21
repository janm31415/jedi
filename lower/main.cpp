#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <chrono>
#include <thread>
#include <algorithm>
#define JTK_FILE_UTILS_IMPLEMENTATION
#include "jtk/jtk/file_utils.h"
#define JTK_PIPE_IMPLEMENTATION
#include "jtk/jtk/pipe.h"

int main(int argc, char** argv)
  {
  std::string input = jtk::read_std_input(50);
  std::transform(input.begin(), input.end(), input.begin(), [](char ch) { return (char)::tolower(ch); });
  std::cout << input;
  return 0;
  }