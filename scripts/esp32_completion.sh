#!/bin/bash
# ESP32 Scripts Tab Completion - Auto-Installing Version
# 
# This script automatically installs itself on first use, making tab completion
# work seamlessly without manual setup steps.
#
# Features:
# - Auto-detects first use and installs completion silently
# - Works across different projects (project-agnostic)
# - Dynamically discovers available options from project structure
# - Graceful fallback when scripts aren't available

# Auto-installation check and setup
_esp32_auto_setup() {
    # Check if we're already set up (avoid recursion)
    if [[ "${ESP32_COMPLETION_LOADED}" == "1" ]]; then
        return 0
    fi
    
    # Mark as loaded to prevent re-setup
    export ESP32_COMPLETION_LOADED=1
    
    # Get the directory where this completion script lives
    local script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
    local completion_script="$script_dir/esp32_completion.sh"
    
    # Check if we need to add to .bashrc for persistence
    if ! grep -q "ESP32 Scripts Auto Tab Completion" ~/.bashrc 2>/dev/null; then
        echo "🚀 Setting up ESP32 tab completion for future sessions..."
        {
            echo ""
            echo "# ESP32 Scripts Auto Tab Completion"
            echo "if [[ -f \"$completion_script\" ]]; then"
            echo "    source \"$completion_script\""
            echo "fi"
        } >> ~/.bashrc
        echo "✅ Tab completion will be available in new terminals!"
        echo "   (Current terminal is ready to use)"
    fi
    
    return 0
}

# Auto-setup on first load
_esp32_auto_setup

# Get the script directory for relative paths
_get_script_dir() {
    local script_path="$1"
    if [[ -x "$script_path" ]]; then
        dirname "$(readlink -f "$script_path")"
    else
        # Fallback: look for scripts in current directory or ../scripts
        if [[ -f "./scripts/build_app.sh" ]]; then
            echo "./scripts"
        elif [[ -f "../scripts/build_app.sh" ]]; then
            echo "../scripts"
        else
            echo "."
        fi
    fi
}

# Extract available app types using generic discovery methods
# This function is designed to work across different projects by:
# 1. First trying the build script's own list command (most reliable)
# 2. Parsing common configuration files for app definitions
# 3. Scanning directory structure for app-like directories  
# 4. Falling back to minimal generic options if nothing found
_get_build_app_types() {
    local script_dir="$1"
    local build_script="$script_dir/build_app.sh"
    
    if [[ -x "$build_script" ]]; then
        # Primary method: extract app types from the build script's list command
        "$build_script" list 2>/dev/null | grep -A 100 "All apps:" | grep "^  " | sed 's/^  \([^ ]*\).*/\1/' | grep -v "^$" | head -20
    else
        # Fallback discovery methods for when script is not executable
        
        # Method 1: Parse common configuration files
        local config_files=("$script_dir/../app_config.yml" "$script_dir/app_config.yml" "$script_dir/../config.yml")
        for config_file in "${config_files[@]}"; do
            if [[ -f "$config_file" ]]; then
                # Extract YAML keys that look like app names (basic parsing)
                grep -E "^\s*[a-zA-Z_][a-zA-Z0-9_]*\s*:" "$config_file" | sed 's/^\s*\([^:]*\):.*/\1/' | grep -E "^[a-zA-Z_]" | head -10
                return
            fi
        done
        
        # Method 2: Scan for app-like directories in main/
        if [[ -d "$script_dir/../main" ]]; then
            find "$script_dir/../main" -maxdepth 1 -type d -name "*_test*" -o -name "*_app*" -o -name "*_demo*" 2>/dev/null | xargs -n1 basename | head -10
        fi
        
        # Method 3: Minimal generic fallback (always works)
        echo "help"
        echo "list"
    fi
}

_get_build_types() {
    local script_dir="$1"
    local build_script="$script_dir/build_app.sh"
    
    if [[ -x "$build_script" ]]; then
        "$build_script" list 2>/dev/null | grep "Build types:" | sed 's/.*Build types: //' | tr ' ' '\n'
    else
        # Generic build types - common across most CMake/ESP-IDF projects
        echo "Release"
        echo "Debug"
    fi
}

