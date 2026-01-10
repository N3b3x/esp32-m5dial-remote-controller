#!/bin/bash
# ESP32 Log Management Script
# This script helps view, search, and manage ESP32 development logs

set -e  # Exit on any error

# Show help if requested
if [ "$1" = "--help" ] || [ "$1" = "-h" ]; then
    echo "ESP32 Log Management Script"
    echo ""
    echo "Usage: ./manage_logs.sh [COMMAND] [OPTIONS] [ARGUMENTS]"
    echo ""
    echo "COMMANDS:"
    echo "  list [pattern]              - List all log files with details (optional pattern filter)"
    echo "  view <log_name>             - View a specific log file"
    echo "  search <pattern>            - Search across all log files for text patterns"
    echo "  clean [days]                - Clean logs older than N days (default: 30)"
    echo "  stats                       - Show log statistics and summary"
    echo "  latest                      - Show the most recent log file"
    echo "  tail <log_name> [lines]     - Show last N lines of a log file (default: 50)"
    echo "  grep <log_name> <pattern>   - Search within a specific log file"
    echo ""
    echo "OPTIONS:"
    echo "  --help, -h                  - Show this help message"
    echo "  --verbose                    - Show detailed output"
    echo "  --no-color                   - Disable colored output"
    echo "  --follow                     - Follow log file in real-time (for tail command)"
    echo ""
    echo "ARGUMENTS:"
    echo "  log_name                    - Name of log file to operate on"
    echo "  pattern                     - Text pattern to search for"
    echo "  days                        - Number of days for log retention"
    echo "  lines                       - Number of lines to show (for tail command)"
    echo ""
    echo "EXAMPLES:"
    echo "  # List and view logs"
    echo "  ./manage_logs.sh list                           # List all logs"
    echo "  ./manage_logs.sh list gpio                     # List logs containing 'gpio'"
    echo "  ./manage_logs.sh view gpio_Release_20241201_143022.log  # View specific log"
    echo "  ./manage_logs.sh latest                         # Show latest log"
    echo ""
    echo "  # Search and filter"
    echo "  ./manage_logs.sh search \"ERROR\"               # Search for errors in all logs"
    echo "  ./manage_logs.sh search \"build failed\"        # Search for build failures"
    echo "  ./manage_logs.sh grep gpio_test.log \"GPIO\"    # Search within specific log"
    echo ""
    echo "  # Log maintenance"
    echo "  ./manage_logs.sh clean 7                        # Clean logs older than 7 days"
    echo "  ./manage_logs.sh clean                          # Clean logs older than 30 days"
    echo "  ./manage_logs.sh stats                          # Show log statistics"
    echo ""
    echo "  # Real-time monitoring"
    echo "  ./manage_logs.sh tail gpio_test.log             # Show last 50 lines"
    echo "  ./manage_logs.sh tail gpio_test.log 100         # Show last 100 lines"
    echo "  ./manage_logs.sh tail gpio_test.log --follow    # Follow log in real-time"
    echo ""
    echo "LOG FILE NAMING CONVENTION:"
    echo "  • Format: {app_type}_{build_type}_{date}_{time}.log"
    echo "  • Example: gpio_test_Release_20241201_143022.log"
    echo "  • Location: examples/esp32/logs/"
    echo "  • Date format: YYYYMMDD"
    echo "  • Time format: HHMMSS"
    echo ""
    echo "SEARCH PATTERNS:"
    echo "  • Simple text: \"ERROR\", \"build failed\", \"GPIO\""
    echo "  • Regular expressions: \"ERROR|WARN\", \"build.*failed\""
    echo "  • Case insensitive: \"error\", \"Error\", \"ERROR\""
    echo "  • Multi-word: \"build failed\", \"GPIO test\""
    echo ""
    echo "LOG RETENTION:"
    echo "  • Default retention: 30 days"
  echo "  • Configurable via days argument"
  echo "  • Automatic cleanup of old logs"
  echo "  • Safe deletion with confirmation"
  echo ""
  echo "OUTPUT FORMATS:"
  echo "  • List: File name, size, date, age"
  echo "  • Stats: Total logs, total size, oldest/newest dates"
  echo "  • Search: File name, line number, matching text"
  echo "  • View: Full log content with syntax highlighting"
  echo ""
  echo "TROUBLESHOOTING:"
  echo "  • No logs found: Check logs directory exists and contains files"
  echo "  • Permission denied: Check file permissions and ownership"
  echo "  • Search not working: Verify pattern syntax and case sensitivity"
  echo "  • Clean not working: Check write permissions and disk space"
  echo ""
  echo "For detailed information, see: docs/README_LOGGING_SYSTEM.md"
  exit 0
fi

# Load configuration
PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LOG_DIR="$PROJECT_DIR/logs"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to print colored output
print_color() {
    local color="$1"
    local message="$2"
    echo -e "${color}${message}${NC}"
}

