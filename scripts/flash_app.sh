#!/bin/bash
# Flash and monitor script for different ESP32 apps (Bash version)
# Usage: ./flash_app.sh [app_type] [build_type] [idf_version] [operation] [--log [log_name]]
# 
# App types and build types are loaded from app_config.yml
# Use './flash_app.sh list' to see all available apps
# Operations: flash, monitor, flash_monitor (default: flash_monitor)
# Logging: --log [log_name] to enable logging with optional custom name
# NEW: ESP-IDF version parameter for compatibility validation

set -e  # Exit on any error

# Parse --project-path and --port flags first (before other argument parsing)
FILTERED_ARGS=()
EXPLICIT_PORT=""
i=1
while [[ $i -le $# ]]; do
    arg="${!i}"
    next_i=$((i+1))
    case "$arg" in
        --project-path)
            # Check if next argument exists and is not another flag
            if [[ $next_i -le $# ]] && [[ "${!next_i}" != -* ]]; then
                PROJECT_PATH="${!next_i}"
                ((i++))  # Skip the next argument since we consumed it
            else
                echo "ERROR: --project-path requires a path argument" >&2
                echo "Usage: --project-path /path/to/project" >&2
                exit 1
            fi
            ;;
        --port|-p)
            # Check if next argument exists and is not another flag
            if [[ $next_i -le $# ]] && [[ "${!next_i}" != -* ]]; then
                EXPLICIT_PORT="${!next_i}"
                ((i++))  # Skip the next argument since we consumed it
            else
                echo "ERROR: --port requires a port path argument" >&2
                echo "Usage: --port /dev/ttyACM0" >&2
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

# Load configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
source "$SCRIPT_DIR/config_loader.sh"

# Replace $@ with filtered arguments for the rest of the script
set -- "${FILTERED_ARGS[@]}"

# Function to show help
show_help() {
    echo "ESP32 HardFOC Interface Wrapper - Flash and Monitor Script"
    echo ""
    echo "Usage: ./flash_app.sh [COMMAND] [OPTIONS]"
    echo ""
    echo "COMMANDS:"
    echo "  flash [app] [build_type] [idf_version]     - Flash firmware only"
    echo "  flash_monitor [app] [build_type] [idf_version] - Flash and monitor (default)"
    echo "  monitor [app] [build_type] [idf_version]   - Monitor existing firmware"
    echo "  size [app] [build_type] [idf_version]      - Show firmware size information"
    echo "  list                                        - List available apps and build types"
    echo "  ports                                       - List available ESP32 serial ports"
    echo ""
    echo "ARGUMENT PATTERNS:"
    echo "  Operation-First Syntax (Recommended):"
    echo "    ./flash_app.sh flash gpio_test Release release/v5.5"
    echo "    ./flash_app.sh size gpio_test Release"
    echo "    ./flash_app.sh monitor"
    echo ""
    echo "  App-First Syntax (Legacy):"
    echo "    ./flash_app.sh gpio_test Release flash"
    echo "    ./flash_app.sh gpio_test Release size"
    echo "    ./flash_app.sh gpio_test Release monitor"
    echo ""
    echo "  Minimal Syntax:"
    echo "    ./flash_app.sh                                    # Use defaults (flash_monitor)"
    echo "    ./flash_app.sh gpio_test                          # App only (defaults: Release, flash_monitor)"
    echo "    ./flash_app.sh gpio_test Release                  # App + build type (defaults: flash_monitor)"
    echo ""
    echo "OPTIONS:"
    echo "  --port, -p <port>                                  - Specify ESP32 serial port (e.g., /dev/ttyACM0)"
    echo "  --project-path <path>                              - Path to project directory (allows scripts to be placed anywhere)"
    echo "  --log [log_name]                                   - Enable logging with optional custom name"
    echo "  -h, --help                                         - Show this help message"
    echo ""
    echo "ENVIRONMENT VARIABLES:"
    echo "  ESPPORT                                             - ESP32 serial port (e.g., /dev/ttyACM0)"
    echo "    - Overrides auto-detected port"
    echo "    - Example: ESPPORT=/dev/ttyACM1 ./flash_app.sh flash"
    echo "  PROJECT_PATH                                        - Path to project directory (optional)"
    echo "    - If set, uses this project directory instead of default location"
    echo "    - Allows scripts to be placed anywhere while finding correct project"
    echo "    - Example: PROJECT_PATH=/path/to/project ./flash_app.sh"
    echo ""
    echo "ARGUMENTS:"
    echo "  operation           - Operation to perform (flash, flash_monitor, monitor, size, list)"
    echo "  app                 - Application type (e.g., gpio_test, adc_test)"
    echo "  build_type          - Build configuration (Debug, Release)"
    echo "  idf_version         - ESP-IDF version (e.g., release/v5.5, release/v5.4)"
    echo "  log_name            - Custom name for log file (optional)"
    echo ""
    echo "EXAMPLES:"
      echo "  # Operation-first syntax (recommended)"
  echo "  ./flash_app.sh flash gpio_test Release release/v5.5"
  echo "  ./flash_app.sh flash_monitor gpio_test Release release/v5.5"
  echo "  ./flash_app.sh size gpio_test Release release/v5.5"
  echo "  ./flash_app.sh monitor"
  echo ""
  echo "  # Portable usage with --project-path flag"
  echo "  ./flash_app.sh --project-path /path/to/project flash gpio_test Release"
  echo "  ./flash_app.sh --project-path ../project flash_monitor gpio_test Release --log"
    echo ""
    echo "  # App-first syntax (legacy)"
    echo "  ./flash_app.sh gpio_test Release flash"
    echo "  ./flash_app.sh gpio_test Release size"
    echo "  ./flash_app.sh gpio_test Release monitor"
    echo ""
    echo "  # With logging"
    echo "  ./flash_app.sh flash gpio_test Release --log"
    echo "  ./flash_app.sh flash_monitor gpio_test Release --log debug_session"
    echo "  ./flash_app.sh size gpio_test Release --log size_analysis"
    echo ""
    echo "  # Minimal usage"
    echo "  ./flash_app.sh                                    # Defaults: ascii_art Release flash_monitor"
    echo "  ./flash_app.sh gpio_test                          # Defaults: Release flash_monitor"
    echo "  ./flash_app.sh gpio_test Release                  # Defaults: flash_monitor"
    echo ""
    echo "  # Multi-device usage (when multiple ESP32s connected)"
    echo "  ./flash_app.sh ports                              # List all available ESP32 serial ports"
    echo "  ./flash_app.sh --port /dev/ttyACM1 flash          # Flash to specific device"
    echo "  ./flash_app.sh -p /dev/ttyUSB0 monitor            # Monitor specific device"
    echo "  ESPPORT=/dev/ttyACM1 ./flash_app.sh flash         # Use environment variable"
    echo ""
    echo "  # Information commands"
    echo "  ./flash_app.sh list                               # List all available apps"
    echo "  ./flash_app.sh ports                              # List available ESP32 devices"
    echo "  ./flash_app.sh --help                             # Show this help"
    echo ""
    echo "DEFAULTS:"
    echo "  Default app:        $CONFIG_DEFAULT_APP"
    echo "  Default build type: $CONFIG_DEFAULT_BUILD_TYPE"
    echo "  Default operation:  flash_monitor"
    echo ""
    echo "For detailed information, see: docs/README_FLASH_SYSTEM.md"
    exit 0
}

# Configuration
LOG_DIR="$PROJECT_DIR/logs"

# Parse logging flags first and filter them out
ENABLE_LOGGING=false
CUSTOM_LOG_NAME=""
FILTERED_ARGS=()

i=1
while [[ $i -le $# ]]; do
    arg="${!i}"
    case "$arg" in
        --log)
            ENABLE_LOGGING=true
            # Check if next argument exists and is not another flag (custom log name)
            if [[ $((i+1)) -le $# ]] && [[ "${!((i+1))}" != -* ]]; then
                CUSTOM_LOG_NAME="${!((i+1))}"
                ((i++))  # Skip the next argument since we consumed it
            fi
            ;;
        *)
            FILTERED_ARGS+=("$arg")
            ;;
    esac
    ((i++))
done

# Replace $@ with filtered arguments for the rest of the script
set -- "${FILTERED_ARGS[@]}"

# Parse arguments (now without --log flags)
case $# in
0)
    # No arguments - use defaults
    OPERATION="flash_monitor"
    APP_TYPE=$CONFIG_DEFAULT_APP
    BUILD_TYPE=$CONFIG_DEFAULT_BUILD_TYPE
    IDF_VERSION=$CONFIG_DEFAULT_IDF_VERSION
    ;;
1)
    # One argument - could be operation or app type
    if [[ "$1" =~ ^(flash|flash_monitor|monitor|size|list|ports)$ ]]; then
        # It's an operation
        OPERATION="$1"
        if [ "$OPERATION" != "monitor" ] && [ "$OPERATION" != "list" ] && [ "$OPERATION" != "ports" ]; then
            # Flash and size operations need app, build type, and IDF version
            APP_TYPE=$CONFIG_DEFAULT_APP
            BUILD_TYPE=$CONFIG_DEFAULT_BUILD_TYPE
            IDF_VERSION=$CONFIG_DEFAULT_IDF_VERSION
        fi
    else
        # It's an app type, use default operation
        OPERATION="flash_monitor"
        APP_TYPE="$1"
        BUILD_TYPE=$CONFIG_DEFAULT_BUILD_TYPE
        IDF_VERSION=$CONFIG_DEFAULT_IDF_VERSION
    fi
    ;;
2)
    # Two arguments
    if [[ "$1" =~ ^(flash|flash_monitor|monitor|size|list|ports)$ ]]; then
        # First is operation, second is app type
        OPERATION="$1"
        if [ "$OPERATION" != "monitor" ] && [ "$OPERATION" != "list" ] && [ "$OPERATION" != "ports" ]; then
            APP_TYPE="$2"
            BUILD_TYPE=$CONFIG_DEFAULT_BUILD_TYPE
            IDF_VERSION=$CONFIG_DEFAULT_IDF_VERSION
        fi
    else
        # First is app type, second is build type
        OPERATION="flash_monitor"
        APP_TYPE="$1"
        BUILD_TYPE="$2"
        IDF_VERSION=$CONFIG_DEFAULT_IDF_VERSION
    fi
    ;;
3)
    # Three arguments
    if [[ "$1" =~ ^(flash|flash_monitor|monitor|size|list|ports)$ ]]; then
        # First is operation, second is app type, third is build type
        OPERATION="$1"
        if [ "$OPERATION" != "monitor" ] && [ "$OPERATION" != "list" ] && [ "$OPERATION" != "ports" ]; then
            APP_TYPE="$2"
            BUILD_TYPE="$3"
            IDF_VERSION=$CONFIG_DEFAULT_IDF_VERSION
        fi
    else
        # Check if third argument is an operation
        if [[ "$3" =~ ^(flash|flash_monitor|monitor|size|list|ports)$ ]]; then
            # App-first format: app build_type operation
            OPERATION="$3"
            APP_TYPE="$1"
            BUILD_TYPE="$2"
            IDF_VERSION=$CONFIG_DEFAULT_IDF_VERSION
        else
            # App-first format: app build_type idf_version (default operation)
            OPERATION="flash_monitor"
            APP_TYPE="$1"
            BUILD_TYPE="$2"
            IDF_VERSION="$3"
        fi
    fi
    ;;
4)
    # Four arguments
    if [[ "$1" =~ ^(flash|flash_monitor|monitor|size|list|ports)$ ]]; then
        # Operation-first format: operation app build_type idf_version
        OPERATION="$1"
        if [ "$OPERATION" != "monitor" ] && [ "$OPERATION" != "list" ] && [ "$OPERATION" != "ports" ]; then
            APP_TYPE="$2"
            BUILD_TYPE="$3"
            IDF_VERSION="$4"
        fi
    else
        # App-first format: app build_type idf_version operation
        OPERATION="$4"
        APP_TYPE="$1"
        BUILD_TYPE="$2"
        IDF_VERSION="$3"
    fi
    ;;
*)
    echo "ERROR: Too many arguments (after filtering --log flags)"
    echo "Usage: ./flash_app.sh [operation] [app] [build_type] [idf_version] [--log [name]]"
    exit 1
    ;;