_get_flash_operations() {
    echo "flash monitor flash_monitor erase_flash read_flash ports"
}

_get_common_flags() {
    echo "--clean --no-clean --use-cache --no-cache --project-path --help -h"
}

_get_flash_flags() {
    echo "$(_get_common_flags) --log --port -p --baud"
}

_get_serial_ports() {
    # Auto-discover available serial ports (ESP32 devices)
    local ports=()
    
    # Linux: Check ttyACM* (ESP32-C6, S3) and ttyUSB* (older ESP32 with CP210x/CH340)
    for port in /dev/ttyACM* /dev/ttyUSB*; do
        if [ -e "$port" ]; then
            ports+=("$port")
        fi
    done
    
    # macOS: Check cu.* devices
    for port in /dev/cu.usbmodem* /dev/cu.usbserial* /dev/cu.SLAB_USBtoUART* /dev/cu.CP210* /dev/cu.CH340*; do
        if [ -e "$port" ]; then
            ports+=("$port")
        fi
    done
    
    if [ ${#ports[@]} -gt 0 ]; then
        echo "${ports[@]}"
    else
        # Fallback for completion to work even if no devices connected
        echo "/dev/ttyACM0 /dev/ttyUSB0"
    fi
}

_get_baud_rates() {
    echo "115200 921600 460800 230400 57600 38400 19200 9600"
}

# Completion function for build_app.sh
_build_app_completion() {
    local cur prev opts script_dir
    COMPREPLY=()
    cur="${COMP_WORDS[COMP_CWORD]}"
    prev="${COMP_WORDS[COMP_CWORD-1]}"
    
    # Get script directory
    script_dir=$(_get_script_dir "${COMP_WORDS[0]}")
    
    # Handle flags that take arguments
    case "$prev" in
        --project-path)
            # Complete with directories
            COMPREPLY=($(compgen -d -- "$cur"))
            return 0
            ;;
        --idf-version)
            # Complete with IDF versions (if available)
            COMPREPLY=($(compgen -W "v5.1 v5.0 v4.4" -- "$cur"))
            return 0
            ;;
    esac
    
    # If current word starts with dash, complete flags
    if [[ "$cur" == -* ]]; then
        opts=$(_get_common_flags)
        COMPREPLY=($(compgen -W "$opts" -- "$cur"))
        return 0
    fi
    
    # Position-based completion
    local pos=0
    local i
    for ((i=1; i<COMP_CWORD; i++)); do
        case "${COMP_WORDS[i]}" in
            --*) 
                # Skip flags and their arguments
                if [[ "${COMP_WORDS[i]}" == "--project-path" ]]; then
                    ((i++)) # Skip the path argument
                fi
                ;;
            *)
                ((pos++))
                ;;
        esac
    done
    
    case $pos in
        0)
            # First argument: special commands or app types
            local special_commands="list info combinations validate help"
            local app_types=$(_get_build_app_types "$script_dir")
            opts="$special_commands $app_types"
            COMPREPLY=($(compgen -W "$opts" -- "$cur"))
            ;;
        1)
            # Second argument depends on first
            case "${COMP_WORDS[1]}" in
                list|help|combinations)
                    # These don't take additional arguments
                    COMPREPLY=()
                    ;;
                info|validate)
                    # These take an app name
                    opts=$(_get_build_app_types "$script_dir")
                    COMPREPLY=($(compgen -W "$opts" -- "$cur"))
                    ;;
                *)
                    # Regular app type, complete with build types
                    opts=$(_get_build_types "$script_dir")
                    COMPREPLY=($(compgen -W "$opts" -- "$cur"))
                    ;;
            esac
            ;;
        2)
            # Third argument: usually IDF version or additional build type args
            case "${COMP_WORDS[1]}" in
                validate)
                    # For validate command, this is build type
                    opts=$(_get_build_types "$script_dir")
                    COMPREPLY=($(compgen -W "$opts" -- "$cur"))
                    ;;
            esac
            ;;
        3)
            # Fourth argument: for validate command, this is IDF version
            if [[ "${COMP_WORDS[1]}" == "validate" ]]; then
                COMPREPLY=($(compgen -W "v5.1 v5.0 v4.4" -- "$cur"))
            fi
            ;;
    esac
}

