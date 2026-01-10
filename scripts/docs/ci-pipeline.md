---
layout: default
title: "⚡ CI Pipeline"
description: "CI/CD integration and automated build pipelines"
nav_order: 3
has_children: true
permalink: /docs/ci-pipeline/
---

# ⚡ CI Pipeline

CI/CD integration and automated build pipelines for ESP-IDF projects using the HardFOC project tools.

## Available Resources

- **[CI Pipeline Usage](/docs/ci-pipeline/)** - Using the ESP-IDF CI pipeline from the separate CI tools repository
- **[CI Integration Patterns](/docs/ci-integration-patterns/)** - Common patterns for integrating ESP-IDF project tools in CI/CD workflows

## Quick Start

### GitHub Actions Integration

```yaml
name: Build ESP-IDF Applications

on:
  push:
    branches: [ main ]
  pull_request:
    branches: [ main ]

jobs:
  build:
    uses: N3b3x/hf-espidf-ci-tools/.github/workflows/ru-build.yml@v1
    with:
      project_dir: examples/esp32
      project_tools_dir: examples/esp32/scripts
```

### Key Features

- **Automated Builds** - Multi-application builds in CI/CD
- **Matrix Testing** - Test multiple configurations and ESP-IDF versions
- **Port Detection** - Automatic ESP32 device detection
- **Log Management** - Comprehensive build and test logging
- **Integration Patterns** - Multiple ways to integrate with existing projects

## Integration Methods

1. **Submodule Setup** - Version-controlled integration
2. **Direct Clone** - Quick setup for experimentation
3. **Shared Tools** - Multiple projects using same tools
4. **Custom Location** - Flexible directory structures

## Related Documentation

- **[Getting Started](/docs/getting-started/)** - Basic setup and integration
- **[Build System](/docs/build-system/)** - Understanding the build system
- **[Configuration](/docs/configuration/)** - YAML configuration management
