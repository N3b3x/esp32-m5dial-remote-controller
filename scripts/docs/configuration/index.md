---
layout: default
title: "Configuration System"
description: "ESP-IDF configuration system with YAML management, validation, and smart defaults"
nav_order: 2
parent: "🔧 Core Systems"
has_children: true
permalink: /docs/configuration/
---

# ESP-IDF Configuration System Guide

This document provides comprehensive documentation for the ESP-IDF configuration system, including
YAML configuration management, validation, and integration with all scripts.

## 📋 **Table of Contents**


## 📋 **Overview**

The ESP-IDF configuration system provides centralized,
intelligent configuration management for all scripts in the HardFOC Interface Wrapper project.
It features YAML-based configuration, automatic validation, intelligent fallbacks,
and cross-platform compatibility.

### **Core Features**
- **Centralized Configuration**: Single YAML file manages all script behavior
- **Enhanced Validation**: Smart combination validation and error prevention
- **Smart Defaults**: Automatic ESP-IDF version selection based on app and build type
- **Smart Fallbacks**: Graceful degradation when configuration is incomplete
- **Cross-Platform**: Consistent behavior across Linux and macOS
- **Environment Integration**: Environment variable overrides and customization

### **Key Capabilities**
- YAML configuration parsing with `yq` and fallback methods
- **Smart combination validation** - Prevents invalid app + build type + IDF version combinations
- **Automatic ESP-IDF version selection** - Chooses the right version when not specified
- Application and build type validation
- ESP-IDF version compatibility checking
- Environment variable override support
- Configuration integrity validation
- Cross-script configuration sharing

## 🏗️ **Architecture and Design**

### **System Architecture**
```yaml
app_config.yml → config_loader.sh → Script Functions → Environment Variables
     ↓              ↓                    ↓                    ↓
Configuration    Parsing &        Validation &      Script
Definitions      Validation       Fallbacks         Execution
```
### **Component Interaction**
- **`app_config.yml`**: Centralized configuration source
- **`config_loader.sh`**: Configuration parsing and validation engine
- **Script Functions**: Configuration access and validation functions
- **Environment Variables**: Runtime configuration overrides

### **Design Principles**
- **Single Source of Truth**: All configuration in one YAML file
- **Fail-Fast Validation**: Configuration errors caught early with clear messages
- **Intelligent Defaults**: Sensible fallbacks when configuration is incomplete
- **Cross-Platform Consistency**: Uniform behavior across operating systems
- **Performance Optimization**: Efficient parsing and caching mechanisms

---

## 🛡️ **Enhanced Validation System**

The configuration system now includes comprehensive validation to prevent invalid build combinations
and provide clear guidance to users.

### **Validation Features**

- **🔍 Combination Validation** - Validates app + build type + IDF version combinations
- **🚫 Invalid Build Prevention** - Blocks builds with unsupported combinations
- **💡 Smart Error Messages** - Clear guidance on what combinations are allowed
- **🧠 Smart Defaults** - Automatic ESP-IDF version selection when not specified

### **Validation Functions**

The configuration system provides several new validation functions:

#### **Combination Validation**
```bash
## Check if a build combination is valid
is_valid_combination() {
    local app_type="$1"
    local build_type="$2"
    local idf_version="$3"
    
    # Validate app type exists
    if ! is_valid_app_type "$app_type"; then return 1; fi
    
    # Validate build type is supported
    if ! is_valid_build_type "$build_type"; then return 1; fi
    
    # Check if app supports this IDF version
    local app_idf_versions_array=$(get_app_idf_versions_array "$app_type")
    if ! echo "$app_idf_versions_array" | grep -q "$idf_version"; then return 1; fi
    
    # Check if app supports this build type for this IDF version
    local app_build_types=$(get_build_types "$app_type")
    local clean_build_types=$(echo "$app_build_types" | sed 's/\[//g' | sed 's/\]//g' | sed 's/"//g' | tr ',' ' ')
    
    if [[ "$clean_build_types" == *"$build_type"* ]]; then return 0; fi
    
    return 1
}
```
#### **Smart Default Selection**
```bash
## Enhanced IDF version selection with comprehensive validation
## Now handled by enhanced get_idf_version() and is_valid_combination()

## Get app-specific IDF version with fallback
idf_version=$(get_idf_version "gpio_test")

## Comprehensive combination validation
if is_valid_combination "gpio_test" "Release" "release/v5.5"; then
    echo "Valid combination for CI pipeline"
fi

## Enhanced build type validation with app overrides
if is_valid_build_type "Release" "gpio_test" "release/v5.5"; then
    echo "Valid build type for app and IDF version"
fi
```
### **Validation Flow**

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           CONFIGURATION LOADING                             │
│  app_config.yml → config_loader.sh → Validation Functions                   │
└─────────────────────┬───────────────────────────────────────────────────────┘
                      │
                      ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                        BASIC VALIDATION FIRST                               │
│  • Validate app type exists                                                 │
│  • Validate build type is supported                                         │
│  • Fail fast if basic validation fails                                      │
└─────────────────────┬───────────────────────────────────────────────────────┘
                      │
                      ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                        SMART DEFAULT SELECTION                              │
│  • Only if basic validation passes                                          │
│  • Check app-specific IDF versions                                          │
│  • Find first version supporting requested build type                       │
│  • Fallback to global defaults if needed                                    │
│  • Result example: release/v5.5                                                     │
└─────────────────────┬───────────────────────────────────────────────────────┘
                      │
                      ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                        FINAL COMBINATION VALIDATION                         │
