---
layout: default
title: "CI/CD Integration Patterns"
description: "Common patterns for integrating ESP-IDF project tools in CI/CD workflows"
nav_order: 2
parent: "⚡ CI Pipeline"
permalink: /docs/ci-integration-patterns/
---

# CI/CD Integration Patterns

This guide covers common patterns for integrating the HardFOC ESP-IDF Project Tools into your CI/CD workflows, including GitHub Actions, GitLab CI, and other automation platforms.

## 📋 Directory Structure Concepts

### Project Directory (`project_dir`)
- **Purpose**: Points to your ESP-IDF project directory (contains `CMakeLists.txt`, `app_config.yml`)
- **Examples**: `examples/esp32`, `firmware/esp32-project`, `src/esp32-app`

### Tools Directory (`project_tools_dir`)
- **Purpose**: Points to directory containing the build scripts (`build_app.sh`, `generate_matrix.py`, etc.)
- **Auto-detection**: If not specified, looks for `hf-espidf-project-tools` subdirectory in project directory
- **Multi-Project Support**: Can be shared across multiple projects since all scripts accept `--project-path` argument
- **Examples**: `examples/esp32/scripts`, `examples/esp32/hf-espidf-project-tools`, `tools/esp32-build-scripts`, `shared-tools`

## 🏗️ Common Integration Patterns

### Pattern 1: Submodule Setup (Recommended)

**Repository structure:**
```
your-repo/
├── .github/workflows/          # CI workflows (at repo root)
│   └── build.yml
├── examples/esp32/             # ESP-IDF project
│   ├── CMakeLists.txt
│   ├── app_config.yml
│   └── hf-espidf-project-tools/  # Submodule containing project tools
│       ├── build_app.sh
│       └── generate_matrix.py
└── README.md
```

**GitHub Actions workflow:**
```yaml
name: Build ESP-IDF Applications

on:
  push:
    branches: [ main, develop ]
  pull_request:
    branches: [ main ]

jobs:
  build:
    uses: N3b3x/hf-espidf-ci-tools/.github/workflows/ru-build.yml@v1
    with:
      project_dir: examples/esp32
      project_tools_dir: examples/esp32/hf-espidf-project-tools  # Points to submodule
      # auto_clone_tools not needed - workflow respects explicit project_tools_dir
```

**✅ Benefits:**
- Automatic behavior when `project_tools_dir` is specified
- No auto-cloning occurs when explicit path is provided
- Submodule detection and validation happens automatically

### Pattern 2: Direct Clone Setup

**Repository structure:**
```
your-repo/
├── .github/workflows/
│   └── build.yml
├── examples/esp32/             # ESP-IDF project
│   ├── CMakeLists.txt
│   ├── app_config.yml
│   └── hf-espidf-project-tools/  # Cloned directly
│       ├── build_app.sh
│       └── generate_matrix.py
└── README.md
```

**GitHub Actions workflow:**
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
      # project_tools_dir not needed - auto-detects hf-espidf-project-tools
```

### Pattern 3: Custom Tools Location

**Repository structure:**
```
your-repo/
├── .github/workflows/
│   └── build.yml
├── firmware/esp32/             # ESP-IDF project
│   ├── CMakeLists.txt
│   └── app_config.yml
└── build-tools/                # Custom tools location
    ├── build_app.sh
    └── generate_matrix.py
```

**GitHub Actions workflow:**
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
      project_dir: firmware/esp32
      project_tools_dir: build-tools
```

### Pattern 4: Shared Tools for Multiple Projects

**Repository structure:**
```
your-repo/
├── .github/workflows/
│   └── build.yml
├── projects/
│   ├── project-a/              # ESP-IDF project A
│   │   ├── CMakeLists.txt
│   │   └── app_config.yml
│   └── project-b/              # ESP-IDF project B
│       ├── CMakeLists.txt
│       └── app_config.yml
└── shared-tools/               # Shared tools directory
    ├── build_app.sh
    └── generate_matrix.py
```

**GitHub Actions workflow:**
```yaml
name: Build ESP-IDF Applications

on:
  push:
    branches: [ main ]
  pull_request:
    branches: [ main ]

jobs:
  build-project-a:
    uses: N3b3x/hf-espidf-ci-tools/.github/workflows/ru-build.yml@v1
    with:
      project_dir: projects/project-a
      project_tools_dir: shared-tools  # Shared across all projects
      
  build-project-b:
    uses: N3b3x/hf-espidf-ci-tools/.github/workflows/ru-build.yml@v1
    with:
      project_dir: projects/project-b
      project_tools_dir: shared-tools  # Same shared tools directory
```

## 🔧 Advanced Configuration

### Environment Variables

```yaml
env:
  ESP32_PROJECT_PATH: "examples/esp32"
  CONFIG_DEFAULT_APP: "main_app"
  CONFIG_DEFAULT_BUILD_TYPE: "Release"
```

### Matrix Builds

```yaml
strategy:
  matrix:
    app: ["main_app", "test_app", "demo_app"]
    build_type: ["Debug", "Release"]
    idf_version: ["release/v5.5", "release/v5.4"]
```

### Custom Build Steps

```yaml
- name: Setup ESP-IDF
  uses: espressif/esp-idf-ci-action@v1
  with:
    esp_idf_version: ${{ matrix.idf_version }}
    target: esp32c6
    path: ${{ matrix.project_dir }}

- name: Build Application
  run: |
    cd ${{ matrix.project_dir }}
    ./scripts/build_app.sh ${{ matrix.app }} ${{ matrix.build_type }}
```

## 🚀 Best Practices

### 1. Directory Structure
- Keep ESP-IDF projects in dedicated directories (`examples/`, `firmware/`, `projects/`)
- Use consistent naming conventions for tools directories
- Document your project structure in README files

### 2. Version Control
- Use submodules for production projects
- Pin specific versions of the tools repository
- Keep tools and project code in separate commits

### 3. CI/CD Optimization
- Use matrix builds for multiple configurations
- Cache build artifacts and dependencies
- Run tests in parallel when possible

### 4. Error Handling
- Validate project structure before building
- Use proper error codes and exit conditions
- Provide clear error messages for debugging

## 🔗 Related Documentation

- **[CI Pipeline](/docs/ci-pipeline/)** - Detailed CI pipeline documentation
- **[Build System](/docs/build-system/)** - Understanding the build system
- **[Configuration](/docs/configuration/)** - YAML configuration management
- **[Getting Started](/docs/getting-started/)** - Basic setup and integration

---

**Need help with a specific pattern?** Check our [Examples](/docs/examples/) or [Troubleshooting](/docs/troubleshooting/) sections for more detailed guidance.
