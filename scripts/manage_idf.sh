#!/bin/bash
# ESP-IDF Version Management Script
# This script helps manage multiple ESP-IDF versions for different app requirements

set -e  # Exit on any error

# Parse --app-config flag first (before other argument parsing)
FILTERED_ARGS=()
i=1
while [[ $i -le $# ]]; do
    arg="${!i}"
    case "$arg" in
        --project-path)
            # Check if next argument exists and is not another flag
            if [[ $((i+1)) -le $# ]] && [[ "${!((i+1))}" != -* ]]; then
                PROJECT_PATH="${!((i+1))}"
                ((i++))  # Skip the next argument since we consumed it
            else
                echo "ERROR: --project-path requires a path argument" >&2
                echo "Usage: --project-path /path/to/project" >&2
                exit 1
            fi
            ;;
        *)
            FILTERED_ARGS+=("$arg")
            ;;
    esac
    ((i++))
done

# Export PROJECT_PATH if it was set via --project-path flag
if [[ -n "$PROJECT_PATH" ]]; then
    export PROJECT_PATH
fi

# Source the common setup functions
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/setup_common.sh"

# Replace $@ with filtered arguments for the rest of the script
set -- "${FILTERED_ARGS[@]}"

# Show help if requested
if [ "$1" = "--help" ] || [ "$1" = "-h" ]; then
    echo "ESP-IDF Version Management Script"
    echo ""
    echo "Usage: ./manage_idf.sh [COMMAND] [OPTIONS] [VERSION]"
    echo ""
    echo "COMMANDS:"
    echo "  install [version]           - Install ESP-IDF versions from app_config.yml"
    echo "  list                        - List installed ESP-IDF versions"
    echo "  export <version>            - Export ESP-IDF environment for specific version"
    echo "  switch <version>            - Switch default ESP-IDF version"
    echo "  update <version>            - Update specific ESP-IDF version"
    echo "  clean <version>             - Remove specific ESP-IDF version"
    echo "  status                      - Show current ESP-IDF status"
    echo ""
    echo "OPTIONS:"
    echo "  --project-path <path>       - Path to project directory (allows scripts to be placed anywhere)"
    echo "  --help, -h                  - Show this help message"
    echo "  --force                     - Force operations (overwrite existing)"
    echo "  --verbose                   - Show detailed output"
    echo ""
    echo "ARGUMENTS:"
    echo "  version                     - ESP-IDF version (e.g., release/v5.5, release/v5.4)"
    echo ""
    echo "EXAMPLES:"
    echo "  # Installation and setup"
    echo "  ./manage_idf.sh install                    # Install all required versions"
    echo "  ./manage_idf.sh install release/v5.5       # Install specific version"
    echo "  ./manage_idf.sh install --force            # Force reinstall all versions"
    echo ""
    echo "  # Version management"
    echo "  ./manage_idf.sh list                       # List installed versions"
    echo "  ./manage_idf.sh status                     # Show current status"
    echo "  ./manage_idf.sh export release/v5.4        # Export environment for v5.4"
    echo "  ./manage_idf.sh switch release/v5.5        # Set v5.5 as default"
    echo ""
    echo "  # Maintenance"
    echo "  ./manage_idf.sh update release/v5.5        # Update v5.5 to latest"
    echo "  ./manage_idf.sh clean release/v5.4         # Remove v5.4 installation"
    echo "  ./manage_idf.sh clean --force              # Force clean all versions"
    echo ""
    echo "  # Environment setup"
    echo "  source <(./manage_idf.sh export release/v5.5)  # Source environment in current shell"
    echo "  eval \$(./manage_idf.sh export release/v5.5)   # Export environment variables"
    echo ""
    echo "VERSION FORMATS:"
    echo "  • release/v5.5              - Latest stable release"
    echo "  • release/v5.4              - Previous stable release"
    echo "  • master                    - Development branch (not recommended)"
    echo "  • v4.4                     - Legacy version (limited support)"
    echo ""
    echo "INSTALLATION LOCATIONS:"
    echo "  • ESP-IDF versions: ~/esp/esp-idf-{version}"
    echo "  • Default symlink: ~/esp/esp-idf"
    echo "  • Tools: ~/.espressif/"
    echo "  • Python packages: ~/.espressif/python_env/"
    echo ""
    echo "ENVIRONMENT VARIABLES:"
    echo "  • IDF_PATH: Path to ESP-IDF installation"
    echo "  • IDF_VERSION: Current ESP-IDF version"
    echo "  • PATH: Updated with ESP-IDF tools"
    echo ""
    echo "TROUBLESHOOTING:"
    echo "  • If installation fails: Check disk space, internet connection"
    echo "  • If export fails: Ensure version is installed first"
    echo "  • If switch fails: Check permissions and existing installations"
    echo "  • For version conflicts: Use --force flag carefully"
    echo ""
    echo "For detailed information, see: docs/README_UTILITY_SCRIPTS.md"
    exit 0
fi

# Function to install ESP-IDF versions
install_idf_versions() {
    print_status "Installing ESP-IDF versions from configuration..."
    
    # Load configuration to get required versions
    source "$SCRIPT_DIR/config_loader.sh"
    if ! load_config; then
        print_error "Failed to load configuration"
        return 1
    fi
    
    local required_versions=$(get_idf_versions)
    if [[ -z "$required_versions" ]]; then
        print_error "No ESP-IDF versions specified in configuration"
        return 1
    fi
    
    print_status "Required ESP-IDF versions: $required_versions"
    
    # Install each required version
    for version in $required_versions; do
        print_status "Installing ESP-IDF $version..."
        
        local esp_dir="$HOME/esp"
        local sanitized_version=$(echo "$version" | sed 's/[^a-zA-Z0-9._-]/-/g')
        local idf_dir="$esp_dir/esp-idf-${sanitized_version}"
        
        if [[ -d "$idf_dir" ]] && [[ "$1" != "--force" ]]; then
            print_warning "ESP-IDF $version already exists at $idf_dir"
            print_status "Use --force to reinstall"
            continue
        fi
        
        # Remove existing directory if forcing
        if [[ -d "$idf_dir" ]] && [[ "$1" == "--force" ]]; then
            print_status "Removing existing ESP-IDF $version..."
            rm -rf "$idf_dir"
        fi
        
        # Clone and install
        cd "$esp_dir"
        git clone --recursive --branch "$version" https://github.com/espressif/esp-idf.git "$idf_dir"
        cd "$idf_dir"
        git submodule sync --recursive
        git submodule update --init --recursive
        ./install.sh esp32c6
        cd - > /dev/null
        
        print_success "ESP-IDF $version installed successfully"
    done
    
    # Set default version (first in the list)
    local first_version=$(echo "$required_versions" | cut -d' ' -f1)
    switch_default_version "$first_version"
    
    print_success "ESP-IDF installation complete"
}

# Function to list installed versions
list_installed_versions() {
    print_status "Listing installed ESP-IDF versions..."
    list_esp_idf_versions
}

# Function to export ESP-IDF environment
export_idf_environment() {
    local version="$1"
    
    if [[ -z "$version" ]]; then
        print_error "ESP-IDF version not specified"
        print_status "Usage: ./manage_idf.sh export <version>"
        return 1
    fi
    
    if export_esp_idf_version "$version"; then
        print_success "ESP-IDF environment exported for version $version"
        print_status "Environment variables set:"
        print_status "  IDF_PATH: $IDF_PATH"
        print_status "  IDF_VERSION: $IDF_VERSION"
        
        # Show available commands
        if command_exists idf.py; then
            print_status "Available commands:"
            print_status "  idf.py --version"
            print_status "  idf.py --help"
        fi
    else
        print_error "Failed to export ESP-IDF environment for version $version"
        return 1
    fi
}

# Function to switch default ESP-IDF version
switch_default_version() {
    local version="$1"
    
    if [[ -z "$version" ]]; then
        print_error "ESP-IDF version not specified"
        print_status "Usage: ./manage_idf.sh switch <version>"
        return 1
    fi
    
    local esp_dir="$HOME/esp"
    local idf_dir="$esp_dir/esp-idf-${version//\//_}"
    local default_link="$esp_dir/esp-idf"
    
    if [[ ! -d "$idf_dir" ]]; then
        print_error "ESP-IDF version $version not found at $idf_dir"
        return 1
    fi
    
    # Remove existing symlink
    if [[ -L "$default_link" ]]; then
        rm "$default_link"
    fi
    
    # Create new symlink
    ln -sf "$idf_dir" "$default_link"
    
    print_success "Default ESP-IDF version switched to $version"
    print_status "Symlink created: $default_link -> $idf_dir"
}

# Function to update specific ESP-IDF version
update_idf_version() {
    local version="$1"
    
    if [[ -z "$version" ]]; then
        print_error "ESP-IDF version not specified"
        print_status "Usage: ./manage_idf.sh update <version>"
        return 1
    fi
    
    local esp_dir="$HOME/esp"
    local idf_dir="$esp_dir/esp-idf-${version//\//_}"
    
    if [[ ! -d "$idf_dir" ]]; then
        print_error "ESP-IDF version $version not found at $idf_dir"
        return 1
    fi
    
    print_status "Updating ESP-IDF $version..."
    
    cd "$idf_dir"
    git fetch origin
    git checkout "$version"
    git pull origin "$version"
    git submodule sync --recursive
    git submodule update --init --recursive
    ./install.sh esp32c6
    cd - > /dev/null
    
    print_success "ESP-IDF $version updated successfully"
}

# Function to clean specific ESP-IDF version
clean_idf_version() {
    local version="$1"
    
    if [[ -z "$version" ]]; then
        print_error "ESP-IDF version not specified"
        print_status "Usage: ./manage_idf.sh clean <version>"
        return 1
    fi
    
    local esp_dir="$HOME/esp"
    local idf_dir="$esp_dir/esp-idf-${version//\//_}"
    local default_link="$esp_dir/esp-idf"
    
    if [[ ! -d "$idf_dir" ]]; then
        print_error "ESP-IDF version $version not found at $idf_dir"
        return 1
    fi
    
    # Check if this is the default version
    if [[ -L "$default_link" ]] && [[ "$(readlink "$default_link")" == "$idf_dir" ]]; then
        print_warning "Cannot remove default ESP-IDF version"
        print_status "Switch to another version first: ./manage_idf.sh switch <other_version>"
        return 1
    fi
    
    print_status "Removing ESP-IDF $version..."
    rm -rf "$idf_dir"
    
    print_success "ESP-IDF $version removed successfully"
}

# Function to show current status
show_status() {
    print_status "ESP-IDF Status Report"
    echo ""
    
    # Show current environment
    if [[ -n "$IDF_PATH" ]]; then
        print_success "Current ESP-IDF environment:"
        print_status "  IDF_PATH: $IDF_PATH"
        print_status "  IDF_VERSION: $IDF_VERSION"
    else
        print_warning "No ESP-IDF environment currently exported"
    fi
    
    echo ""
    
    # Show default symlink
    local esp_dir="$HOME/esp"
    local default_link="$esp_dir/esp-idf"
    
    if [[ -L "$default_link" ]]; then
        local target=$(readlink "$default_link")
        local version_name=$(basename "$target" | sed 's/esp-idf-//' | sed 's/_/\//')
        print_success "Default ESP-IDF version: $version_name"
        print_status "  Symlink: $default_link -> $target"
    else
        print_warning "No default ESP-IDF symlink found"
    fi
    
    echo ""
    
    # List all installed versions
    list_esp_idf_versions
}

# Main function
main() {
    local command="$1"
    local force_flag="$2"
    
    # Check if help is requested
    if [[ "$1" == "--help" ]] || [[ "$1" == "-h" ]]; then
        show_help
    fi
    
    # Check if command is provided
    if [[ -z "$command" ]]; then
        print_error "No command specified"
        show_help
    fi
    
    # Execute command
    case "$command" in
        "install")
            install_idf_versions "$force_flag"
            ;;
        "list")
            list_installed_versions
            ;;
        "export")
            export_idf_environment "$2"
            ;;
        "switch")
            switch_default_version "$2"
            ;;
        "update")
            update_idf_version "$2"
            ;;
        "clean")
            clean_idf_version "$2"
            ;;
        "status")
            show_status
            ;;
        *)
            print_error "Unknown command: $command"
            show_help
            ;;
    esac
}

# Run main function with all arguments
main "$@"
