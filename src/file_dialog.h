#pragma once

#include <string>

/// Opens a native file dialog to select a Game Boy ROM file (.gb / .gbc).
/// Returns the selected file path, or an empty string if the user cancelled.
std::string openFileDialog();
