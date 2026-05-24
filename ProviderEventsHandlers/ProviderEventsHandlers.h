#pragma once

#include "../krabs/krabs.hpp"
#include <string>

bool CreateOpenHandler(krabs::parser& parser, uint32_t processId);
bool RenamePathHandler(krabs::parser& parser, uint32_t processId);
bool RenameHandler(krabs::parser& parser, uint32_t processId);
