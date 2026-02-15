$ErrorActionPreference = "Stop"

# Configuration
$VcpkgUrl = "https://github.com/microsoft/vcpkg.git"
$VcpkgPath = Join-Path $PSScriptRoot "vcpkg"
$BuildDir = Join-Path $PSScriptRoot "build_win"
$Executable = Join-Path $BuildDir "Release\gbemu.exe" # CMake on Windows usually adds Release/Debug folder

# Check for prerequisites
if (-not (Get-Command "git" -ErrorAction SilentlyContinue)) {
    Write-Error "Git is not installed or not in PATH. Please install Git."
}
if (-not (Get-Command "cmake" -ErrorAction SilentlyContinue)) {
    Write-Error "CMake is not installed or not in PATH. Please install CMake (https://cmake.org/download/)."
}

# Check for a C++ compiler (cl or g++)
$hasCompiler = (Get-Command "cl" -ErrorAction SilentlyContinue) -or (Get-Command "g++" -ErrorAction SilentlyContinue)
if (-not $hasCompiler) {
    Write-Warning "No C++ compiler found (cl.exe or g++). detecting.. CMake might fail if no compiler is found."
}

# 1. Setup vcpkg
if (-not (Test-Path $VcpkgPath)) {
    Write-Host "Cloning vcpkg..."
    git clone $VcpkgUrl $VcpkgPath
}

if (-not (Test-Path "$VcpkgPath\vcpkg.exe")) {
    Write-Host "Bootstrapping vcpkg..."
    & "$VcpkgPath\bootstrap-vcpkg.bat"
}

# 2. Install dependencies
Write-Host "Installing dependencies (SDL2)..."
& "$VcpkgPath\vcpkg.exe" install sdl2:x64-windows

# 3. Clean and Configure
Write-Host "Cleaning previous build..."
Stop-Process -Name "gbemu" -ErrorAction SilentlyContinue
if (Test-Path $BuildDir) {
    Remove-Item -Recurse -Force $BuildDir
}
New-Item -ItemType Directory -Path $BuildDir | Out-Null

Write-Host "Configuring CMake..."
Set-Location $BuildDir
cmake .. -DCMAKE_TOOLCHAIN_FILE="$VcpkgPath/scripts/buildsystems/vcpkg.cmake" -DVCPKG_TARGET_TRIPLET=x64-windows -DCMAKE_BUILD_TYPE=Release

# 4. Build
Write-Host "Building project..."
cmake --build . --config Release --parallel

# 5. Run
if (Test-Path $Executable) {
    Write-Host "Running Emulator..."
    & $Executable
}
else {
    Write-Error "Executable not found at $Executable"
}
