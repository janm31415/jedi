#include <array>
#include <iostream>
#include <string>
#include <vector>

#define JTK_FILE_UTILS_IMPLEMENTATION
#include "jtk/file_utils.h"

#include <stdint.h>

uint64_t directories = 0;
uint64_t files = 0;

enum e_type
  {
  T_DIR=0,
  T_FILE=1
  };

void visit(const std::string& directory, const std::string& prefix)
  {
  static std::array<std::string, 2> tags = {{"├── ", "│   " }};
  static std::array<std::string, 2> end_tags = {{"└── ", "    "}};
    
  auto subdirectories = jtk::get_subdirectories_from_directory(directory, false);
  auto fileslist = jtk::get_files_from_directory(directory, false);
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
    std::cout << (i == number_of_items-1 ? end_tags[0] : tags[0]) << jtk::get_filename(item.first) << std::endl;
    if (item.second == T_FILE)
      {
      ++files;
      }
    else
      {
      ++directories;
      visit(item.first, prefix + (i == number_of_items-1 ? end_tags[1] : tags[1]));
      }
    }
  }

int main(int argc, char** argv)
  {
  std::string directory = argc == 1 ? std::string(".") : std::string(argv[1]);
  
  std::cout << directory << std::endl;
  
  std::string prefix;
  visit(directory, prefix);
  
  std::cout << directories << " directories, " << files << " files" << std::endl;
  return 0;
  }
