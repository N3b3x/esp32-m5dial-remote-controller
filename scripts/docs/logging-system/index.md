---
layout: default
title: "Logging System"
description: "ESP-IDF logging system with comprehensive log capture, management, and analysis"
nav_order: 4
parent: "🔧 Core Systems"
permalink: /docs/logging-system/
---

# ESP-IDF Logging System Guide

This document provides comprehensive documentation for the ESP-IDF logging system, including log
generation, management, analysis, and integration with all scripts.

## 📋 **Table of Contents**


## 📋 **Overview**

The ESP-IDF logging system provides comprehensive log capture, management,
and analysis capabilities for all development operations.
It features automatic log generation, intelligent organization, powerful search capabilities,
and seamless integration with the build and flash systems.

### **Core Features**
- **Automatic Log Generation**: Built-in logging for all script operations
- **Intelligent Organization**: Structured log file naming and organization
- **Comprehensive Capture**: Complete capture of build, flash, and monitor output
- **Advanced Search**: Cross-log search and pattern matching capabilities
- **Automatic Rotation**: Smart log management with configurable retention

### **Key Capabilities**
- Automatic log file creation with timestamped naming
- Cross-log search and pattern matching
- Log statistics and analysis tools
- Automatic log rotation and cleanup
- Integration with all ESP32 development scripts
- Professional log formatting and organization

## 🏗️ **Architecture and Design**

### **System Architecture**
```
Script Execution → Log Capture → Log Storage → Log Management → Log Analysis
      ↓              ↓            ↓            ↓              ↓
Build/Flash     Output        File System   Organization   Search/Stats
Operations      Capture       Storage       & Rotation     & Analysis
```

### **Component Interaction**
- **Script Execution**: All scripts generate log output
- **Log Capture**: Automatic capture of stdout/stderr
- **Log Storage**: Organized file system storage
- **Log Management**: Automatic rotation and cleanup
- **Log Analysis**: Search, statistics, and reporting

### **Design Principles**
- **Automatic Operation**: Logging works without user intervention
- **Comprehensive Coverage**: Captures all relevant output
- **Intelligent Organization**: Logical file naming and structure
- **Performance Optimized**: Minimal impact on script execution
- **Cross-Platform**: Consistent behavior across operating systems

## 📝 **Log Generation and Capture**

### **Automatic Log Generation**

#### **Built-in Logging**
The logging system automatically generates logs for all major operations:

```bash
## Build operations automatically log
./build_app.sh gpio_test Release --log

## Flash operations automatically log
./flash_app.sh flash_monitor gpio_test Release --log

## Monitor operations automatically log
./flash_app.sh monitor --log
```
#### **Log File Naming Convention**
Logs follow a consistent naming pattern:

```bash
## Standard log naming format
{app_type}*{build_type}*{timestamp}.log

## Examples:
gpio_test_Release_20250115_143022.log
adc_test_Debug_20250115_150045.log
monitor_session_20250115_151230.log

## Custom log naming
./flash_app.sh flash gpio_test Release --log production_deploy
## Result: production_deploy_20250115_143022.log
```
#### **Log Content Structure**
Each log file contains comprehensive information:

```bash
## # Log file header
## ESP32 HardFOC Interface Wrapper - Operation Log
Date: 2025-01-15
Time: 14:30:22
Operation: flash_monitor
Application: gpio_test
Build Type: Release
ESP-IDF Version: release/v5.5
Target: esp32c6
## Port: /dev/ttyUSB0
## Operation details
- Command execution details
- Configuration validation
- Port detection results
- Build/flash process output
- Monitor session data
- Error messages and warnings
- Performance metrics
- Completion status
```
### **Log Capture Mechanisms**

#### **Output Capture**
The system captures all relevant output:

```bash
## Captured output types
- Standard output (stdout)
- Standard error (stderr)
- Command execution results
- Error messages and warnings
- Progress information
- Performance metrics
- Debug information
```
#### **Integration Points**
Logging integrates with all major systems:

```bash
## Build system integration
- Build process output
- Compilation results
- Build statistics
- Error messages

## Flash system integration
- Port detection results
- Flash operation output
- Monitor session data
- Device communication

## Configuration system integration
- Configuration validation
- Parameter processing
- Environment setup
- Error handling
```
## 🗂️ **Log Management and Organization**

### **Log Directory Structure**

