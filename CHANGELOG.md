# Changelog

All notable changes to the engine are listed here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and versions follow
[Semantic Versioning](https://semver.org/). While the major version is 0, minor releases
may contain breaking changes.

To cut a release, see [docs/releasing.md](docs/releasing.md).

## [Unreleased]

## [0.1.0] - 2026-11-18

First release. The engine as of roadmap phases 0 to 9: sparse-set ECS, scheduler with states, event bus,
SDL3 window, input and audio, batched 2D renderer, and debug panel.

### Added

- Engine version in `project(EcsEngine VERSION ...)`, exposed to code as `<core/version.hpp>`
  (`engine::kVersionString` and friends).
- The engine can be consumed from another CMake project with `FetchContent` pinned to a tag;
  the game, tests and benchmarks are not built when the engine is a subproject.
- Release workflow that publishes a GitHub release when a `vX.Y.Z` tag is pushed.

[Unreleased]: https://github.com/brian-md-rose/ecs-engine/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/brian-md-rose/ecs-engine/releases/tag/v0.1.0
