---
layout: default
title: "Centralized Configuration"
description: "Centralized configuration system with YAML management, validation, and smart defaults"
nav_order: 1
parent: "Configuration System"
permalink: /docs/configuration/centralized/
---

# Centralized Configuration Guide

This document provides comprehensive documentation for the centralized configuration system,
including the `app_config.yml` file structure, configuration loading mechanisms, and integration
with the ESP-IDF development workflow.

---

## 📋 **Table of Contents**


## 📋 **Overview**

The centralized configuration system provides a unified approach to managing ESP-IDF project settings,
application configurations, and build parameters.
The system uses a single `app_config.yml` file to define all project aspects,
enabling consistent configuration management across different scripts and development environments.

### **Core Features**
- **Unified Configuration**: Single YAML file for all project settings
- **Environment Overrides**: Dynamic configuration through environment variables
- **Validation System**: Automatic configuration integrity checking
- **Cross-Platform**: Consistent behavior across different operating systems
- **Integration Ready**: Seamless integration with build and flash systems

### **Key Capabilities**
- Centralized application and build configuration management
- Environment variable-based configuration overrides
- Automatic configuration validation and error reporting
- Integration with CMake and CI/CD systems
- Support for multiple ESP-IDF versions and targets

## 🏗️ **Architecture and Design**

### **System Architecture**
```yaml
Configuration System → YAML Parser → Validation Engine → Script Integration → Environment Overrides
        ↓                ↓              ↓                ↓                ↓
   app_config.yml    yq/grep/sed    Schema Check    Script Loading    Env Var Priority
```
### **Component Interaction**
- **Configuration File**: Central YAML file with all project settings
- **Loading Engine**: Primary (yq) and fallback (grep/sed) loading mechanisms
- **Validation System**: Configuration integrity and schema validation
- **Script Integration**: Configuration access across all scripts
- **Environment Overrides**: Dynamic configuration modification

### **Design Principles**
- **Single Source of Truth**: All configuration in one file
- **Environment Flexibility**: Dynamic configuration through environment variables
- **Validation First**: Automatic configuration integrity checking
- **Performance Optimized**: Efficient loading and caching mechanisms
- **Integration Ready**: Seamless integration with development workflows

## ⚙️ **Configuration File Structure**

### **File Location and Naming**
```bash
## Configuration file location
/examples/esp32/app_config.yml

## Alternative locations (in order of precedence)
./app_config.yml                    # Current directory
../app_config.yml                   # Parent directory
$HOME/.esp32_config.yml            # User home directory
$ESP32_CONFIG_PATH/app_config.yml  # Custom path via environment variable
```
### **Configuration Schema**

#### **Metadata Section**
```yaml
metadata:
  project_name: "ESP32 HardFOC Interface Wrapper"
  version: "2.0.0"
  description: "Comprehensive ESP32 development framework"
  author: "HardFOC Development Team"
  last_updated: "2025-01-27"
  supported_platforms:
    - "Linux (Ubuntu 20.04+)"
    - "macOS (10.15+)"
    - "Windows (WSL2)"
  requirements:
    esp_idf_min: "v5.0.0"
    esp_idf_recommended: "v5.5"
    python_min: "3.8"
    bash_min: "4.0"
```
#### **Applications Section**
```yaml
apps:
  gpio_test:
    description: "GPIO peripheral comprehensive testing"
    source_file: "GpioComprehensiveTest.cpp"
    featured: true
    build_types:
      - "Debug"
      - "Release"
    target: "esp32c6"
    dependencies:
      - "ESP-IDF v5.5+"
      - "GPIO hardware interface"
    ci_config:
      enabled: true
      timeout: 300
      retries: 3
    flash_config:
      port_auto_detect: true
      baud_rate: 921600
      flash_mode: "dio"
      flash_freq: "80m"
      flash_size: "4MB"
    monitor_config:
      baud_rate: 115200
      log_output: true
      log_file: "gpio_test_monitor.log"

  adc_test:
    description: "ADC peripheral comprehensive testing"
    source_file: "AdcComprehensiveTest.cpp"
    featured: true
    build_types:
      - "Debug"
      - "Release"
    target: "esp32c6"
    dependencies:
      - "ESP-IDF v5.5+"
      - "ADC hardware interface"
    ci_config:
      enabled: true
      timeout: 300
      retries: 3
    flash_config:
      port_auto_detect: true
      baud_rate: 921600
      flash_mode: "dio"
      flash_freq: "80m"
      flash_size: "4MB"
    monitor_config:
      baud_rate: 115200
      log_output: true
      log_file: "adc_test_monitor.log"
```
#### **Build Configuration Section**
```yaml
build_config:
  default_build_type: "Release"
  default_target: "esp32c6"
  supported_build_types:
    - "Debug"
    - "Release"
  supported_targets:
    - "esp32"
    - "esp32c3"
    - "esp32c6"
    - "esp32s3"
  
  # ESP-IDF version management
  idf_versions:
    esp32: "v5.0.0"
    esp32c3: "v5.0.0"
    esp32c6: "v5.5"
    esp32s3: "v5.0.0"
  
  # Build optimization
  ccache_enabled: true
  ccache_size: "2G"
  parallel_builds: 4
  incremental_builds: true
  
  # Build validation
  assertions: true
  stack_usage: true
  size_analysis: true
  
  # Debug and development
  logging_level: "INFO"
  sanitizers:
    - "address"
    - "undefined"
  debug_symbols: true
  
  # Performance monitoring
  build_timeout: 1800
  memory_limit: "4G"
  cpu_limit: 4
```
#### **Flash Configuration Section**
```yaml
flash_config:
  # Port detection and management
  port_auto_detect: true
  port_validation: true
  port_timeout: 30
  
  # Flash parameters
  default_baud_rate: 921600
  default_flash_mode: "dio"
  default_flash_freq: "80m"
  default_flash_size: "4MB"
  
  # Flash validation
  flash_verification: true
  flash_checksum: true
  flash_backup: false
  
  # Error handling
  retry_attempts: 3
  retry_delay: 2
  error_recovery: true
  
  # Logging and monitoring
  flash_logging: true
  flash_log_file: "flash_operation.log"
  flash_verbose: false
```
#### **System Configuration Section**
```yaml
system_config:
  # Development environment
  development_mode: true
  debug_mode: false
  verbose_output: false
  
  # Logging configuration
  log_directory: "logs"
  log_retention_days: 30
  log_rotation_size: "100M"
  log_compression: true
  
  # Performance settings
  max_parallel_jobs: 4
  memory_limit: "4G"
  cpu_limit: 4
  
  # Security and permissions
  strict_permissions: false
  auto_fix_permissions: true
  user_group_management: true
  
  # Integration settings
  cmake_integration: true
  ci_cd_integration: true
  ide_integration: true
```

