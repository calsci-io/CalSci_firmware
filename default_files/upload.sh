#!/bin/bash

echo ""
echo "Items present:"
echo ""

find . -type f | while read -r file; do
    echo "$file"
done

echo ""
echo "Uploading files..."
echo ""

find . -type f | while read -r file; do
    # Strip the leading './' to get the relative path
    relative_path="${file#./}"

    echo "Uploading $relative_path"

    # Create directory structure on device if needed
    dir_path=$(dirname "$relative_path")
    if [ "$dir_path" != "." ]; then
        ampy -p /dev/ttyACM0 mkdir "$dir_path" 2>/dev/null
    fi

    # Upload the file
    ampy -p /dev/ttyACM0 put "$file" "$relative_path"

    echo "$relative_path uploaded"
    echo ""
done