│  • Single comprehensive check (no redundant individual validations)         │
│  • Functions remain standalone-safe for independent sourcing                │
│  • Check combination constraints                                            │
└─────────────────────┬───────────────────────────────────────────────────────┘
                      │
                      ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                           VALIDATION RESULT                                 │
│  ✅ VALID: gpio_test + Release + release/v5.5                               │
│  → Proceed with build                                                       │
│                                                                             │
│  ❌ INVALID: gpio_test + Release + release/v5.4                             │
│  → Show error with valid combinations                                       │
│  → Provide helpful next steps                                               │
└─────────────────────────────────────────────────────────────────────────────┘
```

## ⚙️ **Configuration File Structure**

### **Configuration File Location**
The configuration system reads from:
```
/examples/esp32/app_config.yml
```

### **Configuration Schema**

#### **Metadata Section**
```yaml
## Global metadata and defaults
metadata:
  default_app: "ascii_art"            # Default application to build
  default_build_type: "Release"       # Default build configuration
  target: "esp32c6"                   # Target MCU architecture
  idf_versions: ["release/v5.5"]      # Supported ESP-IDF versions
  description: "ESP32 HardFOC Interface Wrapper Configuration"
  version: "2.1.0"
  last_updated: "2025-01-15"
```
#### **Applications Section**
```yaml
## Application definitions and configurations
apps:
  ascii_art:
    description: "ASCII art generator application"
    source_file: "AsciiArtComprehensiveTest.cpp"
    category: "utility"
    build_types: ["Debug", "Release"]
    idf_versions: ["release/v5.5"]
    ci_enabled: true
    featured: true
    dependencies: []
    tags: ["demo", "utility", "ascii"]

  gpio_test:
    description: "GPIO peripheral testing application"
    source_file: "GpioComprehensiveTest.cpp"
    category: "peripheral"
    build_types: ["Debug", "Release"]
    idf_versions: ["release/v5.5"]
    ci_enabled: true
    featured: true
    dependencies: ["gpio_driver"]
    tags: ["peripheral", "gpio", "testing"]

  adc_test:
    description: "ADC peripheral testing application"
    source_file: "AdcComprehensiveTest.cpp"
    category: "peripheral"
    build_types: ["Debug", "Release"]
    idf_versions: ["release/v5.5"]
    ci_enabled: true
    featured: false
    dependencies: ["adc_driver", "gpio_driver"]
    tags: ["peripheral", "adc", "analog"]
```
#### **Build Configuration Section**
```yaml
## Build system configuration
build_config:
  build_types:
    Debug:
      description: "Debug build with symbols and verbose logging"
      cmake_build_type: "Debug"
      optimization: "-O0"
      debug_level: "-g3"
      defines: ["DEBUG", "VERBOSE_LOGGING"]
      assertions: true
      logging_level: "DEBUG"
      stack_usage: true
      
    Release:
      description: "Optimized build for production deployment"
      cmake_build_type: "Release"
      optimization: "-O2"
      debug_level: "-g"
      defines: ["NDEBUG"]
      assertions: false
      logging_level: "INFO"
      stack_usage: false
      
    RelWithDebInfo:
      description: "Release build with debug information"
      cmake_build_type: "RelWithDebInfo"
      optimization: "-O2"
      debug_level: "-g"
      defines: ["NDEBUG"]
      assertions: false
      logging_level: "INFO"
      stack_usage: false
      
    MinSizeRel:
      description: "Release build optimized for size"
      cmake_build_type: "MinSizeRel"
      optimization: "-Os"
      debug_level: "-g"
      defines: ["NDEBUG", "MINIMAL_LOGGING"]
      assertions: false
      logging_level: "WARN"
      stack_usage: false

  # Build system patterns
  build_directory_pattern: "build-app-{app_type}-type-{build_type}-target-{target}-idf-{idf_version}"
  project_name_pattern: "esp32_project*{app_type}*app"
  
  # Build optimization
  ccache_enabled: true
  parallel_builds: true
  incremental_builds: true
  
  # Build validation
  size_analysis: true
  dependency_checking: true
  warning_as_errors: false
```
#### **Flash Configuration Section**
```yaml
## Flash system configuration
flash_config:
  # Port detection
  auto_detect_ports: true
  port_scan_timeout: 5
  port_test_timeout: 3
  
  # Flash parameters
  flash_mode: "dio"
  flash_freq: "80m"
  flash_size: "4MB"
  
  # Monitor settings
  monitor_baud: 115200
  monitor_data_bits: 8
  monitor_parity: "none"
  monitor_stop_bits: 1
  
  # Logging
  auto_logging: true
  log_rotation: true
  max_log_files: 50
  log_retention_days: 30
```
#### **System Configuration Section**
```yaml
## System and environment configuration
system_config:
  # Operating system support
  platforms:
    - "linux"
    - "macos"
    
  
  # Shell requirements
  shell: "bash"
  min_bash_version: "4.0"
  
  # Python requirements
  python_version: "3.6+"
  required_packages: ["PyYAML"]
  
  # Tool requirements
  required_tools: ["git", "cmake", "ninja", "ccache"]
  optional_tools: ["yq", "screen", "tmux"]
  
  # Cache configuration
  cache_directories:
    - "$HOME/.ccache"
    - "$HOME/.espressif"
    - "$HOME/.cache/pip"