## 🔧 **Configuration Loading and Validation**

### **Configuration Loading Process**

#### **Primary Loading Method (yq)**
The system primarily uses `yq` for YAML processing:

```bash
## Primary loading function
load_config_yq() {
    local config_file="$1"
    local query="$2"
    
    if command -v yq &> /dev/null; then
        yq eval "$query" "$config_file" 2>/dev/null
        return $?
    else
        return 1
    fi
}

## Usage examples
load_config_yq "app_config.yml" ".metadata.project_name"
load_config_yq "app_config.yml" ".apps.gpio_test.description"
load_config_yq "app_config.yml" ".build_config.default_build_type"
```
#### **Fallback Loading Method (grep/sed)**
When `yq` is unavailable, the system falls back to `grep` and `sed`:

```bash
## Fallback loading function
load_config_fallback() {
    local config_file="$1"
    local key="$2"
    
    # Convert YAML path to grep pattern
    local pattern=$(echo "$key" | sed 's/\./\\n/g')
    
    # Extract value using grep and sed
    grep -A1 -B1 "$pattern" "$config_file" | \
    sed -n '/^[[:space:]]*[^[:space:]#]/p' | \
    sed 's/^[[:space:]]*//' | \
    sed 's/[[:space:]]*:[[:space:]]*/:/' | \
    sed 's/^[^:]*://'
}

## Usage examples
load_config_fallback "app_config.yml" "metadata.project_name"
load_config_fallback "app_config.yml" "apps.gpio_test.description"
```
#### **Configuration Loading Priority**
```bash
## Loading priority order
1. Environment variable override
2. Primary method (yq)
3. Fallback method (grep/sed)
4. Default value
5. Error (if required)

## Implementation
load_config() {
    local key="$1"
    local default_value="$2"
    local required="${3:-false}"
    
    # Check environment variable first
    local env_var=$(echo "$key" | tr '[:lower:]' '[:upper:]' | tr '.' '*')
    if [ -n "${!env_var}" ]; then
        echo "${!env_var}"
        return 0
    fi
    
    # Try primary method
    local value=$(load_config_yq "app_config.yml" ".$key")
    if [ $? -eq 0 ] && [ -n "$value" ]; then
        echo "$value"
        return 0
    fi
    
    # Try fallback method
    value=$(load_config_fallback "app_config.yml" "$key")
    if [ $? -eq 0 ] && [ -n "$value" ]; then
        echo "$value"
        return 0
    fi
    
    # Use default value
    if [ -n "$default_value" ]; then
        echo "$default_value"
        return 0
    fi
    
    # Handle required configuration
    if [ "$required" = "true" ]; then
        echo "ERROR: Required configuration '$key' not found" >&2
        return 1
    fi
    
    return 0
}
```
### **Configuration Validation**

