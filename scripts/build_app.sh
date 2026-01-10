#!/bin/bash
# Build script for different ESP32 apps (Bash version)
# Usage: ./build_app.sh [app_type] [build_type]
# 
# App types and build types are loaded from app_config.yml
# Use './build_app.sh list' to see all available apps

set -e  # Exit on any error

# Parse arguments: collect non-flag args as positionals
POSITIONAL_ARGS=()
i=1
while [[ $i -le $# ]]; do
	arg="${!i}"
	case "$arg" in
		--clean)
			CLEAN=1
			;;
		--no-clean)
			CLEAN=0
			;;
		--use-cache)
			USE_CCACHE=1
			;;
		--no-cache)
			USE_CCACHE=0
			;;
		--project-path)
			# Check if next argument exists and is not another flag
			next_index=$((i+1))
			if [[ $next_index -le $# ]] && [[ "${!next_index}" != -* ]]; then
				PROJECT_PATH="${!next_index}"
				((i++))  # Skip the next argument since we consumed it
			else
				echo "ERROR: --project-path requires a path argument" >&2
				echo "Usage: --project-path /path/to/project" >&2
				exit 1
			fi
			;;
		-h|--help)
			HELP_REQUESTED=1
			;;
		*)
			POSITIONAL_ARGS+=("$arg")
			;;
	esac
	((i++))
done

# Export PROJECT_PATH if it was set via --project-path flag
if [[ -n "$PROJECT_PATH" ]]; then
    export PROJECT_PATH
fi

# Configuration derived from positionals or config defaults
APP_TYPE=${POSITIONAL_ARGS[0]:-$CONFIG_DEFAULT_APP}
BUILD_TYPE=${POSITIONAL_ARGS[1]:-$CONFIG_DEFAULT_BUILD_TYPE}

# Load configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
source "$SCRIPT_DIR/config_loader.sh"

