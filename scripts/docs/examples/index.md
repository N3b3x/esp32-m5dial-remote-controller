---
layout: default
title: "Examples"
description: "Real-world examples and usage patterns for HardFOC ESP-IDF CI Tools"
nav_order: 2
parent: "📚 Reference"
permalink: /docs/examples/
---

# Examples

This section provides real-world examples and usage patterns for the HardFOC ESP-IDF CI Tools.

## 🚀 Quick Examples

### Basic ESP-IDF Project

A simple ESP-IDF project with GPIO testing:

```yaml
# examples/esp32/app_config.yml
metadata:
  default_app: "gpio_test"
  default_build_type: "Release"
  target: "esp32c6"
  idf_versions: ["release/v5.5"]

apps:
  gpio_test:
    description: "GPIO peripheral testing application"
    source_file: "GpioTest.cpp"
    category: "peripheral"
    build_types: ["Debug", "Release"]
    idf_versions: ["release/v5.5"]
    ci_enabled: true
    featured: true
```

### Multi-Version Testing

Testing across multiple ESP-IDF versions:

```yaml
metadata:
  idf_versions: ["release/v5.5", "release/v5.4"]
  default_build_types: [["Debug", "Release"], ["Debug"]]

apps:
  sensor_app:
    idf_versions: ["release/v5.5", "release/v5.4"]
    build_types: ["Debug", "Release"]
  legacy_app:
    idf_versions: ["release/v5.4"]
    build_types: ["Debug"]
```

## 🔧 Workflow Examples

### Basic Build Workflow

```yaml
name: Build ESP32 Applications

on:
  push:
    branches: [ main ]
  pull_request:
    branches: [ main ]

jobs:
  build:
    runs-on: ubuntu-latest
    strategy:
      matrix:
        app: ["gpio_test", "sensor_app"]
        build_type: ["Debug", "Release"]
        idf_version: ["release/v5.5"]
    
    steps:
    - uses: actions/checkout@v4
    
    - name: Set Project Path
      run: echo "ESP32_PROJECT_PATH=examples/esp32" >> $GITHUB_ENV
    
    - name: Build Application
      uses: espressif/esp-idf-ci-action@v1
      with:
        esp_idf_version: ${{ matrix.idf_version }}
        target: esp32c6
        path: examples/esp32
```

### Security Workflow

```yaml
name: Security Audit

on:
  push:
    branches: [ main ]
  pull_request:
    branches: [ main ]
  schedule:
    - cron: '0 2 * * 1'  # Weekly

jobs:
  security:
    runs-on: ubuntu-latest
    steps:
    - uses: actions/checkout@v4
    
    - name: Run Security Audit
      uses: espressif/esp-idf-ci-action@v1
      with:
        esp_idf_version: "release/v5.5"
        target: esp32c6
        path: examples/esp32
        extra_components: |
          - name: security_audit
            path: .github/security
```

## 📊 Advanced Examples

### Custom Build Types

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
      defines: ["PROFILING", "PERFORMANCE_MONITORING"]
    Test:
      cmake_build_type: "Debug"
      optimization: "-O1"
      debug_level: "-g2"
      defines: ["TESTING", "MOCK_HARDWARE"]
```

### Environment-Specific Configuration

```yaml
# Development environment
metadata:
  environment: "development"
  debug_enabled: true
  verbose_logging: true

# Production environment
metadata:
  environment: "production"
  debug_enabled: false
  verbose_logging: false
  security_audit: true
```

### CI/CD Pipeline with Artifacts

```yaml
- name: Build and Upload Artifacts
  uses: espressif/esp-idf-ci-action@v1
  with:
    esp_idf_version: ${{ matrix.idf_version }}
    target: esp32c6
    path: examples/esp32

