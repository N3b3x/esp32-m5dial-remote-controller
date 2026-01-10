---
layout: default
title: "🔧 Core Systems"
description: "Core development systems for ESP-IDF projects"
nav_order: 2
has_children: true
permalink: /docs/core-systems/
---

# 🔧 Core Systems

The core systems provide the fundamental functionality for ESP-IDF development workflows.

## Available Systems

- **[Build System](/docs/build-system/)** - Multi-application build management
- **[Configuration System](/docs/configuration/)** - YAML-based configuration management
- **[Flash System](/docs/flash-system/)** - ESP32 flashing, monitoring, and multi-device support
- **[Logging System](/docs/logging-system/)** - Log management and analysis

## Quick Start

Each system is designed to work independently or together:

1. **Configure** your project with the Configuration System
2. **Build** your applications with the Build System
3. **Flash** firmware with the Flash System
4. **Monitor** logs with the Logging System

## Integration

All core systems share the same configuration file (`app_config.yml`) and work seamlessly together to provide a complete development experience.
