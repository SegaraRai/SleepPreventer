#include <atomic>
#include <cstddef>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

#include <Windows.h>
#include <Windowsx.h>
#include <Shlwapi.h>

#include "CommandLineArgs.hpp"
#include "ConfigFile.hpp"
#include "NotifyIcon.hpp"
#include "Preventer.hpp"

#include "resource.h"

using namespace std::literals;

namespace {
  enum class CopyDataMessageId : ULONG_PTR {
    SecondInstanceLaunched = 1,
  };

  constexpr std::size_t PathBufferSize = 65600;
  constexpr std::size_t MenuPathLength = 32;
  constexpr auto ClassName = L"SleepPreventer.CLS";
  constexpr auto MutexName = L"SleepPreventer.MTX";
  constexpr auto WindowName = L"SleepPreventer.WND";
  constexpr auto ReadyWindowName = L"SleepPreventer.WNDRDY";
  constexpr UINT NotifyIconId = 0x0001;
  constexpr UINT NotifyIconCallbackMessageId = WM_APP + 0x1101;
  
  const UINT gTaskbarCreatedMessage = RegisterWindowMessageW(L"TaskbarCreated");
  std::optional<ConfigFile> gConfigFile;
  std::optional<NotifyIcon> gNotifyIcon;
  HINSTANCE gHInstance;