# Usage helper
print_usage() {
    echo "ESP32 HardFOC Interface Wrapper - Build System"
    echo ""
    echo "Usage: ./build_app.sh [COMMAND] [OPTIONS]"
    echo ""
    echo "COMMANDS:"
    echo "  <app_type> <build_type> [idf_version]  - Build specific app with options"
    echo "  list                                    - List all available apps and build types"
    echo "  info <app_name>                         - Show detailed information for specific app"
    echo "  combinations                            - Show all valid build combinations"
    echo "  validate <app> <type> [idf]            - Validate specific build combination"
    echo ""
    echo "OPTIONS:"
    echo "  --clean                                - Clean build (remove existing build directory)"
    echo "  --no-clean                             - Incremental build (preserve existing build directory)"
    echo "  --use-cache                            - Enable ccache for faster builds (default)"
    echo "  --no-cache                             - Disable ccache"
    echo "  --project-path <path>                  - Path to project directory (allows scripts to be placed anywhere)"
    echo "  -h, --help                             - Show this help message"
    echo ""
    echo "ARGUMENT PATTERNS:"
    echo "  Positional Arguments:"
    echo "    app_type     - Application type (e.g., gpio_test, adc_test)"
    echo "    build_type   - Build configuration (Debug, Release)"
    echo "    idf_version  - ESP-IDF version (e.g., release/v*.*, release/v*.*)"
    echo ""
    echo "  Environment Variables:"
    echo "    PROJECT_PATH - Path to project directory (optional)"
    echo "      - If set, uses this project directory instead of default location"
    echo "      - Allows scripts to be placed anywhere while finding correct project"
    echo "      - Example: PROJECT_PATH=/path/to/project ./build_app.sh"
    echo "    CLEAN        - Set to 1 for clean builds, 0 for incremental"
    echo "    USE_CCACHE   - Set to 1 to enable ccache, 0 to disable"
    echo ""
    echo "EXAMPLES:"
    echo "  # Basic usage with defaults"
    echo "  ./build_app.sh                                    # Use config defaults"
    echo "  ./build_app.sh gpio_test Release                  # Specific app and build type"
    echo "  ./build_app.sh adc_test Debug release/v5.5        # Full specification"
    echo ""
    echo "  # Build options"
    echo "  ./build_app.sh gpio_test Release --clean          # Clean build"
    echo "  ./build_app.sh adc_test Debug --no-cache          # Without cache"
    echo "  ./build_app.sh gpio_test Release --no-clean       # Incremental build"
    echo ""
    echo "  # Information commands"
    echo "  ./build_app.sh list                               # List all apps and types"
    echo "  ./build_app.sh info gpio_test                     # App-specific details"
    echo "  ./build_app.sh combinations                       # Valid combinations"
    echo "  ./build_app.sh validate gpio_test Release         # Validate combination"
    echo ""
    echo "  # Environment-based usage"
    echo "  CLEAN=1 ./build_app.sh gpio_test Release          # Clean build via env var"
    echo "  USE_CCACHE=0 ./build_app.sh adc_test Debug       # No cache via env var"
    echo "  PROJECT_PATH=/path/to/project ./build_app.sh # Custom project location"
    echo ""
    echo "  # Portable usage with --project-path flag"
    echo "  ./build_app.sh --project-path /path/to/project gpio_test Release"
    echo "  ./build_app.sh --project-path ../project adc_test Debug --clean"
    echo ""
    echo "TROUBLESHOOTING:"
    echo "  # Common Build Issues"
    echo "  • Build fails with 'command not found':"
    echo "    - Run: source $SCRIPT_DIR/setup_repo.sh"
    echo "    - Or: get_idf (if ESP-IDF is installed)"
    echo "    - Verify: which idf.py"
    echo ""
    echo "  • Configuration errors:"
    echo "    - Check: ./build_app.sh validate <app> <type>"
    echo "    - Verify: cat app_config.yml"
    echo "    - Test: python3 scripts/generate_matrix.py --validate"
    echo ""
    echo "  • ESP-IDF version issues:"
    echo "    - List versions: ./manage_idf.sh list"
    echo "    - Switch version: ./manage_idf.sh switch release/v5.5"
    echo "    - Export version: source <(./manage_idf.sh export release/v5.5)"
    echo ""
    echo "  • Build directory problems:"
    echo "    - Clean build: ./build_app.sh <app> <type> --clean"
    echo "    - Check permissions: ls -la builds/build-*/"
    echo "    - Remove manually: rm -rf builds/build-*/"
    echo ""
    echo "  # Cache and Performance Issues"
    echo "  • Slow builds:"
    echo "    - Enable ccache: ./build_app.sh <app> <type> --use-cache"
    echo "    - Check ccache: ccache -s"
    echo "    - Clear ccache: ccache -C"
    echo ""
    echo "  • Build failures after ESP-IDF update:"
    echo "    - Clean all builds: rm -rf builds/build-*"
    echo "    - Reinstall ESP-IDF: ./manage_idf.sh install --force"
    echo "    - Verify environment: ./scripts/manage_idf.sh export $IDF_VERSION"
    echo ""
    echo "  # Environment and Path Issues"
    echo "  • PATH not set correctly:"
    echo "    - Source ESP-IDF: source ~/esp/esp-idf/export.sh"
    echo "    - Check variables: echo \$IDF_PATH, echo \$PATH"
    echo "    - Restart terminal after setup"
    echo ""
    echo "  • Permission denied errors:"
    echo "    - Check ownership: ls -la build_*/"
    echo "    - Fix permissions: chmod -R 755 builds/build-*/"
    echo "    - Run as user, not root"
    echo ""
    echo "  # Debugging Commands"
    echo "  • Show build info: ./build_app.sh info <app_name>"
    echo "  • List combinations: ./build_app.sh combinations"
    echo "  • Validate config: ./build_app.sh validate <app> <type>"
    echo "  • Check ESP-IDF: ./scripts/manage_idf.sh export $IDF_VERSION"
    echo "  • Verify target: ./scripts/build_app.sh validate <app> <type>"
    echo ""
    echo "  # Getting Help"
    echo "  • Script help: ./scripts/build_app.sh --help"
    echo "  • ESP-IDF help: ./scripts/manage_idf.sh --help"
    echo "  • Configuration help: python3 scripts/generate_matrix.py --help"
    echo "  • Documentation: docs/README_BUILD_SYSTEM.md"
    echo ""
    echo "BUILD TYPES: Debug, Release"
    echo "ESP-IDF VERSIONS: release/v5.5, release/v5.4"
    echo ""
    echo "For detailed information, see: docs/README_BUILD_SYSTEM.md"
}

