#!/bin/sh
#    Accepts the following runtime arguments: the first argument is a path to a directory on the filesystem, referred to below as filesdir; the second argument is a text string which will be searched within these files, referred to below as searchstr
#
#    Exits with return value 1 error and print statements if any of the parameters above were not specified
#
#    Exits with return value 1 error and print statements if filesdir does not represent a directory on the filesystem
#
#    Prints a message "The number of files are X and the number of matching lines are Y" where X is the number of files in the directory and all subdirectories and Y is the number of matching lines found in respective files, where a matching line refers to a line which contains searchstr (and may also contain additional content).
#    
#    
SEARCH_DIR="$1"
SEARCH_STR="$2"

if [ "$#" -ne 2 ]; then
	echo "Error: You must provide exactly 2 arguments."
	exit 1
fi

if [ ! -d "$SEARCH_DIR" ]; then
	echo "Error: $SEARCH_DIR is not a directory."
	exit 1
fi

file_count=$(find "$SEARCH_DIR" -type f | wc -l)

lines_count=$(grep -R "$SEARCH_STR" "$SEARCH_DIR" | wc -l)

echo "The number of files are $file_count and the number of matching lines are $lines_count"
