#!/bin/bash
# Common Setup Functions for ESP32 HardFOC Interface Wrapper
# This script contains shared functions used by both local and CI setup scripts

set -e  # Exit on any error

# Show help if requested (only when run directly, not when sourced)
if [[ "${BASH_SOURCE[0]}" == "${0}" ]] && ([ "$1" = "--help" ] || [ "$1" = "-h" ]); then
    echo "ESP32 HardFOC Interface Wrapper - Common Setup Functions"
    echo ""
    echo "Usage: source ./setup_common.sh [OPTIONS]"
    echo ""
    echo "OPTIONS:"
    echo "  --help, -h                  - Show this help message"
    echo ""
    echo "PURPOSE:"
    echo "  This script contains shared functions used by both local and CI setup scripts"
    echo ""
    echo "USAGE:"
    echo "  This script is designed to be sourced by other scripts, not run directly:"
    echo "  source ./setup_common.sh"
    echo ""
    echo "AVAILABLE FUNCTIONS:"
    echo "  # System dependency installation"
    echo "  install_system_deps         - Install system dependencies for detected OS"
    echo "  detect_os                   - Detect operating system automatically"
    echo ""
    echo "  # Clang toolchain installation"
    echo "  install_clang_tools         - Install clang-20 and development tools"
    echo "  install_yq                  - Install yq YAML processor"
    echo ""
    echo "  # ESP-IDF management"
    echo "  install_esp_idf             - Install ESP-IDF versions from configuration"
    echo "  export_esp_idf_version      - Export ESP-IDF environment for specific version"
    echo "  install_esp_idf_version     - Install specific ESP-IDF version"
    echo "  list_esp_idf_versions       - List installed ESP-IDF versions"
    echo ""
    echo "  # Python dependency management"
    echo "  install_python_deps         - Install Python packages and dependencies"
    echo ""
    echo "  # Local development environment"
    echo "  setup_environment_vars      - Setup environment variables and PATH"
    echo "  setup_local_environment     - Configure local development environment"
    echo "  verify_installation         - Verify complete installation"
    echo ""
    echo "  # CI-specific functions"
    echo "  ci_setup_environment        - Setup CI-specific environment"
    echo "  ci_optimize_cache           - Optimize cache for CI environments"
    echo "  ci_check_cache_status       - Check cache status and statistics"
    echo "  ci_prepare_build_directory  - Prepare build directory for CI builds"
    echo "  ci_setup_and_build_project  - Setup and build project for CI"
    echo "  ci_display_build_info       - Display build information and results"
    echo ""
    echo "SUPPORTED OPERATING SYSTEMS:"
    echo "  • Ubuntu: apt-get package management"
    echo "  • Fedora: dnf package management"
    echo "  • CentOS: yum package management"
    echo "  • macOS: Homebrew package management"
    echo ""
    echo "ESP-IDF VERSION SUPPORT:"
    echo "  • release/v5.5: Latest stable release (recommended)"
    echo "  • release/v5.4: Previous stable release"
    echo "  • release/v5.3: Enhanced ESP32-C6 support"
    echo "  • release/v5.2: Improved toolchain and debugging"
    echo "  • release/v5.1: Enhanced performance and security"
    echo "  • release/v5.0: Stable release with modern features"
    echo "  • v4.4: Legacy support for older projects"
    echo ""
    echo "TARGET SUPPORT:"
    echo "  • ESP32-C6: Primary target with full feature support"
    echo "  • ESP32-S3: Secondary target for compatibility"
    echo "  • ESP32: Legacy target support"
    echo ""
    echo "INSTALLATION LOCATIONS:"
    echo "  • ESP-IDF versions: ~/esp/esp-idf-{version}"
    echo "  • Default symlink: ~/esp/esp-idf"
    echo "  • Tools: ~/.espressif/"
    echo "  • Python packages: ~/.espressif/python_env/"
    echo "  • Environment: ~/.bashrc, ~/.profile"
    echo ""
    echo "ENVIRONMENT VARIABLES:"
    echo "  • IDF_PATH: Path to ESP-IDF installation"
    echo "  • IDF_VERSION: Current ESP-IDF version"
    echo "  • IDF_TARGET: Target MCU architecture"
    echo "  • PATH: Updated with ESP-IDF tools"
    echo "  • SETUP_MODE: local or ci for output formatting"
    echo ""
    echo "FUNCTION CATEGORIES:"
    echo "  • System setup: OS detection, package installation"
    echo "  • Toolchain: Clang, yq, ESP-IDF tools"
    echo "  • Environment: PATH, aliases, configuration"
    echo "  • CI optimization: Cache management, build automation"
    echo "  • Verification: Installation testing and validation"
    echo ""
    echo "INTEGRATION:"
    echo "  • setup_repo.sh: Local development environment setup"
    echo "  • ESP-IDF CI action: Direct CI builds (no setup needed)"
    echo "  • build_app.sh: Application building with environment"
    echo "  • flash_app.sh: Device flashing with environment"
    echo ""
    echo "ERROR HANDLING:"
    echo "  • set -e: Exit on any error"
    echo "  • Function return codes for status checking"
    echo "  • Detailed error messages and troubleshooting"
    echo "  • Graceful fallbacks for missing components"
    echo ""
    echo "TROUBLESHOOTING:"
    echo "  • Installation failures: Check sudo access, internet, disk space"
    echo "  • ESP-IDF issues: Verify git, submodules, and tool installation"
    echo "  • Permission problems: Check user groups and file ownership"
    echo "  • Environment issues: Restart terminal or source ~/.bashrc"
    echo ""
    echo "For detailed information, see: docs/README_UTILITY_SCRIPTS.md"
    exit 0
