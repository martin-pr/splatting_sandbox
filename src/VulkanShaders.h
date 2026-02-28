#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

std::vector<uint32_t> LoadSpirvWords(const std::filesystem::path& path);
