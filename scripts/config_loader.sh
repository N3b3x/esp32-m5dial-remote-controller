#!/bin/bash
# ESP32 Apps Configuration Loader
# ===============================
# This script provides enhanced configuration functions for ESP32 applications
# with support for app-specific overrides, version-aware validation, and
# robust fallback parsing for CI pipeline compatibility.
#
# KEY FEATURES:
# • App-specific build type and IDF version overrides
# • Version-aware build type validation
# • Smart fallback to metadata defaults
# • Comprehensive combination validation
# • Both yq and grep/sed fallback parsing
# • CI pipeline optimized functions
#
# USAGE: source ./config_loader.sh

set -e  # Exit on any error

# Show help if requested (only when run directly, not when sourced)
if [[ "${BASH_SOURCE[0]}" == "${0}" ]] && ([ "$1" = "--help" ] || [ "$1" = "-h" ]); then
  echo "ESP32 Apps Configuration Loader"
  echo ""
  echo "Usage: source ./config_loader.sh [OPTIONS]"
  echo ""
  echo "OPTIONS:"
  echo "  --help, -h                  - Show this help message"
  echo ""
  echo "PURPOSE:"
  echo "  Load and manage configuration for ESP32 applications"
  echo ""
  echo "USAGE:"
  echo "  This script is designed to be sourced, not executed directly:"
  echo "  source ./config_loader.sh"
  echo ""
  echo "AVAILABLE FUNCTIONS:"
  echo "  # Configuration initialization"
  echo "  init_config               - Initialize configuration from app_config.yml"
  echo "  load_config                - Load configuration and set environment variables"
  echo ""
  echo "  # App information functions"
  echo "  get_app_types             - Get all valid app types"
  echo "  get_app_description       - Get description for app type"
  echo "  get_app_source_file       - Get source file for app type"
  echo "  is_valid_app_type         - Check if app type is valid"
  echo "  get_featured_app_types    - Get featured app types"
  echo "  get_ci_app_types          - Get CI-enabled app types"
  echo ""
  echo "  # Build configuration functions"
  echo "  get_build_types           - Get build types (with app override support)"
  echo "  is_valid_build_type       - Check if build type is valid (with app/IDF version support)"
  echo "  get_build_directory       - Get build directory for app/build type/target/idf_version"
  echo "  get_project_name          - Get project name for app type"
  echo ""
  echo "  # ESP-IDF and target functions"
  echo "  get_target                - Get target from config (with per-app override)"
  echo "  get_idf_version           - Get IDF version from config (with per-app override)"
  echo "  get_idf_versions          - Get all supported ESP-IDF versions"
  echo "  get_idf_version_index     - Get index of IDF version in metadata array"
  echo ""
  echo "  # Build directory management"
  echo "  parse_build_directory     - Parse build directory name to extract components"
  echo "  get_build_component       - Get specific component from build directory name"
  echo "  is_valid_build_directory  - Validate build directory name format"
  echo "  list_build_directories    - List all build directories with parsed information"
  echo ""
  echo "  # Advanced validation functions"
  echo "  is_valid_combination      - Check if app + build type + IDF version combination is valid"
  echo "  show_valid_combinations   - Display valid combinations for specific app"
  echo "  get_build_types_for_idf_version - Get build types for specific IDF version"
  echo "  get_app_build_types_for_idf_version - Get build types for app + IDF version combination"
  echo ""
  echo "EXAMPLES:"
  echo "  # Source the script to use functions"
  echo "  source ./config_loader.sh"
  echo ""
  echo "  # Get app information"
  echo "  apps=\$(get_app_types)"
  echo "  desc=\$(get_app_description \"gpio_test\")"
  echo "  source=\$(get_app_source_file \"gpio_test\")"
  echo ""
  echo "  # Get build configuration"
  echo "  build_types=\$(get_build_types)                    # Global build types"
  echo "  app_build_types=\$(get_build_types \"gpio_test\")  # App-specific build types"
  echo "  build_dir=\$(get_build_directory \"gpio_test\" \"Release\" \"esp32c6\" \"release/v5.5\")"
  echo "  project_name=\$(get_project_name \"gpio_test\")"
  echo ""
  echo "  # Validate combinations"
  echo "  if is_valid_combination \"gpio_test\" \"Release\" \"release/v5.5\"; then"
  echo "    echo \"Valid combination\""
  echo "  fi"
  echo "  # Advanced validation"
  echo "  if is_valid_build_type \"Release\" \"gpio_test\" \"release/v5.5\"; then"
  echo "    echo \"Valid build type for app and IDF version\""
  echo "  fi"
  echo ""
  echo "  # Get smart defaults"
  echo "  idf_version=\$(get_idf_version \"gpio_test\")"
  echo "  target=\$(get_target)"
  echo "  # Advanced IDF version handling"
  echo "  version_index=\$(get_idf_version_index \"release/v5.5\")"
  echo "  version_build_types=\$(get_build_types_for_idf_version \"release/v5.5\")"
  echo ""
  echo "CONFIGURATION FILE:"
  echo "  • Location: examples/esp32/app_config.yml"
  echo "  • Format: YAML configuration"
  echo "  • Structure: Apps, build types, ESP-IDF versions, targets"
  echo "  • Validation: Automatic validation of configuration integrity"
  echo ""
  echo "ENVIRONMENT VARIABLES:"
  echo "  • PROJECT_PATH: Path to project directory containing app_config.yml (optional)"
  echo "    - If set, uses this project directory instead of default location"
  echo "    - Can be absolute (/path/to/project) or relative (../project)"
  echo "    - Allows scripts to be placed anywhere while finding correct project"
  echo "  • CONFIG_DEFAULT_APP: Default application type"
  echo "  • CONFIG_DEFAULT_BUILD_TYPE: Default build configuration"
  echo "  • CONFIG_DEFAULT_IDF_VERSION: Default ESP-IDF version"
  echo "  • CONFIG_TARGET: Target MCU architecture"
  echo ""
  echo "FUNCTION SAFETY:"
  echo "  • All functions are standalone-safe"
  echo "  • Can be sourced independently"
  echo "  • Error handling for missing configuration"
  echo "  • Fallback values for missing data"
  echo ""
  echo "ENHANCED FUNCTIONALITY:"
  echo "  • App-specific overrides for build types and IDF versions"
  echo "  • Version-aware build type validation"
  echo "  • Smart fallback to metadata defaults when app overrides not specified"
  echo "  • Robust combination validation for CI pipeline compatibility"
  echo ""
  echo "ERROR HANDLING:"
  echo "  • Functions return appropriate exit codes"
  echo "  • Error messages for invalid inputs"
  echo "  • Graceful fallbacks for missing configuration"
  echo "  • Validation of configuration integrity"
  echo ""
  echo "For detailed information, see: docs/README_CONFIG_SYSTEM.md"
  exit 0