esac

# Handle special commands
if [ "$1" = "--help" ] || [ "$1" = "-h" ]; then
    show_help
    exit 0
fi

if [ "$OPERATION" = "list" ]; then
    echo "=== Available App Types ==="
    echo "Featured apps:"
    for app in $(get_featured_app_types); do
        description=$(get_app_description "$app")
        echo "  $app - $description"
    done
    echo ""
    echo "All apps:"
    for app in $(get_app_types); do
        description=$(get_app_description "$app")
        echo "  $app - $description"
    done
    echo ""
    echo "Build types: $(get_build_types)"
    echo "ESP-IDF versions: $(get_idf_versions)"  # NEW: Show available ESP-IDF versions
    echo "Operations: flash, flash_monitor, monitor"
    echo ""
    echo "Operation details:"
    echo "  <operation> [app_type] [build_type] [idf_version] [--log [log_name]]"
    echo ""
    echo "Operations:"
    echo "  flash [app] [build_type] [idf_version]     - Flash firmware only (app/build type/idf version defaulted if not specified)"
    echo "  flash_monitor [app] [build_type] [idf_version] - Flash and monitor (app/build type/idf version defaulted if not specified)"
    echo "  monitor                          - Monitor existing firmware (no app/build type/idf version needed)"
    echo ""
    echo "Parameter order:"
    echo "  Operation-first (recommended):   ./flash_app.sh <operation> [app] [build_type] [idf_version] [--log [name]]"
    echo "  Legacy format:                   ./flash_app.sh [app] [build_type] [idf_version] [operation] [--log [name]]"
    echo ""
    echo "Logging options:"
    echo "  --log [name]     Enable logging with optional custom name"
    echo "  Examples:"
    echo "    ./flash_app.sh flash gpio Release release/v5.5 --log                    # Flash with logging"
    echo "    ./flash_app.sh flash_monitor gpio Release release/v5.5 --log            # Flash and monitor with logging"
    echo "    ./flash_app.sh flash gpio Release --log                                 # Flash with default IDF version and logging"
    echo "    ./flash_app.sh flash gpio --log                                         # Flash with default build type, IDF version and logging"
    echo "    ./flash_app.sh flash --log                                              # Flash with defaults and logging"
    echo "    ./flash_app.sh monitor --log                                            # Monitor with logging"
    echo "    ./flash_app.sh monitor --log debug_session                              # Monitor with custom log name"
    exit 0
