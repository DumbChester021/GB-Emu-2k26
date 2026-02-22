#pragma once

#include <string>

/// Opens a native file dialog to select a Game Boy ROM file (.gb / .gbc).
/// @param initialDir  If non-empty, the dialog opens in this directory.
/// Returns the selected file path, or an empty string if the user cancelled.
std::string openFileDialog(const std::string& initialDir = "");