# Completion function for flash_app.sh
_flash_app_completion() {
    local cur prev opts script_dir
    COMPREPLY=()
    cur="${COMP_WORDS[COMP_CWORD]}"
    prev="${COMP_WORDS[COMP_CWORD-1]}"
    
    # Get script directory  
    script_dir=$(_get_script_dir "${COMP_WORDS[0]}")
    
    # Handle flags that take arguments
    case "$prev" in
        --project-path)
            COMPREPLY=($(compgen -d -- "$cur"))
            return 0
            ;;
        --port|-p)
            COMPREPLY=($(compgen -W "$(_get_serial_ports)" -- "$cur"))
            return 0
            ;;
        --baud)
            COMPREPLY=($(compgen -W "$(_get_baud_rates)" -- "$cur"))
            return 0
            ;;
        --log)
            # Complete with common log filenames
            COMPREPLY=($(compgen -W "debug test session" -- "$cur"))
            return 0
            ;;
    esac
    
    # If current word starts with dash, complete flags
    if [[ "$cur" == -* ]]; then
        opts=$(_get_flash_flags)
        COMPREPLY=($(compgen -W "$opts" -- "$cur"))
        return 0
    fi
    
    # Position-based completion (skip flags when counting)
    local pos=0
    local i
    for ((i=1; i<COMP_CWORD; i++)); do
        case "${COMP_WORDS[i]}" in
            --*|-p) 
                # Skip flags and their arguments
                case "${COMP_WORDS[i]}" in
                    --project-path|--port|-p|--baud|--log)
                        ((i++)) # Skip the flag argument
                        ;;
                esac
                ;;
            *)
                ((pos++))
                ;;
        esac
    done
    
    case $pos in
        0)
            # First argument: operations, special commands, or app types
            local special_commands="list help"
            local operations=$(_get_flash_operations)
            local app_types=$(_get_build_app_types "$script_dir")
            opts="$operations $special_commands $app_types"
            COMPREPLY=($(compgen -W "$opts" -- "$cur"))
            ;;
        1)
            # Second argument depends on first
            case "${COMP_WORDS[1]}" in
                list|help)
                    COMPREPLY=()
                    ;;
                flash|monitor|flash_monitor|erase_flash|read_flash)
                    # Operation specified, complete with app types
                    opts=$(_get_build_app_types "$script_dir")
                    COMPREPLY=($(compgen -W "$opts" -- "$cur"))
                    ;;
                ports)
                    # ports command takes no additional arguments
                    COMPREPLY=()
                    ;;
                *)
                    # App type specified, complete with build types
                    opts=$(_get_build_types "$script_dir")
                    COMPREPLY=($(compgen -W "$opts" -- "$cur"))
                    ;;
            esac
            ;;
        2)
            # Third argument
            case "${COMP_WORDS[1]}" in
                flash|monitor|flash_monitor)
                    # After flash operation and app, complete with build types
                    opts=$(_get_build_types "$script_dir")
                    COMPREPLY=($(compgen -W "$opts" -- "$cur"))
                    ;;
                *)
                    # After app and build type, complete with IDF versions
                    COMPREPLY=($(compgen -W "v5.1 v5.0 v4.4" -- "$cur"))
                    ;;
            esac
            ;;
        3)
            # Fourth argument: usually IDF version
            COMPREPLY=($(compgen -W "v5.1 v5.0 v4.4" -- "$cur"))
            ;;
    esac
}

# Register completion functions
complete -F _build_app_completion build_app.sh
complete -F _flash_app_completion flash_app.sh

# Also register for when scripts are called with path
complete -F _build_app_completion ./build_app.sh
complete -F _flash_app_completion ./flash_app.sh
complete -F _build_app_completion scripts/build_app.sh  
complete -F _flash_app_completion scripts/flash_app.sh
complete -F _build_app_completion ./scripts/build_app.sh
complete -F _flash_app_completion ./scripts/flash_app.sh