  std::wstring GetModuleFilepath(HMODULE hModule) {
    constexpr std::size_t BufferSize = 65600;

    auto buffer = std::make_unique<wchar_t[]>(BufferSize);
    GetModuleFileNameW(hModule, buffer.get(), BufferSize);
    if (GetLastError() != ERROR_SUCCESS) {
      throw std::system_error(std::error_code(GetLastError(), std::system_category()), "GetModukeFileNameW failed");
    }

    return std::wstring(buffer.get());
  }
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  if (gTaskbarCreatedMessage != 0 && uMsg == gTaskbarCreatedMessage) {
    // use if statement because gTaskbarCreatedMessage is not a constexpr
    // NOTE: To receive "TaskbarCreated" message, the window must be a top level window. A message-only window will not receive the message.
    if (gNotifyIcon) {
      if (!gNotifyIcon.value().Register()) {
        const std::wstring message = L"Failed to register notify icon (code "s + std::to_wstring(GetLastError()) + L")"s;
        MessageBoxW(NULL, message.c_str(), L"SleepPreventer", MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
        DestroyWindow(hwnd);
      }
    }
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
  }

  switch (uMsg) {
    // IPC
    case WM_COPYDATA:
    {
      const auto ptrCopyDataStruct = reinterpret_cast<const COPYDATASTRUCT*>(lParam);

      switch (const CopyDataMessageId messageId = static_cast<CopyDataMessageId>(ptrCopyDataStruct->dwData); messageId) {
        case CopyDataMessageId::SecondInstanceLaunched:
          if (ptrCopyDataStruct->cbData != sizeof(DWORD)) {
            return FALSE;
          }
          Preventer::ApplyStateFromIPCFlags(*reinterpret_cast<const DWORD*>(ptrCopyDataStruct->lpData));
          return TRUE;
      }
      return FALSE;
    }

    // menu
    case WM_COMMAND:
    {
      if (lParam != 0 || HIWORD(wParam) != 0) {
        break;
      }

      auto& configFile = gConfigFile.value();

      const auto commandId = LOWORD(wParam);
      switch (commandId) {
        case IDM_CTX_QUIT:
          PostMessageW(hwnd, WM_CLOSE, 0, 0);
          return 0;

        case IDM_CTX_TOGGLE_ENABLE:
          Preventer::gEnable = !Preventer::gEnable;
          Preventer::ApplyState();
          configFile.Set(L"enable"s, Preventer::gEnable ? 1 : 0);
          configFile.Save();
          return 0;

        case IDM_CTX_TOGGLE_SYSTEM:
          Preventer::gSystemFlag = !Preventer::gSystemFlag;
          Preventer::ApplyState();
          configFile.Set(L"system"s, Preventer::gSystemFlag ? 1 : 0);
          configFile.Save();
          return 0;

        case IDM_CTX_TOGGLE_DISPLAY:
          Preventer::gDisplayFlag = !Preventer::gDisplayFlag;
          Preventer::ApplyState();
          configFile.Set(L"display"s, Preventer::gDisplayFlag ? 1 : 0);
          configFile.Save();
          return 0;
      }
      break;
    }

    // notify icon
    case NotifyIconCallbackMessageId:
      if (HIWORD(lParam) != NotifyIconId) {
        break;
      }

      switch (LOWORD(lParam)) {
        case NIN_SELECT:
        case NIN_KEYSELECT:
        case WM_CONTEXTMENU:
        {
          HMENU hMenu = NULL;
          do {
            hMenu = LoadMenuW(gHInstance, MAKEINTRESOURCEW(IDR_MENU));
            if (hMenu == NULL) {
              break;
            }

            const HMENU hSubMenu = GetSubMenu(hMenu, 0);
            if (hMenu == NULL) {
              break;
            }

            {
              const MENUITEMINFOW menuItemInfo{
                sizeof(menuItemInfo),
                MIIM_STATE | MIIM_CHECKMARKS,
                0,
                static_cast<UINT>(Preventer::gEnable ? MFS_CHECKED : MFS_UNCHECKED),
                IDM_CTX_TOGGLE_ENABLE,
                NULL,
                NULL,
                NULL,
                NULL,
                NULL,
                0,
                NULL,
              };
              SetMenuItemInfoW(hSubMenu, IDM_CTX_TOGGLE_ENABLE, FALSE, &menuItemInfo);
            }

            {
              const MENUITEMINFOW menuItemInfo{
                sizeof(menuItemInfo),
                MIIM_STATE | MIIM_CHECKMARKS,
                0,
                static_cast<UINT>((Preventer::gEnable ? MFS_ENABLED : MFS_GRAYED) | (Preventer::gSystemFlag ? MFS_CHECKED : MFS_UNCHECKED)),
                IDM_CTX_TOGGLE_SYSTEM,
                NULL,
                NULL,
                NULL,
                NULL,
                NULL,
                0,
                NULL,
              };
              SetMenuItemInfoW(hSubMenu, IDM_CTX_TOGGLE_SYSTEM, FALSE, &menuItemInfo);
            }

            {
              const MENUITEMINFOW menuItemInfo{
                sizeof(menuItemInfo),
                MIIM_STATE | MIIM_CHECKMARKS,
                0,
                static_cast<UINT>((Preventer::gEnable ? MFS_ENABLED : MFS_GRAYED) | (Preventer::gDisplayFlag ? MFS_CHECKED : MFS_UNCHECKED)),
                IDM_CTX_TOGGLE_DISPLAY,
                NULL,
                NULL,
                NULL,
                NULL,
                NULL,
                0,
                NULL,
              };
              SetMenuItemInfoW(hSubMenu, IDM_CTX_TOGGLE_DISPLAY, FALSE, &menuItemInfo);
            }

            SetForegroundWindow(hwnd);

            const auto x = GET_X_LPARAM(wParam);
            const auto y = GET_Y_LPARAM(wParam);

            const UINT flags = TPM_LEFTALIGN;
            TrackPopupMenuEx(hSubMenu, flags, x, y, hwnd, NULL);
          } while (false);

          if (hMenu != NULL) {
            DestroyMenu(hMenu);
            hMenu = NULL;
          }

          return 0;
        }
      }
      break;

    // on exit
    case WM_CLOSE:
      DestroyWindow(hwnd);
      return 0;

    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;
  }

  return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nShowCmd) {
  gHInstance = hInstance;

  // parse command line arguments
  const auto& args = GetCurrentCommandLineArgs();

  std::optional<bool> enable;
  std::optional<bool> systemFlag;
  std::optional<bool> displayFlag;
  for (const auto& arg : args) {
    if (arg.size() >= 2 && (arg[1] == L'?' || arg[1] == L'H' || arg[1] == L'h')) {
      const wchar_t message[] =
        L"\n"
        L"SleepPreventer [/E | /-E] [/S | /-S] [/D | /-D]\n"
        L"\n"
        L"  /E    Enable prevention\n"
        L"  /-E   Disable prevention\n"
        L"  /S    Enable sleep prevention\n"
        L"  /-S   Disable sleep prevention\n"
        L"  /D    Enable display-off prevention\n"
        L"  /-D   Disable display-off prevention\n"
        L"\n";

      std::wcout << message;

      AttachConsole(ATTACH_PARENT_PROCESS);
      DWORD written = 0;
      WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE), message, sizeof(message) / sizeof(wchar_t), &written, NULL);
      FreeConsole();

      return 0;
    }