fi

# =============================================================================
# SHARED UTILITY FUNCTIONS
# =============================================================================

# Colors for output (only used in local setup)
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to print colored output (only used in local setup)
print_status() {
    if [[ "${SETUP_MODE}" == "local" ]]; then
        echo -e "${BLUE}[INFO]${NC} $1"
    else
        echo "[INFO] $1"
    fi
}

print_success() {
    if [[ "${SETUP_MODE}" == "local" ]]; then
        echo -e "${GREEN}[SUCCESS]${NC} $1"
    else
        echo "[SUCCESS] $1"
    fi
}

print_warning() {
    if [[ "${SETUP_MODE}" == "local" ]]; then
        echo -e "${YELLOW}[WARNING]${NC} $1"
    else
        echo "[WARNING] $1"
    fi
}

print_error() {
    if [[ "${SETUP_MODE}" == "local" ]]; then
        echo -e "${RED}[ERROR]${NC} $1"
    else
        echo "[ERROR] $1"
    fi
}

# Backward-compatible alias used in some messages
print_info() {
    print_status "$1"
}

# Function to check if command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Function to detect OS
detect_os() {
    if [[ "$OSTYPE" == "linux-gnu"* ]]; then
        if command_exists apt-get; then
            echo "ubuntu"
        elif command_exists dnf; then
            echo "fedora"
        elif command_exists yum; then
            echo "centos"
        else
            echo "linux"
        fi
    elif [[ "$OSTYPE" == "darwin"* ]]; then
        echo "macos"
    else
        echo "unknown"
    fi
}

# =============================================================================
# SYSTEM DEPENDENCY INSTALLATION FUNCTIONS
# =============================================================================

# Function to install system dependencies
install_system_deps() {
    local os=$(detect_os)
    print_status "Installing system dependencies for $os..."
    
    case $os in
        "ubuntu")
            sudo apt-get update
            sudo apt-get install -y \
                build-essential \
                cmake \
                git \
                wget \
                curl \
                unzip \
                python3 \
                python3-pip \
                python3-venv \
                pkg-config \
                libusb-1.0-0-dev \
                libudev-dev
            ;;
        "fedora")
            sudo dnf update -y
            sudo dnf install -y \
                gcc \
                gcc-c++ \
                make \
                cmake \
                git \
                wget \
                curl \
                unzip \
                python3 \
                python3-pip \
                python3-devel \
                pkg-config \
                libusb1-devel \
                systemd-devel
            ;;
        "centos")
            sudo yum update -y
            sudo yum install -y \
                gcc \
                gcc-c++ \
                make \
                cmake \
                git \
                wget \
                curl \
                unzip \
                python3 \
                python3-pip \
                python3-devel \
                pkg-config \
                libusb1-devel \
                systemd-devel
            ;;
        "macos")
            if ! command_exists brew; then
                print_error "Homebrew not found. Please install Homebrew first:"
                print_error "  /bin/bash -c \"\$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)\""
                exit 1
            fi
            brew update
                brew install \
                    cmake \
                    git \
                    wget \
                    curl \
                    python3 \
                    pkg-config \
                libusb
            ;;
        *)
            print_error "Unsupported operating system: $os"
            exit 1
            ;;
    esac
    
    print_success "System dependencies installed"
}

