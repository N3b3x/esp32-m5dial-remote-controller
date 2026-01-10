#!/bin/bash
# ESP32 Port Detection Script
# This script automatically finds ESP32 devices and troubleshoots port issues

set -e  # Exit on any error

# Show help if requested
if [ "$1" = "--help" ] || [ "$1" = "-h" ]; then
    echo "ESP32 Port Detection Script"
    echo ""
    echo "Usage: ./detect_ports.sh [OPTIONS]"
    echo ""
    echo "OPTIONS:"
    echo "  --verbose                   - Show detailed device info (USB details, permissions)"
    echo "  --test-connection           - Test if detected ports are actually accessible"
    echo "  --help, -h                  - Show this help message"
    echo ""
    echo "PURPOSE:"
    echo "  Automatically find ESP32 devices and troubleshoot port issues"
    echo ""
    echo "WHAT IT DOES:"
    echo "  • Scans system for available serial ports"
    echo "  • Identifies ESP32 devices by USB vendor/product IDs"
    echo "  • Checks port permissions and accessibility"
    echo "  • Tests port connectivity and responsiveness"
    echo "  • Provides troubleshooting information for common issues"
    echo "  • Suggests solutions for permission and access problems"
    echo ""
    echo "DETECTION METHODS:"
    echo "  • USB device enumeration (lsusb, udev)"
    echo "  • Serial port scanning (/dev/tty*, /dev/ttyUSB*)"
    echo "  • ESP32-specific device identification"
    echo "  • Permission and ownership verification"
    echo "  • Port accessibility testing"
    echo ""
    echo "EXAMPLES:"
    echo "  ./detect_ports.sh                    # Quick port detection"
    echo "  ./detect_ports.sh --verbose          # Detailed device information"
    echo "  ./detect_ports.sh --test-connection  # Verify ports work and test connectivity"
    echo ""
    echo "OUTPUT INFORMATION:"
    echo "  • Available serial ports"
    echo "  • ESP32 device identification"
    echo "  • Port permissions and ownership"
    echo "  • USB device details (with --verbose)"
    echo "  • Connection test results (with --test-connection)"
    echo "  • Troubleshooting suggestions"
    echo ""
    echo "COMMON ISSUES AND SOLUTIONS:"
    echo "  • Permission denied: Add user to dialout group or use udev rules"
    echo "  • Port not found: Check USB connection and cable"
    echo "  • Device not recognized: Install proper USB drivers"
    echo "  • Port busy: Close other applications using the port"
    echo "  • Access timeout: Check device boot mode and reset if needed"
    echo ""
    echo "TROUBLESHOOTING STEPS:"
    echo "  1. Run: ./detect_ports.sh --verbose"
    echo "  2. Check port permissions and ownership"
    echo "  3. Test connection: ./detect_ports.sh --test-connection"
    echo "  4. Add user to dialout group: sudo usermod -a -G dialout $USER"
    echo "  5. Create udev rules for persistent permissions"
    echo "  6. Restart terminal or log out/in after permission changes"
    echo ""
    echo "UDEV RULES (for persistent permissions):"
    echo "  Create /etc/udev/rules.d/99-esp32.rules:"
    echo "  SUBSYSTEM==\"tty\", ATTRS{idVendor}==\"10c4\", ATTRS{idProduct}==\"ea60\", MODE=\"0666\""
    echo "  SUBSYSTEM==\"tty\", ATTRS{idVendor}==\"1a86\", ATTRS{idProduct}==\"7523\", MODE=\"0666\""
    echo ""
    echo "For detailed information, see: docs/README_PORT_DETECTION.md"
    exit 0
fi

VERBOSE=false
TEST_CONNECTION=false

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --verbose)
            VERBOSE=true
            shift
            ;;
        --test-connection)
            TEST_CONNECTION=true
            shift
            ;;
        *)
            echo "Unknown option: $1"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

# Function to detect operating system
detect_os() {
    case "$(uname -s)" in
        Darwin*)    echo "macos" ;;
        Linux*)     echo "linux" ;;
        CYGWIN*|MINGW*|MSYS*) echo "windows" ;;
        *)          echo "unknown" ;;
    esac
}

# Function to get system information
get_system_info() {
    local os=$(detect_os)
    echo "=== System Information ==="
    echo "Operating System: $os"
    echo "Kernel: $(uname -r)"
    
    case "$os" in
        "macos")
            echo "macOS Version: $(sw_vers -productVersion 2>/dev/null || echo 'Unknown')"
            ;;
        "linux")
            echo "Distribution: $(cat /etc/os-release 2>/dev/null | grep PRETTY_NAME | cut -d'"' -f2 || echo 'Unknown')"
            ;;
    esac
    echo ""
}

