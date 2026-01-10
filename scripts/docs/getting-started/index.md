---
layout: default
title: "Getting Started"
description: "Quick start guide for the HardFOC ESP-IDF Project Tools"
nav_order: 1
parent: "📚 Documentation"
has_children: true
permalink: /docs/getting-started/
---

# Getting Started with HardFOC ESP-IDF Project Tools

Welcome to the HardFOC ESP-IDF Project Tools! This guide will help you **integrate these development scripts into your existing ESP-IDF project** for enhanced multi-application build management and development workflows.

> **📋 Important**: These tools are designed to be **integrated into your ESP-IDF project**, not used as standalone applications.

## 🚀 Integration Methods

Choose the best method to integrate these tools into your ESP-IDF project:

### Method 1: Submodule Integration (Recommended)

**Best for**: Version-controlled projects with team collaboration

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

**Best for**: Quick setup and experimentation

```bash
# Navigate to your ESP-IDF project
cd your-esp-idf-project

# Clone tools directly
git clone https://github.com/n3b3x/hf-espidf-project-tools.git scripts

# Start building
./scripts/build_app.sh your_app Release
```

### Method 3: Shared Tools Directory

**Best for**: Multiple projects using the same tools

```bash
# Create shared tools directory
mkdir -p ~/shared-esp32-tools
cd ~/shared-esp32-tools
git clone https://github.com/n3b3x/hf-espidf-project-tools.git .

# In each ESP-IDF project, use --project-path
cd your-esp-idf-project
~/shared-esp32-tools/build_app.sh --project-path . your_app Release
```

### Method 4: Automated Project Setup

**Best for**: Creating new ESP-IDF projects with tools pre-integrated

```bash
# Clone the tools repository
git clone https://github.com/n3b3x/hf-espidf-project-tools.git
cd hf-espidf-project-tools

# Create a complete ESP-IDF project with tools
./setup_basic.sh my-awesome-project

# Navigate to your new project
cd my-awesome-project

# Build and flash
./scripts/build_app.sh main_app Release
./scripts/flash_app.sh flash main_app Release
```

**That's it!** You now have a complete ESP-IDF project with:
- ✅ ESP-IDF installed and configured
- ✅ Build system set up
- ✅ `app_config.yml` with basic app
- ✅ CMakeLists.txt files
- ✅ Basic `main.cpp` template
- ✅ All development scripts ready to use

### Option 2: Add to Existing Project

If you already have an ESP-IDF project:

```bash
# Navigate to your ESP-IDF project
cd your-esp-idf-project

# Add tools as submodule
git submodule add https://github.com/n3b3x/hf-espidf-project-tools.git scripts

# Create app_config.yml (see Configuration guide)
# Set up CMakeLists.txt files
# Start building!
```

## 📋 Prerequisites

Before you begin, ensure you have:

- **Git** installed on your system
- **Internet connection** for downloading ESP-IDF
- **Write permissions** in your working directory
- **Basic understanding** of ESP-IDF development (for existing projects)

## 🛠️ What You Get

### Complete Project Structure
```
my-esp-idf-project/
├── main/
│   ├── main.cpp              # Your application code
│   └── CMakeLists.txt        # Build configuration
├── scripts/                  # Development tools
│   ├── build_app.sh          # Build applications
│   ├── flash_app.sh          # Flash to ESP32
│   └── ...                   # Other utilities
├── app_config.yml           # App configuration
├── CMakeLists.txt           # Project configuration
└── README.md                # Project documentation
```

### Ready-to-Use Commands
- **Build apps:** `./scripts/build_app.sh <app_type> <build_type>`
- **Flash to ESP32:** `./scripts/flash_app.sh flash <app_type> <build_type>`
- **Monitor output:** `./scripts/flash_app.sh monitor`
- **List apps:** `./scripts/build_app.sh list`
- **List ESP32 devices:** `./scripts/flash_app.sh ports`
- **Flash to specific device:** `./scripts/flash_app.sh --port /dev/ttyACM1 flash <app_type> <build_type>`

## 🎯 Next Steps

Once you have a project set up:

1. **[Project Setup](project-setup/)** - Learn about automated project creation
2. **[Build System](build-system/)** - Master the build system
3. **[Configuration](configuration/)** - Configure your applications
4. **[CI Pipeline](ci-pipeline/)** - Set up automated builds
5. **[Advanced Topics](advanced/multi-version-idf/)** - Explore advanced features

## 🆘 Need Help?

- **[Troubleshooting](troubleshooting/)** - Common issues and solutions
- **[Examples](examples/)** - Real-world usage examples
- **[GitHub Issues](https://github.com/n3b3x/hf-espidf-project-tools/issues)** - Report bugs or request features

## 🎯 What's Next?

- **Customize workflows** for your specific needs
- **Add security scanning** to your pipeline
- **Set up automated documentation** updates
- **Configure multi-version testing** across ESP-IDF versions

---

**Ready to dive deeper?** Check out our [Build System Documentation](build-system/) or explore [Advanced Topics](advanced/multi-version-idf/) for power users.