fi

# Handle ports command - list available ESP32 devices
if [ "$OPERATION" = "ports" ]; then
    echo "=== Available ESP32 Serial Ports ==="
    echo ""
    
    # Detect OS
    case "$(uname -s)" in
        Darwin*)    OS_TYPE="macos" ;;
        Linux*)     OS_TYPE="linux" ;;
        *)          OS_TYPE="unknown" ;;
    esac
    
    # Find all ESP32 devices
    declare -a PORTS=()
    declare -a PORT_INFO=()
    
    case "$OS_TYPE" in
        "linux")
            for port in /dev/ttyACM* /dev/ttyUSB*; do
                if [ -e "$port" ]; then
                    PORTS+=("$port")
                    # Get device info using udevadm if available
                    if command -v udevadm &> /dev/null; then
                        vendor=$(udevadm info --name="$port" 2>/dev/null | grep "ID_VENDOR=" | cut -d= -f2)
                        model=$(udevadm info --name="$port" 2>/dev/null | grep "ID_MODEL=" | cut -d= -f2)
                        serial=$(udevadm info --name="$port" 2>/dev/null | grep "ID_SERIAL_SHORT=" | cut -d= -f2)
                        if [ -n "$model" ]; then
                            PORT_INFO+=("$vendor $model${serial:+ (S/N: $serial)}")
                        else
                            PORT_INFO+=("Unknown device")
                        fi
                    else
                        PORT_INFO+=("(install udevadm for device info)")
                    fi
                fi
            done
            ;;
        "macos")
            for pattern in "usbmodem" "usbserial" "SLAB_USBtoUART" "CP210" "CH340"; do
                for port in /dev/cu.$pattern* /dev/tty.$pattern*; do
                    if [ -e "$port" ]; then
                        PORTS+=("$port")
                        PORT_INFO+=("USB Serial Device")
                    fi
                done
            done
            ;;
    esac
    
    if [ ${#PORTS[@]} -eq 0 ]; then
        echo "No ESP32 devices found!"
        echo ""
        echo "Troubleshooting:"
        echo "  1. Check if device is connected via USB"
        echo "  2. Try: ls /dev/ttyACM* /dev/ttyUSB*"
        echo "  3. Check: dmesg | tail (after connecting)"
        echo "  4. Run: ./detect_ports.sh --verbose"
        exit 1
    fi
    
    echo "Found ${#PORTS[@]} device(s):"
    echo ""
    
    for i in "${!PORTS[@]}"; do
        port="${PORTS[$i]}"
        info="${PORT_INFO[$i]}"
        access=""
        
        # Check permissions
        if [ -r "$port" ] && [ -w "$port" ]; then
            access="✓ accessible"
        else
            access="✗ permission denied (run: sudo chmod 666 $port)"
        fi
        
        printf "  [%d] %s\n" "$((i+1))" "$port"
        printf "      Device: %s\n" "$info"
        printf "      Status: %s\n" "$access"
        echo ""
    done
    
    echo "Usage examples:"
    echo "  ./flash_app.sh --port ${PORTS[0]} flash              # Flash to first device"
    if [ ${#PORTS[@]} -gt 1 ]; then
        echo "  ./flash_app.sh --port ${PORTS[1]} flash              # Flash to second device"
    fi
    echo "  ./flash_app.sh -p ${PORTS[0]} monitor                # Monitor first device"
    echo "  ESPPORT=${PORTS[0]} ./flash_app.sh flash             # Use environment variable"
    echo ""
    echo "Tip: Use tab completion with --port for quick selection!"
    exit 0
fi

# Handle monitor command as standalone operation
if [ "$OPERATION" = "monitor" ]; then
    # Monitor command - no app type or build type needed
    # Don't set fake values, just leave them empty for monitor-only operations
    :
fi

# NEW: Validate ESP-IDF version and build type compatibility for flash and size operations
if [[ "$OPERATION" =~ ^(flash|flash_monitor|size)$ ]] && [[ -n "$APP_TYPE" ]]; then
    # Validate combination using enhanced function
    if ! is_valid_combination "$APP_TYPE" "$BUILD_TYPE" "$IDF_VERSION"; then
        echo "ERROR: Invalid combination: $APP_TYPE + $BUILD_TYPE + $IDF_VERSION"
        echo "Supported versions for '$APP_TYPE': $(get_app_idf_versions "$APP_TYPE")"
        echo "Supported build types for '$APP_TYPE': $(get_build_types "$APP_TYPE")"
        echo "Global ESP-IDF versions: $(get_idf_versions)"
        echo "Global build types: $(get_build_types)"
        exit 1
    fi
fi

# Ensure ESP-IDF environment is sourced
if [ -z "$IDF_PATH" ] || ! command -v idf.py &> /dev/null; then
    echo "ESP-IDF environment not found, attempting to source..."
    if [ -f "$HOME/esp/esp-idf/export.sh" ]; then
        source "$HOME/esp/esp-idf/export.sh"
        echo "ESP-IDF environment sourced successfully"
    else
        echo "ERROR: ESP-IDF export.sh not found at $HOME/esp/esp-idf/export.sh"
        echo "Please ensure ESP-IDF is installed and IDF_PATH is set"
        exit 1
    fi
fi

# Ensure ESP32-C6 target is set
export IDF_TARGET=$CONFIG_TARGET

echo "=== ESP32 HardFOC Interface Wrapper Flash System ==="
echo "Project Directory: $PROJECT_DIR"
echo "App Type: $APP_TYPE"
echo "Build Type: $BUILD_TYPE"
echo "Operation: $OPERATION"
echo "Target: $IDF_TARGET"
if [ "$ENABLE_LOGGING" = true ]; then
    echo "Logging: ENABLED"
    echo "Log Directory: $LOG_DIR"
    if [ -n "$CUSTOM_LOG_NAME" ]; then
        echo "Custom Log Name: $CUSTOM_LOG_NAME"
    fi
else
    echo "Logging: DISABLED"
fi
echo "======================================================"

# Validate app type (skip for monitor operation)
if [ "$OPERATION" != "monitor" ]; then
    if is_valid_app_type "$APP_TYPE"; then
        echo "Valid app type: $APP_TYPE"
        description=$(get_app_description "$APP_TYPE")
        echo "Description: $description"
    else
        echo "ERROR: Invalid app type: $APP_TYPE"
        echo "Available types: $(get_app_types)"
        echo "Use './flash_app.sh list' to see all apps with descriptions"
        exit 1
    fi

    # Validate build type
    if is_valid_build_type "$BUILD_TYPE" "$APP_TYPE"; then
        echo "Valid build type: $BUILD_TYPE"
    else
        echo "ERROR: Invalid build type: $BUILD_TYPE"
        echo "Available types for $APP_TYPE: $(get_build_types "$APP_TYPE")"
        exit 1
    fi
else
    echo "Monitor operation - no app type or build type validation needed"
fi

# Validate operation
case $OPERATION in
    flash|monitor|flash_monitor|size)
        echo "Valid operation: $OPERATION"
        ;;
    *)
        echo "ERROR: Invalid operation: $OPERATION"
        echo "Available operations: flash, monitor, flash_monitor, size"
        exit 1
        ;;
esac

# Switch to project directory
cd "$PROJECT_DIR"

# Set build directory using configuration (same logic as build_app.sh)
if [ "$OPERATION" != "monitor" ]; then
    BUILD_DIR=$(get_build_directory "$APP_TYPE" "$BUILD_TYPE" "$IDF_TARGET" "$IDF_VERSION")
    echo "Build directory: $BUILD_DIR"

    # Get project information using configuration
    PROJECT_NAME=$(get_project_name "$APP_TYPE")
    BIN_FILE="$BUILD_DIR/$PROJECT_NAME.bin"
    echo "Expected binary: $BIN_FILE"
    echo "Project name: $PROJECT_NAME"
else
    # Monitor operation - no build directory needed for monitor-only
    # idf.py monitor only needs a serial port, not a build directory
    BUILD_DIR=""
    PROJECT_NAME="monitor_only"
    BIN_FILE=""
    echo "Monitor operation - no build directory needed"
fi

# Check if build exists and is valid (skip for monitor operation)
if [ "$OPERATION" != "monitor" ] && [ "$OPERATION" != "size" ]; then
    BUILD_EXISTS=false
    if [ -d "$BUILD_DIR" ]; then
        # Check for multiple indicators of a valid build
        if [ -f "$BIN_FILE" ]; then
            echo "Found existing build with main binary: $BIN_FILE"
            BUILD_EXISTS=true
        elif [ -f "$BUILD_DIR/bootloader/bootloader.bin" ]; then
            echo "Found existing build with bootloader binary"
            BUILD_EXISTS=true
        elif [ -f "$BUILD_DIR/.bin_timestamp" ] && [ -f "$BUILD_DIR/build.ninja" ]; then
            echo "Found existing build with build artifacts (.bin_timestamp and build.ninja)"
            BUILD_EXISTS=true
        elif [ -f "$BUILD_DIR/CMakeCache.txt" ] && [ -f "$BUILD_DIR/build.ninja" ]; then
            echo "Found existing build with CMake artifacts"
            BUILD_EXISTS=true
        else
            echo "Build directory exists but no clear indicators of valid build found"
            echo "Checking for any binary files..."
            # Look for any .bin files in the build directory
            if find "$BUILD_DIR" -name "*.bin" -type f | grep -q .; then
                echo "Found .bin files, considering build valid"
                echo "Available .bin files:"
                find "$BUILD_DIR" -name "*.bin" -type f | sed 's/^/  /'
                BUILD_EXISTS=true
            else
                echo "No .bin files found, build may be incomplete"
                echo "Build directory contents:"
                ls -la "$BUILD_DIR" 2>/dev/null | head -20 || echo "Cannot list build directory contents"
            fi
        fi
    else
        echo "No build directory found"
    fi
else
    echo "Monitor operation - no build validation needed"
    BUILD_EXISTS=true
fi

# Auto-build if necessary (skip for monitor and size operations)
if [ "$OPERATION" != "monitor" ] && [ "$OPERATION" != "size" ] && [ "$BUILD_EXISTS" = false ]; then
    echo ""
    echo "======================================================"
    echo "NO VALID BUILD FOUND - STARTING AUTO-BUILD"
    echo "======================================================"
    echo "Building $APP_TYPE ($BUILD_TYPE) before flashing..."
    echo ""
    echo "Build validation failed because:"
    if [ ! -d "$BUILD_DIR" ]; then
        echo "  - Build directory does not exist: $BUILD_DIR"
    else
        echo "  - Build directory exists but validation failed"
        echo "  - Expected binary: $BIN_FILE"
        echo "  - Build directory: $BUILD_DIR"
    fi
    echo ""
    
    # Clean any existing incomplete build
    if [ -d "$BUILD_DIR" ]; then
        echo "Cleaning incomplete build..."
        rm -rf "$BUILD_DIR"
    fi
    
    # Use build_app.sh as the single source of truth for building
    echo "Calling build_app.sh to ensure consistent build process..."
    echo ""
    
    # Call build_app.sh with the same parameters
    if ! "$(dirname "${BASH_SOURCE[0]}")/build_app.sh" "$APP_TYPE" "$BUILD_TYPE"; then
        echo "ERROR: Build failed - see build_app.sh output above"
        exit 1
    fi
    
    echo "Build completed successfully via build_app.sh!"
    echo "=================================================="
elif [ "$OPERATION" = "monitor" ]; then
    echo "Monitor operation - no build needed"
else
    echo "Using existing build in $BUILD_DIR"
fi

# Verify binary exists after build (skip for monitor operation)
if [ "$OPERATION" != "monitor" ] && [ ! -f "$BIN_FILE" ] && [ ! -f "$BUILD_DIR/bootloader/bootloader.bin" ]; then
    echo "ERROR: No valid binary found after build attempt"
    echo "Expected: $BIN_FILE"
    echo "Build directory contents:"
    ls -la "$BUILD_DIR" 2>/dev/null || echo "Build directory not accessible"
    exit 1
fi

# Smart port detection and permission handling
echo ""
echo "======================================================"
echo "SMART PORT DETECTION AND PERMISSION HANDLING"
echo "======================================================"

# Function to detect operating system
detect_os() {
    case "$(uname -s)" in
        Darwin*)    echo "macos" ;;
        Linux*)     echo "linux" ;;
        CYGWIN*|MINGW*|MSYS*) echo "windows" ;;
        *)          echo "unknown" ;;
    esac
}

