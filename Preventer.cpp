#include "Preventer.hpp"

#include <Windows.h>

namespace Preventer {
  extern std::atomic<bool> gEnable = false;
  extern std::atomic<bool> gSystemFlag = false;
  extern std::atomic<bool> gDisplayFlag = false;

  void ApplyState() {
    DWORD flags = ES_CONTINUOUS;

    if (gEnable) {
      if (gSystemFlag) {
        flags |= ES_SYSTEM_REQUIRED;
      }

      if (gDisplayFlag) {
        flags |= ES_DISPLAY_REQUIRED;
      }
    }

    SetThreadExecutionState(flags);
   }

  void ApplyStateFromIPCFlags(DWORD flags) {
    if (flags & IPCFlags::Enable) {
      gEnable = true;
    }

    if (flags & IPCFlags::Disable) {
      gEnable = false;
    }

    //

    if (flags & IPCFlags::SetSystemFlag) {
      gSystemFlag = true;
    }

    if (flags & IPCFlags::UnsetSystemFlag) {
      gSystemFlag = false;
    }

    //

    if (flags & IPCFlags::SetDisplayFlag) {
      gDisplayFlag = true;
    }

    if (flags & IPCFlags::UnsetDisplayFlag) {
      gDisplayFlag = false;
    }

    //

    ApplyState();
  }

  void Finish() {
    SetThreadExecutionState(ES_CONTINUOUS);
  }
}