# =============================================================================
# CLANG TOOLCHAIN INSTALLATION FUNCTIONS
# =============================================================================

# Function to install clang tools
install_clang_tools() {
    print_status "Installing clang tools..."
    
    local os=$(detect_os)
    
    case $os in
        "ubuntu")
            # Add LLVM APT repository for clang-20
            print_status "Adding LLVM APT repository for clang-20..."
            wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key | sudo apt-key add - 2>/dev/null || {
                # Fallback for newer systems that deprecated apt-key
                wget -qO- https://apt.llvm.org/llvm-snapshot.gpg.key | sudo tee /etc/apt/trusted.gpg.d/apt.llvm.org.asc
            }
            
            # Detect Ubuntu codename
            local ubuntu_codename=$(lsb_release -cs 2>/dev/null || echo "noble")
            echo "deb http://apt.llvm.org/${ubuntu_codename}/ llvm-toolchain-${ubuntu_codename}-20 main" | sudo tee /etc/apt/sources.list.d/llvm.list >/dev/null
            echo "deb-src http://apt.llvm.org/${ubuntu_codename}/ llvm-toolchain-${ubuntu_codename}-20 main" | sudo tee -a /etc/apt/sources.list.d/llvm.list >/dev/null
            
            # Update package lists
            sudo apt-get update
            
            # Install clang-20 and tools
            print_status "Installing clang-20 and tools..."
            sudo apt-get install -y \
                clang-20 \
                clang-format-20 \
                clang-tidy-20 \
                clang-tools-20 \
                libclang-common-20-dev \
                libclang-20-dev \
                libclang1-20 \
                cppcheck \
                valgrind \
                gdb \
                make
            
            # Set clang-20 as default
            if command_exists update-alternatives; then
                print_status "Setting clang-20 as default..."
                sudo update-alternatives --install /usr/bin/clang clang /usr/bin/clang-20 100
                sudo update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-20 100
                sudo update-alternatives --install /usr/bin/clang-format clang-format /usr/bin/clang-format-20 100
                sudo update-alternatives --install /usr/bin/clang-tidy clang-tidy /usr/bin/clang-tidy-20 100
            fi
            ;;
        "fedora"|"centos")
            if [[ "$os" == "fedora" ]]; then
            sudo dnf install -y \
                clang \
                clang-tools-extra \
                cppcheck \
                valgrind \
                gdb \
                make
            else
                sudo yum install -y \
                    clang \
                    clang-tools-extra \
                    cppcheck \
                    valgrind \
                    gdb \
                    make
            fi
            ;;
        "macos")
            if command_exists brew; then
                brew install \
                    llvm \
                    cppcheck \
                    valgrind \
                    gdb \
                    make
                
                # Set up symlinks for macOS LLVM
                print_status "Setting up LLVM symlinks for macOS..."
                brew link --force llvm
            fi
            ;;
    esac
    
    if command_exists clang; then
        print_success "Clang tools installed successfully"
    else
        print_error "Failed to install clang tools"
        return 1
    fi
}

# =============================================================================
# YQ INSTALLATION FUNCTIONS
# =============================================================================