- name: Upload Firmware
  uses: actions/upload-artifact@v3
  with:
    name: firmware-${{ matrix.app }}-${{ matrix.build_type }}
    path: examples/esp32/build/*.bin

- name: Upload Build Logs
  uses: actions/upload-artifact@v3
  with:
    name: logs-${{ matrix.app }}-${{ matrix.build_type }}
    path: examples/esp32/build/log/
```

## 🎯 Use Case Examples

### IoT Sensor Node

```yaml
apps:
  sensor_node:
    description: "IoT sensor node with WiFi and MQTT"
    source_file: "SensorNode.cpp"
    category: "iot"
    build_types: ["Debug", "Release"]
    idf_versions: ["release/v5.5"]
    features: ["wifi", "mqtt", "sensors"]
    ci_enabled: true
```

### Industrial Controller

```yaml
apps:
  industrial_controller:
    description: "Industrial control system with Modbus"
    source_file: "IndustrialController.cpp"
    category: "industrial"
    build_types: ["Release"]
    idf_versions: ["release/v5.5"]
    features: ["modbus", "ethernet", "safety"]
    ci_enabled: true
    security_audit: true
```

### Educational Project

```yaml
apps:
  learning_project:
    description: "Educational ESP-IDF project for students"
    source_file: "LearningProject.cpp"
    category: "education"
    build_types: ["Debug"]
    idf_versions: ["release/v5.5"]
    features: ["tutorial", "examples", "documentation"]
    ci_enabled: true
```

## 🔌 Multi-Device Examples

### Listing Available Devices

```bash
# List all connected ESP32 devices with details
./scripts/flash_app.sh ports

# Example output:
=== Available ESP32 Serial Ports ===

Found 2 device(s):

  [1] /dev/ttyACM0
      Device: Espressif USB_JTAG_serial_debug_unit (S/N: FC:01:2C:FF:E4:DC)
      Status: ✓ accessible

  [2] /dev/ttyUSB0
      Device: CP2102N USB_to_UART_Bridge_Controller
      Status: ✓ accessible
```

### Flashing to Specific Device

```bash
# Flash to specific device using --port flag
./scripts/flash_app.sh --port /dev/ttyACM1 flash gpio_test Release

# Use short form -p
./scripts/flash_app.sh -p /dev/ttyUSB0 flash_monitor sensor_app Debug

# Monitor specific device
./scripts/flash_app.sh -p /dev/ttyACM0 monitor --log debug_session
```

### Multi-Device Deployment Script

```bash
#!/bin/bash
# deploy_all.sh - Flash firmware to all connected devices

# List and verify devices first
./scripts/flash_app.sh ports

# Flash to each device
for port in /dev/ttyACM0 /dev/ttyACM1; do
    echo "Flashing to $port..."
    ./scripts/flash_app.sh --port "$port" flash gpio_test Release --log "deploy_${port##*/}"
done
```

### CI/CD with Explicit Port

```yaml
# For CI environments - always specify port to avoid interactive prompts
- name: Flash Firmware
  run: |
    cd examples/esp32
    # Use first available device
    PORT=$(ls /dev/ttyACM* 2>/dev/null | head -1)
    ./scripts/flash_app.sh --port $PORT flash gpio_test Release
```

## 🔍 Testing Examples

### Unit Testing

```yaml
- name: Run Unit Tests
  run: |
    cd examples/esp32
    ./scripts/build_app.sh test_app Test
    ./scripts/run_tests.sh
```

### Integration Testing

```yaml
- name: Integration Tests
  run: |
    cd examples/esp32
    ./scripts/build_app.sh integration_test Release
    ./scripts/flash_app.sh integration_test
    ./scripts/monitor_app.sh integration_test
```

### Performance Testing

```yaml
- name: Performance Tests
  run: |
    cd examples/esp32
    ./scripts/build_app.sh performance_test Profile
    ./scripts/benchmark.sh performance_test
```

## 📚 More Examples

### Project Templates

*Note: Example project templates will be added in future releases. For now, use the configuration examples above to set up your own projects.*

### Workflow Templates

*Note: Example workflow templates will be added in future releases. Use the workflow examples in the code blocks above as starting points.*

## 🆘 Need Help?

- **[Getting Started](/docs/getting-started/)** - Basic setup guide
- **[Configuration](/docs/configuration/)** - Configuration management
- **[Troubleshooting](/docs/troubleshooting/)** - Common issues and solutions
- **[GitHub Issues](https://github.com/n3b3x/hf-espidf-project-tools/issues)** - Report bugs or request features

---

**Found an example you like?** Copy the configuration and adapt it for your project. Need help customizing? Check out our [Configuration Guide](/docs/configuration/) or [Troubleshooting](/docs/troubleshooting/) section.