#### **Schema Validation**
```bash
## Basic schema validation
validate_config_schema() {
    local config_file="$1"
    local errors=0
    
    # Check required sections
    local required_sections=("metadata" "apps" "build_config")
    for section in "${required_sections[@]}"; do
        if ! load_config "$section" "" true >/dev/null 2>&1; then
            echo "ERROR: Missing required section '$section'" >&2
            ((errors++))
        fi
    done
    
    # Check required metadata fields
    local required_metadata=("project_name" "version")
    for field in "${required_metadata[@]}"; do
        if ! load_config "metadata.$field" "" true >/dev/null 2>&1; then
            echo "ERROR: Missing required metadata field '$field'" >&2
            ((errors++))
        fi
    done
    
    # Check application configurations
    local apps=$(load_config "apps" "" false)
    if [ -n "$apps" ]; then
        for app in $(echo "$apps" | tr ' ' '\n'); do
            validate_app_config "$app"
        done
    fi
    
    return $errors
}
```
#### **Application Configuration Validation**
```bash
## Validate individual application configuration
validate_app_config() {
    local app_name="$1"
    local errors=0
    
    # Check required app fields
    local required_fields=("description" "source_file" "build_types" "target")
    for field in "${required_fields[@]}"; do
        if ! load_config "apps.$app_name.$field" "" true >/dev/null 2>&1; then
            echo "ERROR: App '$app_name' missing required field '$field'" >&2
            ((errors++))
        fi
    done
    
    # Validate source file existence
    local source_file=$(load_config "apps.$app_name.source_file" "" false)
    if [ -n "$source_file" ]; then
        local full_path="/examples/esp32/main/$source_file"
        if [ ! -f "$full_path" ]; then
            echo "ERROR: App '$app_name' source file not found: $full_path" >&2
            ((errors++))
        fi
    fi
    
    # Validate build types
    local build_types=$(load_config "apps.$app_name.build_types" "" false)
    if [ -n "$build_types" ]; then
        for build_type in $(echo "$build_types" | tr ' ' '\n'); do
            if [[ ! "$build_type" =~ ^(Debug|Release)$ ]]; then
                echo "ERROR: App '$app_name' invalid build type: $build_type" >&2
                ((errors++))
            fi
        done
    fi
    
    return $errors
}
```
#### **Configuration Integrity Checking**
```bash
## Comprehensive configuration validation
validate_config_integrity() {
    local config_file="$1"
    local errors=0
    
    echo "Validating configuration file: $config_file"
    
    # Check file existence
    if [ ! -f "$config_file" ]; then
        echo "ERROR: Configuration file not found: $config_file" >&2
        return 1
    fi
    
    # Check YAML syntax (if yq available)
    if command -v yq &> /dev/null; then
        if ! yq eval "." "$config_file" >/dev/null 2>&1; then
            echo "ERROR: Invalid YAML syntax in configuration file" >&2
            ((errors++))
        fi
    fi
    
    # Validate schema
    if ! validate_config_schema "$config_file"; then
        ((errors++))
    fi
    
    # Check for configuration conflicts
    if ! check_config_conflicts "$config_file"; then
        ((errors++))
    fi
    
    if [ $errors -eq 0 ]; then
        echo "✓ Configuration validation passed"
        return 0
    else
        echo "✗ Configuration validation failed with $errors error(s)"
        return 1
    fi
}
```
## 🌍 **Environment Variable Overrides**

### **Override Priority System**

#### **Priority Order**
```bash
## Configuration override priority (highest to lowest)
1. Command line arguments
2. Environment variables
3. Configuration file values
4. Default values
5. System defaults

## Implementation
get_config_value() {
    local key="$1"
    local default_value="$2"
    
    # Check command line arguments first
    local cmd_arg=$(get_cmd_line_arg "$key")
    if [ -n "$cmd_arg" ]; then
        echo "$cmd_arg"
        return 0
    fi
    
    # Check environment variables
    local env_var=$(get_env_var "$key")
    if [ -n "$env_var" ]; then
        echo "$env_var"
        return 0
    fi
    
    # Load from configuration file
    local config_value=$(load_config "$key" "" false)
    if [ -n "$config_value" ]; then
        echo "$config_value"
        return 0
    fi
    
    # Use default value
    echo "$default_value"
}
```
#### **Environment Variable Naming Convention**
```bash
## Environment variable naming pattern
## Convert YAML path to environment variable name
## Format: ESP32*<SECTION>*<SUBSECTION>*<FIELD>

## Examples:
ESP32_METADATA_PROJECT_NAME="My Project"
ESP32_BUILD_CONFIG_DEFAULT_BUILD_TYPE="Debug"
ESP32_APPS_GPIO_TEST_TARGET="esp32s3"
ESP32_FLASH_CONFIG_PORT_AUTO_DETECT="false"
ESP32_SYSTEM_CONFIG_DEBUG_MODE="true"

## Conversion function
yaml_path_to_env_var() {
    local yaml_path="$1"
    echo "$yaml_path" | \
    tr '[:lower:]' '[:upper:]' | \
    tr '.' '*' | \
    sed 's/^/ESP32*/'
}

## Usage
env_var=$(yaml_path_to_env_var "build_config.default_build_type")
## Result: ESP32_BUILD_CONFIG_DEFAULT_BUILD_TYPE
```
### **Supported Environment Variables**