# Function to install yq
install_yq() {
    print_status "Installing yq..."
    
    if command_exists yq; then
        local yq_version=$(yq --version 2>/dev/null | grep -oE '[0-9]+\.[0-9]+' | head -1)
        print_status "yq already installed: version $yq_version"
        return 0
    fi
    
    local os=$(detect_os)
    
    case $os in
        "ubuntu"|"fedora"|"centos")
            # Try package manager first
            if [[ "$os" == "ubuntu" ]]; then
                sudo apt-get install -y yq
            elif [[ "$os" == "fedora" ]]; then
                sudo dnf install -y yq
            elif [[ "$os" == "centos" ]]; then
                sudo yum install -y yq
            fi
            
            # If package manager failed, install manually
            if ! command_exists yq; then
                print_status "Installing yq manually..."
                local yq_version="4.40.5"
                local arch=$(uname -m)
                
                if [[ "$arch" == "x86_64" ]]; then
                    local arch="amd64"
                elif [[ "$arch" == "aarch64" ]]; then
                    local arch="arm64"
                fi
                
                wget -O yq "https://github.com/mikefarah/yq/releases/download/v${yq_version}/yq_linux_${arch}"
                chmod +x yq
                # Install to user-writable directory for better caching
                mkdir -p ~/.local/bin
                mv yq ~/.local/bin/
                export PATH="$HOME/.local/bin:$PATH"
            fi
            ;;
        "macos")
            if command_exists brew; then
                brew install yq
            else
                print_error "Homebrew not found. Please install Homebrew first."
                return 1
            fi
            ;;
    esac
    
    if command_exists yq; then
        print_success "yq installed successfully"
    else
        print_error "Failed to install yq"
        return 1
    fi
}

# =============================================================================
# ESP-IDF INSTALLATION FUNCTIONS
# =============================================================================

# Function to install ESP-IDF
install_esp_idf() {
    print_status "Installing ESP-IDF..."
    
    # Load configuration to get IDF versions
    local script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
    local project_dir="$(cd "$script_dir/.." && pwd)"
    local config_file="$project_dir/app_config.yml"
    
    local idf_versions=("release/v5.5")  # Default fallback
    
    if [[ -f "$config_file" ]]; then
        source "$script_dir/config_loader.sh"
        if load_config; then
            local config_idf_versions=$(get_idf_versions)
            if [[ -n "$config_idf_versions" ]]; then
                # Convert space-separated string to array
                IFS=' ' read -ra idf_versions <<< "$config_idf_versions"
                print_status "Using IDF versions from config: ${idf_versions[*]}"
            else
                print_warning "Could not read IDF versions from config, using default: release/v5.5"
            fi
        else
            print_warning "Could not load config, using default IDF version: release/v5.5"
        fi
    else
        print_warning "Config file not found, using default IDF version: release/v5.5"
    fi
    
    local esp_dir="$HOME/esp"
    
    # Create esp directory if it doesn't exist
    mkdir -p "$esp_dir"
    
    # Install each required ESP-IDF version
    for idf_version in "${idf_versions[@]}"; do
        # Use consistent sanitization method
        local sanitized_version=$(echo "$idf_version" | sed 's/[^a-zA-Z0-9._-]/-/g')
        local idf_dir="$esp_dir/esp-idf-${sanitized_version}"
    
    if [[ -d "$idf_dir" ]]; then
            print_status "ESP-IDF $idf_version already exists, updating..."
        cd "$idf_dir"
        git fetch origin
        git checkout "$idf_version"
        git pull origin "$idf_version"
        # Ensure submodules are synced and updated to match the checked-out branch
        if ! git submodule sync --recursive; then
            print_warning "Failed to sync submodules, continuing anyway..."
        fi
        if ! git submodule update --init --recursive; then
            print_warning "Failed to update submodules, continuing anyway..."
        fi
        if ! ./install.sh esp32c6; then
            print_error "Failed to install ESP-IDF tools"
            return 1
        fi
        cd - > /dev/null
    else
            print_status "Cloning ESP-IDF $idf_version..."
        cd "$esp_dir"
        if ! git clone --recursive --branch "$idf_version" https://github.com/espressif/esp-idf.git "$idf_dir"; then
            print_error "Failed to clone ESP-IDF $idf_version"
            return 1
        fi
        cd "$idf_dir"
        # Extra safety to keep submodules in sync even after clone
        if ! git submodule sync --recursive; then
            print_warning "Failed to sync submodules, continuing anyway..."
        fi
        if ! git submodule update --init --recursive; then
            print_warning "Failed to update submodules, continuing anyway..."
        fi
        if ! ./install.sh esp32c6; then
            print_error "Failed to install ESP-IDF tools"
            return 1
        fi
        cd - > /dev/null
    fi
        
        print_success "ESP-IDF $idf_version installed/updated"
    done
    
    # Create symlink for default version (first in the list)
    local default_idf_dir="$esp_dir/esp-idf"
    local first_version="${idf_versions[0]}"
    local first_sanitized=$(echo "$first_version" | sed 's/[^a-zA-Z0-9._-]/-/g')
    local first_idf_dir="$esp_dir/esp-idf-${first_sanitized}"
    
    if [[ -L "$default_idf_dir" ]]; then
        rm "$default_idf_dir"
    fi
    ln -sf "$first_idf_dir" "$default_idf_dir"
    print_status "Default ESP-IDF symlink created: $default_idf_dir -> $first_idf_dir"
    
    # If IDF_VERSION is set, override the symlink to point to that version
    if [[ -n "$IDF_VERSION" ]]; then
        local ci_sanitized=$(echo "$IDF_VERSION" | sed 's/[^a-zA-Z0-9._-]/-/g')
        local ci_idf_dir="$esp_dir/esp-idf-${ci_sanitized}"
        if [[ -d "$ci_idf_dir" ]]; then
            if [[ -L "$default_idf_dir" ]]; then
                rm "$default_idf_dir"
            fi
            ln -sf "$ci_idf_dir" "$default_idf_dir"
            print_status "CI mode: Overriding symlink to point to $IDF_VERSION: $default_idf_dir -> $ci_idf_dir"
        else
            print_warning "CI mode: Requested IDF_VERSION $IDF_VERSION not found, keeping default symlink"
        fi
    fi
    
    # Optimize for caching by ensuring all tools are properly installed
    print_status "Optimizing ESP-IDF installation for caching..."
    
    # Ensure all ESP-IDF tools are available
    if [[ -f "$HOME/.espressif/export.sh" ]]; then
        source "$HOME/.espressif/export.sh"
        print_success "ESP-IDF tools environment loaded"
    fi
    
    print_success "ESP-IDF versions installed/updated and optimized for caching"
}