# Function to find ESP32 devices using system-specific methods
find_esp32_devices() {
    local os=$(detect_os)
    local devices=()
    
    case "$os" in
        "macos")
            # macOS: Look for ESP32 devices in /dev/cu.* (callout devices are better for serial)
            # ESP32 devices typically appear as usbmodem, usbserial, or similar
            for port in /dev/cu.usbmodem* /dev/cu.usbserial* /dev/cu.SLAB_USBtoUART* /dev/cu.CP210* /dev/cu.CH340*; do
                if [ -e "$port" ]; then
                    devices+=("$port")
                fi
            done
            
            # Also check /dev/tty.* as fallback
            for port in /dev/tty.usbmodem* /dev/tty.usbserial* /dev/tty.SLAB_USBtoUART* /dev/tty.CP210* /dev/tty.CH340*; do
                if [ -e "$port" ]; then
                    devices+=("$port")
                fi
            done
            ;;
            
        "linux")
            # Linux: Check for ESP32 devices in /dev/ttyACM* and /dev/ttyUSB*
            # ESP32-C6 typically uses /dev/ttyACM*, older ESP32 uses /dev/ttyUSB*
            for port in /dev/ttyACM* /dev/ttyUSB*; do
                if [ -e "$port" ]; then
                    devices+=("$port")
                fi
            done
            
            # Also check for specific ESP32 device names if available
            if command -v lsusb &> /dev/null; then
                # Look for ESP32-related USB devices
                if lsusb | grep -q "Silicon Labs\|CP210\|CH340\|ESP\|Espressif"; then
                    # Silent detection - no output needed here
                    :
                fi
            fi
            ;;
            
        *)
            # Silent warning for unsupported OS
            :
            ;;
    esac
    
    # Return devices as space-separated string
    echo "${devices[@]}"
}

