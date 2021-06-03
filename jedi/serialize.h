#pragma once

#include "engine.h"
#include <iostream>
#include <string>



void save_to_stream(std::ostream& str, const app_state& state);

app_state load_from_stream(std::istream& str, const settings& s);

void save_to_file(const std::string& filename, const app_state& state);

app_state load_from_file(app_state init, const std::string& filename, const settings& s);

