#!/bin/bash
# ESP32 Local Development Setup Script
# This script sets up a complete ESP32 development environment on your machine

set -e  # Exit on any error

# Set setup mode for colored output
export SETUP_MODE="local"

# Get the directory where this script is located
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Show help if requested
if [ "$1" = "--help" ] || [ "$1" = "-h" ]; then
    echo "ESP32 Local Development Setup Script"
    echo ""
    echo "Usage: ./setup_repo.sh [OPTIONS]"
    echo ""
    echo "OPTIONS:"
    echo "  --help, -h          Show this help message"
    echo ""
    echo "PURPOSE:"
    echo "  Set up complete ESP32 development environment on your machine"
    echo ""
    echo "WHAT IT INSTALLS:"
    echo "  • System dependencies (build tools, libraries, development packages)"
    echo "  • Clang-20 toolchain (compiler, formatter, analyzer, tools)"
    echo "  • ESP-IDF v5.5 (ESP32 development framework with tools)"
    echo "  • Python dependencies (PyYAML, pip packages)"
    echo "  • yq (YAML processor for configuration parsing)"
    echo "  • Development aliases and environment variables"
    echo "  • ESP-IDF environment setup and configuration"
    echo ""
    echo "WHAT IT DOES:"
    echo "  • Detects your OS automatically (Ubuntu, Fedora, CentOS, macOS)"
    echo "  • Installs all required system dependencies via package manager"
    echo "  • Downloads and configures ESP-IDF development framework"
    echo "  • Sets up Clang toolchain with latest compiler tools"
    echo "  • Configures your development environment and PATH"
    echo "  • Adds useful aliases to ~/.bashrc for easy access"
    echo "  • Verifies the complete installation"
    echo "  • Optimizes environment for local development"
    echo ""
    echo "REQUIREMENTS:"
    echo "  • sudo access for package installation"
    echo "  • Internet connection for downloads and package updates"
    echo "  • Regular user account (not root user)"
    echo "  • At least 2GB free disk space for ESP-IDF"
    echo "  • Supported operating systems: Ubuntu, Fedora, CentOS, macOS"
    echo ""
    echo "INSTALLATION PROCESS:"
    echo "  1. System dependency installation (build tools, libraries)"
    echo "  2. Clang toolchain setup (compiler, formatter, analyzer)"
    echo "  3. ESP-IDF download and installation"
    echo "  4. Python environment setup and package installation"
    echo "  5. Environment configuration and PATH setup"
    echo "  6. Development alias creation"
    echo "  7. Installation verification and testing"
    echo ""
    echo "POST-INSTALLATION:"
    echo "  • Restart your terminal or run: source ~/.bashrc"
    echo "  • Navigate to examples/esp32 directory"
    echo "  • Run: get_idf (to activate ESP-IDF environment)"
    echo "  • Build apps with: ./scripts/build_app.sh <app_type> <build_type>"
    echo "  • Flash apps with: ./scripts/flash_app.sh <operation> <app_type> <build_type>"
    echo ""
    echo "ALTERNATIVES:"
    echo "  • For CI builds, use ESP-IDF CI action directly (no setup needed)"
    echo "  • For shared functions only, use: source ./setup_common.sh"
    echo ""
    echo "TROUBLESHOOTING:"
    echo "  • If installation fails, check: sudo access, internet connection, disk space"
    echo "  • For ESP-IDF issues, see: https://docs.espressif.com/projects/esp-idf/"
    echo "  • For script issues, check: docs/README_UTILITY_SCRIPTS.md"
    echo ""
    echo "For CI builds, use ESP-IDF CI action directly (no setup needed)"
    echo "For detailed information, see: docs/README_UTILITY_SCRIPTS.md"
    exit 0
fi

# Source the common setup functions
source "$SCRIPT_DIR/setup_common.sh"

# Main setup function for local development
main() {
    echo "================================================================"
    echo "🚀 ESP32 HardFOC Interface Wrapper - Local Development Setup"
    echo "================================================================"
    echo ""
    
    # Check if running as root
    if [[ $EUID -eq 0 ]]; then
        print_error "This script should not be run as root. Please run as a regular user."
        exit 1
    fi
    
    # Detect OS and inform user
    local os=$(detect_os)
    print_status "Detected operating system: $os"
    
    # Check for sudo access
    if ! sudo -n true 2>/dev/null; then
        print_warning "This script requires sudo access to install packages."
        print_status "You may be prompted for your password during installation."
        echo ""
    fi
    
    # Confirm with user
    echo "This script will install:"
    echo "  • System dependencies (build tools, libraries)"
    echo "  • Clang-20 toolchain (compiler, formatter, analyzer)"
    echo "  • ESP-IDF v5.5 (ESP32 development framework)"
    echo "  • Python dependencies (PyYAML)"
    echo "  • yq (YAML processor)"
    echo "  • Development aliases and environment variables"
    echo ""
    
    read -p "Do you want to continue? (y/N): " -n 1 -r
    echo ""
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        print_status "Setup cancelled by user."
        exit 0
    fi
    
    echo ""
    print_status "Starting installation..."
    echo ""
    
    # Install all components
    install_system_deps
    echo ""
    
    install_clang_tools
    echo ""
    
    install_yq
    echo ""
    
    install_esp_idf
    echo ""
    
    install_python_deps
    echo ""
    
    setup_local_environment
    echo ""
    
    # Verify everything is working
    verify_installation
    echo ""
    
    echo "================================================================"
    print_success "🎉 Local development environment setup complete!"
    echo "================================================================"
    echo ""
    print_status "Next steps:"
    print_status "1. Restart your terminal or run: source ~/.bashrc"
    print_status "2. Navigate to the examples/esp32 directory"
    print_status "3. Run: get_idf"
    print_status "4. Build apps with: ./scripts/build_app.sh <app_type> <build_type>"
    print_status "5. List available apps with: ./scripts/build_app.sh list"
    echo ""
    print_status "Useful aliases have been added to your ~/.bashrc:"
    print_status "  • build_app - Build apps from anywhere"
    print_status "  • flash_app - Flash apps to ESP32"
    print_status "  • list_apps - Show all available apps"
    echo ""
    print_status "Happy coding! 🚀"
}

# Run main function
main "$@"