#### **Standard Organization**
/examples/esp32/logs/
├── 2025-01-15/                    # Date-based organization
│   ├── gpio_test_Release_143022.log
│   ├── adc_test_Debug_150045.log
│   └── monitor_session_151230.log
├── latest/                         # Symlinks to most recent logs
│   ├── build_latest.log
│   ├── flash_latest.log
│   └── monitor_latest.log
└── archive/                        # Archived logs
    ├── 2025-01-14/
    └── 2025-01-13/
#### **Automatic Organization**
The system automatically organizes logs:

```bash
## Automatic directory creation
- Date-based subdirectories
- Latest log symlinks
- Archive organization
- Cleanup and rotation
```
### **Log Rotation and Cleanup**

#### **Automatic Rotation**
```bash
## Rotation policies
- Maximum log files: 50 (configurable)
- Retention period: 30 days (configurable)
- Archive old logs automatically
- Clean up expired logs
```
#### **Cleanup Operations**
```bash
## Manual cleanup
./manage_logs.sh clean 7          # Keep logs for 7 days
./manage_logs.sh clean 30         # Keep logs for 30 days
./manage_logs.sh clean            # Use default retention

## Automatic cleanup
- Daily cleanup of expired logs
- Archive creation for old logs
- Storage optimization
```
### **Log File Management**

#### **File Operations**
```bash
## List all logs
./manage_logs.sh list

## View specific log
./manage_logs.sh view gpio_test_Release_20250115_143022.log

## Show latest log
./manage_logs.sh latest

## Get log statistics
./manage_logs.sh stats
```
#### **Storage Optimization**
```bash
## Storage features
- Automatic compression of old logs
- Efficient file organization
- Configurable retention policies
- Storage usage monitoring
```
## 🔍 **Log Analysis and Search**

### **Cross-Log Search**

#### **Pattern Search**
```bash
## Search for specific patterns
./manage_logs.sh search "ERROR"           # Find all errors
./manage_logs.sh search "WARNING"         # Find all warnings
./manage_logs.sh search "FAILED"          # Find all failures
./manage_logs.sh search "SUCCESS"         # Find all successes

## Advanced search patterns
./manage_logs.sh search "gpio.*error"     # GPIO-related errors
./manage_logs.sh search "flash.*failed"   # Flash failures
./manage_logs.sh search "build.*success"  # Build successes
```
#### **Search Capabilities**
The search system provides powerful capabilities:

```bash
## Search features
- Regular expression support
- Case-insensitive search
- Multi-line pattern matching
- Context display
- Result highlighting
- Search result summary
```
### **Log Statistics and Analysis**

#### **Statistical Analysis**
```bash
## Get log statistics
./manage_logs.sh stats

## Statistics include
- Total log files
- Total log size
- Log age distribution
- Error frequency
- Success rates
- Performance metrics
```
#### **Trend Analysis**
```bash
## Trend analysis features
- Error rate trends
- Performance trends
- Build success trends
- Flash success trends
- Time-based patterns
```
### **Log Comparison and Diff**

#### **Cross-Log Comparison**
```bash
## Compare different logs
./manage_logs.sh compare log1.log log2.log

## Comparison features
- Side-by-side comparison
- Difference highlighting
- Common pattern identification
- Change tracking
```
## 🚀 **Usage Examples and Patterns**

### **Basic Log Management**

#### **1. View Logs**
```bash
## List all available logs
./manage_logs.sh list

## View the most recent log
./manage_logs.sh latest

## View a specific log file
./manage_logs.sh view gpio_test_Release_20250115_143022.log
```
#### **2. Search Logs**
```bash
## Search for errors
./manage_logs.sh search "ERROR"

## Search for specific patterns
./manage_logs.sh search "build.*failed"
./manage_logs.sh search "flash.*success"

## Search with context
./manage_logs.sh search "gpio.*error" --context 5
```
#### **3. Log Maintenance**
```bash
## Clean old logs
./manage_logs.sh clean 7          # Keep 7 days
./manage_logs.sh clean 30         # Keep 30 days

## Get log statistics
./manage_logs.sh stats

## Archive old logs
./manage_logs.sh archive
```
### **Advanced Log Patterns**