# Show help if requested (after config loading)
if [ "$HELP_REQUESTED" = "1" ]; then
    print_usage
    exit 0
fi

# Handle special commands first (before validation)
# Handle special commands
if [ "$APP_TYPE" = "list" ]; then
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
    echo "ESP-IDF versions: $(get_idf_versions)"
    echo ""
    echo "Flags: --clean | --no-clean | --use-cache | --no-cache"
    echo ""
    echo "Additional Commands:"
    echo "  info <app_name>        - Show detailed information for specific app"
    echo "  combinations            - Show all valid build combinations"
    echo "  validate <app> <type> [idf] - Validate specific build combination"
    exit 0
fi

# NEW: Show app-specific information
if [ "$APP_TYPE" = "info" ] && [ -n "${POSITIONAL_ARGS[1]}" ]; then
    app_name="${POSITIONAL_ARGS[1]}"
    echo "=== App Information: $app_name ==="
    show_valid_combinations "$app_name"
    exit 0
fi

# NEW: Show all valid combinations
if [ "$APP_TYPE" = "combinations" ]; then
    echo "=== All Valid Build Combinations ==="
    for app in $(get_app_types); do
        echo ""
        show_valid_combinations "$app"
    done
    exit 0
fi

# NEW: Validate specific combination
if [ "$APP_TYPE" = "validate" ] && [ -n "${POSITIONAL_ARGS[1]}" ] && [ -n "${POSITIONAL_ARGS[2]}" ]; then
    app_name="${POSITIONAL_ARGS[1]}"
    build_type="${POSITIONAL_ARGS[2]}"
    idf_version="${POSITIONAL_ARGS[3]:-$(get_idf_version_for_build_type "$app_name" "$build_type")}"
    
    echo "=== Validating Build Combination ==="
    echo "App: $app_name"
    echo "Build Type: $build_type"
    echo "ESP-IDF Version: $idf_version"
    echo ""
    
    if is_valid_combination "$app_name" "$build_type" "$idf_version"; then
        echo "✅ VALID: This combination is allowed!"
    else
        echo "❌ INVALID: This combination is not allowed!"
        echo ""
        echo "Valid combinations for '$app_name':"
        show_valid_combinations "$app_name"
    fi
    exit 0
fi

# Basic validation after command parsing (before smart default selection)
# Validate app type
if ! is_valid_app_type "$APP_TYPE"; then
    echo "ERROR: Invalid app type: $APP_TYPE"
    echo "Available types: $(get_app_types)"
    echo "Use './build_app.sh list' to see all apps with descriptions"
    exit 1
fi

# Validate build type
if ! is_valid_build_type "$BUILD_TYPE"; then
    echo "ERROR: Invalid build type: $BUILD_TYPE"
    echo "Available types: $(get_build_types)"
    exit 1
fi

# Smart IDF version selection (only after basic validation passes)
if [ -n "${POSITIONAL_ARGS[2]}" ]; then
    IDF_VERSION="${POSITIONAL_ARGS[2]}"
else
    # Use smart default based on app and build type
    source "$SCRIPT_DIR/config_loader.sh"
    IDF_VERSION=$(get_idf_version_for_build_type "$APP_TYPE" "$BUILD_TYPE")
    echo "No IDF version specified, using smart default for $BUILD_TYPE: $IDF_VERSION"
fi