fi

# Get the directory where this script is located
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Determine project directory and config file location
# Priority: 1) PROJECT_PATH env var, 2) Default project location
if [[ -n "$PROJECT_PATH" ]]; then
    # Use provided project path (can be absolute or relative)
    if [[ "$PROJECT_PATH" = /* ]]; then
        # Absolute path
        PROJECT_DIR="$PROJECT_PATH"
    else
        # Relative path - resolve from current working directory
        echo "DEBUG: Resolving relative PROJECT_PATH: $PROJECT_PATH from $(pwd)" >&2
        if ! PROJECT_DIR="$(cd "$PROJECT_PATH" && pwd)"; then
            echo "ERROR: Cannot resolve PROJECT_PATH: $PROJECT_PATH" >&2
            echo "Current working directory: $(pwd)" >&2
            echo "Directory contents:" >&2
            ls -la . >&2
            return 1
        fi
        echo "DEBUG: Resolved PROJECT_DIR to: $PROJECT_DIR" >&2
    fi
    
    # Set config file path within the project directory
    CONFIG_FILE="$PROJECT_DIR/app_config.yml"
    echo "DEBUG: Looking for config file at: $CONFIG_FILE" >&2
    
    # Validate that the config file exists
    if [[ ! -f "$CONFIG_FILE" ]]; then
        echo "ERROR: PROJECT_PATH specified but app_config.yml not found: $CONFIG_FILE" >&2
        echo "Please check the project path or unset PROJECT_PATH to use default location." >&2
        echo "Directory contents of $PROJECT_DIR:" >&2
        ls -la "$PROJECT_DIR" >&2
        return 1
    fi
else
    # Default behavior: assume scripts are in project/scripts/
    PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
    CONFIG_FILE="$PROJECT_DIR/app_config.yml"
fi

# Check if yq is available for YAML parsing and detect version
check_yq() {
    # Always check for yq availability (no caching) - this allows detecting newly installed yq
    if ! command -v yq &> /dev/null; then
        # Only show warning if we haven't shown it in this script execution
        if [[ -z "$YQ_WARNING_SHOWN" ]]; then
            echo "Warning: yq not found. Falling back to basic parsing." >&2
            export YQ_WARNING_SHOWN=1
        fi
        return 1
    fi
    
    # Detect yq version and set appropriate syntax
    local yq_version=$(yq --version 2>/dev/null | grep -oE '[0-9]+\.[0-9]+' | head -1)
    if [[ -n "$yq_version" ]]; then
        local major_version=$(echo "$yq_version" | cut -d. -f1)
        if [[ "$major_version" -ge 4 ]]; then
            # yq v4+ uses 'eval' syntax
            export YQ_SYNTAX="eval"
        else
            # yq v3.x uses direct syntax
            export YQ_SYNTAX="direct"
        fi
        # echo "Detected yq version $yq_version, using $YQ_SYNTAX syntax" >&2
    else
        # Fallback to direct syntax for unknown versions
        export YQ_SYNTAX="direct"
        echo "Could not detect yq version, using direct syntax" >&2
    fi
    
    return 0
}

# Helper function to execute yq with appropriate syntax
run_yq() {
    local query="$1"
    local raw_flag="$2"
    
    if [[ "$YQ_SYNTAX" == "eval" ]]; then
        if [[ "$raw_flag" == "-r" ]]; then
            yq eval "$query" "$CONFIG_FILE" -r
        else
            yq eval "$query" "$CONFIG_FILE"
        fi
    else
        if [[ "$raw_flag" == "-r" ]]; then
            yq "$query" "$CONFIG_FILE" -r
        else
            yq "$query" "$CONFIG_FILE"
        fi
    fi
}

# Load configuration using yq (preferred method)
load_config_yq() {
    if ! check_yq; then
        return 1
    fi
    
    # Export configuration as environment variables (raw output, no quotes)
    export CONFIG_DEFAULT_APP=$(run_yq '.metadata.default_app' -r)
    export CONFIG_DEFAULT_BUILD_TYPE=$(run_yq '.metadata.default_build_type' -r)
    export CONFIG_TARGET=$(run_yq '.metadata.target' -r)
    
    # Load ESP-IDF version information
    export CONFIG_DEFAULT_IDF_VERSION=$(run_yq '.metadata.idf_versions[0]' -r 2>/dev/null || echo "release/v5.5")
    
    return 0
}

# Fallback: Basic parsing without yq
load_config_basic() {
    # Extract basic configuration using grep and sed (cleaner quote handling)
    export CONFIG_DEFAULT_APP=$(grep -A 10 "metadata:" "$CONFIG_FILE" | grep "default_app:" | sed 's/.*default_app: *"*\([^"]*\)"*.*/\1/')
    export CONFIG_DEFAULT_BUILD_TYPE=$(grep -A 10 "metadata:" "$CONFIG_FILE" | grep "default_build_type:" | sed 's/.*default_build_type: *"*\([^"]*\)"*.*/\1/')
    export CONFIG_TARGET=$(grep -A 10 "metadata:" "$CONFIG_FILE" | grep "target:" | sed 's/.*target: *"*\([^"]*\)"*.*/\1/')
    
    # Extract default ESP-IDF version
    export CONFIG_DEFAULT_IDF_VERSION=$(grep -A 10 "metadata:" "$CONFIG_FILE" | grep "idf_versions:" | sed 's/.*idf_versions: *\[*"*\([^"]*\)"*.*/\1/' | head -1 || echo "release/v5.5")
    
    return 0
}

# Get list of valid app types
get_app_types() {
    if check_yq; then
        run_yq '.apps | keys | .[]' -r | tr '\n' ' '
    else
        # Fallback: extract from apps section (use more specific range)
        sed -n '/^apps:/,/^build_config:/p' "$CONFIG_FILE" | grep '^  [a-zA-Z0-9_]*:$' | sed 's/^  \(.*\):$/\1/' | sort | tr '\n' ' '
    fi
}

# Get list of valid build types with smart app override handling
# Usage: get_build_types [app_type]
# - Without app_type: returns global metadata build types
# - With app_type: checks app-specific overrides first, falls back to metadata
get_build_types() {
    local app_type="${1:-}"
    
    # If app type is specified, check for app-specific overrides first
    if [[ -n "$app_type" ]]; then
        if check_yq; then
            local app_build_types=$(run_yq ".apps.$app_type.build_types | .[]" -r 2>/dev/null | tr '\n' ' ')
            if [[ -n "$app_build_types" && "$app_build_types" != "null" ]]; then
                # App has specific build types, return them
                echo "$app_build_types"
                return 0
            fi
        else
            # Fallback: extract from apps section
            local build_line=$(sed -n "/^  $app_type:/,/^  [a-zA-Z0-9_]*:/p" "$CONFIG_FILE" | grep "build_types:")
            if [[ -n "$build_line" ]]; then
                # Extract all build types from array, handling quotes and commas
                local app_build_types=$(echo "$build_line" | sed 's/.*build_types: *\[//' | sed 's/\].*//' | sed 's/"//g' | sed 's/,/ /g' | tr '\n' ' ' | sed 's/^ *//' | sed 's/ *$//')
                if [[ -n "$app_build_types" ]]; then
                    echo "$app_build_types"
                    return 0
                fi
            fi
        fi
    fi
    
    # Fall back to metadata defaults (flattened for backward compatibility)
    if check_yq; then
        run_yq '.metadata.build_types | .[] | .[]' -r 2>/dev/null | sort -u | tr '\n' ' '
    else
        # Fallback: extract from metadata section
        local build_line=$(grep -A 10 "metadata:" "$CONFIG_FILE" | grep "build_types:")
        if [[ -n "$build_line" ]]; then
            # Extract all build types from nested array, handling quotes and commas
            # Format: [["Debug", "Release"], ["Debug"]] -> Debug Release
            # First, extract the content between the outer brackets
            local content=$(echo "$build_line" | sed 's/.*build_types: *\[//' | sed 's/\].*//')
            # Then extract individual build types, handling nested arrays
            echo "$content" | sed 's/\[//g' | sed 's/\]//g' | sed 's/"//g' | sed 's/,/ /g' | sed 's/  */ /g' | sed 's/^ *//' | sed 's/ *$//' | tr ' ' '\n' | sort -u | tr '\n' ' '
        else
            echo "ERROR: Could not extract build types from config" >&2
            return 1
        fi
    fi
}

# Get build types for a specific IDF version (maintains version relationship)
get_build_types_for_idf_version() {
    local idf_version="$1"
    
    if check_yq; then
        # Get the index of the IDF version
        local version_index=$(run_yq ".metadata.idf_versions | index(\"$idf_version\")" -r 2>/dev/null)
        if [[ -n "$version_index" && "$version_index" != "null" ]]; then
            # Get build types for that specific index
            run_yq ".metadata.build_types[$version_index] | .[]" -r 2>/dev/null | tr '\n' ' '
        else
            echo "ERROR: IDF version $idf_version not found in metadata" >&2
            return 1
        fi
    else
        # Fallback: extract using grep and sed
        local idf_line=$(grep -A 10 "metadata:" "$CONFIG_FILE" | grep "idf_versions:")
        local build_line=$(grep -A 10 "metadata:" "$CONFIG_FILE" | grep "build_types:")
        
        if [[ -n "$idf_line" && -n "$build_line" ]]; then
            # Extract IDF versions array
            local idf_content=$(echo "$idf_line" | sed 's/.*idf_versions: *\[//' | sed 's/\].*//' | sed 's/"//g' | sed 's/,/ /g')
            
            # Find the index of the requested IDF version
            local index=0
            local found=false
            while IFS= read -r version; do
                if [[ "$version" == "$idf_version" ]]; then
                    found=true
                    break
                fi
                ((index++))
            done < <(echo "$idf_content" | tr ' ' '\n')
            
            if [[ "$found" == "true" ]]; then
                # Extract build types for that specific index
                local build_content=$(echo "$build_line" | sed 's/.*build_types: *\[//' | sed 's/\].*//')
                
                # Split by '], [' to get individual arrays
                local arrays=()
                IFS='], [' read -ra arrays <<< "$build_content"
                
                # Get the array at the specified index
                if [[ $index -lt ${#arrays[@]} ]]; then
                    local target_array="${arrays[$index]}"
                    # Clean up the array content
                    echo "$target_array" | sed 's/"//g' | sed 's/,/ /g' | sed 's/^ *//' | sed 's/ *$//'
                else
                    echo "ERROR: Build types index $index not found" >&2
                    return 1
                fi
            else
                echo "ERROR: IDF version $idf_version not found" >&2
                return 1
            fi
        else
            echo "ERROR: Could not extract IDF versions or build types from config" >&2
            return 1
        fi
    fi
}

# Get the index of an IDF version in the metadata array
# Usage: get_idf_version_index idf_version
# - idf_version: required - the IDF version to find
# Returns: index number (0-based) or error message
# Useful for accessing version-specific build types in nested arrays
get_idf_version_index() {
    local idf_version="$1"
    
    if check_yq; then
        run_yq ".metadata.idf_versions | index(\"$idf_version\")" -r 2>/dev/null
    else
        # Fallback: extract using grep and find index
        local idf_line=$(grep -A 10 "metadata:" "$CONFIG_FILE" | grep "idf_versions:")
        if [[ -n "$idf_line" ]]; then
            local idf_content=$(echo "$idf_line" | sed 's/.*idf_versions: *\[//' | sed 's/\].*//' | sed 's/"//g' | sed 's/,/ /g')
            
            local index=0
            while IFS= read -r version; do
                if [[ "$version" == "$idf_version" ]]; then
                    echo "$index"
                    return 0
                fi
                ((index++))
            done < <(echo "$idf_content" | tr ' ' '\n')
        fi
        echo "ERROR: IDF version $idf_version not found" >&2
        return 1
    fi
}

# Get build types for a specific app and IDF version combination
# Usage: get_app_build_types_for_idf_version app_type idf_version
# - app_type: required - the application type
# - idf_version: required - the ESP-IDF version
# Returns: build types string or error message
# This function handles complex nested configurations and app-specific overrides
get_app_build_types_for_idf_version() {
    local app_type="$1"
    local idf_version="$2"
    
    if check_yq; then
        # Check if app has specific build types
        local app_build_types=$(run_yq ".apps.$app_type.build_types" -r 2>/dev/null)
        if [[ -n "$app_build_types" && "$app_build_types" != "null" ]]; then
            # App has specific build types
            if [[ "$app_build_types" =~ ^\[.*\]$ ]]; then
                # Check if it's nested (version-specific) or flat (global)
                local first_element=$(run_yq ".apps.$app_type.build_types[0]" -r 2>/dev/null)
                if [[ "$first_element" =~ ^\[.*\]$ ]]; then
                    # Nested format: [["Debug", "Release"], ["Debug"]]
                    # Get app's IDF versions to find the right index
                    local app_idf_versions=$(run_yq ".apps.$app_type.idf_versions" -r 2>/dev/null)
                    if [[ -n "$app_idf_versions" && "$app_idf_versions" != "null" ]]; then
                        # Find index of this IDF version in app's IDF versions
                        local app_version_index=$(run_yq ".apps.$app_type.idf_versions | index(\"$idf_version\")" -r 2>/dev/null)
                        if [[ -n "$app_version_index" && "$app_version_index" != "null" ]]; then
                            # Get build types for that specific index
                            run_yq ".apps.$app_type.build_types[$app_version_index] | .[]" -r 2>/dev/null | tr '\n' ' '
                            return 0
                        fi
                    fi
                else
                    # Flat format: ["Debug", "Release"] (global for all IDF versions)
                    run_yq ".apps.$app_type.build_types | .[]" -r 2>/dev/null | tr '\n' ' '
                    return 0
                fi
            fi
        fi
        
        # Fall back to global metadata for this IDF version
        local global_version_index=$(run_yq ".metadata.idf_versions | index(\"$idf_version\")" -r 2>/dev/null)
        if [[ -n "$global_version_index" && "$global_version_index" != "null" ]]; then
            run_yq ".metadata.build_types[$global_version_index] | .[]" -r 2>/dev/null | tr '\n' ' '
            return 0
        fi
        
        echo "ERROR: Could not determine build types for $app_type with $idf_version" >&2
        return 1
    else
        # Fallback: extract using grep and sed
        local app_idf_versions=$(sed -n "/^  ${app_type}:/,/^  [a-zA-Z0-9_]*:/p" "$CONFIG_FILE" | grep "idf_versions:")
        local app_build_types=$(sed -n "/^  ${app_type}:/,/^  [a-zA-Z0-9_]*:/p" "$CONFIG_FILE" | grep "build_types:")
        
        if [[ -n "$app_build_types" ]]; then
            # App has specific build types
            if [[ "$app_build_types" =~ \[.*\[.*\].*\] ]]; then
                # Nested format detected
                if [[ -n "$app_idf_versions" ]]; then
                    # Find index of this IDF version in app's IDF versions
                    local app_idf_content=$(echo "$app_idf_versions" | sed 's/.*idf_versions: *\[//' | sed 's/\].*//' | sed 's/"//g' | sed 's/,/ /g')
                    local index=0
                    local found=false
                    while IFS= read -r version; do
                        if [[ "$version" == "$idf_version" ]]; then
                            found=true
                            break
                        fi
                        ((index++))
                    done < <(echo "$app_idf_content" | tr ' ' '\n')
                    
                    if [[ "$found" == "true" ]]; then
                        # Extract build types for that specific index
                        local build_content=$(echo "$app_build_types" | sed 's/.*build_types: *\[//' | sed 's/\].*//')
                        local arrays=()
                        IFS='], [' read -ra arrays <<< "$build_content"
                        
                        if [[ $index -lt ${#arrays[@]} ]]; then
                            local target_array="${arrays[$index]}"
                            echo "$target_array" | sed 's/"//g' | sed 's/,/ /g' | sed 's/^ *//' | sed 's/ *$//'
                            return 0
                        fi
                    fi
                fi
            else
                # Flat format: extract directly
                echo "$app_build_types" | sed 's/.*build_types: *\[//' | sed 's/\].*//' | sed 's/"//g' | sed 's/,/ /g' | sed 's/^ *//' | sed 's/ *$//'
                return 0
            fi
        fi
        
        # Fall back to global metadata for this IDF version
        local global_idf_line=$(grep -A 10 "metadata:" "$CONFIG_FILE" | grep "idf_versions:")
        local global_build_line=$(grep -A 10 "metadata:" "$CONFIG_FILE" | grep "build_types:")
        
        if [[ -n "$global_idf_line" && -n "$global_build_line" ]]; then
            local global_idf_content=$(echo "$global_idf_line" | sed 's/.*idf_versions: *\[//' | sed 's/\].*//' | sed 's/"//g' | sed 's/,/ /g')
            local index=0
            local found=false
            while IFS= read -r version; do
                if [[ "$version" == "$idf_version" ]]; then
                    found=true
                    break
                fi
                ((index++))
            done < <(echo "$global_idf_content" | tr ' ' '\n')
            
            if [[ "$found" == "true" ]]; then
                local build_content=$(echo "$global_build_line" | sed 's/.*build_types: *\[//' | sed 's/\].*//')
                local arrays=()
                IFS='], [' read -ra arrays <<< "$build_content"
                
                if [[ $index -lt ${#arrays[@]} ]]; then
                    local target_array="${arrays[$index]}"
                    echo "$target_array" | sed 's/"//g' | sed 's/,/ /g' | sed 's/^ *//' | sed 's/ *$//'
                    return 0
                fi
            fi
        fi
        
        echo "ERROR: Could not determine build types for $app_type with $idf_version" >&2
        return 1
    fi
}

# Get list of available ESP-IDF versions
get_idf_versions() {
    if check_yq; then
        run_yq '.metadata.idf_versions | .[]' -r 2>/dev/null | tr '\n' ' '
    else
        # Fallback: extract from metadata section
        local idf_line=$(grep -A 10 "metadata:" "$CONFIG_FILE" | grep "idf_versions:")
        if [[ -n "$idf_line" ]]; then
            # Extract all versions from array, handling quotes and commas
            echo "$idf_line" | sed 's/.*\[//' | sed 's/\].*//' | sed 's/"//g' | sed 's/,/ /g' | tr '\n' ' '
        else
            echo "ERROR: Could not extract IDF versions from config" >&2
            return 1
        fi
    fi
}

# Get app-specific ESP-IDF versions
get_app_idf_versions() {
    local app_type="$1"
    if check_yq; then
        run_yq ".apps.$app_type.idf_versions | .[]" -r 2>/dev/null | tr '\n' ' '
    else
        # Fallback: extract from apps section
                    local idf_line=$(sed -n "/^  $app_type:/,/^  [a-zA-Z0-9_]*:/p" "$CONFIG_FILE" | grep "idf_versions:")
        if [[ -n "$idf_line" ]]; then
            # Extract all versions from array, handling quotes and commas
            echo "$idf_line" | sed 's/.*\[//' | sed 's/\].*//' | sed 's/"//g' | sed 's/,/ /g' | tr '\n' ' '
        else
            echo "ERROR: Could not extract app IDF versions from config" >&2
            return 1
        fi
    fi
}

# REMOVED: get_app_build_types() - Now handled by enhanced get_build_types(app_type)

# REMOVED: validate_app_idf_version() - Now handled by enhanced is_valid_combination()

# REMOVED: validate_app_build_type() - Now handled by enhanced is_valid_build_type()

# Get description for an app type
get_app_description() {
    local app_type="$1"
    if check_yq; then
        run_yq ".apps.${app_type}.description" -r
    else
        # Fallback: extract description using grep (improved regex)
        sed -n "/^  ${app_type}:/,/^  [a-zA-Z0-9_]*:/p" "$CONFIG_FILE" | grep "description:" | sed 's/.*description: *["\x27]*\([^"\x27]*\)["\x27]*.*/\1/' | head -1
    fi
}

# Get source file for an app type
get_app_source_file() {
    local app_type="$1"
    if check_yq; then
        run_yq ".apps.${app_type}.source_file" -r
    else
        # Fallback: extract source_file using grep
        sed -n "/^  ${app_type}:/,/^  [a-zA-Z0-9_]*:/p" "$CONFIG_FILE" | grep "source_file:" | sed 's/.*source_file: *"\(.*\)".*/\1/'
    fi
}

# Check if app type is valid
is_valid_app_type() {
    local app_type="$1"
    local valid_types=$(get_app_types)
    echo "$valid_types" | grep -q "\b$app_type\b"
}

# Check if build type is valid with comprehensive validation
# Usage: is_valid_build_type build_type [app_type] [idf_version]
# - build_type: required - the build type to validate
# - app_type: optional - if provided, checks app-specific overrides
# - idf_version: optional - if provided, checks version-specific compatibility
# Returns: 0 if valid, 1 if invalid
is_valid_build_type() {
    local build_type="$1"
    local app_type="${2:-}"
    local idf_version="${3:-}"
    
    # If both app type and IDF version are specified, use the robust parser
    if [[ -n "$app_type" && -n "$idf_version" ]]; then
        local app_version_build_types=$(get_app_build_types_for_idf_version "$app_type" "$idf_version")
        if [[ -n "$app_version_build_types" ]]; then
            if echo "$app_version_build_types" | grep -q "\b$build_type\b"; then
                return 0  # Valid for this app and IDF version
            fi
        fi
    fi
    
    # If only app type is specified, check app-specific build types (including overrides)
    if [[ -n "$app_type" ]]; then
        local app_build_types=$(get_build_types "$app_type")
        if [[ -n "$app_build_types" ]]; then
            if echo "$app_build_types" | grep -q "\b$build_type\b"; then
                return 0  # Valid for this app
            fi
        fi
    fi
    
    # If only IDF version is specified, check version-specific global build types
    if [[ -n "$idf_version" ]]; then
        local version_build_types=$(get_build_types_for_idf_version "$idf_version")
        if [[ -n "$version_build_types" ]]; then
            if echo "$version_build_types" | grep -q "\b$build_type\b"; then
                return 0  # Valid for this IDF version
            fi
        fi
    fi
    
    # Fall back to global build types (for backward compatibility)
    local valid_types=$(get_build_types)
    echo "$valid_types" | grep -q "\b$build_type\b"
}

# Get build directory pattern
get_build_directory() {
    local app_type="$1"
    local build_type="$2"
    local target="${3:-$(get_target)}"
    local idf_version="${4:-$(get_idf_version)}"
    
    # Sanitize IDF version for directory names (replace / and . with _)
    local sanitized_idf_version=$(echo "$idf_version" | sed 's/[\/\.]/_/g')
    
    # Get the build directory name pattern
    local build_dir_name
    if check_yq; then
        local pattern=$(run_yq '.build_config.build_directory_pattern' -r)
        build_dir_name=$(echo "${pattern}" | sed "s/{app_type}/${app_type}/g" | sed "s/{build_type}/${build_type}/g" | sed "s/{target}/${target}/g" | sed "s/{idf_version}/${sanitized_idf_version}/g")
    else
        # Fallback pattern with hyphens and prefixes
        build_dir_name="build-app-${app_type}-type-${build_type}-target-${target}-idf-${sanitized_idf_version}"
    fi
    
    # Always return absolute path relative to project directory, placing builds in builds/ subdirectory
    echo "$PROJECT_DIR/builds/$build_dir_name"
}

# Get project name pattern
get_project_name() {
    local app_type="$1"
    if check_yq; then
        local pattern=$(run_yq '.build_config.project_name_pattern' -r)
        echo "${pattern}" | sed "s/{app_type}/${app_type}/g"
    else
        echo "esp32_iid_${app_type}_app"
    fi
}

# Get CI-enabled app types
get_ci_app_types() {
    if check_yq; then
        run_yq '.apps | to_entries | map(select(.value.ci_enabled == true)) | .[].key' -r | tr '\n' ' '
    else
        # Fallback: return all app types
        get_app_types
    fi
}

# Get featured app types
get_featured_app_types() {
    if check_yq; then
        run_yq '.apps | to_entries | map(select(.value.featured == true)) | .[].key' -r | tr '\n' ' '
    else
        # Fallback: return default featured apps
        echo "ascii_art gpio_test adc_test pio_test bluetooth_test utils_test "
    fi
}

# Get target from config (with per-app override support)
get_target() {
    local app_type="${1:-}"
    
    if [[ -n "$app_type" ]]; then
        # Check for per-app target override first
        if check_yq; then
            local app_target=$(run_yq ".apps.${app_type}.target" -r 2>/dev/null)
            if [[ -n "$app_target" && "$app_target" != "null" ]]; then
                echo "$app_target"
                return 0
            fi
        else
            # Fallback: extract per-app target using grep
            local app_target=$(sed -n "/^  ${app_type}:/,/^  [a-zA-Z0-9_]*:/p" "$CONFIG_FILE" | grep "target:" | sed 's/.*target: *"*\([^"]*\)"*.*/\1/' | head -1)
            if [[ -n "$app_target" ]]; then
                echo "$app_target"
                return 0
            fi
        fi
    fi
    
    # Fall back to global target
    if check_yq; then
        run_yq '.metadata.target' -r
    else
        # Fallback: extract target using grep
        grep -A 5 "metadata:" "$CONFIG_FILE" | grep "target:" | sed 's/.*target: *"*\([^"]*\)"*.*/\1/'
    fi
}

# Get IDF version from config (with per-app override support)
get_idf_version() {
    local app_type="${1:-}"
    
    if [[ -n "$app_type" ]]; then
        # Check for per-app IDF version override first
        if check_yq; then
            local app_idf_versions=$(run_yq ".apps.${app_type}.idf_versions" -r 2>/dev/null)
            if [[ -n "$app_idf_versions" && "$app_idf_versions" != "null" ]]; then
                # Get the first version from the array
                echo "$app_idf_versions" | sed 's/\[//' | sed 's/\]//' | sed 's/,.*//' | tr -d '"'
                return 0
            fi
        else
            # Fallback: extract per-app IDF version using grep
            local app_idf_versions=$(sed -n "/^  ${app_type}:/,/^  [a-zA-Z0-9_]*:/p" "$CONFIG_FILE" | grep "idf_versions:")
            if [[ -n "$app_idf_versions" ]]; then
                # Extract first version from array, handling quotes and commas
                echo "$app_idf_versions" | sed 's/.*\["*\([^",]*\)"*.*/\1/' | tr -d '[]"' | head -1
                return 0
            fi
        fi
    fi
    
    # Fall back to global IDF version
    if check_yq; then
        # Get the first IDF version from the array
        run_yq '.metadata.idf_versions[0]' -r
    else
        # Fallback: extract IDF version using grep
        # Extract the first version from the array, handling both ["v1", "v2"] and ["v1"] formats
        local idf_line=$(grep -A 5 "metadata:" "$CONFIG_FILE" | grep "idf_versions:")
        if [[ -n "$idf_line" ]]; then
            # Extract first version from array, handling quotes and commas
            echo "$idf_line" | sed 's/.*\["*\([^",]*\)"*.*/\1/' | tr -d '[]"' | head -1
        else
            echo "ERROR: Could not extract IDF version from config" >&2
            return 1
        fi
    fi
}

# Get IDF version that supports a specific build type (smart selection)
get_idf_version_for_build_type() {
    local app_type="$1"
    local build_type="$2"
    
    if [[ -z "$app_type" || -z "$build_type" ]]; then
        echo "ERROR: Both app_type and build_type are required" >&2
        return 1
    fi
    
    # First, check if the app has specific IDF version requirements
    local app_idf_versions=""
    if check_yq; then
        app_idf_versions=$(run_yq ".apps.${app_type}.idf_versions[0]" -r 2>/dev/null)
    else
        # Fallback: extract per-app IDF version using grep
        local app_idf_versions_line=$(sed -n "/^  ${app_type}:/,/^  [a-zA-Z0-9_]*:/p" "$CONFIG_FILE" | grep "idf_versions:")
        if [[ -n "$app_idf_versions_line" ]]; then
            app_idf_versions=$(echo "$app_idf_versions_line" | sed 's/.*\[//' | sed 's/\].*//' | sed 's/"//g' | sed 's/,/ /g' | awk '{print $1}')
        fi
    fi
    
    # If no app-specific versions, use global versions
    if [[ -z "$app_idf_versions" || "$app_idf_versions" == "null" ]]; then
        if check_yq; then
            app_idf_versions=$(run_yq '.metadata.idf_versions | .[]' -r | tr '\n' ' ')
        else
            # Fallback: extract global IDF versions
            local idf_line=$(grep -A 5 "metadata:" "$CONFIG_FILE" | grep "idf_versions:")
            if [[ -n "$idf_line" ]]; then
                app_idf_versions=$(echo "$idf_line" | sed 's/.*\[//' | sed 's/\].*//' | sed 's/"//g' | sed 's/,/ /g')
            fi
        fi
    fi
    
    # Convert to array and check each version for build type support
    local idf_versions_array=($app_idf_versions)
    for idf_version in "${idf_versions_array[@]}"; do
        if [[ -n "$idf_version" ]]; then
            # Check if this IDF version supports the requested build type
            if is_valid_build_type "$build_type" "$app_type" "$idf_version"; then
                echo "$idf_version"
                return 0
            fi
        fi
    done
    
    # If no version supports the build type, return the first available version
    # (this will fail later in validation, but at least we have a version to work with)
    echo "${idf_versions_array[0]}"
    return 0
}

# Load configuration
load_config() {
    if ! [ -f "$CONFIG_FILE" ]; then
        echo "Error: Configuration file not found: $CONFIG_FILE" >&2
        return 1
    fi
    
    if load_config_yq; then
        return 0
    else
        load_config_basic
        return 0
    fi
}

# Initialize configuration
init_config() {
    load_config
    
    # Set defaults if not already set
    : ${CONFIG_DEFAULT_APP:="ascii_art"}
    : ${CONFIG_DEFAULT_BUILD_TYPE:="Release"}
    : ${CONFIG_TARGET:="esp32c6"}
}

# Export functions for use in other scripts
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    # Script is being executed directly, not sourced
    echo "ESP32 Apps Configuration Loader"
    echo "Usage: source this script to use configuration functions"
    echo ""
    echo "Available functions:"
    echo "  # Core configuration"
    echo "  init_config               - Initialize configuration"
    echo "  load_config               - Load configuration and set environment variables"
    echo ""
    echo "  # App information"
    echo "  get_app_types             - Get all valid app types"
    echo "  get_app_description       - Get description for app type"
    echo "  get_app_source_file       - Get source file for app type"
    echo "  is_valid_app_type         - Check if app type is valid"
    echo "  get_featured_app_types    - Get featured app types"
    echo "  get_ci_app_types          - Get CI-enabled app types"
    echo ""
    echo "  # Enhanced build configuration"
    echo "  get_build_types           - Get build types (with app override support)"
    echo "  is_valid_build_type       - Check if build type is valid (with app/IDF version support)"
    echo "  get_build_directory       - Get build directory for app/build type/target/idf_version"
    echo "  get_project_name          - Get project name for app type"
    echo ""
    echo "  # ESP-IDF and target management"
    echo "  get_target                - Get target from config (with per-app override)"
    echo "  get_idf_version           - Get IDF version from config (with per-app override)"
    echo "  get_idf_versions          - Get all supported ESP-IDF versions"
    echo "  get_idf_version_index     - Get index of IDF version in metadata array"
    echo ""
    echo "  # Advanced validation"
    echo "  is_valid_combination      - Check if app + build type + IDF version combination is valid"
    echo "  show_valid_combinations   - Display valid combinations for specific app"
    echo "  get_build_types_for_idf_version - Get build types for specific IDF version"
    echo "  get_app_build_types_for_idf_version - Get build types for app + IDF version combination"
    echo ""
    echo "  # Build directory management"
    echo "  parse_build_directory     - Parse build directory name to extract components"
    echo "  get_build_component       - Get specific component from build directory name"
    echo "  is_valid_build_directory  - Validate build directory name format"
    echo "  list_build_directories    - List all build directories with parsed information"
else
    # Script is being sourced, initialize configuration
    init_config
fi

# =============================================================================
# BUILD DIRECTORY PARSING FUNCTIONS
# =============================================================================

# Parse build directory name to extract components
parse_build_directory() {
    local build_dir="$1"
    
    if [[ -z "$build_dir" ]]; then
        echo "Error: Build directory name not specified" >&2
        return 1
    fi
    
    # Extract just the directory name if it's a full path
    local dirname=$(basename "$build_dir")
    
    # Extract all components using hyphen-prefix format
    # Handle the format: build-app-{app_type}-type-{build_type}-target-{target}-idf-{idf_version}
    # Note: basename() strips any path prefix, so we parse the directory name itself
    
    # Extract app_type from app-{value}-type format
    # Handle hyphenated app names by capturing everything between app- and -type
    local app_type=$(echo "$dirname" | sed -n 's/.*app-\(.*\)-type.*/\1/p')
    
    # Extract build_type from type-{value}-target format
    local build_type=$(echo "$dirname" | sed -n 's/.*type-\([^-]*\)-target.*/\1/p')
    
    # Extract target from target-{value}-idf format
    local target=$(echo "$dirname" | sed -n 's/.*target-\([^-]*\)-idf.*/\1/p')
    
    # Extract IDF version from idf-{value} format (end of string)
    local idf_version=$(echo "$dirname" | sed -n 's/.*idf-\([^-]*\)$/\1/p')
    
    # Output as key:value pairs for easy parsing
    echo "app_type:$app_type"
    echo "build_type:$build_type"
    echo "target:$target"
    echo "idf_version:$idf_version"
}

# Get specific component from build directory name
get_build_component() {
    local build_dir="$1"
    local component="$2"
    
    case "$component" in
        "app_type"|"build_type"|"target"|"idf_version")
            parse_build_directory "$build_dir" | grep "^${component}:" | cut -d: -f2
            ;;
        *)
            echo "Error: Unknown component '$component'" >&2
            return 1
            ;;
    esac
}

# Validate build directory name format
is_valid_build_directory() {
    local build_dir="$1"
    
    # Extract just the directory name if it's a full path
    local dirname=$(basename "$build_dir")
    
    # Check if it matches the expected pattern with hyphens and prefixes
    if [[ "$dirname" =~ ^build-app-[a-zA-Z0-9_-]+-type-[a-zA-Z0-9_-]+-target-[a-zA-Z0-9_-]+-idf-[a-zA-Z0-9_-]+$ ]]; then
        return 0
    else
        return 1
    fi
}

# List all build directories with parsed information
list_build_directories() {
    local project_dir="${1:-.}"
    
    echo "=== Build Directory Analysis ==="
    # Look in builds/ directory if it exists, otherwise fall back to root for backward compatibility
    local search_path="$project_dir/builds"
    if [[ ! -d "$search_path" ]]; then
        search_path="$project_dir"
    fi
    for dir in "$search_path"/build-*; do
        if [[ -d "$dir" ]]; then
            local dirname=$(basename "$dir")
            echo ""
            echo "Directory: $dirname"
            if is_valid_build_directory "$dirname"; then
                echo "✓ Valid format"
                parse_build_directory "$dirname" | while IFS=: read -r key value; do
                    echo "  $key: $value"
                done
            else
                echo "✗ Invalid format"
            fi
        fi
    done
}

# =============================================================================
# BUILD COMBINATION VALIDATION FUNCTIONS
# =============================================================================

# Check if a complete build combination is valid
# Usage: is_valid_combination app_type build_type idf_version
# - app_type: required - the application type
# - build_type: required - the build configuration
# - idf_version: required - the ESP-IDF version
# Returns: 0 if valid combination, 1 if invalid
# This function provides comprehensive validation for CI pipeline compatibility
is_valid_combination() {
    local app_type="$1"
    local build_type="$2"
    local idf_version="$3"
    
    # Check if app type is valid
    if ! is_valid_app_type "$app_type"; then
        return 1
    fi
    
    # Check if app supports this IDF version
    local app_idf_versions_array=$(get_app_idf_versions_array "$app_type")
    if ! echo "$app_idf_versions_array" | grep -q "$idf_version"; then
        return 1
    fi
    
    # Check if build type is valid for this app and IDF version combination
    if ! is_valid_build_type "$build_type" "$app_type" "$idf_version"; then
        return 1
    fi
    
    # All validations passed
    return 0
}

# REMOVED: get_version_index() - Functionality now handled by get_idf_version_index()

# Get app-specific IDF versions
get_app_idf_versions() {
    local app_type="$1"
    
    if check_yq; then
        local app_idf_versions=$(run_yq ".apps.${app_type}.idf_versions" -r 2>/dev/null)
        if [ "$app_idf_versions" != "null" ] && [ -n "$app_idf_versions" ]; then
            # Extract first version from array
            echo "$app_idf_versions" | sed 's/\[//g' | sed 's/\]//g' | sed 's/"//g' | tr ',' '\n' | head -n1
            return 0
        fi
    fi
    
    # Fallback to global IDF versions
    get_idf_versions
}

# Get app-specific IDF versions as array (for parsing)
get_app_idf_versions_array() {
    local app_type="$1"
    
    if check_yq; then
        local app_idf_versions=$(run_yq ".apps.${app_type}.idf_versions" -r 2>/dev/null)
        if [ "$app_idf_versions" != "null" ] && [ -n "$app_idf_versions" ]; then
            echo "$app_idf_versions"
            return 0
        fi
    fi
    
    # Fallback to global IDF versions
    get_idf_versions
}

# Show valid combinations for a specific app
show_valid_combinations() {
    local app_type="$1"
    
    echo "   Application: $app_type"
    echo "   Description: $(get_app_description "$app_type")"
    echo ""
    echo "   Supported combinations:"
    
    local app_idf_versions=$(get_app_idf_versions "$app_type")
    local app_build_types=$(get_build_types "$app_type")
    
    # Clean up the build types array
    local clean_build_types=$(echo "$app_build_types" | sed 's/\[//g' | sed 's/\]//g' | sed 's/"//g' | tr ',' ' ')
    
    # For each IDF version, show supported build types
    while IFS= read -r version; do
        if [ -n "$version" ]; then
            echo "     • $version: $clean_build_types"
        fi
    done < <(echo "$app_idf_versions" | tr ',' '\n')
}



# REMOVED: get_idf_version_smart() - Functionality now handled by enhanced get_idf_version() and is_valid_combination()
