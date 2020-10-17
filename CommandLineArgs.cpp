#include "CommandLineArgs.hpp"

#include <string>
#include <system_error>
#include <vector>

#include <Windows.h>

std::vector<std::wstring> ParseCommandLineArgs(LPCWSTR commandLineString) {
  int numArgs = 0;
  auto args = CommandLineToArgvW(commandLineString, &numArgs);
  if (!args) {
    throw std::system_error(std::error_code(GetLastError(), std::system_category()), "CommandLineToArgvW failed");
  }
  std::vector<std::wstring> vArgs(numArgs);
  for (int i = 0; i < numArgs; i++) {
    vArgs[i] = std::wstring(args[i]);
  }
  LocalFree(args);
  return vArgs;
}

std::vector<std::wstring> ParseCommandLineArgs(const std::wstring& commandLineString) {
  return ParseCommandLineArgs(commandLineString.c_str());
}

const std::vector<std::wstring>& GetCurrentCommandLineArgs() {
  static const std::vector<std::wstring> args = ParseCommandLineArgs(GetCommandLineW());
  return args;
}
