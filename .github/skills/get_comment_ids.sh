#!/bin/bash

# Use gh cli to get comment IDs for a specific PR review comment
reviewId=$1
prNum=$2
repo=ReceiptReader
GH_USER=GregoryOrd

# Print Usage if not enough arguments are provided
if [ $# -ne 2 ]; then
    echo "Usage: $0 <reviewId>"
    exit 1
fi

comments_json=$(gh api repos/$GH_USER/$repo/pulls/$prNum/reviews/$reviewId/comments)
echo "$comments_json" | jq -r '.[].id | @base64' | while IFS= read -r b64; do   id=$(printf '%s' "$b64" | base64 --decode);  printf 'Comment ID: %s\n' "$id"; done
