---
layout: default
title: "📚 Documentation"
description: "Development Scripts for HardFOC ESP-IDF Projects - Multi-application build system with intelligent configuration management for ESP-IDF development"
nav_order: 1
has_children: true
permalink: /docs/
---

# HardFOC ESP-IDF Project Tools

Welcome to the **HardFOC ESP-IDF Project Tools** - a comprehensive suite of development scripts designed to be **integrated into your existing ESP-IDF projects**. These tools enhance your ESP-IDF development workflow with multi-application build management, intelligent configuration, and automated development processes.

> **📋 Important**: These tools are meant to be **integrated into your ESP-IDF project**, not used as standalone applications. They work alongside your existing ESP-IDF project structure.

## 🚀 Integration Methods

Choose the best integration method for your ESP-IDF project:

### Method 1: Submodule Integration (Recommended)
```bash
# Navigate to your ESP-IDF project
cd your-esp-idf-project

# Add tools as submodule
git submodule add https://github.com/n3b3x/hf-espidf-project-tools.git scripts

# Your project structure:
# your-esp-idf-project/
# ├── CMakeLists.txt
# ├── app_config.yml
# ├── main/
# └── scripts/                    # Project tools
#     ├── build_app.sh
#     └── flash_app.sh

# Start building
./scripts/build_app.sh your_app Release
```

### Method 2: Direct Clone Integration
```bash
# Navigate to your ESP-IDF project
cd your-esp-idf-project

# Clone tools directly
git clone https://github.com/n3b3x/hf-espidf-project-tools.git scripts

# Your project structure:
# your-esp-idf-project/
# ├── CMakeLists.txt
# ├── app_config.yml
# ├── main/
# └── scripts/                    # Project tools
#     ├── build_app.sh
#     └── flash_app.sh

# Start building
./scripts/build_app.sh your_app Release
```

### Method 3: Shared Tools Directory
```bash
# For multiple projects, use shared tools
mkdir -p ~/shared-esp32-tools
cd ~/shared-esp32-tools
git clone https://github.com/n3b3x/hf-espidf-project-tools.git .

# In each ESP-IDF project, use --project-path
cd your-esp-idf-project
~/shared-esp32-tools/build_app.sh --project-path . your_app Release
```

## ✨ Key Features

- **🔧 Multi-Application Build System** - Build multiple applications from a single ESP-IDF project
- **📋 Intelligent Configuration** - YAML-based configuration management with `app_config.yml`
- **📱 ESP32 Flash System** - Automated port detection and firmware flashing
- **🔌 Multi-Device Support** - Intelligent handling of multiple ESP32 devices with interactive selection
- **📊 Comprehensive Logging** - Detailed build and development logs
- **🔍 Port Detection** - Cross-platform ESP32 device identification with device info display
- **📚 Multi-Version ESP-IDF** - Support for multiple ESP-IDF versions
- **🛠️ Development Scripts** - Build, flash, and utility management tools

## 📖 Documentation

Explore our comprehensive documentation:

- **[Getting Started](/docs/getting-started/)** - Quick start guide and installation
- **[Build System](/docs/build-system/)** - Multi-application build management
- **[Configuration](/docs/configuration/)** - YAML-based configuration system
- **[Flash System](/docs/flash-system/)** - ESP32 flashing and monitoring
- **[Logging System](/docs/logging-system/)** - Log management and analysis
- **[CI Integration Patterns](/docs/ci-integration-patterns/)** - CI/CD integration patterns
- **[Advanced Topics](/docs/advanced/multi-version-idf/)** - Advanced features and utilities

## 📁 Project Structure

### Typical ESP-IDF Project with Tools
```
your-esp-idf-project/
├── CMakeLists.txt              # ESP-IDF project root
├── app_config.yml              # Multi-app configuration
├── main/                       # Your main application
│   ├── main.cpp
│   └── CMakeLists.txt
├── components/                 # Your components
│   └── your-component/
└── scripts/                    # Project tools (integrated)
    ├── build_app.sh
    ├── flash_app.sh
    ├── generate_matrix.py
    └── config_loader.sh
```

### CI/CD Integration Patterns

#### Pattern 1: Submodule Setup (Recommended)
```yaml
# .github/workflows/build.yml
jobs:
  build:
    uses: N3b3x/hf-espidf-ci-tools/.github/workflows/ru-build.yml@v1
    with:
      project_dir: examples/esp32
      project_tools_dir: examples/esp32/scripts  # Submodule path
```

#### Pattern 2: Shared Tools
```yaml
# .github/workflows/build.yml
jobs:
  build:
    uses: N3b3x/hf-espidf-ci-tools/.github/workflows/ru-build.yml@v1
    with:
      project_dir: firmware/esp32
      project_tools_dir: build-tools  # Shared tools directory
```

## 🎯 Perfect For

- **Multi-Application ESP-IDF Projects** - Build multiple test applications from a single project
- **Library Testing** - Test different aspects of your library with separate applications
- **Development Workflows** - Streamlined build and flash processes for development
- **Team Development** - Consistent development environments across teams
- **CI/CD Integration** - Automated builds and testing in GitHub Actions

## 🤝 Contributing

We welcome contributions! See our [Contributing Guide](/docs/contributing/) for details.

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](https://github.com/n3b3x/hf-espidf-project-tools/blob/main/LICENSE) file for details.

---

**Ready to get started?** Check out our [Quick Start Guide](/docs/getting-started/) or browse the [Examples](/docs/examples/) to see the tools in action!