#### **1. Development Workflow Logging**
```bash
## Complete development workflow with logging
./build_app.sh gpio_test Debug --log
./flash_app.sh flash_monitor gpio_test Debug --log
./manage_logs.sh latest

## Expected result
- Build log with debug information
- Flash log with port detection
- Monitor log with device output
- Comprehensive development trace
```
#### **2. Production Deployment Logging**
```bash
## Production deployment with logging
./build_app.sh gpio_test Release --log production_build
./flash_app.sh flash gpio_test Release --log production_deploy
./manage_logs.sh search "production.*success"

## Expected result
- Production build log
- Deployment log
- Success verification
- Audit trail for production
```
#### **3. Debugging and Troubleshooting**
```bash
## Debugging workflow with logging
./flash_app.sh monitor --log debug_session
## ... perform debugging operations ...
./manage_logs.sh search "ERROR" --context 10
./manage_logs.sh search "WARNING" --context 5

## Expected result
- Complete debugging session log
- Error context and patterns
- Warning identification
- Troubleshooting guidance
```
### **Log Analysis Workflows**

#### **1. Error Analysis**
```bash
## Comprehensive error analysis
./manage_logs.sh search "ERROR" > errors.txt
./manage_logs.sh search "FAILED" >> errors.txt
./manage_logs.sh search "WARNING" >> warnings.txt

## Analyze error patterns
cat errors.txt | grep -o "ERROR:.*" | sort | uniq -c
cat warnings.txt | grep -o "WARNING:.*" | sort | uniq -c
```
#### **2. Performance Analysis**
```bash
## Performance analysis
./manage_logs.sh search "build.*completed" | grep -o "in [0-9.]*s"
./manage_logs.sh search "flash.*completed" | grep -o "in [0-9.]*s"

## Performance trends
for log in $(./manage_logs.sh list | grep "build.*Release"); do
    echo "$log: $(./manage_logs.sh search "build.*completed" "$log" | grep -o "in [0-9.]*s")"
done
```
#### **3. Success Rate Analysis**
```bash
## Success rate analysis
total_ops=$(./manage_logs.sh list | wc -l)
success_ops=$(./manage_logs.sh search "SUCCESS" | wc -l)
error_ops=$(./manage_logs.sh search "ERROR" | wc -l)

echo "Total operations: $total_ops"
echo "Successful operations: $success_ops"
echo "Failed operations: $error_ops"
echo "Success rate: $((success_ops * 100 / total_ops))%"
```
## 🔧 **Integration and Automation**

### **Script Integration**

#### **Automatic Logging**
All scripts automatically support logging:

```bash
## Build script logging
./build_app.sh gpio_test Release --log

## Flash script logging
./flash_app.sh flash_monitor gpio_test Release --log

## Monitor script logging
./flash_app.sh monitor --log

## Log management integration
./manage_logs.sh latest
./manage_logs.sh search "ERROR"
```
#### **Log Configuration**
Logging can be configured through environment variables:

```bash
## Logging configuration
export LOG_ENABLED=1              # Enable logging
export LOG_LEVEL=INFO             # Set log level
export LOG_RETENTION_DAYS=30      # Set retention period
export LOG_MAX_FILES=50           # Set maximum log files
export LOG_COMPRESSION=1          # Enable compression
```
### **CI/CD Integration**

#### **Automated Logging**
```yaml
## GitHub Actions workflow
- name: Build and Flash with Logging
  run: |
    cd /examples/esp32
    ./scripts/build_app.sh gpio_test Release --log ci_build
    ./scripts/flash_app.sh flash gpio_test Release --log ci_deploy

- name: Analyze Logs
  run: |
    cd /examples/esp32
    ./scripts/manage_logs.sh search "ERROR"
    ./scripts/manage_logs.sh stats
    ./scripts/manage_logs.sh latest
```
#### **Log Artifacts**
```yaml
## Upload logs as artifacts
- name: Upload Logs
  uses: actions/upload-artifact@v3
  with:
    name: esp32-logs
    path: /examples/esp32/logs/
    retention-days: 30
```
### **Automation Scripts**

