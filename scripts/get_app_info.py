#!/usr/bin/env python3
"""
ESP32 App Information Script
This script provides information about ESP32 apps from configuration
"""

import sys
import yaml
import argparse
from pathlib import Path

def show_help():
    """Show comprehensive help information."""
    print("ESP32 App Information Script")
    print("")
    print("Usage: python3 get_app_info.py [COMMAND] [ARGUMENTS]")
    print("")
    print("COMMANDS:")
    print("  source_file <app_type>      - Get source file path for app")
    print("  list                        - List all available apps")
    print("  validate <app_type>         - Check if app type is valid")
    print("  info <app_type>             - Show detailed information for app")
    print("  build_types <app_type>      - Show supported build types for app")
    print("  idf_versions <app_type>     - Show supported ESP-IDF versions for app")
    print("  dependencies <app_type>     - Show app dependencies")
    print("  tags <app_type>             - Show app tags")
    print("  category <app_type>         - Show app category")
    print("")
    print("OPTIONS:")
    print("  --help, -h                  - Show this help message")
    print("  --verbose                    - Show detailed output")
    print("  --format <format>           - Output format: text, json, yaml (default: text)")
    print("  --project-path <path>       - Path to project directory containing app_config.yml")
    print("")
    print("ARGUMENTS:")
    print("  app_type                    - Application type (e.g., gpio_test, adc_test)")
    print("  format                      - Output format for structured data")
    print("  path                        - Custom configuration file path")
    print("")
    print("EXAMPLES:")
    print("  # Basic information")
    print("  python3 get_app_info.py list")
    print("  python3 get_app_info.py source_file gpio_test")
    print("  python3 get_app_info.py validate adc_test")
    print("")
    print("  # Detailed app information")
    print("  python3 get_app_info.py info gpio_test")
    print("  python3 get_app_info.py build_types gpio_test")
    print("  python3 get_app_info.py idf_versions gpio_test")
    print("")
    print("  # Dependencies and metadata")
    print("  python3 get_app_info.py dependencies gpio_test")
    print("  python3 get_app_info.py tags gpio_test")
    print("  python3 get_app_info.py category gpio_test")
    print("")
    print("  # Output formatting")
    print("  python3 get_app_info.py info gpio_test --format json")
    print("  python3 get_app_info.py list --format yaml")
    print("")
    print("  # Custom configuration file")
    print("  python3 get_app_info.py list --project-path /path/to/project")
    print("")
    print("OUTPUT FORMATS:")
    print("  • text: Human-readable formatted output (default)")
    print("  • json: Structured JSON output for scripts")
    print("  • yaml: YAML format for configuration files")
    print("")
    print("CONFIGURATION FILE:")
    print("  • Location: examples/esp32/app_config.yml (auto-detected)")
    print("  • Format: YAML configuration with app definitions")
    print("  • Structure: Apps with metadata, dependencies, and configuration")
    print("")
    print("APP METADATA:")
    print("  • description: Human-readable app description")
    print("  • source_file: Main source file for the application")
    print("  • category: App category (peripheral, utility, demo, etc.)")
    print("  • build_types: Supported build configurations")
    print("  • idf_versions: Supported ESP-IDF versions")
    print("  • dependencies: Required components or libraries")
    print("  • tags: Searchable tags for app discovery")
    print("  • ci_enabled: Whether app is enabled for CI builds")
    print("  • featured: Whether app is featured/promoted")
    print("")
    print("USE CASES:")
    print("  • CMake integration: Get source files for build systems")
    print("  • CI automation: Validate app configurations")
    print("  • Documentation: Generate app catalogs and guides")
    print("  • Development: Discover available apps and their requirements")
    print("  • Scripting: Parse app information for automation")
    print("")
    print("TROUBLESHOOTING:")
    print("  • Configuration not found: Check file paths and working directory")
    print("  • Invalid app type: Use 'list' command to see valid app types")
    print("  • YAML errors: Validate configuration file syntax")
    print("  • Permission denied: Check file read permissions")
    print("")
    print("For detailed information, see: docs/README_UTILITY_SCRIPTS.md")
    sys.exit(0)

def parse_arguments():
    """Parse command line arguments."""
    parser = argparse.ArgumentParser(
        description="Get information about ESP32 apps from configuration",
        add_help=False  # We'll handle help manually
    )
    
    parser.add_argument("command", nargs="?", help="Command to execute")
    parser.add_argument("app_type", nargs="?", help="Application type")
    parser.add_argument("--help", "-h", action="store_true", help="Show help message")
    parser.add_argument("--verbose", "-v", action="store_true", help="Verbose output")
    parser.add_argument("--format", "-f", choices=["text", "json", "yaml"], default="text", help="Output format")
    parser.add_argument("--project-path", "-p", help="Path to project directory containing app_config.yml")
    
    args = parser.parse_args()
    
    if args.help or not args.command:
        show_help()
    
    return args

def load_config(project_path=None):
    """Load the apps configuration file."""
    if project_path:
        # Use provided project path
        project_dir = Path(project_path).resolve()
        config_file = project_dir / "app_config.yml"
    else:
        # Default behavior: assume script is in project/scripts/
        config_file = Path(__file__).parent.parent / "app_config.yml"
    
    if not config_file.exists():
        print(f"Error: Configuration file not found: {config_file}", file=sys.stderr)
        if project_path:
            print(f"Please check the project path: {project_path}", file=sys.stderr)
        sys.exit(1)
    
    try:
        with open(config_file, 'r') as f:
            return yaml.safe_load(f)
    except Exception as e:
        print(f"Error loading configuration: {e}", file=sys.stderr)
        sys.exit(1)

def get_app_source_file(app_type, project_path=None):
    """Get source file for an app type."""
    config = load_config(project_path)
    
    if app_type not in config['apps']:
        print(f"Error: Unknown app type: {app_type}", file=sys.stderr)
        sys.exit(1)
    
    return config['apps'][app_type]['source_file']

def list_apps(project_path=None):
    """List all available apps."""
    config = load_config(project_path)
    apps = list(config['apps'].keys())
    return apps

def validate_app(app_type, project_path=None):
    """Validate if app type exists."""
    config = load_config(project_path)
    return app_type in config['apps']

def main():
    """Main function."""
    args = parse_arguments()
    
    command = args.command
    app_type = args.app_type
    verbose = args.verbose
    format_output = args.format
    project_path = args.project_path

    if command == "source_file":
        if not app_type:
            print("Usage: get_app_info.py source_file <app_type>", file=sys.stderr)
            sys.exit(1)
        source_file = get_app_source_file(app_type, project_path)
        print(source_file)
    
    elif command == "list":
        apps = list_apps(project_path)
        print(" ".join(apps))
    
    elif command == "validate":
        if not app_type:
            print("Usage: get_app_info.py validate <app_type>", file=sys.stderr)
            sys.exit(1)
        is_valid = validate_app(app_type, project_path)
        print("true" if is_valid else "false")
        if not is_valid:
            sys.exit(1)
    
    else:
        print(f"Error: Unknown command: {command}", file=sys.stderr)
        sys.exit(1)

if __name__ == '__main__':
    main()