# Function to export ESP-IDF environment for specific version
export_esp_idf_version() {
    local idf_version="$1"
    local auto_install="${2:-false}"
    
    if [[ -z "$idf_version" ]]; then
        print_error "ESP-IDF version not specified"
        return 1
    fi
    
    local esp_dir="$HOME/esp"
    # Use the same sanitization as installation function
    local sanitized_version=$(echo "$idf_version" | sed 's/[^a-zA-Z0-9._-]/-/g')
    local idf_dir="$esp_dir/esp-idf-${sanitized_version}"
    
    if [[ ! -d "$idf_dir" ]]; then
        if [[ "$auto_install" == "true" ]]; then
            print_status "ESP-IDF version $idf_version not found, attempting auto-installation..."
            if install_esp_idf_version "$idf_version"; then
                print_success "ESP-IDF version $idf_version installed successfully"
            else
                print_error "Failed to install ESP-IDF version $idf_version"
                return 1
            fi
        else
            print_error "ESP-IDF version $idf_version not found at $idf_dir"
            print_status "Available versions:"
            for dir in "$esp_dir"/esp-idf-*; do
                if [[ -d "$dir" ]]; then
                    local version_name=$(basename "$dir" | sed 's/esp-idf-//' | sed 's/_/\//')
                    echo "  - $version_name"
                fi
            done
            print_status "To auto-install missing versions, use: export_esp_idf_version '$idf_version' true"
            return 1
        fi
    fi
    
    print_status "Exporting ESP-IDF environment for version: $idf_version"
    
    # Source the ESP-IDF export script
    if [[ -f "$idf_dir/export.sh" ]]; then
        source "$idf_dir/export.sh"
        
        # Verify the environment is loaded
        if command_exists idf.py; then
            local idf_path=$(idf.py --version 2>/dev/null | head -1 || echo "Unknown")
            print_success "ESP-IDF environment loaded: $idf_path"
            
            # Export IDF_PATH for this session
            export IDF_PATH="$idf_dir"
            export IDF_VERSION="$idf_version"
            
            return 0
        else
            print_error "Failed to load ESP-IDF environment for version $idf_version"
            return 1
        fi
    else
        print_error "ESP-IDF export script not found at $idf_dir/export.sh"
        return 1
    fi
}