#### **Log Analysis Automation**
```bash
#!/bin/bash
## Automated log analysis script

cd /examples/esp32

## Generate daily log report
echo "=== Daily Log Report $(date +%Y-%m-%d) ===" > daily_report.txt

## Get log statistics
echo "Log Statistics:" >> daily_report.txt
./scripts/manage_logs.sh stats >> daily_report.txt

## Check for errors
echo -e "\nError Summary:" >> daily_report.txt
./scripts/manage_logs.sh search "ERROR" | head -20 >> daily_report.txt

## Check for warnings
echo -e "\nWarning Summary:" >> daily_report.txt
./scripts/manage_logs.sh search "WARNING" | head -20 >> daily_report.txt

## Send report
mail -s "ESP32 Daily Log Report" admin@example.com < daily_report.txt
```
#### **Log Cleanup Automation**
```bash
#!/bin/bash
## Automated log cleanup script

cd /examples/esp32

## Clean old logs
./scripts/manage_logs.sh clean 30

## Archive old logs
./scripts/manage_logs.sh archive

## Generate cleanup report
echo "Log cleanup completed at $(date)" > cleanup_report.txt
echo "Logs older than 30 days have been cleaned up" >> cleanup_report.txt
echo "Old logs have been archived" >> cleanup_report.txt

## Send cleanup report
mail -s "ESP32 Log Cleanup Report" admin@example.com < cleanup_report.txt
```
## 🔍 **Troubleshooting and Debugging**

### **Common Log Issues**

#### **1. Log Files Not Created**
**Problem**: Log files are not being generated
**Symptoms**: No log files in logs directory
**Solutions**:
```bash
## Check if logging is enabled
echo $LOG_ENABLED

## Check log directory permissions
ls -la logs/
chmod 755 logs/

## Check script logging support
./flash_app.sh --help | grep -i log

## Enable logging explicitly
export LOG_ENABLED=1
./flash_app.sh flash gpio_test Release --log
```
#### **2. Log Directory Issues**
**Problem**: Cannot access or create log directory
**Symptoms**: "Permission denied" or "Directory not found" errors
**Solutions**:
```bash
## Check directory existence
ls -la /examples/esp32/

## Create log directory
mkdir -p /examples/esp32/logs

## Check permissions
ls -la /examples/esp32/logs/

## Fix permissions
chmod 755 /examples/esp32/logs/
```
#### **3. Log File Corruption**
**Problem**: Log files are corrupted or incomplete
**Symptoms**: Incomplete log files or parsing errors
**Solutions**:
```bash
## Check log file integrity
./manage_logs.sh view latest.log

## Check file size
ls -la logs/*.log

## Remove corrupted logs
rm logs/corrupted.log

## Regenerate logs
./flash_app.sh flash gpio_test Release --log
```
### **Debug and Verbose Mode**

#### **Enabling Log Debug Output**
```bash
## Enable log debug mode
export LOG_DEBUG=1
export LOG_VERBOSE=1

## Check log system status
./manage_logs.sh --help
./manage_logs.sh list --verbose

## Debug information available
- Log file creation details
- Log capture process information
- Log organization details
- Search and analysis information
```
#### **Log System Debugging**
```bash
## Debug log system
debug_log_system() {
    echo "=== Log System Debug ==="
    echo "Log directory: $LOG_DIR"
    echo "Log directory exists: $([ -d "$LOG_DIR" ] && echo "Yes" || echo "No")"
    echo "Log directory writable: $([ -w "$LOG_DIR" ] && echo "Yes" || echo "No")"
    echo "Log files count: $(find "$LOG_DIR" -name "*.log" | wc -l)"
    echo "Latest log: $(./manage_logs.sh latest 2>/dev/null || echo "None")"
}
```
### **Log Performance Issues**

#### **Large Log Files**
**Problem**: Log files are very large and slow to process
**Symptoms**: Slow search operations or high memory usage
**Solutions**:
```bash
## Check log file sizes
./manage_logs.sh list | xargs ls -lh

## Compress old logs
./manage_logs.sh compress

## Clean old logs
./manage_logs.sh clean 7

## Split large logs
./manage_logs.sh split large_log.log
```
#### **Search Performance**
**Problem**: Log search operations are slow
**Symptoms**: Long search times or high CPU usage
**Solutions**:
```bash
## Use more specific search patterns
./manage_logs.sh search "ERROR.*gpio"  # More specific than just "ERROR"

## Limit search scope
./manage_logs.sh search "ERROR" --max-results 100

## Use indexed search (if available)
./manage_logs.sh search "ERROR" --indexed

## Search in specific time range
./manage_logs.sh search "ERROR" --since "2025-01-15"
```
## 📚 **Reference and Examples**

### **Command Reference**