# Function to show usage
show_usage() {
    echo "ESP32 Log Management Script"
    echo ""
    echo "Usage: $0 [command] [options] [arguments]"
    echo ""
    echo "Purpose: View, search, and manage ESP32 development logs"
    echo ""
    echo "Commands:"
    echo "  list                    List all log files with details"
    echo "  view <log_name>        View a specific log file"
    echo "  search <pattern>       Search across all log files"
    echo "  clean [days]           Clean logs older than N days (default: 30)"
    echo "  stats                  Show log statistics"
    echo "  latest                 Show the most recent log file"
    echo ""
    echo "Examples:"
    echo "  $0 list                           # List all logs"
    echo "  $0 view gpio_Release_20241201_143022.log  # View specific log"
    echo "  $0 search \"ERROR\"               # Search for errors in all logs"
    echo "  $0 clean 7                        # Clean logs older than 7 days"
    echo "  $0 latest                         # Show latest log"
}

# Function to check if logs directory exists
check_logs_dir() {
    if [ ! -d "$LOG_DIR" ]; then
        print_color $RED "ERROR: Logs directory not found: $LOG_DIR"
        echo "No logs have been generated yet. Use flash_app.sh with --log flag to create logs."
        exit 1
    fi
}

# Function to list all log files
list_logs() {
    check_logs_dir
    
    local log_files=($(find "$LOG_DIR" -name "*.log" -type f | sort -r))
    
    if [ ${#log_files[@]} -eq 0 ]; then
        print_color $YELLOW "No log files found in $LOG_DIR"
        return
    fi
    
    print_color $BLUE "Found ${#log_files[@]} log file(s):"
    echo ""
    
    printf "%-50s %-20s %-15s %-10s\n" "Filename" "Date" "Time" "Size"
    printf "%-50s %-20s %-15s %-10s\n" "---------" "----" "----" "----"
    
    for log_file in "${log_files[@]}"; do
        local filename=$(basename "$log_file")
        local date_str=$(stat -c "%y" "$log_file" 2>/dev/null | cut -d' ' -f1 || echo "Unknown")
        local time_str=$(stat -c "%y" "$log_file" 2>/dev/null | cut -d' ' -f2 | cut -d'.' -f1 || echo "Unknown")
        local size=$(du -h "$log_file" 2>/dev/null | cut -f1 || echo "Unknown")
        
        printf "%-50s %-20s %-15s %-10s\n" "$filename" "$date_str" "$time_str" "$size"
    done
}

# Function to view a specific log file
view_log() {
    check_logs_dir
    
    if [ $# -eq 0 ]; then
        print_color $RED "ERROR: Please specify a log file to view"
        echo "Use '$0 list' to see available log files"
        exit 1
    fi
    
    local log_name="$1"
    local log_path=""
    
    # Check if it's a full path or just filename
    if [[ "$log_name" == *"/"* ]]; then
        log_path="$log_name"
    else
        log_path="$LOG_DIR/$log_name"
    fi
    
    if [ ! -f "$log_path" ]; then
        print_color $RED "ERROR: Log file not found: $log_path"
        echo "Use '$0 list' to see available log files"
        exit 1
    fi
    
    print_color $GREEN "Viewing log: $log_path"
    echo "File size: $(du -h "$log_path" | cut -f1)"
    echo "Last modified: $(stat -c "%y" "$log_path")"
    echo "======================================================"
    echo ""
    
    # Use less for better viewing experience
    if command -v less &> /dev/null; then
        less "$log_path"
    else
        cat "$log_path"
    fi
}

# Function to search across all log files
search_logs() {
    check_logs_dir
    
    if [ $# -eq 0 ]; then
        print_color $RED "ERROR: Please specify a search pattern"
        echo "Usage: $0 search <pattern>"
        exit 1
    fi
    
    local pattern="$1"
    local log_files=($(find "$LOG_DIR" -name "*.log" -type f))
    
    if [ ${#log_files[@]} -eq 0 ]; then
        print_color $YELLOW "No log files found to search"
        return
    fi
    
    print_color $BLUE "Searching for pattern: '$pattern'"
    echo "Searching across ${#log_files[@]} log file(s)..."
    echo ""
    
    local found_matches=false
    
    for log_file in "${log_files[@]}"; do
        local filename=$(basename "$log_file")
        local matches=$(grep -c "$pattern" "$log_file" 2>/dev/null || echo "0")
        
        if [ "$matches" -gt 0 ]; then
            found_matches=true
            print_color $GREEN "Found $matches match(es) in: $filename"
            
            # Show context around matches
            echo "Matches:"
            grep -n "$pattern" "$log_file" | head -5 | sed 's/^/  /'
            
            if [ "$matches" -gt 5 ]; then
                echo "  ... and $((matches - 5)) more matches"
            fi
            echo ""
        fi
    done
    
    if [ "$found_matches" = false ]; then
        print_color $YELLOW "No matches found for pattern: '$pattern'"
    fi
}

# Function to clean old log files
clean_logs() {
    check_logs_dir
    
    local days=${1:-30}
    
    if ! [[ "$days" =~ ^[0-9]+$ ]]; then
        print_color $RED "ERROR: Days must be a positive integer"
        exit 1
    fi
    
    print_color $YELLOW "Cleaning log files older than $days days..."
    
    local old_logs=($(find "$LOG_DIR" -name "*.log" -type f -mtime +$days))
    
    if [ ${#old_logs[@]} -eq 0 ]; then
        print_color $GREEN "No log files older than $days days found"
        return
    fi
    
    print_color $BLUE "Found ${#old_logs[@]} log file(s) to clean:"
    for log_file in "${old_logs[@]}"; do
        echo "  $(basename "$log_file")"
    done
    
    echo ""
    echo "Are you sure you want to delete these files? (y/N)"
    read -r response
    
    if [[ "$response" =~ ^[Yy]$ ]]; then
        for log_file in "${old_logs[@]}"; do
            rm -f "$log_file"
            echo "Deleted: $(basename "$log_file")"
        done
        print_color $GREEN "Cleanup completed successfully"
    else
        print_color $YELLOW "Cleanup cancelled"
    fi
}

# Function to show log statistics
show_stats() {
    check_logs_dir
    
    local log_files=($(find "$LOG_DIR" -name "*.log" -type f))
    
    if [ ${#log_files[@]} -eq 0 ]; then
        print_color $YELLOW "No log files found"
        return
    fi
    
    print_color $BLUE "Log Statistics:"
    echo "======================================================"
    echo "Total log files: ${#log_files[@]}"
    
    # Calculate total size
    local total_size=0
    for log_file in "${log_files[@]}"; do
        local size=$(stat -c "%s" "$log_file" 2>/dev/null || echo "0")
        total_size=$((total_size + size))
    done
    
    # Convert to human readable
    if [ $total_size -gt 1048576 ]; then
        total_size_str=$(echo "scale=2; $total_size / 1048576" | bc 2>/dev/null || echo "Unknown")
        echo "Total size: ${total_size_str} MB"
    elif [ $total_size -gt 1024 ]; then
        total_size_str=$(echo "scale=2; $total_size / 1024" | bc 2>/dev/null || echo "Unknown")
        echo "Total size: ${total_size_str} KB"
    else
        echo "Total size: ${total_size} bytes"
    fi
    
    # Show oldest and newest
    local oldest=$(find "$LOG_DIR" -name "*.log" -type f -printf '%T@ %p\n' | sort -n | head -1 | cut -d' ' -f2-)
    local newest=$(find "$LOG_DIR" -name "*.log" -type f -printf '%T@ %p\n' | sort -nr | head -1 | cut -d' ' -f2-)
    
    if [ -n "$oldest" ]; then
        echo "Oldest log: $(basename "$oldest") ($(stat -c "%y" "$oldest" | cut -d' ' -f1))"
    fi
    
    if [ -n "$newest" ]; then
        echo "Newest log: $(basename "$newest") ($(stat -c "%y" "$newest" | cut -d' ' -f1))"
    fi
    
    # Show apps breakdown
    echo ""
    echo "Apps breakdown:"
    local apps=($(find "$LOG_DIR" -name "*.log" -type f -exec basename {} \; | sed 's/_.*//' | sort | uniq -c))
    for app in "${apps[@]}"; do
        echo "  $app"
    done
}

# Function to show latest log
show_latest() {
    check_logs_dir
    
    local latest=$(find "$LOG_DIR" -name "*.log" -type f -printf '%T@ %p\n' | sort -nr | head -1 | cut -d' ' -f2-)
    
    if [ -z "$latest" ]; then
        print_color $YELLOW "No log files found"
        return
    fi
    
    print_color $GREEN "Latest log file: $(basename "$latest")"
    echo "Path: $latest"
    echo "Size: $(du -h "$latest" | cut -f1)"
    echo "Modified: $(stat -c "%y" "$latest")"
    echo ""
    echo "Last 20 lines:"
    echo "======================================================"
    tail -20 "$latest"
}

# Main script logic
case "${1:-help}" in
    "list")
        list_logs
        ;;
    "view")
        shift
        view_log "$@"
        ;;
    "search")
        shift
        search_logs "$@"
        ;;
    "clean")
        shift
        clean_logs "$@"
        ;;
    "stats")
        show_stats
        ;;
    "latest")
        show_latest
        ;;
    "help"|"--help"|"-h"|*)
        show_usage
        exit 1
        ;;
esac