# Function to install specific ESP-IDF version
install_esp_idf_version() {
    local idf_version="$1"
    
    if [[ -z "$idf_version" ]]; then
        print_error "ESP-IDF version not specified"
        return 1
    fi
    
    print_status "Installing ESP-IDF version: $idf_version"
    
    # Create ESP directory if it doesn't exist
    local esp_dir="$HOME/esp"
    mkdir -p "$esp_dir"
    
    # Change to ESP directory
    cd "$esp_dir"
    
    # Sanitize version string for directory name - handle more special characters
    local sanitized_version=$(echo "$idf_version" | sed 's/[^a-zA-Z0-9._-]/-/g')
    local idf_dir="esp-idf-${sanitized_version}"
    
    print_status "Using sanitized directory name: $idf_dir"
    
    if [[ -d "$idf_dir" ]]; then
        print_status "ESP-IDF directory already exists, updating..."
        cd "$idf_dir"
        if ! git fetch origin; then
            print_error "Failed to fetch from origin"
            return 1
        fi
        if ! git checkout "$idf_version"; then
            print_error "Failed to checkout $idf_version"
            return 1
        fi
        if ! git pull origin "$idf_version"; then
            print_error "Failed to pull $idf_version"
            return 1
        fi
    else
        print_status "Cloning ESP-IDF version $idf_version..."
        if git clone --recursive --branch "$idf_version" https://github.com/espressif/esp-idf.git "$idf_dir"; then
            cd "$idf_dir"
        else
            print_error "Failed to clone ESP-IDF version $idf_version"
            return 1
        fi
    fi
    
    # Install ESP-IDF tools
    print_status "Installing ESP-IDF tools..."
    print_status "Current directory: $(pwd)"
    print_status "Installing for targets: esp32c6"
    
    if ./install.sh esp32c6; then
        print_success "ESP-IDF tools installed successfully"
        
        # Create a symlink for easy access
        cd "$esp_dir"
        if [[ -L "esp-idf" ]]; then
            rm "esp-idf"
        fi
        ln -sf "$idf_dir" "esp-idf"
        print_status "Created symlink: esp-idf -> $idf_dir"
        
        # Verify that tools are installed to the correct location
        if [[ -f "$HOME/.espressif/export.sh" ]]; then
            print_status "ESP-IDF tools verified at $HOME/.espressif/export.sh"
        else
            print_warning "ESP-IDF tools export.sh not found at expected location"
            print_status "Checking what was installed..."
            ls -la "$HOME/.espressif" 2>/dev/null || print_warning "Could not list ESP-IDF tools directory"
        fi
        
        # Check if the ESP-IDF source directory has export.sh
        if [[ -f "$idf_dir/export.sh" ]]; then
            print_status "ESP-IDF source export.sh found at $idf_dir/export.sh"
        else
            print_warning "ESP-IDF source export.sh not found at $idf_dir/export.sh"
        fi
    else
        print_error "Failed to install ESP-IDF tools"
        return 1
    fi
    
    print_success "ESP-IDF version $idf_version installed successfully"
    return 0
}

# Function to list available ESP-IDF versions
list_esp_idf_versions() {
    local esp_dir="$HOME/esp"
    
    if [[ ! -d "$esp_dir" ]]; then
        print_warning "ESP directory not found: $esp_dir"
        return 1
    fi
    
    print_status "Available ESP-IDF versions:"
    
    local found_versions=0
    for dir in "$esp_dir"/esp-idf-*; do
        if [[ -d "$dir" ]]; then
            local version_name=$(basename "$dir" | sed 's/esp-idf-//' | sed 's/-/\//')
            local is_default=""
            
            # Check if this is the default symlink
            if [[ -L "$esp_dir/esp-idf" ]] && [[ "$(readlink "$esp_dir/esp-idf")" == "$dir" ]]; then
                is_default=" (default)"
            fi
            
            echo "  - $version_name$is_default"
            found_versions=$((found_versions + 1))
        fi
    done
    
    if [[ $found_versions -eq 0 ]]; then
        print_warning "No ESP-IDF versions found"
        return 1
    fi
    
    return 0
}