    //

    if (arg == L"/E"sv || arg == L"/e"sv) {
      enable = true;
      continue;
    }

    if (arg == L"/-E"sv || arg == L"/-e"sv) {
      enable = false;
      continue;
    }

    //

    if (arg == L"/S"sv || arg == L"/s"sv) {
      systemFlag = true;
      continue;
    }

    if (arg == L"/-S"sv || arg == L"/-s"sv) {
      systemFlag = false;
      continue;
    }

    //

    if (arg == L"/D"sv || arg == L"/d"sv) {
      displayFlag = true;
      continue;
    }

    if (arg == L"/-D"sv || arg == L"/-d"sv) {
      displayFlag = false;
      continue;
    }
  }

  DWORD ipcFlags = 0;
  {
    using namespace Preventer::IPCFlags;

    if (enable.has_value()) {
      ipcFlags |= enable.value() ? Enable : Disable;
    }
    if (systemFlag.has_value()) {
      ipcFlags |= systemFlag.value() ? SetSystemFlag : UnsetSystemFlag;
    }
    if (displayFlag.has_value()) {
      ipcFlags |= displayFlag.value() ? SetDisplayFlag : UnsetDisplayFlag;
    }
  }

  // open icon
  const auto hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_ICON));
  if (hIcon == NULL) {
    const std::wstring message = L"Initialization error: LoadIconW failed with code "s + std::to_wstring(GetLastError());
    MessageBoxW(NULL, message.c_str(), L"SleepPreventer", MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
    return 1;
  }

  // register window class
  const WNDCLASSEXW wndClassExW{
    sizeof(wndClassExW),
    0,
    WindowProc,
    0,
    0,
    hInstance,
    hIcon,
    LoadCursorW(NULL, IDC_ARROW),
    reinterpret_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)),
    NULL,
    ClassName,
    hIcon,
  };
  if (!RegisterClassExW(&wndClassExW)) {
    const std::wstring message = L"Initialization error: RegisterClassExW failed with code "s + std::to_wstring(GetLastError());
    MessageBoxW(NULL, message.c_str(), L"SleepPreventer", MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
    return 1;
  }

  // create window
  // this must be done before checking multiple instance because a window is needed to send a message
  HWND hWnd = CreateWindowExW(0, ClassName, WindowName, WS_OVERLAPPED, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, hInstance, NULL);
  if (hWnd == NULL) {
    const std::wstring message = L"Initialization error: CreateWindowExW failed with code "s + std::to_wstring(GetLastError());
    MessageBoxW(NULL, message.c_str(), L"SleepPreventer", MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
    return 1;
  }
  
  // multiple instance check
  SetLastError(ERROR_SUCCESS);
  auto hMutex = CreateMutexW(NULL, TRUE, MutexName);
  if (const auto error = GetLastError(); error != ERROR_SUCCESS) {
    if (hMutex != NULL) {
      ReleaseMutex(hMutex);
      hMutex = NULL;
    }

    // unexpected error
    if (error != ERROR_ALREADY_EXISTS) {
      const std::wstring message = L"Initialization error: CreateMutexW failed with code "s + std::to_wstring(error);
      MessageBoxW(NULL, message.c_str(), L"SleepPreventer", MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
      DestroyWindow(hWnd);
      return 1;
    }

    // find existing instance (window)
    const auto hWndFirstInstance = FindWindowExW(NULL, NULL, ClassName, ReadyWindowName);
    if (hWndFirstInstance == NULL) {
      const std::wstring message = L"Initialization error: FindWindowExW failed with code "s + std::to_wstring(GetLastError());
      MessageBoxW(NULL, message.c_str(), L"SleepPreventer", MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
      DestroyWindow(hWnd);
      return 1;
    }

    // IPC
    COPYDATASTRUCT copyDataStruct{
      static_cast<ULONG_PTR>(CopyDataMessageId::SecondInstanceLaunched),
      static_cast<DWORD>(sizeof(DWORD)),
      const_cast<PVOID>(reinterpret_cast<LPCVOID>(&ipcFlags)),
    };
    SetLastError(ERROR_SUCCESS);
    SendMessageW(hWndFirstInstance, WM_COPYDATA, reinterpret_cast<WPARAM>(hWnd), reinterpret_cast<LPARAM>(&copyDataStruct));
    if (GetLastError() != ERROR_SUCCESS) {
      const std::wstring message = L"Initialization error: SendMessageW failed with code "s + std::to_wstring(GetLastError());
      MessageBoxW(NULL, message.c_str(), L"SleepPreventer", MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
      DestroyWindow(hWnd);
      return 1;
    }

    DestroyWindow(hWnd);
    return 0;
  }

  // set window name to ReadyWindowName
  if (!SetWindowTextW(hWnd, ReadyWindowName)) {
    ReleaseMutex(hMutex);
    const std::wstring message = L"Initialization error: SetWindowTextW failed with code "s + std::to_wstring(GetLastError());
    MessageBoxW(NULL, message.c_str(), L"SleepPreventer", MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
    DestroyWindow(hWnd);
    return 1;
  }

  // register notify icon
  gNotifyIcon.emplace(NOTIFYICONDATAW{
    sizeof(NOTIFYICONDATAW),
    hWnd,
    NotifyIconId,
    NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP,
    NotifyIconCallbackMessageId,
    hIcon,
    L"SleepPreventer",
    0,
    0,
    {},
    {
      NOTIFYICON_VERSION_4,
    },
    {},
    NIIF_NONE,
    {},
    NULL,
  }, true);

  if (!gNotifyIcon.value().Register()) {
    ReleaseMutex(hMutex);
    const std::wstring message = L"Initialization error: Failed to register notify icon (code "s + std::to_wstring(GetLastError()) + L")"s;
    MessageBoxW(NULL, message.c_str(), L"SleepPreventer", MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
    DestroyWindow(hWnd);
    return 1;
  }

  // instantiate ConfigFile
  const auto exeFilepath = GetModuleFilepath(NULL);
  const std::wstring configFilepath = exeFilepath.substr(0, exeFilepath.size() - 4) + L".cfg"s;
  gConfigFile.emplace(configFilepath);

  auto& configFile = gConfigFile.value();
  configFile.Set(L"enable"s, 0, true);
  configFile.Set(L"system"s, 0, true);
  configFile.Set(L"display"s, 0, true);
  configFile.Save();

  // start
  Preventer::gEnable = configFile.Get(L"enable"s).value_or(0) != 0;
  Preventer::gSystemFlag = configFile.Get(L"system"s).value_or(0) != 0;
  Preventer::gDisplayFlag = configFile.Get(L"display"s).value_or(0) != 0;
  Preventer::ApplyStateFromIPCFlags(ipcFlags);

  // message loop
  MSG msg;
  BOOL gmResult;
  while (true) {
    gmResult = GetMessageW(&msg, hWnd, 0, 0);
    if (gmResult == 0 || gmResult == -1) {
      break;
    }
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }

  // finish
  Preventer::Finish();

  ReleaseMutex(hMutex);

  return msg.message == WM_QUIT ? static_cast<int>(msg.wParam) : 0;
}
