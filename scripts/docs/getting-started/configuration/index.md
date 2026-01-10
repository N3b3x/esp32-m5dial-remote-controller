---
layout: default
title: "Configuration"
description: "Configuration guide for HardFOC ESP-IDF CI Tools"
nav_order: 2
parent: "Getting Started"
permalink: /docs/getting-started/configuration/
---

# Configuration Guide

This guide explains how to configure the HardFOC ESP-IDF CI Tools for your specific project needs.

## 📋 Overview

The HardFOC ESP-IDF CI Tools use a centralized configuration system that allows you to:

- **Define applications** and their build requirements
- **Specify ESP-IDF versions** for testing
- **Configure build types** (Debug, Release)
- **Set up CI/CD parameters** for GitHub Actions

## ⚙️ Basic Configuration

### 1. Project Structure

Ensure your project follows this structure:

```
your-esp32-project/
├── .github/
│   └── workflows/
│       ├── build.yml
│       └── security.yml
├── /examples/
│   └── esp32/
│       ├── app_config.yml
│       └── esp32/
│            ├── your-app.cpp
│       └── your_apps/
└── README.md
```

### 2. Application Configuration

Create or update `/examples/esp32/app_config.yml`:

```yaml
# Global metadata and defaults
metadata:
  default_app: "your_app"           # Default application to build
  default_build_type: "Release"     # Default build configuration
  target: "esp32c6"                 # Target MCU architecture
  idf_versions: ["release/v5.5"]    # Supported ESP-IDF versions

# Application definitions
apps:
  your_app:
    description: "Your ESP32 application"
    source_file: "YourApp.cpp"
    category: "main"
    build_types: ["Debug", "Release"]
    idf_versions: ["release/v5.5"]
    ci_enabled: true
    featured: true

# Build configuration
build_config:
  build_types:
    Debug:
      description: "Debug build with symbols"
      cmake_build_type: "Debug"
      optimization: "-O0"
      debug_level: "-g3"
      defines: ["DEBUG"]
    Release:
      description: "Optimized production build"
      cmake_build_type: "Release"
      optimization: "-O2"
      debug_level: "-g"
      defines: ["NDEBUG"]
```

### 3. Workflow Configuration

Update `.github/workflows/build.yml`:

```yaml
name: Build ESP32 Applications

on:
  push:
    branches: [ main, develop ]
  pull_request:
    branches: [ main ]

jobs:
  build:
    runs-on: ubuntu-latest
    strategy:
      matrix:
        app: ["your_app"]  # Add your apps here
        build_type: ["Debug", "Release"]
        idf_version: ["release/v5.5"]
    
    steps:
    - uses: actions/checkout@v4
    
    - name: Set Project Path
      run: echo "ESP32_PROJECT_PATH=/examples/esp32" >> $GITHUB_ENV
    
    - name: Build Application
      uses: espressif/esp-idf-ci-action@v1
      with:
        esp_idf_version: ${{ matrix.idf_version }}
        target: esp32c6
        path: /examples/esp32
        app_path: /examples/esp32
```

## 🔧 Advanced Configuration

### Multi-Version ESP-IDF Support

To test against multiple ESP-IDF versions:

```yaml
# In app_config.yml
metadata:
  idf_versions: ["release/v5.5", "release/v5.4"]
  default_build_types: [["Debug", "Release"], ["Debug"]]

apps:
  your_app:
    idf_versions: ["release/v5.5", "release/v5.4"]
    build_types: ["Debug", "Release"]
```

### Custom Build Types

Define custom build configurations:

```yaml
build_config:
  build_types:
    Debug:
      cmake_build_type: "Debug"
      optimization: "-O0"
      debug_level: "-g3"
      defines: ["DEBUG", "VERBOSE_LOGGING"]
    Release:
      cmake_build_type: "Release"
      optimization: "-O2"
      debug_level: "-g"
      defines: ["NDEBUG"]
    Profile:
      cmake_build_type: "RelWithDebInfo"
      optimization: "-O2"
      debug_level: "-g"
      defines: ["PROFILING"]
```

### Environment Variables

Override configuration with environment variables:

```bash
# Override default application
export CONFIG_DEFAULT_APP="your_app"

# Override default build type
export CONFIG_DEFAULT_BUILD_TYPE="Debug"

# Override default ESP-IDF version
export CONFIG_DEFAULT_IDF_VERSION="release/v5.4"
```

## 🚀 CI/CD Configuration

### GitHub Actions Secrets

Set up these secrets in your repository:

- `ESP_IDF_GITHUB_TOKEN` - For ESP-IDF CI action
- `CODECOV_TOKEN` - For code coverage (optional)
- `SONAR_TOKEN` - For SonarCloud analysis (optional)

### Workflow Triggers

Configure when workflows run:

```yaml
on:
  push:
    branches: [ main, develop ]
    paths: ['/examples/esp32/**', '.github/workflows/**']
  pull_request:
    branches: [ main ]
    paths: ['/examples/esp32/**']
  schedule:
    - cron: '0 2 * * 1'  # Weekly on Monday at 2 AM
```

### Matrix Strategy

Optimize build matrix for your needs:

```yaml
strategy:
  matrix:
    app: ["app1", "app2", "app3"]
    build_type: ["Debug", "Release"]
    idf_version: ["release/v5.5"]
    exclude:
      - app: "app1"
        build_type: "Debug"
        idf_version: "release/v5.4"
```

## 🔍 Validation

### Configuration Validation

Test your configuration:

```bash
# Validate app_config.yml
yq eval /examples/esp32/app_config.yml

# Test build configuration
cd /examples/esp32
./scripts/build_app.sh validate your_app Release
```

### CI Validation

Check workflow syntax:

```bash
# Install act for local testing
npm install -g @nektos/act

# Test workflow locally
act -j build
```

## 📚 Next Steps

After configuration:

1. **[Build System](build-system/)** - Learn about build management
2. **[CI Pipeline](ci-pipeline/)** - Optimize your workflows
3. **[Advanced Topics](advanced/multi-version-idf/)** - Explore advanced features
4. **[Troubleshooting](troubleshooting/)** - Common issues and solutions

## 🆘 Troubleshooting

Common configuration issues:

- **Invalid YAML syntax** - Use `yq eval` to validate
- **Missing applications** - Check `apps` section in config
- **ESP-IDF version errors** - Verify version availability
- **Build failures** - Check build configuration

For more help, see [Troubleshooting](troubleshooting/) or [GitHub Issues](https://github.com/n3b3x/hf-espidf-project-tools/issues).

---

**Configuration complete?** Check out the [Build System](build-system/) documentation to learn about intelligent build management.