# =============================================================================
# PYTHON DEPENDENCY INSTALLATION FUNCTIONS
# =============================================================================

# Function to install Python dependencies
install_python_deps() {
    print_status "Installing Python dependencies..."
    
    # Upgrade pip
    python3 -m pip install --upgrade pip
    
    # Install required packages
    python3 -m pip install pyyaml
    
    print_success "Python dependencies installed"
}

# =============================================================================
# LOCAL DEVELOPMENT ENVIRONMENT FUNCTIONS
# =============================================================================

# Function to setup environment variables
setup_environment_vars() {
    print_status "Setting up environment variables..."
    
    local bashrc="$HOME/.bashrc"
    local profile="$HOME/.profile"
    
    # Add ESP-IDF to PATH
    if ! grep -q "esp-idf" "$bashrc" 2>/dev/null; then
        echo "" >> "$bashrc"
        echo "# ESP-IDF Environment" >> "$bashrc"
        echo "export IDF_PATH=\"\$HOME/esp/esp-idf\"" >> "$bashrc"
        echo "alias get_idf='. \$HOME/esp/esp-idf/export.sh'" >> "$bashrc"
    fi
    
    # Add to profile for non-interactive shells
    if ! grep -q "esp-idf" "$profile" 2>/dev/null; then
        echo "" >> "$profile"
        echo "# ESP-IDF Environment" >> "$profile"
        echo "export IDF_PATH=\"\$HOME/esp/esp-idf\"" >> "$profile"
    fi
    
    print_success "Environment variables configured"
}

# Function to setup local-specific environment
setup_local_environment() {
    print_status "Setting up local development environment..."
    
    # Setup environment variables
    setup_environment_vars
    
    # Create useful aliases
    local bashrc="$HOME/.bashrc"
    if ! grep -q "alias build_app" "$bashrc" 2>/dev/null; then
        echo "" >> "$bashrc"
        echo "# ESP32 Development Aliases" >> "$bashrc"
        echo "alias build_app='./scripts/build_app.sh'" >> "$bashrc"
        echo "alias flash_app='./scripts/flash_app.sh'" >> "$bashrc"
        echo "alias list_apps='./scripts/build_app.sh list'" >> "$bashrc"
    fi
    
    print_success "Local development environment configured"
}

# =============================================================================
# VERIFICATION FUNCTIONS
# =============================================================================

# Function to verify installation
verify_installation() {
    print_status "Verifying installation..."
    
    local errors=0
    
    # Check essential commands
    local essential_commands=("gcc" "make" "cmake" "git" "python3")
    for cmd in "${essential_commands[@]}"; do
        if command_exists "$cmd"; then
            print_success "$cmd: $(command -v "$cmd")"
        else
            print_error "$cmd: NOT FOUND"
            ((errors++))
        fi
    done
    
    # Check clang tools
    print_status "Checking clang tools..."
    if command_exists clang; then
        local clang_version=$(clang --version | head -1)
        print_success "clang: $clang_version"
    else
        print_error "clang: NOT FOUND"
        ((errors++))
    fi
    
    if command_exists clang-format; then
        local format_version=$(clang-format --version | head -1)
        print_success "clang-format: $format_version"
    else
        print_error "clang-format: NOT FOUND"
        ((errors++))
    fi
    
    if command_exists clang-tidy; then
        local tidy_version=$(clang-tidy --version | head -1)
        print_success "clang-tidy: $tidy_version"
    else
        print_error "clang-tidy: NOT FOUND"
        ((errors++))
    fi
    
    # Check ESP-IDF
    if [[ -d "$HOME/esp/esp-idf" ]]; then
        print_success "ESP-IDF: $HOME/esp/esp-idf"
    else
        print_error "ESP-IDF: NOT FOUND"
        ((errors++))
    fi
    
    # Check Python packages
    if python3 -c "import yaml" 2>/dev/null; then
        print_success "PyYAML: installed"
    else
        print_error "PyYAML: NOT FOUND"
        ((errors++))
    fi
    
    if [[ $errors -eq 0 ]]; then
        print_success "All dependencies verified successfully!"
        if [[ "${SETUP_MODE}" == "local" ]]; then
            print_status "Next steps:"
            print_status "1. Restart your terminal or run: source ~/.bashrc"
            print_status "2. Navigate to examples/esp32 directory"
            print_status "3. Run: get_idf"
            print_status "4. Build apps with: ./scripts/build_app.sh <app_type> <build_type>"
        fi
    else
        print_error "Installation verification failed with $errors errors"
        return 1
    fi
}

