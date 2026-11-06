#pragma once

#include <algorithm>
#include <string>

/** The SHA-256 of some bytes, as 64 lowercase hex digits. */
std::string sha256_hex (const std::string &data);
/** The same of a file's contents; empty when it cannot be read. */
std::string sha256_file (const std::string &path);
