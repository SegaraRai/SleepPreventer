#pragma once

#include <fstream>
#include <map>
#include <optional>
#include <shared_mutex>
#include <string>

class ConfigFile {
  mutable std::shared_mutex mMutex;
  std::map<std::wstring, int> mConfigMap;
  std::wstring mFilepath;
  
  void Load();

public:
  ConfigFile(const std::wstring& filepath);
  ~ConfigFile();

  void Save();

  std::optional<int> Get(const std::wstring& key) const;
  void Set(const std::wstring& key, int value, bool skipIfExists = false);
};