# =============================================================================
# CI-SPECIFIC FUNCTIONS
# =============================================================================

# Function to setup CI-specific environment
ci_setup_environment() {
    print_status "Setting up CI environment..."
    
    # Set clang-20 as default for CI
    if command_exists update-alternatives; then
        sudo update-alternatives --install /usr/bin/clang clang /usr/bin/clang-20 100
        sudo update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-20 100
        sudo update-alternatives --install /usr/bin/clang-format clang-format /usr/bin/clang-format-20 100
        sudo update-alternatives --install /usr/bin/clang-tidy clang-tidy /usr/bin/clang-tidy-20 100
    fi
    
    print_success "CI environment configured"
}

# Function to optimize cache for CI environment
ci_optimize_cache() {
    print_status "Optimizing cache for CI environment..."
    
    # Clean up unnecessary files to reduce cache size
    if [[ -d "$HOME/.espressif" ]]; then
        # Remove git history from ESP-IDF to save space
        if [[ -d "$HOME/esp/esp-idf/.git" ]]; then
            print_status "Cleaning ESP-IDF git history for cache optimization..."
            rm -rf "$HOME/esp/esp-idf/.git"
            print_success "Git history cleaned (saves ~100-200MB in cache)"
        fi
        
        # Clean up temporary build files
        if [[ -d "$HOME/esp/esp-idf/build" ]]; then
            print_status "Cleaning ESP-IDF build files for cache optimization..."
            rm -rf "$HOME/esp/esp-idf/build"
            print_success "Build files cleaned"
        fi
    fi
    
    # Clean up pip cache if it's too large
    if [[ -d "$HOME/.cache/pip" ]]; then
        local pip_cache_size=$(du -sh "$HOME/.cache/pip" 2>/dev/null | cut -f1)
        print_info "Pip cache size: $pip_cache_size"
        
        # If pip cache is larger than 500MB, clean it
        local pip_cache_size_bytes=$(du -sb "$HOME/.cache/pip" 2>/dev/null | cut -f1)
        if [[ $pip_cache_size_bytes -gt 524288000 ]]; then  # 500MB in bytes
            print_status "Pip cache is large, cleaning for optimization..."
            pip cache purge
            print_success "Pip cache cleaned"
        fi
    fi
    
    print_success "Cache optimization complete"
}

# Function to check cache status
ci_check_cache_status() {
    print_status "Checking cache status..."
    
    # Check ESP-IDF cache
    if [[ -d "$HOME/.espressif" && -d "$HOME/esp/esp-idf" ]]; then
        print_success "ESP-IDF cache: AVAILABLE"
        print_info "  ESP-IDF: $HOME/esp/esp-idf"
        print_info "  Tools: $HOME/.espressif"
    else
        print_warning "ESP-IDF cache: NOT AVAILABLE"
    fi
    
    # Check Python cache
    if [[ -d "$HOME/.cache/pip" && -d "$HOME/.local/lib" ]]; then
        print_success "Python cache: AVAILABLE"
        print_info "  Pip cache: $HOME/.cache/pip"
        print_info "  Site packages: $HOME/.local/lib"
    else
        print_warning "Python cache: NOT AVAILABLE"
    fi
    
    # Check ccache
    if [[ -d "$HOME/.ccache" ]]; then
        print_success "ccache: AVAILABLE"
        print_info "  ccache: $HOME/.ccache"
    else
        print_warning "ccache: NOT AVAILABLE"
    fi
}

