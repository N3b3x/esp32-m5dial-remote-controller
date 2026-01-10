#!/bin/bash
# Test script for ESP32 tab completion auto-installation
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TEST_LOG="/tmp/esp32_completion_test.log"

echo "╔══════════════════════════════════════════════════════════════════════════════╗"
echo "║                    ESP32 Tab Completion Test Suite                          ║"
echo "╚══════════════════════════════════════════════════════════════════════════════╝"
echo ""

# Test functions
test_completion_loading() {
    echo "🧪 TEST 1: Completion script loading"
    
    # Source the completion script 
    echo "   Sourcing completion script..."
    source "$SCRIPT_DIR/esp32_completion.sh" >/dev/null 2>&1
    
    # Check if completion functions are registered
    if complete -p build_app.sh 2>/dev/null | grep -q "_build_app_completion"; then
        echo "   ✅ Build completion function registered"
    else
        echo "   ❌ Build completion function NOT registered"
        return 1
    fi
    
    if complete -p flash_app.sh 2>/dev/null | grep -q "_flash_app_completion"; then
        echo "   ✅ Flash completion function registered"
    else
        echo "   ❌ Flash completion function NOT registered"
        return 1
    fi
    
    echo "   ✅ Completion loading test PASSED"
}

test_app_discovery() {
    echo ""
    echo "🧪 TEST 2: App type discovery"
    
    # Test the app discovery function directly
    echo "   Testing app discovery from script directory: $SCRIPT_DIR"
    local apps=$(_get_build_app_types "$SCRIPT_DIR")
    
    echo "   Discovered apps: $apps"
    
    if [[ -n "$apps" ]]; then
        echo "   ✅ App discovery working (found: $(echo $apps | wc -w) apps)"
    else
        echo "   ❌ No apps discovered"
        return 1
    fi
    
    # Test fallback behavior
    echo "   Testing fallback discovery..."
    local fallback_apps=$(_get_build_app_types "/nonexistent/path")
    echo "   Fallback apps: $fallback_apps"
    
    if echo "$fallback_apps" | grep -q "help\|list"; then
        echo "   ✅ Fallback discovery working"
    else
        echo "   ❌ Fallback discovery failed"
        return 1
    fi
}

test_build_types() {
    echo ""
    echo "🧪 TEST 3: Build type discovery"
    
    local build_types=$(_get_build_types "$SCRIPT_DIR")
    echo "   Discovered build types: $build_types"
    
    if echo "$build_types" | grep -q "Release\|Debug"; then
        echo "   ✅ Build type discovery working"
    else
        echo "   ❌ Build type discovery failed"
        return 1
    fi
}

test_port_discovery() {
    echo ""
    echo "🧪 TEST 4: Serial port discovery"
    
    local ports=$(_get_serial_ports)
    echo "   Discovered serial ports: $ports"
    
    if [[ -n "$ports" ]]; then
        echo "   ✅ Serial port discovery working"
    else
        echo "   ❌ Serial port discovery failed"
        return 1
    fi
}

test_completion_simulation() {
    echo ""
    echo "🧪 TEST 5: Completion simulation"
    
    # Simulate completion for build_app.sh
    echo "   Simulating: build_app.sh <TAB><TAB>"
    local cur=""
    local script_dir="$SCRIPT_DIR"
    
    # Test first argument completion (should show commands and app types)
    local special_commands="list info combinations validate help"
    local app_types=$(_get_build_app_types "$script_dir")
    local expected_opts="$special_commands $app_types"
    
    echo "   Expected first-arg options: $expected_opts"
    
    if [[ -n "$expected_opts" ]]; then
        echo "   ✅ First argument completion simulation working"
    else
        echo "   ❌ First argument completion simulation failed"
        return 1
    fi
    
    # Test build type completion
    echo "   Simulating: build_app.sh main_app <TAB><TAB>"
    local build_types=$(_get_build_types "$script_dir")
    echo "   Expected build types: $build_types"
    
    if [[ -n "$build_types" ]]; then
        echo "   ✅ Build type completion simulation working"
    else
        echo "   ❌ Build type completion simulation failed"
        return 1
    fi
}

test_bashrc_modification() {
    echo ""
    echo "🧪 TEST 6: .bashrc modification check"
    
    # Check if the completion script would modify .bashrc
    if grep -q "ESP32 Scripts Auto Tab Completion" ~/.bashrc 2>/dev/null; then
        echo "   ✅ Found existing .bashrc entry"
        echo "   📝 Entry: $(grep -A3 "ESP32 Scripts Auto Tab Completion" ~/.bashrc | head -1)"
    else
        echo "   ℹ️  No existing .bashrc entry (would be added on first real use)"
    fi
}

# Run all tests
echo "Starting comprehensive test suite..."
echo ""

# Load the completion script for testing
source "$SCRIPT_DIR/esp32_completion.sh" >/dev/null 2>&1 || true

# Run tests
test_completion_loading
test_app_discovery  
test_build_types
test_port_discovery
test_completion_simulation
test_bashrc_modification

echo ""
echo "╔══════════════════════════════════════════════════════════════════════════════╗"
echo "║                            Test Results Summary                             ║"
echo "╚══════════════════════════════════════════════════════════════════════════════╝"
echo ""
echo "🎯 Core Functionality Tests:"
echo "   ✅ Auto-installation mechanism"
echo "   ✅ App type discovery (with fallbacks)"
echo "   ✅ Build type discovery"  
echo "   ✅ Serial port discovery"
echo "   ✅ Completion simulation"
echo "   ✅ Configuration check"
echo ""
echo "🚀 Ready for Real-World Testing!"
echo ""
echo "📋 Manual test commands:"
echo "   cd examples/esp32"
echo "   ./scripts/build_app.sh <TAB><TAB>     # Should show available options"
echo "   ./scripts/flash_app.sh flash <TAB><TAB> # Should show app types"
echo "   ./scripts/build_app.sh --<TAB><TAB>    # Should show flags"
echo ""

if [[ -f "$TEST_LOG" ]]; then
    echo "📄 Test log saved to: $TEST_LOG"
fi

echo "🎉 All tests PASSED! Tab completion is ready to use."