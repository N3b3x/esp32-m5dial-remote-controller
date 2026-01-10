---
layout: default
title: "Multi-Version ESP-IDF Support"
description: "Support for multiple ESP-IDF versions with intelligent version management"
nav_order: 1
parent: "🔬 Advanced Topics"
permalink: /docs/advanced/multi-version-idf/
---

# Multi-Version ESP-IDF Support

This document explains how the ESP32 HardFOC Interface Wrapper supports multiple ESP-IDF versions
for different applications.

## Overview

The system now supports installing and managing multiple ESP-IDF versions simultaneously,
allowing different apps to use different ESP-IDF versions as specified in `app_config.yml`.
This is particularly useful when:

- Some apps require newer ESP-IDF features (e.g., v5.5)
- Some apps need to maintain compatibility with older versions (e.g., v5.4)
- Testing compatibility across different ESP-IDF versions
- CI/CD pipelines that need to build against multiple versions

## Architecture

### Directory Structure

```yaml
$HOME/esp/
├── esp-idf-release_v5.5/     # ESP-IDF v5.5 installation
├── esp-idf-release_v5.4/     # ESP-IDF v5.4 installation
└── esp-idf -> esp-idf-release_v5.5/  # Default symlink
```

### Version Naming Convention

ESP-IDF versions are stored with forward slashes converted to underscores:
- `release/v5.5` → `esp-idf-release_v5.5/`
- `release/v5.4` → `esp-idf-release_v5.4/`

## Configuration

### app_config.yml Structure

```yaml
metadata:
  # Global ESP-IDF versions - used by all apps unless overridden
  idf_versions: ["release/v5.5", "release/v5.4"]
  # Build types per IDF version: [["Debug", "Release"], ["Debug"]] 
## First array: build types for first IDF version (v5.5), second array: build types for second IDF
version (v5.4)
  default_build_types: [["Debug", "Release"], ["Debug"]]

apps:
  gpio_test:
    # Override global: use only ESP-IDF v5.5 with Debug and Release
    idf_versions: ["release/v5.5"]
    build_types: ["Debug", "Release"]
    
  legacy_app:
    # Override global: use only ESP-IDF v5.4 with Debug only
    idf_versions: ["release/v5.4"]
    build_types: ["Debug"]
```

### App-Specific Overrides

Apps can override the global ESP-IDF version and build type settings:

1. **No override**: App uses global settings from `metadata.idf_versions`
2. **Partial override**: App specifies only `idf_versions` or only `build_types`
3. **Full override**: App specifies both `idf_versions` and `build_types`

## Management Scripts

### manage_idf.sh

The main script for managing ESP-IDF versions:

```bash
## Install all required ESP-IDF versions from app_config.yml
./manage_idf.sh install

## List installed versions
./manage_idf.sh list

## Export environment for specific version
./manage_idf.sh export release/v5.4

## Switch default version
./manage_idf.sh switch release/v5.4

## Update specific version
./manage_idf.sh update release/v5.5

## Remove specific version
./manage_idf.sh clean release/v5.4

## Show current status
./manage_idf.sh status
```

### build_unified.sh

Enhanced build script with ESP-IDF version support:

```bash
## Build with specific ESP-IDF version
IDF_VERSION=release/v5.4 APP_TYPE=gpio_test ./build_unified.sh

## List available ESP-IDF versions
./build_unified.sh --list-versions

## Validate configuration before building
./build_unified.sh --prepare-only
```

### build_app.sh

Standard build script with automatic ESP-IDF version handling:

```bash
## Build with specific ESP-IDF version
./build_app.sh gpio_test Release release/v5.4

## Build with default version
./build_app.sh gpio_test Release
```

## Usage Examples

### 1. Initial Setup

```bash
## Navigate to /examples/esp32/scripts
cd /examples/esp32/scripts

## Install all required ESP-IDF versions
./manage_idf.sh install

## Verify installation
./manage_idf.sh status
```

### 2. Building Different Apps with Different Versions

```bash
## Build gpio_test with ESP-IDF v5.5
./build_app.sh gpio_test Release release/v5.5

## Build legacy_app with ESP-IDF v5.4
./build_app.sh legacy_app Debug release/v5.4
```

### 3. Manual Version Switching

```bash
## Export ESP-IDF v5.4 environment
./manage_idf.sh export release/v5.4

## Verify environment
echo $IDF_PATH
echo $IDF_VERSION
idf.py --version

## Switch default version
./manage_idf.sh switch release/v5.4
```

## Environment Variables

### Automatic Export

When using the build scripts, the following environment variables are automatically set:

