#!/usr/bin/env python3
"""
Parse .env file and generate C++ header file
"""
import os
import re
import sys

Import("env")

def parse_env_file(env_path, output_path):
    """
    Parse .env file and generate C++ header file
    """
    env_vars = {}
    
    # Read .env file
    if os.path.exists(env_path):
        with open(env_path, 'r', encoding='utf-8') as f:
            for line in f:
                line = line.strip()
                # Skip comments and empty lines
                if not line or line.startswith('#'):
                    continue
                # Parse KEY=VALUE
                if '=' in line:
                    key, value = line.split('=', 1)
                    key = key.strip()
                    value = value.strip()
                    # Remove quotes if present
                    if value.startswith('"') and value.endswith('"'):
                        value = value[1:-1]
                    elif value.startswith("'") and value.endswith("'"):
                        value = value[1:-1]
                    env_vars[key] = value
    
    # Generate C++ header file
    with open(output_path, 'w', encoding='utf-8') as f:
        f.write("// Auto-generated from .env file\n")
        f.write("// DO NOT EDIT MANUALLY\n")
        f.write("#ifndef CONFIG_ENV_H\n")
        f.write("#define CONFIG_ENV_H\n\n")
        
        for key, value in env_vars.items():
            # Escape special characters for C++ string
            c_key = key.upper()
            # Convert to C-style string with proper escaping
            escaped_value = value.replace('\\', '\\\\').replace('"', '\\"')
            f.write(f'#define {c_key} "{escaped_value}"\n')
        
        f.write("\n#endif  // CONFIG_ENV_H\n")
    
    env.Replace(CPPPATH=[os.path.dirname(output_path)])
    sys.stdout.write(f"Generated {output_path} with {len(env_vars)} variables\n")

if __name__ == "__main__":
    # Default paths (relative to project root)
    env_file = "m5GnssLogger/.env"
    output_file = "m5GnssLogger/include/config_env.h"
    
    # Check if env file path is provided
    if len(sys.argv) > 1:
        env_file = sys.argv[1]
    if len(sys.argv) > 2:
        output_file = sys.argv[2]
    
    parse_env_file(env_file, output_file)
