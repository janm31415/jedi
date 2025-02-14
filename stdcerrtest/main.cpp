#include <iostream>

int main(int, char**)
  {
  std::cout << "This is an error message written to std::cout." << std::endl;
  std::cerr << "This is an error message written to std::cerr." << std::endl;
  std::cerr << "Another error message on std::cerr." << std::endl;
  return 0;
  }