# Function to find the best available port
find_best_port() {
    local os=$(detect_os)
    local esp32_devices=($(find_esp32_devices))
    local fallback_ports=()
    
    # First priority: ESP32-specific devices
    if [ ${#esp32_devices[@]} -gt 0 ]; then
        # Prefer callout devices on macOS (cu.* over tty.*)
        if [ "$os" = "macos" ]; then
            for port in "${esp32_devices[@]}"; do
                if [[ "$port" == "/dev/cu."* ]]; then
                    echo "$port"
                    return 0
                fi
            done
        fi
        
        # Use first ESP32 device found
        echo "${esp32_devices[0]}"
        return 0
    fi
    
    # Second priority: Fallback to common serial ports
    case "$os" in
        "macos")
            # macOS fallback ports
            for port in /dev/cu.usbmodem* /dev/cu.usbserial* /dev/cu.*; do
                if [ -e "$port" ] && [[ "$port" != "/dev/cu.Bluetooth"* ]] && [[ "$port" != "/dev/cu.debug"* ]] && [[ "$port" != "/dev/cu.wlan"* ]]; then
                    fallback_ports+=("$port")
                fi
            done
            ;;
        "linux")
            # Linux fallback ports
            for port in /dev/ttyACM* /dev/ttyUSB* /dev/ttyS*; do
                if [ -e "$port" ]; then
                    fallback_ports+=("$port")
                fi
            done
            ;;
    esac
    
    if [ ${#fallback_ports[@]} -gt 0 ]; then
        echo "Using fallback port: ${fallback_ports[0]}"
        echo "${fallback_ports[0]}"
        return 0
    fi
    
    # No ports found
    return 1
}

# Function to fix port permissions (Linux-specific)
fix_port_permissions() {
    local port="$1"
    local os=$(detect_os)
    
    if [ "$os" = "linux" ] && [ -e "$port" ]; then
        # Check if user can access the port
        if ! [ -r "$port" ]; then
            echo "Fixing permissions for $port..."
            sudo chmod 666 "$port" 2>/dev/null || {
                echo "WARNING: Could not fix permissions for $port"
                echo "You may need to run: sudo chmod 666 $port"
                echo "Or add your user to the dialout group: sudo usermod -a -G dialout $USER"
            }
        fi
    elif [ "$os" = "macos" ]; then
        # On macOS, permissions are usually handled by system
        echo "macOS detected - checking port accessibility..."
        if [ -r "$port" ]; then
            echo "Port $port is accessible"
        else
            echo "WARNING: Port $port is not accessible"
            echo "This might be a permission issue. Try:"
            echo "  - Disconnecting and reconnecting the device"
            echo "  - Checking System Preferences > Security & Privacy > Privacy > Full Disk Access"
        fi
    fi
}

# Function to validate port and get device info
validate_port() {
    local port="$1"
    local os=$(detect_os)
    
    if [ ! -e "$port" ]; then
        echo "ERROR: Port $port does not exist"
        return 1
    fi
    
    if [ ! -r "$port" ]; then
        echo "ERROR: Port $port is not readable"
        return 1
    fi
    
    # Try to get device info
    case "$os" in
        "macos")
            if [[ "$port" == "/dev/cu."* ]] || [[ "$port" == "/dev/tty."* ]]; then
                echo "Port $port appears to be a valid serial device"
                return 0
            fi
            ;;
        "linux")
            if [[ "$port" == "/dev/tty"* ]]; then
                echo "Port $port appears to be a valid serial device"
                return 0
            fi
            ;;
    esac
    
    echo "WARNING: Port $port format is unexpected for this OS"
    return 0  # Still allow it as it might work
}