# Ensure ESP-IDF environment is sourced for the specified version
if [ -z "$IDF_PATH" ] || ! command -v idf.py &> /dev/null; then
    echo "ESP-IDF environment not found, attempting to auto-setup version $IDF_VERSION..."
    
    # Source the common setup functions to use export_esp_idf_version
    source "$SCRIPT_DIR/setup_common.sh"
    
    # Try to auto-install and source the required ESP-IDF version
    if export_esp_idf_version "$IDF_VERSION" "true"; then
        echo "ESP-IDF environment sourced successfully for version $IDF_VERSION"
    else
        echo "ERROR: Failed to setup ESP-IDF environment for version $IDF_VERSION"
        echo ""
        echo "Available versions:"
        list_esp_idf_versions
        echo ""
        echo "To manually install required versions, run: ./scripts/setup_repo.sh"
        exit 1
    fi
fi

# Ensure target is set from config
if [[ -z "$CONFIG_TARGET" ]]; then
    # Source config_loader to get target from app_config.yml
    source "$SCRIPT_DIR/config_loader.sh"
    export IDF_TARGET=$(get_target)
    echo "Target set from config: $IDF_TARGET"
else
    export IDF_TARGET="$CONFIG_TARGET"
    echo "Target set from environment: $IDF_TARGET"
fi

echo "=== ESP32 HardFOC Interface Wrapper Build System ==="
echo "Project Directory: $PROJECT_DIR"
echo "App Type: $APP_TYPE"
echo "Build Type: $BUILD_TYPE"
echo "ESP-IDF Version: $IDF_VERSION"  # NEW: Show ESP-IDF version
echo "Target: $CONFIG_TARGET"
echo "Build Directory: $(get_build_directory "$APP_TYPE" "$BUILD_TYPE")"
echo "======================================================="

# Enhanced validation with combination checking
validate_build_combination() {
    local app_type="$1"
    local build_type="$2"
    local idf_version="$3"
    
    echo "Validating build combination: $app_type + $build_type + $idf_version"
    
    # Check if combination is allowed
    if ! is_valid_combination "$app_type" "$build_type" "$idf_version"; then
        echo "❌ ERROR: Invalid build combination!"
        echo ""
        echo "The combination '$app_type + $build_type + $idf_version' is not allowed."
        echo ""
        
        # Show what IS allowed for this app
        echo "✅ Valid combinations for '$app_type':"
        show_valid_combinations "$app_type"
        
        echo ""
        echo "💡 To see all valid combinations:"
        echo "   ./scripts/build_app.sh list"
        echo ""
        echo "💡 To see app-specific options:"
        echo "   ./scripts/build_app.sh info $app_type"
        
        exit 1
    fi
    
    echo "✅ Build combination validated successfully!"
}

# Basic validations moved above - show app info here
echo "Valid app type: $APP_TYPE"
description=$(get_app_description "$APP_TYPE")
echo "Description: $description"
echo "Valid build type: $BUILD_TYPE"

# Validate the complete combination
validate_build_combination "$APP_TYPE" "$BUILD_TYPE" "$IDF_VERSION"

# Set build directory using configuration (includes target and IDF version)
BUILD_DIR=$(get_build_directory "$APP_TYPE" "$BUILD_TYPE" "$IDF_TARGET" "$IDF_VERSION")
echo "Build directory: $BUILD_DIR"

# Clean previous build only if explicitly requested
if [ "$CLEAN" = "1" ] && [ -d "$BUILD_DIR" ]; then
    echo "CLEAN=1 set: removing previous build directory..."
    rm -rf "$BUILD_DIR"
else
    if [ -d "$BUILD_DIR" ]; then
        echo "Incremental build: preserving existing build directory"
    fi
fi

# Get source file from configuration and export as environment variable
echo "Discovering source file for app type: $APP_TYPE"
SOURCE_FILE=$(python3 "$SCRIPT_DIR/get_app_info.py" source_file "$APP_TYPE")
if [ $? -ne 0 ] || [ -z "$SOURCE_FILE" ]; then
    echo "ERROR: Failed to get source file for APP_TYPE: $APP_TYPE"
    exit 1
fi
echo "Source file: $SOURCE_FILE"

# Export source file as environment variable for CMakeLists.txt
export APP_SOURCE_FILE="$SOURCE_FILE"
echo "APP_SOURCE_FILE=$SOURCE_FILE"

