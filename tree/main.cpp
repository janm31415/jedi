#include <array>
#include <iostream>
#include <regex>
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
bool prune = false;
bool inverse_regex_match = false;

std::string pattern;
std::regex reg;

enum e_type
  {
  T_DIR=0,
  T_FILE=1
  };
  
  
struct tree_node
  {
  std::string path;
  uint64_t number_of_children = 0;
  e_type type;
  };
  
struct file_tree
  {
  std::vector<tree_node> nodes;
  };


void visit(file_tree& tree, const std::string& directory, int depth)
  {
  if (depth == max_depth)
    return;
  auto subdirectories = jtk::get_subdirectories_from_directory(directory, false);
  std::vector<std::string> fileslist;
  if (!directories_only)
    fileslist = jtk::get_files_from_directory(directory, false);
  if (!pattern.empty())
    {
    try
      {
      std::vector<std::string> retained_files;
      std::smatch sm;
      for (const auto& f : fileslist)
        {
        if (std::regex_search(f, sm, reg) != inverse_regex_match)
          retained_files.push_back(f);
        }
      fileslist.swap(retained_files);
      }
    catch (const std::regex_error& e)
      {
      std::cout << "tree: regex error caught: " << e.what() << std::endl;
      exit(1);
      }
    }
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
    
  uint64_t current_id = tree.nodes.size();
  tree.nodes.emplace_back();
  tree.nodes.back().path = directory;
  uint64_t current_number_of_files = files;
  const size_t number_of_items = items.size();
  for (size_t i = 0; i < number_of_items; ++i)
    {
    const auto& item = items[i];
    if (item.second == T_FILE)
      {
      tree.nodes.emplace_back();
      tree.nodes.back().path = item.first;
      tree.nodes.back().type = T_FILE;
      ++files;
      }
    else
      {
      visit(tree, item.first, depth+1);
      ++directories;
      }
    }
  tree.nodes[current_id].number_of_children = tree.nodes.size() - 1 - current_id;
  if (prune && (files == current_number_of_files))
    {
    tree.nodes.erase(tree.nodes.begin() + current_id, tree.nodes.end());
    }
  }

void print(const file_tree& tree, uint64_t start_node, uint64_t end_node, std::string prefix)
  {
  static std::array<std::string, 2> tags = {{"├── ", "│   " }};
  static std::array<std::string, 2> end_tags = {{"└── ", "    "}};
  for (uint64_t i = start_node; i < end_node; ++i)
    {
    bool last_item = (i+tree.nodes[i].number_of_children)>=(end_node-1);
    std::cout << prefix << (last_item ? end_tags[0] : tags[0]) << (full_paths ? tree.nodes[i].path : jtk::get_filename(tree.nodes[i].path)) << std::endl;
    if (tree.nodes[i].number_of_children > 0)
      {
      print(tree, i+1, i+tree.nodes[i].number_of_children, prefix + (last_item ? end_tags[1] : tags[1]));
      i += tree.nodes[i].number_of_children;
      }
    }
  }
  
void print(const file_tree& tree)
  {
  std::cout << (full_paths ? tree.nodes[0].path : jtk::get_filename(tree.nodes[0].path)) << std::endl;
  print(tree, 1, tree.nodes.size(), "");
  }
  
void print_help()
  {
  std::cout << "Usage: tree <directory> <options>" << std::endl;
  std::cout << "Recursively lists the content of a given directory or the" << std::endl;
  std::cout << "current working directory if no directory is provided." << std::endl;
  std::cout << std::endl;
  std::cout << "Options:" << std::endl;
  std::cout << "  -?         : Prints this help text" << std::endl;
  std::cout << "  -f         : Display full file paths" << std::endl;
  std::cout << "  -d         : Only list directories" << std::endl;
  std::cout << "  -L <int>   : Restrict the display depth" << std::endl;
  std::cout << "  -P <regex> : Only list files that match the regex" << std::endl;
  std::cout << "  -I <regex> : Only list files that don't match the regex" << std::endl;
  std::cout << "  -prune     : Don't list empty directories" << std::endl;
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
      if (options == std::string("prune"))
        {
        prune = true;
        }
      else
        {
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
          if (ch == 'P')
            {
            ++j;
            if (j < argc)
              pattern = std::string(argv[j]);
            }
          if (ch == 'I')
            {
            ++j;
            if (j < argc)
              {
              pattern = std::string(argv[j]);
              inverse_regex_match = true;
              }
            }
          }
        }
      }
    else
      {
      directory = argv[j];
      }
    }
    
  if (!pattern.empty())
    {
    try
      {
      reg = std::regex(pattern);
      }
    catch (const std::regex_error& e)
      {
      std::cout << "tree: regex error caught: " << e.what() << std::endl;
      exit(1);
      }
    }
  
  file_tree tree;
  visit(tree, directory, 0);
  print(tree);
  std::cout << std::endl;
  std::cout << directories << " directories, " << files << " files" << std::endl;
  
  return 0;
  }