# Function to setup logging directory and generate log filename
setup_logging() {
    if [ "$ENABLE_LOGGING" != true ]; then
        return 0
    fi
    
    # Create logs directory if it doesn't exist
    if [ ! -d "$LOG_DIR" ]; then
        mkdir -p "$LOG_DIR"
        echo "Created log directory: $LOG_DIR"
    fi
    
    # Generate timestamp for log filename
    local timestamp=$(date +"%Y%m%d_%H%M%S")
    local date_str=$(date +"%Y-%m-%d %H:%M:%S")
    
    # Generate log filename
    if [ -n "$CUSTOM_LOG_NAME" ]; then
        # Use custom name with timestamp
        LOG_FILENAME="${CUSTOM_LOG_NAME}_${timestamp}.log"
    else
        # Auto-generate name based on operation and timestamp
        if [ "$OPERATION" = "monitor" ]; then
            LOG_FILENAME="monitor_${timestamp}.log"
        else
            LOG_FILENAME="${APP_TYPE}_${BUILD_TYPE}_${timestamp}.log"
        fi
    fi
    
    LOG_FILEPATH="$LOG_DIR/$LOG_FILENAME"
    
    # Add header to log file
    {
        echo "======================================================"
        echo "ESP32 HardFOC Interface Wrapper - Monitor Log"
        echo "======================================================"
        echo "Date: $date_str"
        if [ "$OPERATION" != "monitor" ]; then
            echo "App Type: $APP_TYPE"
            echo "Build Type: $BUILD_TYPE"
        else
            echo "Operation: Monitor Only (existing firmware)"
        fi
        echo "Operation: $OPERATION"
        echo "Target: $IDF_TARGET"
        echo "Port: $BEST_PORT"
        if [ "$OPERATION" != "monitor" ]; then
            echo "Build Directory: $BUILD_DIR"
            echo "Project Name: $PROJECT_NAME"
        fi
        echo "======================================================"
        echo ""
    } > "$LOG_FILEPATH"
    
    echo "Logging enabled: $LOG_FILEPATH"
    echo "Log file created with header information"
}

# Function to cleanup old log files (keep last 50 logs)
cleanup_old_logs() {
    if [ "$ENABLE_LOGGING" != true ] || [ ! -d "$LOG_DIR" ]; then
        return 0
    fi
    
    local max_logs=50
    local current_logs=$(find "$LOG_DIR" -name "*.log" -type f | wc -l)
    
    if [ "$current_logs" -gt "$max_logs" ]; then
        echo "Cleaning up old log files (keeping last $max_logs)..."
        
        # Find all log files, sort by modification time (newest first), keep only the latest ones
        find "$LOG_DIR" -name "*.log" -type f -printf '%T@ %p\n' | \
            sort -nr | \
            tail -n +$((max_logs + 1)) | \
            cut -d' ' -f2- | \
            xargs -r rm -f
        
        echo "Log cleanup completed"
    fi
}

