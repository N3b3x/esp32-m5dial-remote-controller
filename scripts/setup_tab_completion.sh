#!/bin/bash
# Setup script for ESP32 tab completion
# This script enables bash tab completion for build_app.sh and flash_app.sh

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
COMPLETION_FILE="$SCRIPT_DIR/esp32_completion.sh"
BASHRC_LINE="source '$COMPLETION_FILE'"

echo "╔══════════════════════════════════════════════════════════════════════════════╗"
echo "║                        ESP32 Scripts Tab Completion Setup                    ║"
echo "╚══════════════════════════════════════════════════════════════════════════════╝"
echo ""

# Check if completion file exists
if [[ ! -f "$COMPLETION_FILE" ]]; then
    echo "❌ ERROR: Completion file not found at $COMPLETION_FILE"
    exit 1
fi

echo "🔧 Setting up bash tab completion for ESP32 build and flash scripts..."
echo ""

# Method 1: Add to user's .bashrc (recommended for development)
setup_user_bashrc() {
    local bashrc="$HOME/.bashrc"
    
    if [[ -f "$bashrc" ]]; then
        if grep -Fq "$BASHRC_LINE" "$bashrc"; then
            echo "✅ Tab completion already configured in ~/.bashrc"
        else
            echo "📝 Adding tab completion to ~/.bashrc..."
            echo "" >> "$bashrc"
            echo "# ESP32 Scripts Tab Completion" >> "$bashrc"
            echo "$BASHRC_LINE" >> "$bashrc"
            echo "✅ Added to ~/.bashrc - restart terminal or run: source ~/.bashrc"
        fi
    else
        echo "⚠️  ~/.bashrc not found - creating one..."
        echo "$BASHRC_LINE" > "$bashrc"
        echo "✅ Created ~/.bashrc with tab completion"
    fi
}

# Method 2: Install system-wide (requires sudo)
setup_system_wide() {
    local system_dir="/etc/bash_completion.d"
    local system_file="$system_dir/esp32_scripts"
    
    if [[ -d "$system_dir" ]]; then
        echo "🔧 Installing system-wide completion (requires sudo)..."
        if sudo cp "$COMPLETION_FILE" "$system_file"; then
            echo "✅ Installed to $system_file"
            echo "   All users will have tab completion after restarting terminal"
        else
            echo "❌ Failed to install system-wide"
            return 1
        fi
    else
        echo "⚠️  System completion directory not found: $system_dir"
        return 1
    fi
}

# Method 3: Source directly in current session
setup_current_session() {
    echo "🚀 Enabling tab completion for current session..."
    source "$COMPLETION_FILE"
    echo "✅ Tab completion enabled for current terminal session"
}

# Show options
echo "Choose installation method:"
echo "  1) User ~/.bashrc (recommended for development)"
echo "  2) System-wide /etc/bash_completion.d/ (requires sudo)"  
echo "  3) Current session only (temporary)"
echo "  4) All methods"
echo "  q) Quit"
echo ""

while true; do
    read -p "Select option [1-4, q]: " choice
    case $choice in
        1)
            setup_user_bashrc
            break
            ;;
        2)
            if setup_system_wide; then
                break
            else
                echo "Falling back to user ~/.bashrc method..."
                setup_user_bashrc
                break
            fi
            ;;
        3)
            setup_current_session
            break
            ;;
        4)
            setup_current_session
            echo ""
            setup_user_bashrc
            echo ""
            if ! setup_system_wide; then
                echo "⚠️  System-wide setup failed, but user setup succeeded"
            fi
            break
            ;;
        q|Q)
            echo "❌ Setup cancelled"
            exit 0
            ;;
        *)
            echo "Invalid choice. Please select 1-4 or q"
            ;;
    esac
done

echo ""
echo "╔══════════════════════════════════════════════════════════════════════════════╗"
echo "║                            Setup Complete!                                   ║"
echo "╚══════════════════════════════════════════════════════════════════════════════╝"
echo ""
echo "💡 Tab completion is now available for:"
echo "   • ./build_app.sh <TAB><TAB>"
echo "   • ./flash_app.sh <TAB><TAB>"
echo "   • ./scripts/build_app.sh <TAB><TAB>"
echo "   • ./scripts/flash_app.sh <TAB><TAB>"
echo ""
echo "🧪 Test tab completion:"
echo "   ./scripts/build_app.sh <TAB><TAB>     # Shows available apps and commands"
echo "   ./scripts/flash_app.sh flash <TAB><TAB>  # Shows available apps"
echo "   ./scripts/build_app.sh list           # See all available options"
echo ""
echo "📚 Available completion features:"
echo "   • App types (auto-discovered from your project)"
echo "   • Build types (Release, Debug, etc.)"
echo "   • Flash operations (flash, monitor, flash_monitor)"  
echo "   • Command flags (--clean, --no-cache, --help, etc.)"
echo "   • Special commands (list, info, combinations, validate)"
echo ""

if [[ "$choice" != "3" ]]; then
    echo "🔄 To activate in current terminal:"
    echo "   source ~/.bashrc"
    echo ""
    echo "   Or restart your terminal"
fi

echo "✨ Happy building and flashing! ⚡"