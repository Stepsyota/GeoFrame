import os
from pathlib import Path

def get_sorted_files(directory: str) -> list[Path]:
    files = []
    for dirpath, _, filenames in os.walk(directory):
        for filename in filenames:
            full_path = Path(dirpath) / filename
            try:
                files.append((full_path, full_path.stat().st_ctime))
            except FileNotFoundError:
                continue
    files.sort(key=lambda t: t[1])
    return [file[0] for file in files]