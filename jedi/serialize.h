#pragma once

#include "engine.h"
#include <iostream>
#include <string>



void save_to_stream(std::ostream& str, const app_state& state);

app_state load_from_stream(std::istream& str);

void save_to_file(const std::string& filename, const app_state& state);

app_state load_from_file(const std::string& filename);