#### **Build Configuration Overrides**
```bash
## Build type and target
export ESP32_BUILD_CONFIG_DEFAULT_BUILD_TYPE="Debug"
export ESP32_BUILD_CONFIG_DEFAULT_TARGET="esp32s3"
export ESP32_BUILD_CONFIG_CCACHE_ENABLED="true"
export ESP32_BUILD_CONFIG_PARALLEL_BUILDS="8"
export ESP32_BUILD_CONFIG_BUILD_TIMEOUT="3600"

## ESP-IDF version overrides
export ESP32_BUILD_CONFIG_IDF_VERSIONS_ESP32="v5.0.0"
export ESP32_BUILD_CONFIG_IDF_VERSIONS_ESP32C6="v5.5"
export ESP32_BUILD_CONFIG_IDF_VERSIONS_ESP32S3="v5.0.0"

## Build optimization
export ESP32_BUILD_CONFIG_CCACHE_SIZE="4G"
export ESP32_BUILD_CONFIG_INCREMENTAL_BUILDS="true"
export ESP32_BUILD_CONFIG_DEBUG_SYMBOLS="true"
```
#### **Application Configuration Overrides**
```bash
## Application-specific overrides
export ESP32_APPS_GPIO_TEST_TARGET="esp32s3"
export ESP32_APPS_GPIO_TEST_BUILD_TYPES="Debug Release"
export ESP32_APPS_ADC_TEST_FLASH_CONFIG_BAUD_RATE="460800"
export ESP32_APPS_UART_TEST_MONITOR_CONFIG_BAUD_RATE="230400"

## Application dependencies
export ESP32_APPS_GPIO_TEST_DEPENDENCIES="ESP-IDF v5.5+,GPIO hardware"
export ESP32_APPS_ADC_TEST_DEPENDENCIES="ESP-IDF v5.5+,ADC hardware"
```
#### **Flash Configuration Overrides**
```bash
## Flash parameters
export ESP32_FLASH_CONFIG_PORT_AUTO_DETECT="false"
export ESP32_FLASH_CONFIG_DEFAULT_BAUD_RATE="460800"
export ESP32_FLASH_CONFIG_DEFAULT_FLASH_MODE="qio"
export ESP32_FLASH_CONFIG_DEFAULT_FLASH_FREQ="40m"
export ESP32_FLASH_CONFIG_DEFAULT_FLASH_SIZE="8MB"

## Flash validation
export ESP32_FLASH_CONFIG_FLASH_VERIFICATION="true"
export ESP32_FLASH_CONFIG_FLASH_CHECKSUM="true"
export ESP32_FLASH_CONFIG_RETRY_ATTEMPTS="5"
```
#### **System Configuration Overrides**
```bash
## System settings
export ESP32_SYSTEM_CONFIG_DEVELOPMENT_MODE="true"
export ESP32_SYSTEM_CONFIG_DEBUG_MODE="true"
export ESP32_SYSTEM_CONFIG_VERBOSE_OUTPUT="true"

## Logging configuration
export ESP32_SYSTEM_CONFIG_LOG_DIRECTORY="/tmp/esp32_logs"
export ESP32_SYSTEM_CONFIG_LOG_RETENTION_DAYS="7"
export ESP32_SYSTEM_CONFIG_LOG_ROTATION_SIZE="50M"

## Performance settings
export ESP32_SYSTEM_CONFIG_MAX_PARALLEL_JOBS="8"
export ESP32_SYSTEM_CONFIG_MEMORY_LIMIT="8G"
export ESP32_SYSTEM_CONFIG_CPU_LIMIT="8"
```
### **Dynamic Configuration Updates**

#### **Runtime Configuration Modification**
```bash
## Update configuration at runtime
update_config_runtime() {
    local key="$1"
    local value="$2"
    
    # Set environment variable for immediate effect
    local env_var=$(yaml_path_to_env_var "$key")
    export "$env_var"="$value"
    
    echo "Updated configuration: $key = $value"
}

## Usage examples
update_config_runtime "build_config.default_build_type" "Debug"
update_config_runtime "flash_config.port_auto_detect" "false"
update_config_runtime "system_config.debug_mode" "true"
```
#### **Configuration Reloading**
```bash
## Reload configuration from file
reload_config() {
    local config_file="$1"
    
    echo "Reloading configuration from: $config_file"
    
    # Validate configuration file
    if ! validate_config_integrity "$config_file"; then
        echo "ERROR: Configuration validation failed" >&2
        return 1
    fi
    
    # Clear cached configuration
    unset CONFIG_CACHE
    
    # Reload configuration
    source_config_file "$config_file"
    
    echo "Configuration reloaded successfully"
}
```
## 🚀 **Usage Examples and Patterns**

