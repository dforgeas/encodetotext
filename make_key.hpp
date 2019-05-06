#pragma once

#include <experimental/string_view>
using string_view = std::experimental::string_view;

// Use CbcMac in a sensible way (and use endian neutral functions) and make a key with the first 4 integers of CbcMac::finish
// and write it to the relevant file, namely "encode.key"
void make_key(const string_view password);
