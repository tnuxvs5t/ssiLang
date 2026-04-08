param(
    [string]$Cxx = "g++",
    [string]$Out = "tools\\gui32_helper.exe"
)

$ErrorActionPreference = "Stop"

& $Cxx `
  -std=c++17 `
  -O2 `
  -Wall `
  -Wextra `
  -o $Out `
  tools\gui32_helper.cpp `
  -lws2_32 `
  -lgdi32 `
  -luser32
