#!/bin/bash

# Use gh cli to get the review IDs for a specific PR review
prNum=$1
repo=ReceiptReader
GH_USER=GregoryOrd

# Print Usage if not enough arguments are provided
if [ $# -ne 1 ]; then
    echo "Usage: $0 <prNum>"
    exit 1
fi

reviews_json=$(gh api repos/$GH_USER/$repo/pulls/$prNum/reviews)
echo "$reviews_json" | jq -r '.[].id | @base64' | while IFS= read -r b64; do   id=$(printf '%s' "$b64" | base64 --decode);  printf 'Review ID:%s\n' "$id"; done