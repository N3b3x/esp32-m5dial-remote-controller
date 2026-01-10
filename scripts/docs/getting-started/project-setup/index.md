---
layout: default
title: "Project Setup"
description: "Automated ESP-IDF project setup with hf-espidf-project-tools"
nav_order: 3
parent: "Getting Started"
permalink: /docs/getting-started/project-setup/
---

# 🚀 Project Setup

The `setup_basic.sh` script automates the creation of a complete ESP-IDF project with all necessary configuration files, build system setup, and the tools repository integration.

## Quick Setup

Create a new ESP-IDF project in seconds:

```bash
# Basic setup with defaults
./setup_basic.sh

# Custom project name
./setup_basic.sh my-awesome-project

# Custom project name and ESP-IDF version
./setup_basic.sh my-awesome-project release/v5.4
```

## What the Script Does

The `setup_basic.sh` script performs a complete project initialization:

### 1. **Project Structure Creation**
- Creates project directory with proper structure
- Initializes git repository
- Adds `hf-espidf-project-tools` as submodule

### 2. **ESP-IDF Installation**
- Installs specified ESP-IDF version (default: `release/v5.5`)
- Sets up ESP-IDF environment and tools
- Configures for ESP32-C6 target

### 3. **Build System Setup**
- Creates project root `CMakeLists.txt`
- Creates main component `CMakeLists.txt`
- Sets up proper ESP-IDF integration

### 4. **Configuration Files**
- Generates `app_config.yml` with basic `main_app`
- Creates `.gitignore` for ESP-IDF projects
- Sets up project-specific README.md

### 5. **Template Code**
- Creates basic `main.cpp` with ESP-IDF template
- Includes proper logging and FreeRTOS setup
- Ready-to-build example code

## Usage Examples

### Basic Setup
```bash
# Create project with default settings
./setup_basic.sh
# Creates: my-esp-idf-project/ with ESP-IDF v5.5
```

### Custom Project Name
```bash
# Create project with custom name
./setup_basic.sh temperature-monitor
# Creates: temperature-monitor/ with ESP-IDF v5.5
```

### Custom ESP-IDF Version
```bash
# Create project with specific ESP-IDF version
./setup_basic.sh my-project release/v5.4
# Creates: my-project/ with ESP-IDF v5.4
```

### Short Version Format
```bash
# Use short version format
./setup_basic.sh my-project v5.5
# Automatically converts to: release/v5.5
```

## Generated Project Structure

After running `setup_basic.sh`, you'll have a complete project structure:

```
my-esp-idf-project/
├── main/
│   ├── main.cpp              # Basic ESP-IDF application template
│   └── CMakeLists.txt        # Main component build configuration
├── scripts/                  # Development tools (submodule)
│   ├── build_app.sh          # Build script
│   ├── flash_app.sh          # Flash script
│   ├── setup_common.sh       # Common setup functions
│   └── ...                   # Other utility scripts
├── app_config.yml           # App configuration (with main_app)
├── CMakeLists.txt           # Project root build configuration
├── .gitignore               # Git ignore rules for ESP-IDF
└── README.md                # Project documentation
```

## Generated Configuration

### app_config.yml
The script creates a basic `app_config.yml` with:

```yaml
---
version: "1.0"
metadata:
  project: "your-project-name"
  default_app: "main_app"
  target: "esp32c6"
  idf_versions: ["release/v5.5"]  # or your chosen version
  build_types: [["Debug", "Release"]]

apps:
  main_app:
    description: "Basic main application template"
    source_file: "main.cpp"
    category: "basic"
    idf_versions: ["release/v5.5"]
    build_types: ["Debug", "Release"]
    ci_enabled: true
    featured: true
---
```

### CMakeLists.txt Files
- **Project root**: Validates build parameters and integrates with ESP-IDF
- **Main component**: Handles source file compilation and dependencies

### main.cpp Template
```cpp
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "main";

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "Hello from ESP-IDF!");
    ESP_LOGI(TAG, "This is a basic ESP-IDF project template");
    
    // Your application code goes here
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        ESP_LOGI(TAG, "Running...");
    }
}
```

## Next Steps

After running `setup_basic.sh`:

### 1. **Navigate to Project**
```bash
cd your-project-name
```

### 2. **Build the Project**
```bash
./scripts/build_app.sh main_app Release
```

### 3. **Flash to ESP32**
```bash
./scripts/flash_app.sh flash main_app Release
```

### 4. **Monitor Output**
```bash
./scripts/flash_app.sh monitor
```

## Supported ESP-IDF Versions

The script supports all major ESP-IDF versions:

- **release/v5.5** (default, recommended)
- **release/v5.4**
- **release/v5.3**
- **v5.5, v5.4, v5.3** (short form)

## Requirements

- **Git**: Installed and configured
- **Internet Connection**: For downloading ESP-IDF and tools
- **Write Permissions**: In the current directory
- **Linux/macOS**: Script is designed for Unix-like systems

## Troubleshooting

### Project Directory Already Exists
```bash
ERROR: Project directory 'my-project' already exists!
```
**Solution**: Choose a different project name or remove the existing directory.

### ESP-IDF Installation Failed
```bash
ERROR: Failed to install ESP-IDF version: release/v5.5
```
**Solution**: Check internet connection and available disk space. The script will show available versions.

### Permission Denied
```bash
Permission denied: ./setup_basic.sh
```
**Solution**: Make the script executable:
```bash
chmod +x setup_basic.sh
```

## Advanced Usage

### Custom ESP-IDF Versions
You can specify any valid ESP-IDF version:

```bash
# Use specific release
./setup_basic.sh my-project release/v5.4

# Use development branch
./setup_basic.sh my-project master

# Use specific tag
./setup_basic.sh my-project v5.5.0
```

### Integration with Existing Projects
If you already have an ESP-IDF project, you can still add the tools:

```bash
# Navigate to existing project
cd existing-project

# Add tools as submodule
git submodule add https://github.com/n3b3x/hf-espidf-project-tools.git scripts

# Create app_config.yml manually
# (See Configuration documentation for details)
```

## Script Help

View all available options:

```bash
./setup_basic.sh --help
```

This will show:
- Usage examples
- Supported ESP-IDF versions
- Requirements and troubleshooting
- Detailed explanation of what the script does

---

**Ready to create your first project?** Run `./setup_basic.sh` and start building! 🚀