# Find and configure the best available port (skip for size operations)
if [ "$OPERATION" != "size" ]; then
    
    # Priority 1: Explicit port from --port flag
    if [ -n "$EXPLICIT_PORT" ]; then
        echo "Using explicitly specified port: $EXPLICIT_PORT"
        if [ ! -e "$EXPLICIT_PORT" ]; then
            echo "ERROR: Specified port does not exist: $EXPLICIT_PORT"
            echo ""
            echo "Available ports:"
            ./flash_app.sh ports 2>/dev/null || ls /dev/ttyACM* /dev/ttyUSB* 2>/dev/null || echo "  No ports found"
            exit 1
        fi
        BEST_PORT="$EXPLICIT_PORT"
    
    # Priority 2: ESPPORT environment variable
    elif [ -n "$ESPPORT" ]; then
        echo "Using ESPPORT environment variable: $ESPPORT"
        if [ ! -e "$ESPPORT" ]; then
            echo "ERROR: ESPPORT port does not exist: $ESPPORT"
            echo ""
            echo "Available ports:"
            ls /dev/ttyACM* /dev/ttyUSB* 2>/dev/null || echo "  No ports found"
            exit 1
        fi
        BEST_PORT="$ESPPORT"
    
    # Priority 3: Auto-detect with interactive selection for multiple devices
    else
        echo "Searching for ESP32 devices..."
        
        # Get all available devices
        all_devices=($(find_esp32_devices))
        
        if [ ${#all_devices[@]} -eq 0 ]; then
            echo ""
            echo "ERROR: No suitable serial ports found!"
            echo ""
            echo "Troubleshooting steps:"
            echo "1. Ensure your ESP32 device is connected via USB"
            echo "2. Check if the device appears in your system:"
            
            case "$(detect_os)" in
                "macos")
                    echo "   - System Information > USB"
                    echo "   - Terminal: ls /dev/cu.* /dev/tty.*"
                    echo "   - Look for usbmodem, usbserial, or similar devices"
                    ;;
                "linux")
                    echo "   - lsusb (if available)"
                    echo "   - ls /dev/ttyACM* /dev/ttyUSB*"
                    echo "   - dmesg | tail (after connecting device)"
                    ;;
            esac
            
            echo "3. Try disconnecting and reconnecting the device"
            echo "4. Check if you need to install USB-to-UART drivers"
            echo "5. Ensure the device is not being used by another application"
            echo ""
            
            echo "You can also manually specify a port:"
            echo "  ./flash_app.sh --port /dev/ttyACM0 flash"
            echo "  export ESPPORT=/dev/your_port_here"
            echo ""
            echo "Run ./flash_app.sh ports to see all available devices"
            exit 1
        
        elif [ ${#all_devices[@]} -eq 1 ]; then
            # Only one device found - use it automatically
            BEST_PORT="${all_devices[0]}"
            echo "Found single ESP32 device: $BEST_PORT"
        
        else
            # Multiple devices found - show selection
            echo ""
            echo "Multiple ESP32 devices found!"
            echo ""
            
            for i in "${!all_devices[@]}"; do
                dev_port="${all_devices[$i]}"
                dev_info=""
                
                # Get device info on Linux
                if command -v udevadm &> /dev/null; then
                    dev_model=$(udevadm info --name="$dev_port" 2>/dev/null | grep "ID_MODEL=" | cut -d= -f2)
                    if [ -n "$dev_model" ]; then
                        dev_info=" ($dev_model)"
                    fi
                fi
                
                printf "  [%d] %s%s\n" "$((i+1))" "$dev_port" "$dev_info"
            done
            
            echo ""
            echo "Options:"
            echo "  - Enter a number (1-${#all_devices[@]}) to select a device"
            echo "  - Press Enter to use the first device"
            echo "  - Use --port flag to skip this prompt: ./flash_app.sh --port /dev/ttyACM1 flash"
            echo ""
            
            # Read user selection with timeout
            read -t 30 -p "Select device [1]: " selection
            
            if [ -z "$selection" ]; then
                selection=1
            fi
            
            # Validate selection
            if [[ "$selection" =~ ^[0-9]+$ ]] && [ "$selection" -ge 1 ] && [ "$selection" -le ${#all_devices[@]} ]; then
                BEST_PORT="${all_devices[$((selection-1))]}"
                echo "Selected: $BEST_PORT"
            else
                echo "Invalid selection. Using first device: ${all_devices[0]}"
                BEST_PORT="${all_devices[0]}"
            fi
        fi
    fi

    echo "Using port: $BEST_PORT"

    # Validate the port
    if ! validate_port "$BEST_PORT"; then
        echo "ERROR: Port validation failed for $BEST_PORT"
        exit 1
    fi

    # Fix permissions if needed
    fix_port_permissions "$BEST_PORT"

    # Set the port for ESP-IDF
    export ESPPORT="$BEST_PORT"
    echo "Using port: $ESPPORT"
else
    # For size operations, set a dummy port to avoid errors
    BEST_PORT="size_operation"
    echo "Size operation - no port detection needed"
fi

# Setup logging if enabled
if [ "$ENABLE_LOGGING" = true ]; then
    echo ""
    echo "======================================================"
    echo "SETTING UP LOGGING"
    echo "======================================================"
    setup_logging
    cleanup_old_logs
    echo "======================================================"
fi

# Execute the requested operation
echo ""
echo "======================================================"
echo "EXECUTING OPERATION: $OPERATION"
echo "======================================================"

case $OPERATION in
    flash)
        echo "Flashing $APP_TYPE app to $BEST_PORT..."
        if ! idf.py -B "$BUILD_DIR" -p "$BEST_PORT" flash; then
            echo "ERROR: Flash operation failed"
            exit 1
        fi
        echo "Flash completed successfully!"
        ;;
    monitor)
        echo "Starting monitor on $BEST_PORT..."
        echo "Press Ctrl+] to exit monitor"
        
        # Find an existing build directory for symbol decoding (optional but helpful)
        MONITOR_BUILD_DIR=""
        if [ -d "$PROJECT_DIR/builds" ]; then
            # Find the most recently modified build directory
            MONITOR_BUILD_DIR=$(find "$PROJECT_DIR/builds" -maxdepth 1 -type d -name "build-*" -printf '%T@ %p\n' 2>/dev/null | sort -rn | head -1 | cut -d' ' -f2-)
        fi
        
        if [ -n "$MONITOR_BUILD_DIR" ] && [ -d "$MONITOR_BUILD_DIR" ]; then
            echo "Using build directory for symbol decoding: $(basename "$MONITOR_BUILD_DIR")"
            MONITOR_CMD="idf.py -B \"$MONITOR_BUILD_DIR\" -p \"$BEST_PORT\" monitor"
        else
            echo "No existing build found - using basic serial monitor (no symbol decoding)"
            # Use idf_monitor.py directly for basic monitoring without project configuration
            if [ -n "$IDF_PATH" ] && [ -f "$IDF_PATH/tools/idf_monitor.py" ]; then
                MONITOR_CMD="python \"$IDF_PATH/tools/idf_monitor.py\" -p \"$BEST_PORT\""
            else
                # Fallback to simple serial monitor if idf_monitor.py not found
                echo "Note: For full symbol decoding, build an app first with: ./build_app.sh <app> <type>"
                MONITOR_CMD="python -m serial.tools.miniterm \"$BEST_PORT\" 115200"
            fi
        fi
        
        if [ "$ENABLE_LOGGING" = true ]; then
            echo "Monitor output will be logged to: $LOG_FILEPATH"
            echo "Note: Using tee to capture output"
            if ! eval $MONITOR_CMD 2>&1 | tee -a "$LOG_FILEPATH"; then
                echo "ERROR: Monitor operation failed"
                exit 1
            fi
        else
            if ! eval $MONITOR_CMD; then
                echo "ERROR: Monitor operation failed"
                exit 1
            fi
        fi
        ;;
    flash_monitor)
        echo "Flashing and monitoring $APP_TYPE app on $BEST_PORT..."
        echo "Press Ctrl+] to exit monitor after flashing"
        if [ "$ENABLE_LOGGING" = true ]; then
            echo "Monitor output will be logged to: $LOG_FILEPATH"
            echo "Note: Using tee to capture output (--log-file not available in this ESP-IDF version)"
            # Flash first, then monitor with logging
            if ! idf.py -B "$BUILD_DIR" -p "$BEST_PORT" flash; then
                echo "ERROR: Flash operation failed"
                exit 1
            fi
            echo "Flash completed successfully! Starting monitor with logging..."
            if ! idf.py -B "$BUILD_DIR" -p "$BEST_PORT" monitor 2>&1 | tee -a "$LOG_FILEPATH"; then
                echo "ERROR: Monitor operation failed"
                exit 1
            fi
        else
            if ! idf.py -B "$BUILD_DIR" -p "$BEST_PORT" flash monitor; then
                echo "ERROR: Flash and monitor operation failed"
                exit 1
            fi
        fi
        ;;
    size)
        echo "Showing size information for $APP_TYPE app..."
        echo "Build directory: $BUILD_DIR"
        echo "Project name: $PROJECT_NAME"
        echo ""
        
        # Check if build directory exists
        if [ ! -d "$BUILD_DIR" ]; then
            echo "ERROR: Build directory not found: $BUILD_DIR"
            echo "Please build the project first using: ./build_app.sh $APP_TYPE $BUILD_TYPE $IDF_VERSION"
            exit 1
        fi
        
        # Show size information using idf.py
        echo "=== Firmware Size Analysis ==="
        if ! idf.py -B "$BUILD_DIR" size; then
            echo "ERROR: Size analysis failed"
            exit 1
        fi
        
        echo ""
        echo "=== Component Size Breakdown ==="
        if ! idf.py -B "$BUILD_DIR" size-components; then
            echo "WARNING: Component size breakdown failed"
        fi
        
        echo ""
        echo "=== Memory Usage Summary ==="
        if ! idf.py -B "$BUILD_DIR" size --format json | python3 -m json.tool 2>/dev/null | grep -E "(used|total|available)" || echo "JSON format not available"; then
            echo "WARNING: Memory usage summary failed"
        fi
        ;;
