# Changelog

All notable changes to this project will be documented in this file.

The format is inspired by Keep a Changelog, and the project follows semantic versioning where practical.

## [Unreleased]

### Added
- Premium GitHub-focused README presentation.
- Updated project documentation based on the current firmware structure.
- Initial changelog file in English.

## [1.4.3] - 2026-10-07

### Added
- UDP command processing with support for system, network, SD card, and LED actions.
- OLED status output with live clock and scrollable message history.
- SD card log recording with timestamp support.
- NTP synchronization using configurable stored time settings.
- Ethernet initialization and route priority configuration.
- Wi-Fi configuration portal with saved credentials in flash preferences.
- Reset button support for clearing saved Wi-Fi settings.

### Changed
- Main firmware orchestration refined around modular controllers for display, storage, network, time, and commands.
- Network startup flow now displays the assigned IP address after a successful Wi-Fi connection.
- Time management now loads time zone configuration from preferences before NTP sync.
- Documentation refreshed to match the current repository modules and runtime behavior.

### Fixed
- Improved consistency between the documented architecture and the active codebase.