#### **Log Management Commands**
```bash
./manage_logs.sh [command] [options]

## Commands:
##   list                    - List all log files with details
##   view <log_name>        - View a specific log file
##   search <pattern>       - Search across all log files
##   clean [days]           - Clean logs older than N days
##   stats                  - Show log statistics and summary
##   latest                 - Show the most recent log file
##   compare <log1> <log2>  - Compare two log files
##   archive                - Archive old logs
##   compress               - Compress old logs
```
#### **Log Options**
- **`--verbose`**: Enable verbose output
- **`--context <lines>`**: Show context lines around search results
- **`--max-results <count>`**: Limit search results
- **`--since <date>`**: Search logs since specific date
- **`--until <date>`**: Search logs until specific date
- **`--help`**: Show usage information

#### **Environment Variables**
```bash
## Logging configuration
export LOG_ENABLED=1              # Enable/disable logging
export LOG_LEVEL=INFO             # Set log level (DEBUG, INFO, WARN, ERROR)
export LOG_DIR="logs/"            # Set log directory
export LOG_RETENTION_DAYS=30      # Set log retention period
export LOG_MAX_FILES=50           # Set maximum log files
export LOG_COMPRESSION=1          # Enable/disable compression
export LOG_ROTATION=1             # Enable/disable rotation
export LOG_ARCHIVE=1              # Enable/disable archiving
```
### **Configuration Examples**

#### **Minimal Log Configuration**
```yaml
## app_config.yml minimal logging configuration
flash_config:
  auto_logging: true
  log_rotation: true
  max_log_files: 30
  log_retention_days: 7
```
#### **Standard Log Configuration**
```yaml
## app_config.yml standard logging configuration
flash_config:
  auto_logging: true
  log_rotation: true
  max_log_files: 50
  log_retention_days: 30
  log_compression: true
  log_archive: true
  log_format: "detailed"
  log_level: "INFO"
```
#### **Advanced Log Configuration**
```yaml
## app_config.yml advanced logging configuration
flash_config:
  auto_logging: true
  log_rotation: true
  max_log_files: 100
  log_retention_days: 90
  log_compression: true
  log_archive: true
  log_format: "detailed"
  log_level: "DEBUG"
  
  # Log organization
  log_directory: "logs/"
  log_subdirectories: true
  date_based_organization: true
  
  # Log analysis
  log_indexing: true
  log_search_cache: true
  log_analytics: true
  
  # Performance
  log_buffer_size: "1MB"
  log_flush_interval: 5
  log_async_writing: true
```
### **Integration Examples**

#### **CMake Integration**
```cmake
## CMakeLists.txt logging integration
cmake_minimum_required(VERSION 3.16)

## Log target integration
add_custom_target(logs
    COMMAND ${CMAKE_SOURCE_DIR}/scripts/manage_logs.sh list
    COMMENT "Listing ESP32 development logs"
)

## Log cleanup target
add_custom_target(clean_logs
    COMMAND ${CMAKE_SOURCE_DIR}/scripts/manage_logs.sh clean 7
    COMMENT "Cleaning logs older than 7 days"
)
```
#### **CI/CD Integration**
```yaml
## GitHub Actions logging workflow
- name: Setup Logging
  run: |
    cd /examples/esp32
    mkdir -p logs
    export LOG_ENABLED=1
    export LOG_LEVEL=DEBUG

- name: Build with Logging
  run: |
    cd /examples/esp32
    ./scripts/build_app.sh gpio_test Release --log ci_build

- name: Flash with Logging
  run: |
    cd /examples/esp32
    ./scripts/flash_app.sh flash gpio_test Release --log ci_deploy

- name: Analyze Logs
  run: |
    cd /examples/esp32
    ./scripts/manage_logs.sh stats
    ./scripts/manage_logs.sh search "ERROR"
    ./scripts/manage_logs.sh search "SUCCESS"

- name: Upload Logs
  uses: actions/upload-artifact@v3
  with:
    name: esp32-logs
    path: /examples/esp32/logs/
    retention-days: 30
```
### **Best Practices**

#### **1. Log Management**
- Always enable logging for important operations
- Use descriptive log names for easy identification
- Regular log cleanup to prevent disk space issues
- Archive important logs for long-term retention

#### **2. Log Analysis**
- Use specific search patterns for better results
- Regular review of error logs for pattern identification
- Monitor log statistics for system health
- Use log analysis for performance optimization

#### **3. Log Integration**
- Integrate logging into all development workflows
- Use logging for CI/CD pipeline monitoring
- Implement automated log analysis and reporting
- Use logs for debugging and troubleshooting

#### **4. Log Performance**
- Monitor log file sizes and growth rates
- Implement log rotation and compression
- Use efficient search patterns
- Regular log maintenance and cleanup

---