# Function to detect ESP32 devices
detect_esp32_devices() {
    local os=$(detect_os)
    local devices=()
    
    echo "=== ESP32 Device Detection ==="
    
    case "$os" in
        "macos")
            echo "Searching for ESP32 devices on macOS..."
            
            # Check for ESP32-specific devices
            for pattern in "usbmodem" "usbserial" "SLAB_USBtoUART" "CP210" "CH340"; do
                for port in /dev/cu.$pattern* /dev/tty.$pattern*; do
                    if [ -e "$port" ]; then
                        devices+=("$port")
                        echo "  Found: $port"
                    fi
                done
            done
            
            # Check for any other USB serial devices
            for port in /dev/cu.usb* /dev/tty.usb*; do
                if [ -e "$port" ] && [[ ! " ${devices[@]} " =~ " ${port} " ]]; then
                    devices+=("$port")
                    echo "  Found: $port"
                fi
            done
            ;;
            
        "linux")
            echo "Searching for ESP32 devices on Linux..."
            
            # Check for ESP32 devices
            for port in /dev/ttyACM* /dev/ttyUSB*; do
                if [ -e "$port" ]; then
                    devices+=("$port")
                    echo "  Found: $port"
                fi
            done
            
            # Use lsusb if available to identify ESP32 devices
            if command -v lsusb &> /dev/null; then
                echo "USB device enumeration:"
                if lsusb | grep -q "Silicon Labs\|CP210\|CH340\|ESP\|Espressif"; then
                    echo "  ESP32-related devices found:"
                    lsusb | grep -E "Silicon Labs|CP210|CH340|ESP|Espressif" | sed 's/^/    /'
                else
                    echo "  No ESP32-related USB devices found"
                fi
            fi
            ;;
            
        *)
            echo "Unsupported operating system: $os"
            return 1
            ;;
    esac
    
    if [ ${#devices[@]} -eq 0 ]; then
        echo "  No ESP32 devices found"
        return 1
    fi
    
    echo ""
    echo "Total ESP32 devices found: ${#devices[@]}"
    return 0
}

# Function to get detailed device information
get_device_info() {
    local port="$1"
    local os=$(detect_os)
    
    echo "=== Device Information: $port ==="
    
    if [ ! -e "$port" ]; then
        echo "  ERROR: Port does not exist"
        return 1
    fi
    
    # Basic file information
    echo "  File permissions: $(ls -la "$port" 2>/dev/null | awk '{print $1, $3, $4}')"
    echo "  Readable: $([ -r "$port" ] && echo "Yes" || echo "No")"
    echo "  Writable: $([ -w "$port" ] && echo "Yes" || echo "No")"
    
    case "$os" in
        "macos")
            # macOS specific info
            if [[ "$port" == "/dev/cu."* ]]; then
                echo "  Type: Callout device (preferred for serial communication)"
            elif [[ "$port" == "/dev/tty."* ]]; then
                echo "  Type: Terminal device"
            fi
            
            # Try to get device details from system_profiler
            if command -v system_profiler &> /dev/null; then
                echo "  System Information:"
                system_profiler SPUSBDataType 2>/dev/null | grep -A 10 -B 2 "$(basename "$port")" | sed 's/^/    /' || echo "    No detailed information available"
            fi
            ;;
            
        "linux")
            # Linux specific info
            if [[ "$port" == "/dev/ttyACM"* ]]; then
                echo "  Type: CDC ACM device (common for ESP32-C6)"
            elif [[ "$port" == "/dev/ttyUSB"* ]]; then
                echo "  Type: USB-to-UART bridge (common for older ESP32)"
            fi
            
            # Try to get device details from udev
            if command -v udevadm &> /dev/null; then
                echo "  UDEV Information:"
                udevadm info --name="$port" 2>/dev/null | grep -E "(ID_VENDOR|ID_MODEL|ID_SERIAL)" | sed 's/^/    /' || echo "    No UDEV information available"
            fi
            ;;
    esac
    
    echo ""
}