- `IDF_PATH`: Path to the ESP-IDF installation directory
- `IDF_VERSION`: Current ESP-IDF version being used
- `PATH`: Updated to include ESP-IDF tools

### Manual Export

```bash
## Source the common setup functions
source ./setup_common.sh

## Export specific version
export_esp_idf_version "release/v5.4"
```

## Validation

### App Compatibility

The system automatically validates:

1. **ESP-IDF Version Compatibility**: Ensures the requested version is supported by the app
2. **Build Type Compatibility**: Ensures the requested build type is supported by the app
3. **Version Availability**: Checks if the requested ESP-IDF version is installed

### Error Handling

```bash
## Example error when app doesn't support ESP-IDF version
ERROR: App 'gpio_test' does not support ESP-IDF version 'release/v5.4'
Supported versions for 'gpio_test': release/v5.5
Global ESP-IDF versions: release/v5.5 release/v5.4

## Example error when ESP-IDF version not installed
ERROR: Failed to source ESP-IDF environment for version release/v5.4
Available versions:
  - release/v5.5
To install required versions, run: ./scripts/manage_idf.sh install
```

## CI/CD Integration

### GitHub Actions

The multi-version system integrates seamlessly with CI/CD:

```yaml
jobs:
  build-matrix:
    strategy:
      matrix:
        idf_version: [release/v5.5, release/v5.4]
        app_type: [gpio_test, adc_test]
        build_type: [Debug, Release]
    
    steps:
      - uses: actions/checkout@v3
      - name: Setup ESP-IDF ${{ matrix.idf_version }}
        run: |
          cd /examples/esp32/scripts
          ./manage_idf.sh install
          ./manage_idf.sh export ${{ matrix.idf_version }}
      
      - name: Build ${{ matrix.app_type }}
        run: |
          cd /examples/esp32/scripts
          ./build_app.sh ${{ matrix.app_type }} ${{ matrix.build_type }} ${{ matrix.idf_version }}
```

### Local Development

```bash
## Quick development cycle with version switching
./manage_idf.sh export release/v5.5
./build_app.sh gpio_test Debug
## ... make changes ...
./build_app.sh gpio_test Debug

## Test with different version
./manage_idf.sh export release/v5.4
./build_app.sh gpio_test Debug
```

## Troubleshooting

### Common Issues

1. **ESP-IDF Version Not Found**
   ```bash
   # Install missing versions
   ./manage_idf.sh install
   ```

1. **Environment Not Exported**
   ```bash
   # Manually export environment
   ./manage_idf.sh export release/v5.5
   ```

1. **App Compatibility Issues**
   ```bash
   # Check app requirements
   ./test_multi_idf.sh --test-apps
   ```

1. **Build Failures**
   ```bash
   # Validate configuration
   ./build_unified.sh --prepare-only
   ```

### Debug Mode

Enable verbose output for troubleshooting:

```bash
## Set debug environment variable
export SETUP_DEBUG=1

## Run commands with verbose output
./manage_idf.sh status
./build_app.sh gpio_test Debug release/v5.5
```

## Best Practices

### 1. Version Management

- Keep only the ESP-IDF versions you actually need
- Regularly update installed versions with `./manage_idf.sh update <version>`
- Use `./manage_idf.sh clean <version>` to remove unused versions

### 2. App Configuration

- Specify app-specific ESP-IDF versions only when necessary
- Use global defaults for most apps to maintain consistency
- Document version requirements in app descriptions

### 3. Build Workflows

- Always validate ESP-IDF version compatibility before building
- Use the `--prepare-only` flag to validate configuration without building
- Test builds with multiple ESP-IDF versions in CI/CD

### 4. Environment Management

- Use `./manage_idf.sh export <version>` for manual development
- Let build scripts handle automatic environment switching
- Avoid manually setting `IDF_PATH` when using the build system

## Future Enhancements

### Planned Features

1. **Automatic Version Detection**: Detect ESP-IDF version from project files
2. **Version Migration**: Automated migration between ESP-IDF versions
3. **Dependency Resolution**: Automatic resolution of ESP-IDF version conflicts
4. **Performance Optimization**: Parallel installation and caching improvements

### Contributing

To contribute to the multi-version ESP-IDF system:

1. Test with multiple ESP-IDF versions
2. Validate app compatibility across versions
3. Update documentation for new features
4. Add tests to `test_multi_idf.sh`

## Conclusion

The multi-version ESP-IDF support provides flexibility and compatibility for the ESP32 HardFOC
Interface Wrapper.
By allowing different apps to use different ESP-IDF versions,
the system can accommodate diverse requirements while maintaining a clean and manageable development
environment.

For questions or issues, refer to the main project documentation or create an issue in the project
repository.
