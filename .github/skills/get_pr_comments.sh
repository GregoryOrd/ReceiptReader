#!/bin/bash

prNum=$1
reviewId=$2
repo=ReceiptReader
GH_USER=GregoryOrd

# Print Usage if not enough arguments are provided
if [ $# -ne 2 ]; then
    echo "Usage: $0 <prNum> <reviewId>"
    exit 1
fi

comments_json=$(gh api repos/$GH_USER/$repo/pulls/$prNum/reviews/$reviewId/comments)

echo "$comments_json" | jq -r '.[].id | @base64' | while IFS= read -r b64; do   id=$(printf '%s' "$b64" | base64 --decode);  printf 'ID: %s\n' "$id"; done
echo "$comments_json" | jq -r '.[].body | @base64' | while IFS= read -r b64; do   body=$(printf '%s' "$b64" | base64 --decode);   printf 'Body: %s\n' "$body"; done