```

### **Configuration File Validation**

#### **YAML Schema Validation**
The configuration system validates:
- **Syntax**: Valid YAML format and structure
- **Required Fields**: Essential configuration elements
- **Data Types**: Correct data types for each field
- **Relationships**: Valid references between sections
- **Constraints**: Value ranges and valid options

#### **Configuration Integrity Checks**
```yaml
## Validation rules
validation:
  # App validation
  apps:
    - source_file must exist
    - build_types must be valid
    - idf_versions must be supported
    
  # Build type validation
  build_types:
    - cmake_build_type must be valid
    - optimization flags must be valid
    - defines must be strings
    
  # Metadata validation
  metadata:
    - default_app must exist in apps
    - default_build_type must be valid
    - target must be supported
```
## 🔧 **Configuration Loading and Validation**

### **Configuration Loading Process**

#### **1. Primary Loading Method (yq)**
```bash
## Check for yq availability
check_yq() {
    if command -v yq &> /dev/null; then
        # Detect yq version and set syntax
        local yq_version=$(yq --version | grep -oE '[0-9]+\.[0-9]+' | head -1)
        if [[ "$(echo "$yq_version" | cut -d. -f1)" -ge 4 ]]; then
            export YQ_SYNTAX="eval"
        else
            export YQ_SYNTAX="direct"
        fi
        return 0
    fi
    return 1
}

## Load configuration with yq
load_config_yq() {
    if ! check_yq; then
        return 1
    fi
    
    # Export configuration as environment variables
    export CONFIG_DEFAULT_APP=$(run_yq '.metadata.default_app' -r)
    export CONFIG_DEFAULT_BUILD_TYPE=$(run_yq '.metadata.default_build_type' -r)
    export CONFIG_TARGET=$(run_yq '.metadata.target' -r)
    export CONFIG_DEFAULT_IDF_VERSION=$(run_yq '.metadata.idf_versions[0]' -r)
    
    return 0
}
```
#### **2. Fallback Loading Method (grep/sed)**
```bash
## Fallback parsing without yq
load_config_basic() {
    # Extract basic configuration using grep and sed
    export CONFIG_DEFAULT_APP=$(grep -A 10 "metadata:" "$CONFIG_FILE" | \
        grep "default_app:" | sed 's/.*default_app: *"*\([^"]*\)"*.*/\1/')
    
    export CONFIG_DEFAULT_BUILD_TYPE=$(grep -A 10 "metadata:" "$CONFIG_FILE" | \
        grep "default_build_type:" | sed 's/.*default_build_type: *"*\([^"]*\)"*.*/\1/')
    
    export CONFIG_TARGET=$(grep -A 10 "metadata:" "$CONFIG_FILE" | \
        grep "target:" | sed 's/.*target: *"*\([^"]*\)"*.*/\1/')
    
    export CONFIG_DEFAULT_IDF_VERSION=$(grep -A 10 "metadata:" "$CONFIG_FILE" | \
        grep "idf_versions:" | sed 's/.*idf_versions: *\[*"*\([^"]*\)"*.*/\1/' | head -1)
}
```
### **Configuration Validation Functions**

#### **Application Validation**
```bash
## Validate application type
validate_app_type() {
    local app_type="$1"
    if is_valid_app_type "$app_type"; then
        return 0
    else
        echo "ERROR: Invalid app type: $app_type" >&2
        echo "Available types: $(get_app_types)" >&2
        return 1
    fi
}

## Check if app type is valid
is_valid_app_type() {
    local app_type="$1"
    local app_types=$(get_app_types)
    [[ " $app_types " =~ " $app_type " ]]
}

## Get all available app types
get_app_types() {
    if check_yq; then
        run_yq '.apps | keys | .[]' -r | tr '\n' ' '
    else
        # Fallback: extract from apps section
        grep -A 20 "apps:" "$CONFIG_FILE" | \
            grep -E "^  [a-zA-Z*][a-zA-Z0-9*]*:" | \
            sed 's/^  \([^:]*\):.*/\1/' | tr '\n' ' '
    fi
}
```
#### **Build Type Validation**
```bash
## Validate build type
validate_build_type() {
    local build_type="$1"
    if is_valid_build_type "$build_type"; then
        return 0
    else
        echo "ERROR: Invalid build type: $build_type" >&2
        echo "Available types: $(get_build_types)" >&2
        return 1
    fi
}

## Check if build type is valid
is_valid_build_type() {
    local build_type="$1"
    local build_types=$(get_build_types)
    [[ " $build_types " =~ " $build_type " ]]
}

## Get all available build types
get_build_types() {
    if check_yq; then
        run_yq '.build_config.build_types | keys | .[]' -r | tr '\n' ' '
    else
        # Fallback: extract from build_config section
        grep -A 20 "build_config:" "$CONFIG_FILE" | \
            grep -A 10 "build_types:" | \
            grep -E "^    [A-Za-z][A-Za-z0-9]*:" | \
            sed 's/^    \([^:]*\):.*/\1/' | tr '\n' ' '
    fi
}
```
#### **ESP-IDF Version Validation**
```bash
## Validate ESP-IDF version compatibility
validate_app_idf_version() {
    local app_type="$1"
    local idf_version="$2"
    
    if is_valid_app_idf_version "$app_type" "$idf_version"; then
        return 0
    else
        echo "ERROR: App '$app_type' does not support ESP-IDF version '$idf_version'" >&2
        echo "Supported versions for '$app_type': $(get_app_idf_versions "$app_type")" >&2
        return 1
    fi
}

## Check if app supports ESP-IDF version
is_valid_app_idf_version() {
    local app_type="$1"
    local idf_version="$2"
    
    # Check app-specific versions first
    local app_versions=$(get_app_idf_versions "$app_type")
    if [[ -n "$app_versions" ]]; then
        [[ " $app_versions " =~ " $idf_version " ]]
        return $?
    fi
    
    # Fall back to global versions
    local global_versions=$(get_idf_versions)
    [[ " $global_versions " =~ " $idf_version " ]]
}

