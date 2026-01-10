---
layout: default
title: "📚 Reference"
description: "Reference materials, examples, and troubleshooting"
nav_order: 5
has_children: true
permalink: /docs/reference/
---

# 📚 Reference

Reference materials, examples, and troubleshooting guides for the HardFOC ESP-IDF Project Tools.

## Available Resources

- **[Features](/docs/features/)** - Complete feature overview and capabilities
- **[Examples](/docs/examples/)** - Real-world usage examples and patterns
- **[Troubleshooting](/docs/troubleshooting/)** - Common issues and solutions
- **[Contributing](/docs/contributing/)** - How to contribute to the project

## Quick Reference

### Common Commands
```bash
# Build an application
./scripts/build_app.sh <app_type> <build_type>

# Flash to ESP32
./scripts/flash_app.sh flash <app_type> <build_type>

# Monitor output
./scripts/flash_app.sh monitor

# List available ESP32 devices
./scripts/flash_app.sh ports

# Flash to specific device (when multiple connected)
./scripts/flash_app.sh --port /dev/ttyACM1 flash <app_type> <build_type>
./scripts/flash_app.sh -p /dev/ttyUSB0 monitor
```

### Configuration
- **Main config**: `app_config.yml`
- **Build config**: `build_config` section
- **Flash config**: `flash_config` section

## Getting Help

- **Issues**: [GitHub Issues](https://github.com/n3b3x/hf-espidf-project-tools/issues)
- **Discussions**: [GitHub Issues](https://github.com/n3b3x/hf-espidf-project-tools/issues) (Discussions not enabled)
- **Documentation**: This reference section

## Examples by Use Case

- **Basic ESP32 Project**: See [Examples](/docs/examples/) for simple setups
- **Multi-Application Build**: Check [Build System](/docs/build-system/) documentation
- **CI/CD Integration**: Review [CI Pipeline](/docs/ci-pipeline/) guide
- **Advanced Configuration**: Explore [Advanced Topics](/docs/advanced-topics/)
