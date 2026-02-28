#include "VulkanShaders.h"

#include <fmt/core.h>

#include <fstream>
#include <stdexcept>

std::vector<uint32_t> LoadSpirvWords(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  if (!file)
    throw std::runtime_error(fmt::format("Failed to open SPIR-V file: {}", path.string()));

  const std::streamsize size = file.tellg();
  if (size <= 0 || (size % 4) != 0)
    throw std::runtime_error(fmt::format("Invalid SPIR-V file size: {}", path.string()));
  file.seekg(0, std::ios::beg);

  std::vector<uint32_t> words(static_cast<size_t>(size) / sizeof(uint32_t));
  if (!file.read(reinterpret_cast<char*>(words.data()), size))
    throw std::runtime_error(fmt::format("Failed to read SPIR-V file: {}", path.string()));

  return words;
}
