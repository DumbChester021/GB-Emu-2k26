#include "file_dialog.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <string>


#ifdef _WIN32
#define popen _popen
#define pclose _pclose

std::string openFileDialog() {
  // PowerShell command to open a file dialog
  const char *cmd =
      "powershell -NoProfile -Command \"Add-Type -AssemblyName "
      "System.Windows.Forms; $f = New-Object "
      "System.Windows.Forms.OpenFileDialog; $f.Filter = 'Game Boy ROMs "
      "(*.gb;*.gbc)|*.gb;*.gbc|All files (*.*)|*.*'; $f.ShowHelp = $true; if "
      "($f.ShowDialog() -eq [System.Windows.Forms.DialogResult]::OK) { "
      "Write-Host $f.FileName }\"";

  FILE *pipe = popen(cmd, "r");
  if (!pipe) {
    std::fprintf(
        stderr,
        "Warning: Could not open file dialog (powershell not found?).\n");
    return "";
  }

  std::array<char, 1024> buffer{};
  std::string result;

  while (std::fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) !=
         nullptr) {
    result += buffer.data();
  }

  int status = pclose(pipe);

  // Strip trailing newline/whitespace
  while (!result.empty() && (result.back() == '\n' || result.back() == '\r' ||
                             result.back() == ' ')) {
    result.pop_back();
  }

  return result;
}

#else

std::string openFileDialog() {
  // Use zenity on Linux for a native-feeling file picker
  const char *cmd = "zenity --file-selection "
                    "--title='Select a Game Boy ROM' "
                    "--file-filter='Game Boy ROMs (*.gb, *.gbc)|*.gb *.gbc' "
                    "--file-filter='All files|*' "
                    "2>/dev/null";

  FILE *pipe = popen(cmd, "r");
  if (!pipe) {
    std::fprintf(stderr,
                 "Warning: Could not open file dialog (zenity not found?).\n");
    return "";
  }

  std::array<char, 1024> buffer{};
  std::string result;

  while (std::fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) !=
         nullptr) {
    result += buffer.data();
  }

  int status = pclose(pipe);

  // zenity returns 0 on OK, 1 on Cancel, 5 on timeout
  if (status != 0) {
    return "";
  }

  // Strip trailing newline
  while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) {
    result.pop_back();
  }

  return result;
}
#endif