# Function to test port accessibility
test_port_connection() {
    local port="$1"
    local os=$(detect_os)
    
    echo "=== Testing Port Connection: $port ==="
    
    if [ ! -e "$port" ]; then
        echo "  ERROR: Port does not exist"
        return 1
    fi
    
    if [ ! -r "$port" ]; then
        echo "  ERROR: Port is not readable"
        echo "  This usually indicates a permission issue"
        
        case "$os" in
            "linux")
                echo "  Try: sudo chmod 666 $port"
                echo "  Or add user to dialout group: sudo usermod -a -G dialout $USER"
                ;;
            "macos")
                echo "  Try disconnecting and reconnecting the device"
                echo "  Check System Preferences > Security & Privacy > Privacy > Full Disk Access"
                ;;
        esac
        return 1
    fi
    
    echo "  Port is accessible"
    
    # Try to open the port (this will fail if it's in use)
    if command -v stty &> /dev/null; then
        echo "  Testing port configuration..."
        if stty -F "$port" 2>/dev/null; then
            echo "  Port configuration successful"
        else
            echo "  WARNING: Port configuration failed (might be in use by another application)"
        fi
    fi
    
    echo ""
    return 0
}

# Function to provide troubleshooting advice
provide_troubleshooting() {
    local os=$(detect_os)
    
    echo "=== Troubleshooting Guide ==="
    
    case "$os" in
        "macos")
            echo "macOS-specific troubleshooting:"
            echo "1. Check System Information > USB for connected devices"
            echo "2. Look for devices named 'USB Serial' or similar"
            echo "3. Ensure you have the necessary USB-to-UART drivers:"
            echo "   - Silicon Labs CP210x: Usually built into macOS"
            echo "   - CH340: May need driver installation"
            echo "4. Check System Preferences > Security & Privacy > Privacy > Full Disk Access"
            echo "5. Try different USB cables and ports"
            ;;
            
        "linux")
            echo "Linux-specific troubleshooting:"
            echo "1. Check if device appears: ls /dev/ttyACM* /dev/ttyUSB*"
            echo "2. Check USB enumeration: lsusb (if available)"
            echo "3. Check kernel messages: dmesg | tail (after connecting device)"
            echo "4. Fix permissions: sudo chmod 666 /dev/ttyACM*"
            echo "5. Add user to dialout group: sudo usermod -a -G dialout $USER"
            echo "6. Install udev rules for persistent permissions"
            ;;
    esac
    
    echo ""
    echo "General troubleshooting:"
    echo "1. Disconnect and reconnect the ESP32 device"
    echo "2. Try a different USB cable"
    echo "3. Try a different USB port"
    echo "4. Check if the device is being used by another application"
    echo "5. Ensure the ESP32 is in download mode (hold BOOT button while connecting)"
    echo "6. Check if you need to install specific drivers for your USB-to-UART chip"
    echo ""
}

# Main execution
main() {
    echo "ESP32 Port Detection and Troubleshooting Script"
    echo "=============================================="
    echo ""
    
    get_system_info
    
    if detect_esp32_devices; then
        # Get the list of devices
        local devices=()
        case "$(detect_os)" in
            "macos")
                for pattern in "usbmodem" "usbserial" "SLAB_USBtoUART" "CP210" "CH340"; do
                    for port in /dev/cu.$pattern* /dev/tty.$pattern*; do
                        if [ -e "$port" ]; then
                            devices+=("$port")
                        fi
                    done
                done
                for port in /dev/cu.usb* /dev/tty.usb*; do
                    if [ -e "$port" ] && [[ ! " ${devices[@]} " =~ " ${port} " ]]; then
                        devices+=("$port")
                    fi
                done
                ;;
            "linux")
                for port in /dev/ttyACM* /dev/ttyUSB*; do
                    if [ -e "$port" ]; then
                        devices+=("$port")
                    fi
                done
                ;;
        esac
        
        # Show detailed information for each device
        if [ "$VERBOSE" = true ]; then
            for device in "${devices[@]}"; do
                get_device_info "$device"
            done
        fi
        
        # Test connections if requested
        if [ "$TEST_CONNECTION" = true ]; then
            for device in "${devices[@]}"; do
                test_port_connection "$device"
            done
        fi
        
        echo "=== Summary ==="
        echo "Found ${#devices[@]} ESP32 device(s):"
        for device in "${devices[@]}"; do
            echo "  $device"
        done
        echo ""
        echo "To use with ESP-IDF, set: export ESPPORT=\"${devices[0]}\""
        echo "Or use the port directly: idf.py -p \"${devices[0]}\" flash monitor"
        
    else
        echo "No ESP32 devices detected"
        echo ""
        provide_troubleshooting
    fi
}

# Run main function
main "$@"