esac

echo ""
echo "======================================================"
echo "OPERATION COMPLETED SUCCESSFULLY"
echo "======================================================"
if [ "$OPERATION" != "monitor" ]; then
    echo "App Type: $APP_TYPE"
    echo "Build Type: $BUILD_TYPE"
    echo "Build Directory: $BUILD_DIR"
    echo "Project Name: $PROJECT_NAME"
    if [ -f "$BIN_FILE" ]; then
        echo "Binary: $BIN_FILE"
    fi
else
    echo "Operation: Monitor Only (existing firmware)"
fi
echo "Operation: $OPERATION"
echo "Target: $IDF_TARGET"
if [ "$ENABLE_LOGGING" = true ] && [ -n "$LOG_FILEPATH" ]; then
    echo "Log File: $LOG_FILEPATH"
    echo "Log Size: $(du -h "$LOG_FILEPATH" 2>/dev/null | cut -f1 || echo 'Unknown')"
fi
echo ""
echo "Available operations:"
if [ "$OPERATION" != "monitor" ]; then
    echo "  Flash only:        ./flash_app.sh $APP_TYPE $BUILD_TYPE flash"
    echo "  Flash & monitor:   ./flash_app.sh $APP_TYPE $BUILD_TYPE flash_monitor"
    echo "  Size analysis:     ./flash_app.sh $APP_TYPE $BUILD_TYPE size"
    echo "  Build only:        ./build_app.sh $APP_TYPE $BUILD_TYPE"
    echo ""
    echo "Logging options:"
    echo "  Enable logging:    ./flash_app.sh $APP_TYPE $BUILD_TYPE [operation] --log"
    echo "  Custom log name:   ./flash_app.sh $APP_TYPE $BUILD_TYPE [operation] --log custom_name"
    echo "  Examples:"
    echo "    ./flash_app.sh gpio Release flash --log                    # Flash with logging"
    echo "    ./flash_app.sh gpio Release flash_monitor --log            # Flash and monitor with logging"
    echo "    ./flash_app.sh gpio Release size                           # Size analysis"
else
    echo "  Monitor existing firmware: ./flash_app.sh monitor"
    echo "  Monitor with logging:      ./flash_app.sh monitor --log"
    echo "  Monitor with custom name:  ./flash_app.sh monitor --log custom_name"
    echo ""
    echo "Logging options:"
    echo "  Enable logging:    ./flash_app.sh monitor --log"
    echo "  Custom log name:   ./flash_app.sh monitor --log custom_name"
    echo "  Examples:"
    echo "    ./flash_app.sh monitor --log                               # Monitor with logging"
    echo "    ./flash_app.sh monitor --log debug_session                 # Monitor with custom log name"
fi
echo "======================================================"