# Configure and build with proper error handling
echo "Configuring project for $IDF_TARGET..."

# Change to project directory before running idf.py commands
cd "$PROJECT_DIR"

if ! idf.py -B "$BUILD_DIR" -D CMAKE_BUILD_TYPE="$BUILD_TYPE" -D BUILD_TYPE="$BUILD_TYPE" -D APP_TYPE="$APP_TYPE" -D APP_SOURCE_FILE="$SOURCE_FILE" -D IDF_CCACHE_ENABLE="$USE_CCACHE" reconfigure; then
    echo "ERROR: Configuration failed"
    exit 1
fi

echo "Building project..."
if ! idf.py -B "$BUILD_DIR" build; then
    echo "ERROR: Build failed"
    exit 1
fi

# Get actual binary information using configuration
PROJECT_NAME=$(get_project_name "$APP_TYPE")
BIN_FILE="$BUILD_DIR/$PROJECT_NAME.bin"

# Export build directory for CI and other scripts to use
# This allows CI pipelines and other scripts to know where the build artifacts are located
export ESP32_BUILD_APP_MOST_RECENT_DIRECTORY="$BUILD_DIR"
echo "ESP32_BUILD_APP_MOST_RECENT_DIRECTORY=$BUILD_DIR"

echo "======================================================"
echo "BUILD COMPLETED SUCCESSFULLY"
echo "======================================================"
echo "App Type: $APP_TYPE"
echo "Build Type: $BUILD_TYPE"
echo "Target: $IDF_TARGET"
echo "Build Directory: $BUILD_DIR"
echo "Project Name: $PROJECT_NAME"
if [ -f "$BIN_FILE" ]; then
    echo "Binary: $BIN_FILE"
else
    echo "Binary: Check $BUILD_DIR for output files"
fi
echo ""
echo "Next steps:"
echo "  Flash and monitor: ./scripts/flash_app.sh flash_monitor $APP_TYPE $BUILD_TYPE"
echo "  Flash only:        ./scripts/flash_app.sh flash $APP_TYPE $BUILD_TYPE"
echo "  Monitor only:      ./scripts/flash_app.sh monitor"
echo "  Size analysis:     ./scripts/flash_app.sh size $APP_TYPE $BUILD_TYPE"
echo ""
echo "======================================================"
echo "BUILD SIZE INFORMATION"
echo "======================================================"

# Print and capture size in one call - make this non-fatal
if idf.py -B "$BUILD_DIR" size | tee "$BUILD_DIR/size.txt" >/dev/null 2>&1; then
  # If supported, also emit JSON for machine parsing
  if idf.py -B "$BUILD_DIR" size-json >/dev/null 2>&1; then
    idf.py -B "$BUILD_DIR" size-json > "$BUILD_DIR/size.json" 2>/dev/null || true
  fi

  # (Optional) map/ELF pointers to aid later analysis
  ELF_FILE="$(find "$BUILD_DIR" -maxdepth 1 -name '*.elf' 2>/dev/null | head -n1 || true)"
  MAP_FILE="$(find "$BUILD_DIR" -maxdepth 1 -name '*.map' 2>/dev/null | head -n1 || true)"
  [ -n "$ELF_FILE" ] && echo "ELF_FILE=$ELF_FILE" >> "$BUILD_DIR/size.meta" 2>/dev/null || true
  [ -n "$MAP_FILE" ] && echo "MAP_FILE=$MAP_FILE" >> "$BUILD_DIR/size.meta" 2>/dev/null || true

  # Helpful metadata for your PR summarizer job
  {
    echo "APP=$APP_TYPE"
    echo "BUILD=$BUILD_TYPE"
    echo "IDF=$IDF_VERSION"
    echo "TARGET=$IDF_TARGET"
    echo "PROJECT_NAME=$PROJECT_NAME"
    echo "BUILD_DIR=$BUILD_DIR"
  } > "$BUILD_DIR/size.info" 2>/dev/null || true
else
  echo "WARNING: Could not display size information"
fi

echo "======================================================"

