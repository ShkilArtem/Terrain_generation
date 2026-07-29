# Changelog

All notable changes to this project are documented in this file.

## [1.0.0] - 2026-07-29

### Added

- Portable CMake build, installation, and ZIP packaging.
- One-command Windows build launcher that bypasses restrictive PowerShell
  execution policies and discovers the installed Visual Studio toolchain.
- Windows and Linux continuous-integration builds.
- Reproducible benchmark output under `outputs/benchmark_results/`.
- Python dependency declaration for the Excel report generator.
- Project documentation, citation metadata, and third-party notices.

### Changed

- Replaced machine-specific paths with repository-relative paths.
- Made the legacy Visual Studio project independent of its original checkout
  location.
- Enabled C++17 and stricter compiler diagnostics.

[1.0.0]: https://github.com/ShkilArtem/Terrain_generation/releases/tag/v1.0.0
