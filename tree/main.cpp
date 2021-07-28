#include <array>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#define JTK_FILE_UTILS_IMPLEMENTATION
#include "jtk/file_utils.h"

#include <stdint.h>

uint64_t directories = 0;
uint64_t files = 0;

int max_depth = -1;

bool full_paths = false;
bool directories_only = false;

enum e_type
  {
  T_DIR=0,
  T_FILE=1
  };

void visit(const std::string& directory, const std::string& prefix, int depth)
  {
  if (depth == max_depth)
    return;
  static std::array<std::string, 2> tags = {{"├── ", "│   " }};
  static std::array<std::string, 2> end_tags = {{"└── ", "    "}};
    
  auto subdirectories = jtk::get_subdirectories_from_directory(directory, false);
  std::vector<std::string> fileslist;
  if (!directories_only)
    fileslist = jtk::get_files_from_directory(directory, false);
  std::vector<std::pair<std::string, e_type>> items;
  items.reserve(subdirectories.size() + fileslist.size());
  for (const auto& s : subdirectories)
    items.emplace_back(s, T_DIR);
  for (const auto& f : fileslist)
    items.emplace_back(f, T_FILE);
  std::sort(items.begin(), items.end(), [](const auto& lhs, const auto& rhs)
    {
    const auto result = std::mismatch(lhs.first.cbegin(), lhs.first.cend(), rhs.first.cbegin(), rhs.first.cend(), [](const unsigned char lhsc, const unsigned char rhsc) {return tolower(lhsc) == tolower(rhsc); });
    return result.second != rhs.first.cend() && (result.first == lhs.first.cend() || tolower(*result.first) < tolower(*result.second));
    });
    
  const size_t number_of_items = items.size();
  for (size_t i = 0; i < number_of_items; ++i)
    {
    const auto& item = items[i];
    std::cout << prefix;
    std::cout << (i == number_of_items-1 ? end_tags[0] : tags[0]) << (full_paths ? item.first : jtk::get_filename(item.first)) << std::endl;
    if (item.second == T_FILE)
      {
      ++files;
      }
    else
      {
      ++directories;
      visit(item.first, prefix + (i == number_of_items-1 ? end_tags[1] : tags[1]), depth+1);
      }
    }
  }
  
void print_help()
  {
  std::cout << "Usage: tree <directory> <options>" << std::endl;
  std::cout << "Recursively lists the content of a given directory or the" << std::endl;
  std::cout << "current working directory if no directory is provided." << std::endl;
  std::cout << std::endl;
  std::cout << "Options:" << std::endl;
  std::cout << "  -?       : Prints this help text" << std::endl;
  std::cout << "  -f       : Display full file paths" << std::endl;
  std::cout << "  -d       : Only list directories" << std::endl;
  std::cout << "  -L <int> : Restrict the display depth" << std::endl;
  std::cout << std::endl;
  exit(0);
  }

int main(int argc, char** argv)
  {
  std::string directory = jtk::get_cwd();
  
  for (int j = 1; j < argc; ++j)
    {
    if (argv[j][0] == '-')
      {
      std::string options(argv[j]+1);
      for (const auto ch : options)
        {
        if (ch == 'f')
          full_paths = true;
        if (ch == 'd')
          directories_only = true;
        if (ch == '?')
          print_help();
        if (ch == 'L')
          {
          ++j;
          if (j < argc)
            {
            std::stringstream ss;
            ss << argv[j];
            ss >> max_depth;
            }
          }
        }
      }
    else
      {
      directory = argv[j];
      }
    }
  
  std::cout << directory << std::endl;
  
  std::string prefix;
  visit(directory, prefix, 0);
  std::cout << std::endl;
  std::cout << directories << " directories, " << files << " files" << std::endl;
  return 0;
  }
