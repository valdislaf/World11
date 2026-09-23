param([string]$MsysRoot = 'C:\msys64')
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$build = Join-Path $repo 'build-review/clang'
$output = Join-Path $repo 'build-review/after'
$compilerBin = Join-Path $MsysRoot 'clang64/bin'
$compiler = Join-Path $compilerBin 'clang++.exe'
$glfwRoot = Join-Path $MsysRoot 'mingw64'
foreach ($required in @($compiler, "$glfwRoot/include/GLFW/glfw3.h",
    "$glfwRoot/lib/libglfw3.dll.a", "$glfwRoot/bin/glfw3.dll")) {
  if (!(Test-Path -LiteralPath $required)) { throw "Missing dependency: $required" }
}

# Isolate GLFW's C headers: adding the entire MinGW include directory to Clang
# mixes incompatible C runtime headers with libc++ on this machine.
$stage = Join-Path $repo 'build-review/glfw'
New-Item -ItemType Directory -Force "$stage/include", "$stage/lib/cmake/glfw3", $output | Out-Null
Copy-Item -LiteralPath "$glfwRoot/include/GLFW" -Destination "$stage/include" -Recurse -Force
$cmakeStage = $stage.Replace('\', '/')
$cmakeGlfw = $glfwRoot.Replace('\', '/')
@"
add_library(glfw SHARED IMPORTED)
set_target_properties(glfw PROPERTIES
  IMPORTED_IMPLIB "$cmakeGlfw/lib/libglfw3.dll.a"
  IMPORTED_LOCATION "$cmakeGlfw/bin/glfw3.dll"
  INTERFACE_INCLUDE_DIRECTORIES "$cmakeStage/include"
  INTERFACE_COMPILE_DEFINITIONS GLFW_DLL)
"@ | Set-Content -LiteralPath "$stage/lib/cmake/glfw3/glfw3Config.cmake" -Encoding utf8

function InvokeChecked([string]$command, [string[]]$arguments) {
  & $command @arguments
  if ($LASTEXITCODE -ne 0) { throw "$command failed with exit code $LASTEXITCODE" }
}

$previousPath = $env:PATH
try {
  $env:PATH = "$compilerBin;$MsysRoot/mingw64/bin;$env:PATH"
  InvokeChecked 'cmake' @('-S', $repo, '-B', $build, '-G', 'Ninja',
    '-DCMAKE_BUILD_TYPE=Release', "-DCMAKE_CXX_COMPILER=$compiler",
    "-Dglfw3_DIR=$stage/lib/cmake/glfw3", '-DBUILD_TESTING=ON')
  InvokeChecked 'cmake' @('--build', $build, '--target', 'horizongates_cpp',
    'world11_scene_probe', 'world11_decor_generator_tests',
    'world11_coral_geometry_tests', 'world11_fish_trajectory_tests', '-j', '6')
  InvokeChecked 'ctest' @('--test-dir', $build, '--output-on-failure')
  Push-Location $build
  try { InvokeChecked './world11_scene_probe.exe' @($output) }
  finally { Pop-Location }
  Write-Output "Visual frames and timings: $output"
} finally {
  $env:PATH = $previousPath
}