### **Basic Configuration Usage**

#### **1. Loading Configuration Values**
```bash
## Load basic configuration values
project_name=$(load_config "metadata.project_name" "Unknown Project")
version=$(load_config "metadata.version" "1.0.0")
default_build_type=$(load_config "build_config.default_build_type" "Release")

## Load application configuration
gpio_description=$(load_config "apps.gpio_test.description" "")
gpio_target=$(load_config "apps.gpio_test.target" "esp32c6")
gpio_build_types=$(load_config "apps.gpio_test.build_types" "Debug Release")
```
#### **2. Configuration Validation**
```bash
## Validate configuration file
if ! validate_config_integrity "app_config.yml"; then
    echo "Configuration validation failed"
    exit 1
fi

## Validate specific application
if ! validate_app_config "gpio_test"; then
    echo "GPIO test configuration validation failed"
    exit 1
fi
```
#### **3. Environment Variable Overrides**
```bash
## Override build configuration
export ESP32_BUILD_CONFIG_DEFAULT_BUILD_TYPE="Debug"
export ESP32_BUILD_CONFIG_DEFAULT_TARGET="esp32s3"

## Override application configuration
export ESP32_APPS_GPIO_TEST_TARGET="esp32s3"
export ESP32_APPS_GPIO_TEST_BUILD_TYPES="Debug"

## Override flash configuration
export ESP32_FLASH_CONFIG_PORT_AUTO_DETECT="false"
export ESP32_FLASH_CONFIG_DEFAULT_BAUD_RATE="460800"
```
### **Advanced Configuration Patterns**

#### **1. Configuration-Driven Scripts**
```bash
#!/bin/bash
## Configuration-driven build script

## Load configuration
source ./scripts/config_loader.sh

## Get build parameters
build_type=$(load_config "build_config.default_build_type" "Release")
target=$(load_config "build_config.default_target" "esp32c6")
parallel_jobs=$(load_config "build_config.parallel_builds" "4")

## Validate configuration
if ! validate_config_integrity "app_config.yml"; then
    echo "Configuration validation failed"
    exit 1
fi

## Execute build with configuration
idf.py set-target "$target"
idf.py build -b "$build_type" -j "$parallel_jobs"
```
#### **2. Dynamic Configuration Updates**
```bash
#!/bin/bash
## Dynamic configuration management

## Update configuration based on environment
if [ "$CI" = "true" ]; then
    update_config_runtime "system_config.debug_mode" "false"
    update_config_runtime "build_config.parallel_builds" "8"
    update_config_runtime "flash_config.port_auto_detect" "false"
fi

## Update configuration based on user preferences
if [ "$USER_PREFERS_DEBUG" = "true" ]; then
    update_config_runtime "build_config.default_build_type" "Debug"
    update_config_runtime "system_config.verbose_output" "true"
fi
```
#### **3. Configuration Testing and Validation**
```bash
#!/bin/bash
## Configuration testing script

## Test configuration loading
echo "Testing configuration loading..."

## Test metadata loading
project_name=$(load_config "metadata.project_name" "")
if [ -n "$project_name" ]; then
    echo "✓ Project name loaded: $project_name"
else
    echo "✗ Failed to load project name"
fi

## Test application loading
apps=$(load_config "apps" "")
if [ -n "$apps" ]; then
    echo "✓ Applications loaded: $apps"
else
    echo "✗ Failed to load applications"
fi

## Test build configuration
build_type=$(load_config "build_config.default_build_type" "")
if [ -n "$build_type" ]; then
    echo "✓ Build type loaded: $build_type"
else
    echo "✗ Failed to load build type"
fi
```
### **Integration Examples**

