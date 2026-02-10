#!/usr/bin/env python3
import re
import sys

def calculate_coverage(file_path):
    with open(file_path, 'r') as f:
        content = f.read()

    # Find all table rows with checkboxes
    # Example: | **Login** | ✅ Implemented | ... |
    rows = re.findall(r'\|.*\|.*[✅❌].*\|.*\|', content)
    
    total = len(rows)
    implemented = len([r for r in rows if '✅' in r])
    
    if total == 0:
        return 0, 0, 0
    
    percentage = (implemented / total) * 100
    return implemented, total, percentage

if __name__ == "__main__":
    file_path = "SDK_COVERAGE_REPORT.md"
    if len(sys.argv) > 1:
        file_path = sys.argv[1]
    
    implemented, total, percentage = calculate_coverage(file_path)
    print(f"SDK Implementation Coverage: {percentage:.1f}% ({implemented}/{total} features)")