## Get ESP-IDF versions supported by app
get_app_idf_versions() {
    local app_type="$1"
    
    if check_yq; then
        run_yq ".apps.$app_type.idf_versions[]" -r 2>/dev/null | tr '\n' ' '
    else
        # Fallback: extract app-specific versions
        grep -A 20 "apps:" "$CONFIG_FILE" | \
            grep -A 10 "  $app_type:" | \
            grep "idf_versions:" | \
            sed 's/.*idf_versions: *\[*"*\([^"]*\)"*.*/\1/' | tr '\n' ' '
    fi
}
```
### **Configuration Access Functions**

#### **Application Information**
```bash
## Get app description
get_app_description() {
    local app_type="$1"
    
    if check_yq; then
        run_yq ".apps.$app_type.description" -r 2>/dev/null
    else
        # Fallback: extract description
        grep -A 20 "apps:" "$CONFIG_FILE" | \
            grep -A 10 "  $app_type:" | \
            grep "description:" | \
            sed 's/.*description: *"*\([^"]*\)"*.*/\1/'
    fi
}

## Get app source file
get_app_source_file() {
    local app_type="$1"
    
    if check_yq; then
        run_yq ".apps.$app_type.source_file" -r 2>/dev/null
    else
        # Fallback: extract source file
        grep -A 20 "apps:" "$CONFIG_FILE" | \
            grep -A 10 "  $app_type:" | \
            grep "source_file:" | \
            sed 's/.*source_file: *"*\([^"]*\)"*.*/\1/'
    fi
}

## Get app category
get_app_category() {
    local app_type="$1"
    
    if check_yq; then
        run_yq ".apps.$app_type.category" -r 2>/dev/null
    else
        # Fallback: extract category
        grep -A 20 "apps:" "$CONFIG_FILE" | \
            grep -A 10 "  $app_type:" | \
            grep "category:" | \
            sed 's/.*category: *"*\([^"]*\)"*.*/\1/'
    fi
}
```
#### **Build Configuration**
```bash
## Get build directory
get_build_directory() {
    local app_type="$1"
    local build_type="$2"
    
    local pattern=$(get_build_directory_pattern)
    echo "$pattern" | sed "s/{app_type}/$app_type/g" | sed "s/{build_type}/$build_type/g"
}

## Get project name
get_project_name() {
    local app_type="$1"
    
    local pattern=$(get_project_name_pattern)
    echo "$pattern" | sed "s/{app_type}/$app_type/g"
}

## Get build directory pattern
get_build_directory_pattern() {
    if check_yq; then
        run_yq '.build_config.build_directory_pattern' -r 2>/dev/null || echo "build-app-{app_type}-type-{build_type}-target-{target}-idf-{idf_version}"
    else
        # Fallback: extract pattern
        grep -A 20 "build_config:" "$CONFIG_FILE" | \
            grep "build_directory_pattern:" | \
            sed 's/.*build_directory_pattern: *"*\([^"]*\)"*.*/\1/' || echo "build-app-{app_type}-type-{build_type}-target-{target}-idf-{idf_version}"
    fi
}
```
## 🔄 **Environment Variable Overrides**

### **Configuration Override System**

#### **Environment Variable Priority**
The configuration system uses a priority-based override system:

```bash
## Priority order (highest to lowest)
1. Command line arguments
2. Environment variables
3. Configuration file defaults
4. Hard-coded fallbacks
```
#### **Supported Environment Variables**
```bash
## Project path configuration (for portable scripts)
export PROJECT_PATH="/path/to/project"  # Override project directory location

## Application configuration overrides
export CONFIG_DEFAULT_APP="gpio_test"
export CONFIG_DEFAULT_BUILD_TYPE="Debug"
export CONFIG_DEFAULT_IDF_VERSION="release/v5.4"
export CONFIG_TARGET="esp32c6"

## Build system overrides
export CLEAN=1                    # Force clean builds
export USE_CCACHE=0               # Disable ccache
export BUILD_VERBOSE=1            # Enable verbose build output

## Flash system overrides
export ESPPORT="/dev/ttyUSB0"     # Override port detection
export MONITOR_BAUD=230400        # Override monitor baud rate
export FLASH_MODE="dio"           # Override flash mode

## Debug and verbose overrides
export DEBUG=1                    # Enable debug mode
export IDF_VERBOSE=1              # Enable ESP-IDF verbose output
export CONFIG_VERBOSE=1           # Enable configuration verbose output
```
### **Portable Configuration**

The configuration system supports portable scripts through the `PROJECT_PATH` environment variable
and `--project-path` command-line flag.

#### **Project Path Resolution**
```bash
## Priority order for project path resolution:
1. --project-path command-line flag
2. PROJECT_PATH environment variable  
3. Default: ../ relative to script location
```
#### **Portable Usage Examples**
```bash
## Using --project-path flag
./build_app.sh --project-path /path/to/project gpio_test Release
./flash_app.sh --project-path ../project flash_monitor adc_test
./manage_idf.sh --project-path /opt/esp32-project list

## Using PROJECT_PATH environment variable
export PROJECT_PATH=/path/to/project
./build_app.sh gpio_test Release
./flash_app.sh flash_monitor adc_test

