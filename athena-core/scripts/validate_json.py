#!/usr/bin/env python3
"""
ATHENA Platform Database Validator
Validates all JSON files against schema and checks for common issues.
"""

import json
import os
import sys
from pathlib import Path

def validate_file(filepath):
    """Validate a single JSON file."""
    errors = []
    warnings = []
    
    try:
        with open(filepath, 'r') as f:
            data = json.load(f)
    except json.JSONDecodeError as e:
        return [f"JSON parse error: {e}"], []
    
    if 'platforms' not in data:
        return ["Missing 'platforms' array"], []
    
    platforms = data['platforms']
    if not isinstance(platforms, list):
        return ["'platforms' is not an array"], []
    
    ids_seen = set()
    
    for i, p in enumerate(platforms):
        prefix = f"Platform [{i}]"
        
        if 'id' not in p:
            errors.append(f"{prefix}: Missing required field 'id'")
            continue
        
        pid = p['id']
        prefix = f"Platform '{pid}'"
        
        if 'name' not in p:
            errors.append(f"{prefix}: Missing required field 'name'")
        
        if not pid or len(pid) < 4:
            errors.append(f"{prefix}: ID too short")
        elif '-' not in pid:
            warnings.append(f"{prefix}: ID should contain dash")
        
        if pid in ids_seen:
            errors.append(f"{prefix}: Duplicate ID in file")
        ids_seen.add(pid)
        
        if 'country' in p:
            country = p['country']
            if len(country) != 2 or not country.isupper():
                warnings.append(f"{prefix}: Country '{country}' should be 2-letter ISO")
        
        if 'year' in p:
            year = p['year']
            if isinstance(year, int) and (year < 1900 or year > 2035):
                warnings.append(f"{prefix}: Year {year} out of range")
    
    return errors, warnings

def validate_directory(base_path):
    """Validate all JSON files in directory tree."""
    results = {
        'total_files': 0,
        'valid_files': 0,
        'files_with_warnings': 0,
        'total_errors': 0,
        'total_warnings': 0,
        'all_ids': set(),
        'duplicate_ids': [],
        'details': []
    }
    
    for root, dirs, files in os.walk(base_path):
        for filename in sorted(files):
            if not filename.endswith('.json'):
                continue
            
            filepath = os.path.join(root, filename)
            rel_path = os.path.relpath(filepath, base_path)
            
            results['total_files'] += 1
            errors, warnings = validate_file(filepath)
            
            try:
                with open(filepath, 'r') as f:
                    data = json.load(f)
                    for p in data.get('platforms', []):
                        pid = p.get('id', '')
                        if pid:
                            if pid in results['all_ids']:
                                results['duplicate_ids'].append((pid, rel_path))
                            results['all_ids'].add(pid)
            except:
                pass
            
            if errors:
                results['total_errors'] += len(errors)
                results['details'].append({
                    'file': rel_path,
                    'errors': errors,
                    'warnings': warnings
                })
            elif warnings:
                results['valid_files'] += 1
                results['files_with_warnings'] += 1
                results['total_warnings'] += len(warnings)
            else:
                results['valid_files'] += 1
    
    return results

def main():
    base_path = sys.argv[1] if len(sys.argv) > 1 else 'data/platforms'
    
    print("=" * 60)
    print("ATHENA Platform Database Validation")
    print("=" * 60)
    print(f"\nValidating: {base_path}\n")
    
    results = validate_directory(base_path)
    
    print(f"Files scanned:     {results['total_files']}")
    print(f"Valid files:       {results['valid_files']}")
    print(f"Files w/warnings:  {results['files_with_warnings']}")
    print(f"Total errors:      {results['total_errors']}")
    print(f"Total warnings:    {results['total_warnings']}")
    print(f"Unique IDs:        {len(results['all_ids'])}")
    print(f"Global duplicates: {len(results['duplicate_ids'])}")
    
    if results['duplicate_ids']:
        print("\nDuplicate IDs found:")
        for pid, filepath in results['duplicate_ids']:
            print(f"  - {pid} in {filepath}")
    
    if results['details']:
        print("\nFiles with errors:")
        for detail in results['details'][:10]:
            print(f"\n  {detail['file']}:")
            for err in detail['errors'][:5]:
                print(f"    ERROR: {err}")
    
    print("\n" + "=" * 60)
    if results['total_errors'] == 0 and len(results['duplicate_ids']) == 0:
        print("VALIDATION PASSED")
        return 0
    else:
        print("VALIDATION FAILED")
        return 1

if __name__ == '__main__':
    sys.exit(main())
