#include "ConfigFile.hpp"

#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <shared_mutex>
#include <string>
#include <string_view>

using namespace std::literals;

ConfigFile::ConfigFile(const std::wstring& filepath) :
  mFilepath(filepath)
{
  Load();
}

ConfigFile::~ConfigFile() {
  Save();
}

void ConfigFile::Load() {
  std::lock_guard lock(mMutex);

  mConfigMap.clear();

  std::wfstream wfs;
  wfs.open(mFilepath, std::ios_base::in);
  if (wfs.fail()) {
    return;
  }

  for (std::wstring line; std::getline(wfs, line); ) {
    if (line.empty()) {
      continue;
    }

    const auto eqPos = line.find_first_of(L'=');
    if (eqPos == std::wstring::npos) {
      continue;
    }

    std::size_t keyEnd = eqPos;
    while (keyEnd > 0 && line[keyEnd - 1] == L' ') {
      keyEnd--;
    }

    std::size_t valueBegin = eqPos + 1;
    while (valueBegin < line.size() && line[valueBegin] == L' ') {
      valueBegin++;
    }

    try {
      mConfigMap.insert_or_assign(line.substr(0, keyEnd), std::stoi(line.substr(valueBegin)));
    } catch (...) {}
  }
}

void ConfigFile::Save() {
  std::lock_guard lock(mMutex);

  std::wfstream wfs;
  wfs.open(mFilepath, std::ios_base::out | std::ios_base::trunc);
  if (wfs.fail()) {
    return;
  }

  for (const auto& [key, value] : mConfigMap) {
    wfs << key << L" = "sv << value << std::endl;
  }
  wfs << std::flush;
}

std::optional<int> ConfigFile::Get(const std::wstring& key) const {
  std::shared_lock lock(mMutex);
  const auto itr = mConfigMap.find(key);
  if (itr == mConfigMap.cend()) {
    return std::nullopt;
  }
  return std::make_optional(itr->second);
}

void ConfigFile::Set(const std::wstring& key, int value, bool skipIfExists) {
  std::lock_guard lock(mMutex);
  if (skipIfExists) {
    mConfigMap.try_emplace(key, value);
  } else {
    mConfigMap.insert_or_assign(key, value);
  }
}