## Python scripts
python3 get_app_info.py list --project-path /path/to/project
python3 generate_matrix.py --project-path /path/to/project
```

#### **Configuration File Discovery**
When using portable scripts, the system automatically:
1. Resolves the project directory path (absolute or relative)
2. Looks for `app_config.yml` in the project directory
3. Validates that the configuration file exists
4. Loads and parses the configuration

#### **Error Handling**
```bash
## Clear error messages for missing project or config
ERROR: PROJECT_PATH specified but app_config.yml not found: /path/to/project/app_config.yml
Please check the project path or unset PROJECT_PATH to use default location.
```
### **Dynamic Configuration Updates**

#### **Runtime Configuration Changes**
```bash
## Update configuration at runtime
update_config() {
    local key="$1"
    local value="$2"
    
    case "$key" in
        "default_app")
            export CONFIG_DEFAULT_APP="$value"
            ;;
        "default_build_type")
            export CONFIG_DEFAULT_BUILD_TYPE="$value"
            ;;
        "default_idf_version")
            export CONFIG_DEFAULT_IDF_VERSION="$value"
            ;;
        "target")
            export CONFIG_TARGET="$value"
            ;;
        *)
            echo "Unknown configuration key: $key" >&2
            return 1
            ;;
    esac
}

## Usage example
update_config "default_app" "adc_test"
update_config "default_build_type" "Debug"
```
#### **Configuration Validation at Runtime**
```bash
## Validate current configuration
validate_current_config() {
    local errors=0
    
    # Validate default app
    if ! is_valid_app_type "$CONFIG_DEFAULT_APP"; then
        echo "ERROR: Invalid default app: $CONFIG_DEFAULT_APP" >&2
        ((errors++))
    fi
    
    # Validate default build type
    if ! is_valid_build_type "$CONFIG_DEFAULT_BUILD_TYPE"; then
        echo "ERROR: Invalid default build type: $CONFIG_DEFAULT_BUILD_TYPE" >&2
        ((errors++))
    fi
    
    # Validate ESP-IDF version
    if ! is_valid_idf_version "$CONFIG_DEFAULT_IDF_VERSION"; then
        echo "ERROR: Invalid ESP-IDF version: $CONFIG_DEFAULT_IDF_VERSION" >&2
        ((errors++))
    fi
    
    return $errors
}
```
## 🚀 **Usage Examples and Patterns**

### **Basic Configuration Usage**

#### **1. Load and Validate Configuration**
```bash
## Source configuration loader
source ./scripts/config_loader.sh

## Initialize configuration
init_config

## Validate configuration
if ! validate_current_config; then
    echo "Configuration validation failed"
    exit 1
fi

## Use configuration
echo "Default app: $CONFIG_DEFAULT_APP"
echo "Default build type: $CONFIG_DEFAULT_BUILD_TYPE"
echo "Target: $CONFIG_TARGET"
```
#### **2. Application Configuration Access**
```bash
## Get application information
app_types=$(get_app_types)
echo "Available apps: $app_types"

## Get specific app details
description=$(get_app_description "gpio_test")
source_file=$(get_app_source_file "gpio_test")
category=$(get_app_category "gpio_test")

echo "GPIO Test: $description"
echo "Source: $source_file"
echo "Category: $category"
```
#### **3. Build Configuration Access**
```bash
## Get build configuration
build_types=$(get_build_types)
echo "Available build types: $build_types"

## Get build directory
build_dir=$(get_build_directory "gpio_test" "Release")
echo "Build directory: $build_dir"

## Get project name
project_name=$(get_project_name "gpio_test")
echo "Project name: $project_name"
```
### **Advanced Configuration Patterns**

#### **1. Configuration-Driven Scripts**
```bash
#!/bin/bash
## Example: Configuration-driven build script

source ./scripts/config_loader.sh
init_config

## Validate parameters
APP_TYPE=${1:-$CONFIG_DEFAULT_APP}
BUILD_TYPE=${2:-$CONFIG_DEFAULT_BUILD_TYPE}

## Validate configuration
if ! validate_app_type "$APP_TYPE"; then
    exit 1
fi

if ! validate_build_type "$BUILD_TYPE"; then
    exit 1
fi

## Get configuration
SOURCE_FILE=$(get_app_source_file "$APP_TYPE")
BUILD_DIR=$(get_build_directory "$APP_TYPE" "$BUILD_TYPE")
PROJECT_NAME=$(get_project_name "$APP_TYPE")

echo "Building $APP_TYPE ($BUILD_TYPE)"
echo "Source: $SOURCE_FILE"
echo "Build dir: $BUILD_DIR"
echo "Project: $PROJECT_NAME"
```
#### **2. Dynamic Configuration Updates**
```bash
#!/bin/bash
## Example: Dynamic configuration management

source ./scripts/config_loader.sh
init_config

## Function to update configuration
update_app_config() {
    local app_type="$1"
    local key="$2"
    local value="$3"
    
    # Validate app type
    if ! is_valid_app_type "$app_type"; then
        echo "Invalid app type: $app_type" >&2
        return 1
    fi
    
    # Update configuration (this would modify the YAML file)
    echo "Updating $app_type.$key = $value"
    # Implementation would use yq or similar tool
}

## Usage
update_app_config "gpio_test" "featured" "true"
update_app_config "adc_test" "ci_enabled" "false"
```
#### **3. Configuration Validation Scripts**
```bash
#!/bin/bash
## Example: Configuration validation script

source ./scripts/config_loader.sh
init_config

echo "Validating configuration..."

