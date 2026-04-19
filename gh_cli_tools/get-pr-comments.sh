#!/bin/bash

reviewId=4136500022
repo=ReceiptReader
prNum=2
GH_USER=GregoryOrd
comments_json=$(gh api repos/$GH_USER/$repo/pulls/$prNum/reviews/$reviewId/comments)

echo "$comments_json" | jq -r '.[].id | @base64' | while IFS= read -r b64; do   id=$(printf '%s' "$b64" | base64 --decode);  printf 'ID: %s\n' "$id"; done
echo "$comments_json" | jq -r '.[].body | @base64' | while IFS= read -r b64; do   body=$(printf '%s' "$b64" | base64 --decode);   printf 'Body: %s\n' "$body"; done
