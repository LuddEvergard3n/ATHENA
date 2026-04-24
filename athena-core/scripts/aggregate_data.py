#!/usr/bin/env python3
"""Aggregate all platform JSON files into a single file for the UI."""

import json
import os
import sys
from pathlib import Path

def aggregate_platforms(base_path):
    """Aggregate all platforms into a single data structure."""
    result = {
        "metadata": {
            "version": "0.8.3",
            "generated": "2026-01-31",
            "total_platforms": 0
        },
        "categories": {},
        "platforms": [],
        "countries": set(),
        "years": set()
    }
    
    for root, dirs, files in os.walk(base_path):
        for filename in sorted(files):
            if not filename.endswith('.json'):
                continue
            
            filepath = os.path.join(root, filename)
            rel_dir = os.path.relpath(root, base_path)
            category = rel_dir if rel_dir != '.' else 'unknown'
            
            try:
                with open(filepath, 'r') as f:
                    data = json.load(f)
                
                platforms = data.get('platforms', [])
                
                if category not in result['categories']:
                    result['categories'][category] = {
                        'count': 0,
                        'subcategories': set()
                    }
                
                for p in platforms:
                    # Add category info
                    p['_category'] = category
                    p['_source_file'] = os.path.basename(filename)
                    
                    # Extract country
                    country = p.get('country', p.get('country_of_origin', ''))
                    if country:
                        result['countries'].add(country)
                    
                    # Extract year
                    year = p.get('year', p.get('service_entry', 0))
                    if year and isinstance(year, int):
                        result['years'].add(year)
                    
                    # Track subcategory
                    subcat = p.get('type', '')
                    if subcat:
                        result['categories'][category]['subcategories'].add(subcat)
                    
                    result['platforms'].append(p)
                    result['categories'][category]['count'] += 1
                
            except Exception as e:
                print(f"Error processing {filepath}: {e}", file=sys.stderr)
    
    # Convert sets to lists for JSON serialization
    result['countries'] = sorted(list(result['countries']))
    result['years'] = sorted(list(result['years']))
    for cat in result['categories']:
        result['categories'][cat]['subcategories'] = sorted(
            list(result['categories'][cat]['subcategories'])
        )
    
    result['metadata']['total_platforms'] = len(result['platforms'])
    
    return result

def main():
    base_path = sys.argv[1] if len(sys.argv) > 1 else 'data/platforms'
    output_path = sys.argv[2] if len(sys.argv) > 2 else 'ui/platforms_data.js'
    
    print(f"Aggregating platforms from: {base_path}")
    data = aggregate_platforms(base_path)
    
    # Write as JavaScript module
    with open(output_path, 'w') as f:
        f.write("// Auto-generated platform database\n")
        f.write("// Do not edit manually\n\n")
        f.write("const ATHENA_DATA = ")
        json.dump(data, f, indent=2)
        f.write(";\n")
    
    print(f"Generated: {output_path}")
    print(f"Total platforms: {data['metadata']['total_platforms']}")
    print(f"Categories: {len(data['categories'])}")
    print(f"Countries: {len(data['countries'])}")

if __name__ == '__main__':
    main()