## Check all applications
errors=0
for app in $(get_app_types); do
    echo "Checking app: $app"
    
    # Validate source file exists
    source_file=$(get_app_source_file "$app")
    if [[ ! -f "$source_file" ]]; then
        echo "ERROR: Source file not found: $source_file" >&2
        ((errors++))
    fi
    
    # Validate build types
    app_build_types=$(get_build_types "$app")
    for build_type in $app_build_types; do
        if ! is_valid_build_type "$build_type"; then
            echo "ERROR: Invalid build type for $app: $build_type" >&2
            ((errors++))
        fi
    done
    
    # Validate ESP-IDF versions
    app_idf_versions=$(get_app_idf_versions "$app")
    for idf_version in $app_idf_versions; do
        if ! is_valid_idf_version "$idf_version"; then
            echo "ERROR: Invalid ESP-IDF version for $app: $idf_version" >&2
            ((errors++))
        fi
    done
done

if [[ $errors -eq 0 ]]; then
    echo "Configuration validation passed"
    exit 0
else
    echo "Configuration validation failed with $errors errors"
    exit 1
fi
```
### **Integration Examples**

#### **1. CMake Integration**
```cmake
## CMakeLists.txt configuration integration
cmake_minimum_required(VERSION 3.16)

## Get app information from configuration
execute_process(
    COMMAND bash -c "source ${CMAKE_SOURCE_DIR}/scripts/config_loader.sh && init_config && echo \$CONFIG_DEFAULT_APP"
    OUTPUT_VARIABLE DEFAULT_APP
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

## Get build type from configuration
execute_process(
    COMMAND bash -c "source ${CMAKE_SOURCE_DIR}/scripts/config_loader.sh && init_config && echo \$CONFIG_DEFAULT_BUILD_TYPE"
    OUTPUT_VARIABLE DEFAULT_BUILD_TYPE
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

## Use configuration values
project(esp32*${DEFAULT_APP}*app)

## Set build type
set(CMAKE_BUILD_TYPE ${DEFAULT_BUILD_TYPE})
```
#### **2. CI/CD Integration**
```yaml
## GitHub Actions configuration integration
- name: Validate Configuration
  run: |
    cd /examples/esp32
    source ./scripts/config_loader.sh
    init_config
    if ! validate_current_config; then
      echo "Configuration validation failed"
      exit 1
    fi

- name: Build All CI Apps
  run: |
    cd /examples/esp32
    source ./scripts/config_loader.sh
    init_config
    
    # Get CI-enabled apps
    for app in $(get_app_types); do
      if is_ci_enabled "$app"; then
        echo "Building $app"
        ./scripts/build_app.sh "$app" Release
      fi
    done
```

## 🔍 **Troubleshooting and Debugging**

### **Common Configuration Issues**

#### **1. Configuration File Not Found**
**Problem**: Scripts cannot find `app_config.yml`
**Symptoms**: "Configuration file not found" or "app_config.yml not found" errors
**Solutions**:
```bash
## Check file existence
ls -la app_config.yml

## Check file permissions
ls -la app_config.yml

## Verify working directory
pwd
ls -la

## Check file path
find . -name "app_config.yml"
```

#### **2. YAML Syntax Errors**
**Problem**: Invalid YAML syntax in configuration file
**Symptoms**: "YAML parse error" or "Invalid YAML" errors
**Solutions**:
```bash
## Validate YAML syntax with yq
yq eval app_config.yml

## Check for common syntax issues
grep -n ":" app_config.yml | grep -v "^\s*#"

## Use online YAML validator
## https://www.yamllint.com/
```
#### **3. Configuration Validation Failures**
**Problem**: Configuration validation fails
**Symptoms**: "Configuration validation failed" or "Invalid configuration" errors
**Solutions**:
```bash
## Enable verbose configuration output
export CONFIG_VERBOSE=1

## Check configuration manually
source ./scripts/config_loader.sh
init_config
echo "Default app: $CONFIG_DEFAULT_APP"
echo "Default build type: $CONFIG_DEFAULT_BUILD_TYPE"

## Validate specific sections
validate_app_type "gpio_test"
validate_build_type "Release"
```
#### **4. Environment Variable Conflicts**
**Problem**: Environment variables override configuration incorrectly
**Symptoms**: Unexpected behavior or wrong defaults
**Solutions**:
```bash
## Check environment variables
env | grep CONFIG*
env | grep ESP

## Clear conflicting variables
unset CONFIG_DEFAULT_APP
unset CONFIG_DEFAULT_BUILD_TYPE

## Reload configuration
source ./scripts/config_loader.sh
init_config
```
### **Debug and Verbose Mode**

#### **Enabling Configuration Debug Output**
```bash
## Enable configuration debug mode
export CONFIG_DEBUG=1
export CONFIG_VERBOSE=1

## Source configuration loader
source ./scripts/config_loader.sh
init_config

## Debug information available
- Configuration file loading details
- YAML parsing results
- Validation process information
- Environment variable overrides
- Fallback mechanism details
```
#### **Configuration Debugging Functions**
```bash
## Debug configuration loading
debug_config_loading() {
    echo "=== Configuration Loading Debug ==="
    echo "Config file: $CONFIG_FILE"
    echo "YQ available: $(check_yq && echo "Yes" || echo "No")"
    echo "YQ syntax: $YQ_SYNTAX"
    echo "Config file exists: $([ -f "$CONFIG_FILE" ] && echo "Yes" || echo "No")"
    echo "Config file readable: $([ -r "$CONFIG_FILE" ] && echo "Yes" || echo "No")"
}

## Debug current configuration
debug_current_config() {
    echo "=== Current Configuration Debug ==="
    echo "Default app: $CONFIG_DEFAULT_APP"
    echo "Default build type: $CONFIG_DEFAULT_BUILD_TYPE"
    echo "Target: $CONFIG_TARGET"
    echo "Default IDF version: $CONFIG_DEFAULT_IDF_VERSION"
    echo "Available apps: $(get_app_types)"
    echo "Available build types: $(get_build_types)"
}
```
### **Configuration Testing**

#### **Configuration Test Scripts**
```bash
#!/bin/bash
## Example: Configuration testing script

source ./scripts/config_loader.sh

echo "Testing configuration system..."

## Test 1: Basic loading
echo "Test 1: Basic configuration loading"
if init_config; then
    echo "✓ Configuration loaded successfully"
else
    echo "✗ Configuration loading failed"
    exit 1
fi

## Test 2: Validation
echo "Test 2: Configuration validation"
if validate_current_config; then
    echo "✓ Configuration validation passed"
else
    echo "✗ Configuration validation failed"
    exit 1
fi

## Test 3: App access
echo "Test 3: Application access"
app_types=$(get_app_types)
if [[ -n "$app_types" ]]; then
    echo "✓ Available apps: $app_types"
else
    echo "✗ No apps found"
    exit 1
fi

## Test 4: Build type access
echo "Test 4: Build type access"
build_types=$(get_build_types)
if [[ -n "$build_types" ]]; then
    echo "✓ Available build types: $build_types"
else
    echo "✗ No build types found"
    exit 1
fi

echo "All configuration tests passed!"
```
## 📚 **Reference and Examples**

### **Configuration Function Reference**

#### **Core Functions**
```bash
## Configuration initialization
init_config                    # Initialize configuration system
load_config_yq                 # Load configuration using yq
load_config_basic              # Load configuration using fallback method

## Configuration validation
validate_current_config        # Validate current configuration
validate_app_type             # Validate application type
validate_build_type           # Validate build type

## Configuration access
get_app_types                 # Get all available application types
get_app_description           # Get application description
get_app_source_file           # Get application source file
get_app_category              # Get application category
get_build_types               # Get all available build types
get_idf_versions              # Get supported ESP-IDF versions
get_app_idf_versions          # Get ESP-IDF versions for specific app
get_build_types               # Get build types (with app override support)

## Configuration utilities
get_build_directory           # Get build directory path
get_project_name              # Get project name
get_build_directory_pattern   # Get build directory pattern
get_project_name_pattern      # Get project name pattern
```
#### **Helper Functions**
```bash
## YAML processing
check_yq                      # Check if yq is available
run_yq                        # Execute yq with appropriate syntax
detect_yq_version             # Detect yq version and set syntax

## Validation helpers
is_valid_app_type             # Check if app type is valid
is_valid_build_type           # Check if build type is valid
is_valid_idf_version          # Check if ESP-IDF version is valid
is_valid_combination           # Check if app + build type + IDF version combination is valid

## Configuration utilities
get_featured_app_types        # Get featured application types
is_ci_enabled                # Check if app is CI-enabled
is_featured                  # Check if app is featured
```
### **Configuration Examples**

#### **Minimal Configuration**
```yaml
## Minimal app_config.yml
metadata:
  default_app: "gpio_test"
  default_build_type: "Release"
  target: "esp32c6"

apps:
  gpio_test:
    source_file: "GpioComprehensiveTest.cpp"
    build_types: ["Debug", "Release"]
```
#### **Standard Configuration**
```yaml
## Standard app_config.yml
metadata:
  default_app: "gpio_test"
  default_build_type: "Release"
  target: "esp32c6"
  idf_versions: ["release/v5.5"]
  description: "ESP32 HardFOC Interface Wrapper"
  version: "2.1.0"

apps:
  gpio_test:
    description: "GPIO testing application"
    source_file: "GpioComprehensiveTest.cpp"
    category: "peripheral"
    build_types: ["Debug", "Release"]
    idf_versions: ["release/v5.5"]
    ci_enabled: true
    featured: true

build_config:
  build_types:
    Debug:
      cmake_build_type: "Debug"
      optimization: "-O0"
      debug_level: "-g3"
    Release:
      cmake_build_type: "Release"
      optimization: "-O2"
      debug_level: "-g"
```
#### **Advanced Configuration**
```yaml
## Advanced app_config.yml with all features
metadata:
  default_app: "gpio_test"
  default_build_type: "Release"
  target: "esp32c6"
  idf_versions: ["release/v5.5", "release/v5.4"]
  description: "ESP32 HardFOC Interface Wrapper - Advanced Configuration"
  version: "2.1.0"
  last_updated: "2025-01-15"
  maintainer: "HardFOC Team"
  repository: "https://github.com/example/hf-internal-interface-wrap"

apps:
  gpio_test:
    description: "Comprehensive GPIO testing and validation application"
    source_file: "GpioComprehensiveTest.cpp"
    category: "peripheral"
    build_types: ["Debug", "Release", "RelWithDebInfo"]
    idf_versions: ["release/v5.5", "release/v5.4"]
    ci_enabled: true
    featured: true
    dependencies: ["gpio_driver", "common_utils"]
    tags: ["peripheral", "gpio", "testing", "validation"]
    test_timeout: 300
    memory_requirements: "2MB"
    
  adc_test:
    description: "ADC peripheral testing and calibration application"
    source_file: "AdcComprehensiveTest.cpp"
    category: "peripheral"
    build_types: ["Debug", "Release"]
    idf_versions: ["release/v5.5"]
    ci_enabled: true
    featured: false
    dependencies: ["adc_driver", "gpio_driver", "calibration"]
    tags: ["peripheral", "adc", "analog", "calibration"]
    test_timeout: 600
    memory_requirements: "3MB"

build_config:
  build_types:
    Debug:
      description: "Debug build with maximum debugging information"
      cmake_build_type: "Debug"
      optimization: "-O0"
      debug_level: "-g3"
      defines: ["DEBUG", "VERBOSE_LOGGING", "ASSERTIONS_ENABLED"]
      assertions: true
      logging_level: "DEBUG"
      stack_usage: true
      sanitizers: ["address", "undefined"]
      
    Release:
      description: "Production-ready optimized build"
      cmake_build_type: "Release"
      optimization: "-O2"
      debug_level: "-g"
      defines: ["NDEBUG", "PRODUCTION_BUILD"]
      assertions: false
      logging_level: "INFO"
      stack_usage: false
      sanitizers: []
      
    RelWithDebInfo:
      description: "Release build with debug information for profiling"
      cmake_build_type: "RelWithDebInfo"
      optimization: "-O2"
      debug_level: "-g"
      defines: ["NDEBUG", "PROFILING_ENABLED"]
      assertions: false
      logging_level: "INFO"
      stack_usage: false
      sanitizers: []

  build_directory_pattern: "build-app-{app_type}-type-{build_type}-target-{target}-idf-{idf_version}"
  project_name_pattern: "esp32*{app_type}*app"
  
  ccache_enabled: true
  parallel_builds: true
  incremental_builds: true
  size_analysis: true
  dependency_checking: true
  warning_as_errors: false

flash_config:
  auto_detect_ports: true
  port_scan_timeout: 5
  port_test_timeout: 3
  flash_mode: "dio"
  flash_freq: "80m"
  flash_size: "4MB"
  monitor_baud: 115200
  auto_logging: true
  log_rotation: true
  max_log_files: 50
  log_retention_days: 30

system_config:
  platforms: ["linux", "macos"]
  shell: "bash"
  min_bash_version: "4.0"
  python_version: "3.6+"
  required_packages: ["PyYAML"]
  required_tools: ["git", "cmake", "ninja", "ccache"]
  optional_tools: ["yq", "screen", "tmux"]
  cache_directories: ["$HOME/.ccache", "$HOME/.espressif", "$HOME/.cache/pip"]
```
## 🚀 **Enhanced Functionality**

#### **App-Specific Overrides**
- **Smart Build Type Handling**: `get_build_types(app_type)` now checks app-specific overrides first
- **Version-Aware Validation**: Functions now understand the relationship between IDF versions and build types
- **Intelligent Fallbacks**: When app overrides aren't specified, functions fall back to metadata defaults

#### **🆕 Enhanced Validation Functions**
- **`is_valid_build_type(build_type, app_type, idf_version)`**: Comprehensive validation with app and version context
- **`is_valid_combination(app_type, build_type, idf_version)`**: Single function for complete combination validation
- **`get_app_build_types_for_idf_version(app_type, idf_version)`**: Get build types for specific app-IDF combinations

#### **CI Pipeline Optimization**
- **Robust Combination Validation**: Prevents invalid app + build type + IDF version combinations
- **Smart Error Messages**: Clear guidance on what combinations are allowed
- **Fallback Parsing**: Works reliably even when `yq` is not available in CI environments

### **Enhanced Function Examples**

#### **Smart Build Type Retrieval**
```bash
## Before: Only global build types
build_types=$(get_build_types)  # Returns: Debug Release

## After: App-specific with fallback
build_types=$(get_build_types)                    # Global: Debug Release
app_build_types=$(get_build_types "gpio_test")   # App-specific: Debug Release
```
#### **Comprehensive Validation**
```bash
## Before: Separate validation functions
validate_app_type "gpio_test"
validate_build_type "Release"
validate_app_idf_version "gpio_test" "release/v5.5"

## After: Single comprehensive validation
if is_valid_combination "gpio_test" "Release" "release/v5.5"; then
    echo "Valid combination for CI pipeline"
fi
```
#### **Version-Aware Build Type Validation**
```bash
## Enhanced validation with context
if is_valid_build_type "Release" "gpio_test" "release/v5.5"; then
    echo "Valid build type for app and IDF version"
fi

## Get version-specific build types
version_build_types=$(get_build_types_for_idf_version "release/v5.5")
echo "Build types for v5.5: $version_build_types"
```
### **Migration Guide**

#### **Functions Removed in Version 2.0**
- `get_app_build_types()` → Use `get_build_types(app_type)` instead
- `validate_app_build_type()` → Use `is_valid_build_type(build_type, app_type)` instead
- `validate_app_idf_version()` → Use `is_valid_combination(app_type, build_type, idf_version)` instead
- `get_idf_version_smart()` → Use `get_idf_version(app_type)` + `is_valid_combination()` instead

#### **Updated Function Signatures**
```bash
## Before
get_build_types()                    # Only global
is_valid_build_type(build_type)      # Basic validation

## After
get_build_types([app_type])          # Global or app-specific
is_valid_build_type(build_type, [app_type], [idf_version])  # Comprehensive validation
```
---

### **Best Practices**

#### **1. Configuration Structure**
- Use consistent naming conventions
- Group related configuration items
- Provide clear descriptions for all items
- Use appropriate data types and constraints

#### **2. Validation and Error Handling**
- Always validate configuration before use
- Provide clear error messages
- Implement graceful fallbacks
- Test configuration with various scenarios

#### **3. Documentation and Maintenance**
- Document all configuration options
- Keep configuration examples updated
- Version control configuration changes
- Regular configuration validation

#### **4. Performance and Optimization**
- Use efficient parsing methods
- Implement configuration caching
- Minimize configuration file size
- Optimize validation processes

---