#### **1. CMake Integration**
```cmake
## CMakeLists.txt configuration integration
cmake_minimum_required(VERSION 3.16)

## Load configuration values
execute_process(
    COMMAND bash -c "source ${CMAKE_SOURCE_DIR}/scripts/config_loader.sh && load_config 'build_config.default_target' 'esp32c6'"
    OUTPUT_VARIABLE ESP32_TARGET
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

execute_process(
    COMMAND bash -c "source ${CMAKE_SOURCE_DIR}/scripts/config_loader.sh && load_config 'build_config.default_build_type' 'Release'"
    OUTPUT_VARIABLE ESP32_BUILD_TYPE
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

## Use configuration values
set(IDF_TARGET ${ESP32_TARGET})
set(CONFIG_DEFAULT_BUILD_TYPE ${ESP32_BUILD_TYPE})

## Configuration validation
add_custom_target(validate_config
    COMMAND bash -c "cd ${CMAKE_SOURCE_DIR} && source scripts/config_loader.sh && validate_config_integrity app_config.yml"
    COMMENT "Validating configuration"
)
```
#### **2. CI/CD Integration**
```yaml
## GitHub Actions configuration integration
- name: Validate Configuration
  run: |
    cd /examples/esp32
    source scripts/config_loader.sh
    validate_config_integrity app_config.yml

- name: Load Build Configuration
  run: |
    cd /examples/esp32
    source scripts/config_loader.sh
    echo "BUILD_TYPE=$(load_config 'build_config.default_build_type' 'Release')" >> $GITHUB_ENV
    echo "TARGET=$(load_config 'build_config.default_target' 'esp32c6')" >> $GITHUB_ENV

- name: Build Application
  run: |
    cd /examples/esp32
    idf.py set-target ${{ env.TARGET }}
    idf.py build -b ${{ env.BUILD_TYPE }}
```
#### **3. Development Environment Setup**
```bash
#!/bin/bash
## Development environment configuration

## Load configuration
source ./scripts/config_loader.sh

## Validate configuration
if ! validate_config_integrity "app_config.yml"; then
    echo "Configuration validation failed"
    exit 1
fi

## Setup environment based on configuration
target=$(load_config "build_config.default_target" "esp32c6")
build_type=$(load_config "build_config.default_build_type" "Release")
debug_mode=$(load_config "system_config.debug_mode" "false")

## Configure ESP-IDF
idf.py set-target "$target"

## Set environment variables
export IDF_TARGET="$target"
export CONFIG_DEFAULT_BUILD_TYPE="$build_type"

if [ "$debug_mode" = "true" ]; then
    export ESP32_DEBUG_MODE="true"
    export ESP32_VERBOSE_OUTPUT="true"
fi

echo "Development environment configured:"
echo "  Target: $target"
echo "  Build Type: $build_type"
echo "  Debug Mode: $debug_mode"
```
## 🔍 **Troubleshooting and Debugging**

### **Common Configuration Issues**

#### **1. Configuration File Not Found**
**Problem**: Configuration file not found or inaccessible
**Symptoms**: "Configuration file not found" or "File not accessible" errors
**Solutions**:
```bash
## Check file existence
ls -la app_config.yml
ls -la /examples/esp32/app_config.yml

## Check file permissions
ls -la app_config.yml
chmod 644 app_config.yml

## Check file path
pwd
find . -name "app_config.yml" -type f

## Verify file location
## Configuration file should be in /examples/esp32/ directory
```

#### **2. YAML Syntax Errors**
**Problem**: Invalid YAML syntax in configuration file
**Symptoms**: "Invalid YAML syntax" or parsing errors
**Solutions**:
```bash
## Validate YAML syntax with yq
yq eval "." app_config.yml

## Check for common YAML issues
## - Proper indentation (spaces, not tabs)
## - Valid key-value pairs
## - Proper list formatting
## - Valid string escaping

## Use YAML linter
yamllint app_config.yml

## Check for hidden characters
cat -A app_config.yml
```
#### **3. Configuration Validation Failures**
**Problem**: Configuration validation fails during loading
**Symptoms**: "Configuration validation failed" or missing required fields
**Solutions**:
```bash
## Run validation manually
source ./scripts/config_loader.sh
validate_config_integrity app_config.yml

## Check specific validation errors
validate_config_schema app_config.yml
validate_app_config "gpio_test"

## Verify required fields
load_config "metadata.project_name" "" true
load_config "apps.gpio_test.description" "" true
load_config "build_config.default_build_type" "" true
```
#### **4. Environment Variable Conflicts**
**Problem**: Environment variables conflict with configuration file values
**Symptoms**: Unexpected configuration values or override issues
**Solutions**:
```bash
## Check environment variables
env | grep ESP32

## Clear conflicting environment variables
unset ESP32_BUILD_CONFIG_DEFAULT_BUILD_TYPE
unset ESP32_APPS_GPIO_TEST_TARGET

## Verify configuration loading priority
load_config "build_config.default_build_type" "Release"
update_config_runtime "build_config.default_build_type" "Debug"
load_config "build_config.default_build_type" "Release"
```
### **Debug and Verbose Mode**

#### **Enabling Debug Output**
```bash
## Enable debug mode
export DEBUG=1
export VERBOSE=1
export CONFIG_DEBUG=1

## Run with debug output
source ./scripts/config_loader.sh
load_config "metadata.project_name" "" true

## Debug information available
- Configuration file loading process
- Environment variable processing
- Configuration validation details
- Error context and resolution
```
#### **Configuration Testing**
```bash
## Test configuration loading
test_config_loading() {
    echo "Testing configuration loading..."
    
    # Test metadata
    local project_name=$(load_config "metadata.project_name" "" false)
    echo "Project name: $project_name"
    
    # Test applications
    local apps=$(load_config "apps" "" false)
    echo "Applications: $apps"
    
    # Test build configuration
    local build_type=$(load_config "build_config.default_build_type" "" false)
    echo "Build type: $build_type"
    
    # Test environment variable overrides
    export ESP32_BUILD_CONFIG_DEFAULT_BUILD_TYPE="Debug"
    local override_value=$(load_config "build_config.default_build_type" "" false)
    echo "Override value: $override_value"
}

## Run configuration test
test_config_loading
```
## 📚 **Reference and Examples**

