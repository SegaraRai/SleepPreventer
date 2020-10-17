#pragma once

#include <atomic>

#include <Windows.h>

namespace Preventer {
  namespace IPCFlags {
    constexpr DWORD Enable = 0x00000001;
    constexpr DWORD Disable = 0x00000002;
    constexpr DWORD SetSystemFlag = 0x00000010;
    constexpr DWORD UnsetSystemFlag = 0x00000020;
    constexpr DWORD SetDisplayFlag = 0x00000100;
    constexpr DWORD UnsetDisplayFlag = 0x00000200;
  } // namespace IPCFlags

  extern std::atomic<bool> gEnable;
  extern std::atomic<bool> gSystemFlag;
  extern std::atomic<bool> gDisplayFlag;

  void ApplyState();
  void ApplyStateFromIPCFlags(DWORD flags);
  void Finish();
} // namespace Preventer