### **Configuration Function Reference**

#### **Core Loading Functions**
```bash
## Primary configuration loading
load_config <key> [default_value] [required]

## YAML-based loading
load_config_yq <config_file> <query>

## Fallback loading
load_config_fallback <config_file> <key>

## Configuration validation
validate_config_integrity <config_file>
validate_config_schema <config_file>
validate_app_config <app_name>
```
#### **Environment Variable Functions**
```bash
## Environment variable management
yaml_path_to_env_var <yaml_path>
get_env_var <key>
update_config_runtime <key> <value>

## Configuration reloading
reload_config <config_file>
source_config_file <config_file>
```
#### **Utility Functions**
```bash
## Configuration utilities
get_config_value <key> [default_value]
check_config_conflicts <config_file>
get_cmd_line_arg <key>
```
### **Configuration Examples**

#### **Minimal Configuration**
```yaml
metadata:
  project_name: "ESP32 Example"
  version: "1.0.0"

apps:
  basic_test:
    description: "Basic ESP32 test"
    source_file: "BasicTest.cpp"
    build_types: ["Debug", "Release"]
    target: "esp32c6"

build_config:
  default_build_type: "Release"
  default_target: "esp32c6"
  supported_build_types: ["Debug", "Release"]
  supported_targets: ["esp32", "esp32c6"]
```
#### **Standard Configuration**
```yaml
metadata:
  project_name: "ESP32 HardFOC Interface Wrapper"
  version: "2.0.0"
  description: "Comprehensive ESP32 development framework"
  author: "HardFOC Development Team"
  last_updated: "2025-01-27"
  supported_platforms: ["Linux (Ubuntu 20.04+)", "macOS (10.15+)", "Windows (WSL2)"]
  requirements:
    esp_idf_min: "v5.0.0"
    esp_idf_recommended: "v5.5.0"
    python_min: "3.8"
    bash_min: "4.0"

apps:
  gpio_test:
    description: "GPIO peripheral comprehensive testing"
    source_file: "GpioComprehensiveTest.cpp"
    featured: true
    build_types: ["Debug", "Release"]
    target: "esp32c6"
    dependencies: ["ESP-IDF v5.5+", "GPIO hardware interface"]
    ci_config:
      enabled: true
      timeout: 300
      retries: 3
    flash_config:
      port_auto_detect: true
      baud_rate: 921600
      flash_mode: "dio"
      flash_freq: "80m"
      flash_size: "4MB"
    monitor_config:
      baud_rate: 115200
      log_output: true
      log_file: "gpio_test_monitor.log"

build_config:
  default_build_type: "Release"
  default_target: "esp32c6"
  supported_build_types: ["Debug", "Release"]
  supported_targets: ["esp32", "esp32c3", "esp32c6", "esp32s3"]
  idf_versions:
    esp32: "v5.0.0"
    esp32c3: "v5.0.0"
    esp32c6: "v5.5"
    esp32s3: "v5.0.0"
  ccache_enabled: true
  ccache_size: "2G"
  parallel_builds: 4
  incremental_builds: true
  assertions: true
  stack_usage: true
  size_analysis: true
  logging_level: "INFO"
  sanitizers: ["address", "undefined"]
  debug_symbols: true
  build_timeout: 1800
  memory_limit: "4G"
  cpu_limit: 4

flash_config:
  port_auto_detect: true
  port_validation: true
  port_timeout: 30
  default_baud_rate: 921600
  default_flash_mode: "dio"
  default_flash_freq: "80m"
  default_flash_size: "4MB"
  flash_verification: true
  flash_checksum: true
  flash_backup: false
  retry_attempts: 3
  retry_delay: 2
  error_recovery: true
  flash_logging: true
  flash_log_file: "flash_operation.log"
  flash_verbose: false

system_config:
  development_mode: true
  debug_mode: false
  verbose_output: false
  log_directory: "logs"
  log_retention_days: 30
  log_rotation_size: "100M"
  log_compression: true
  max_parallel_jobs: 4
  memory_limit: "4G"
  cpu_limit: 4
  strict_permissions: false
  auto_fix_permissions: true
  user_group_management: true
  cmake_integration: true
  ci_cd_integration: true
  ide_integration: true
```
#### **Advanced Configuration**
```yaml
metadata:
  project_name: "ESP32 Advanced Development Framework"
  version: "3.0.0"
  description: "Advanced ESP32 development framework with CI/CD integration"
  author: "Advanced Development Team"
  last_updated: "2025-01-27"
  supported_platforms: ["Linux (Ubuntu 20.04+)", "macOS (10.15+)", "Docker"]
  requirements:
    esp_idf_min: "v5.0.0"
    esp_idf_recommended: "v5.5"
    python_min: "3.8"
    bash_min: "4.0"
    docker_min: "20.10.0"
  ci_cd:
    github_actions: true
    gitlab_ci: true
    jenkins: true
    docker: true

apps:
  comprehensive_test:
    description: "Comprehensive ESP32 testing suite"
    source_file: "ComprehensiveTest.cpp"
    featured: true
    build_types: ["Debug", "Release", "Profiling"]
    target: "esp32c6"
    dependencies: ["ESP-IDF v5.5+", "All hardware interfaces", "Testing framework"]
    ci_config:
      enabled: true
      timeout: 600
      retries: 5
      parallel_execution: true
      resource_requirements:
        memory: "8G"
        cpu: "8"
        storage: "20G"
    flash_config:
      port_auto_detect: true
      baud_rate: 921600
      flash_mode: "dio"
      flash_freq: "80m"
      flash_size: "8MB"
      flash_verification: true
      flash_checksum: true
      flash_backup: true
      backup_directory: "flash_backups"
    monitor_config:
      baud_rate: 115200
      log_output: true
      log_file: "comprehensive_test_monitor.log"
      log_rotation: true
      log_compression: true
    testing_config:
      unit_tests: true
      integration_tests: true
      performance_tests: true
      stress_tests: true
      test_timeout: 300
      test_retries: 3

build_config:
  default_build_type: "Release"
  default_target: "esp32c6"
  supported_build_types: ["Debug", "Release", "Profiling", "Production"]
  supported_targets: ["esp32", "esp32c3", "esp32c6", "esp32s3", "esp32h2"]
  idf_versions:
    esp32: "v5.0.0"
    esp32c3: "v5.0.0"
    esp32c6: "v5.5"
    esp32s3: "v5.0.0"
    esp32h2: "v5.5"
  ccache_enabled: true
  ccache_size: "4G"
  ccache_compression: true
  parallel_builds: 8
  incremental_builds: true
  distributed_builds: true
  build_farm:
    enabled: true
    nodes: 4
    load_balancing: true
  assertions: true
  stack_usage: true
  size_analysis: true
  memory_analysis: true
  performance_analysis: true
  logging_level: "DEBUG"
  sanitizers: ["address", "undefined", "thread", "memory"]
  debug_symbols: true
  optimization_level: "O2"
  build_timeout: 3600
  memory_limit: "8G"
  cpu_limit: 8
  storage_limit: "50G"
  network_access: true
  build_caching: true
  artifact_storage: true

flash_config:
  port_auto_detect: true
  port_validation: true
  port_timeout: 60
  port_retry_attempts: 5
  default_baud_rate: 921600
  default_flash_mode: "dio"
  default_flash_freq: "80m"
  default_flash_size: "8MB"
  flash_verification: true
  flash_checksum: true
  flash_backup: true
  flash_encryption: false
  flash_secure_boot: false
  retry_attempts: 5
  retry_delay: 3
  error_recovery: true
  flash_logging: true
  flash_log_file: "flash_operation.log"
  flash_verbose: true
  flash_progress: true
  flash_validation: true
  flash_optimization: true

system_config:
  development_mode: true
  debug_mode: true
  verbose_output: true
  profiling_mode: false
  production_mode: false
  log_directory: "logs"
  log_retention_days: 90
  log_rotation_size: "500M"
  log_compression: true
  log_encryption: false
  log_archiving: true
  log_analysis: true
  max_parallel_jobs: 8
  memory_limit: "8G"
  cpu_limit: 8
  storage_limit: "100G"
  network_limit: "1Gbps"
  strict_permissions: false
  auto_fix_permissions: true
  user_group_management: true
  security_scanning: true
  vulnerability_checking: true
  cmake_integration: true
  ci_cd_integration: true
  ide_integration: true
  docker_integration: true
  cloud_integration: true
  monitoring: true
  alerting: true
  reporting: true
  analytics: true
```
### **Best Practices**

#### **1. Configuration Management**
- Use a single configuration file for all settings
- Implement proper validation and error checking
- Use environment variables for dynamic overrides
- Regular configuration validation and testing
- Version control configuration files

#### **2. Environment Variable Usage**
- Use consistent naming conventions
- Document all supported environment variables
- Implement proper override priority
- Validate environment variable values
- Clear documentation of override behavior

#### **3. Configuration Validation**
- Implement comprehensive schema validation
- Check for configuration conflicts
- Validate file paths and dependencies
- Regular configuration integrity checks
- Automated validation in CI/CD

#### **4. Integration and Automation**
- Integrate with build systems (CMake, Make)
- Support CI/CD pipeline integration
- Implement configuration reloading
- Support dynamic configuration updates
- Provide configuration testing tools

---

**Navigation**: [← Back to Configuration](configuration/) | [Home](/)

