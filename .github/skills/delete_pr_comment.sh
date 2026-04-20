#!/bin/bash

commentId=$1
repo=ReceiptReader
GH_USER=GregoryOrd

# Print Usage if not enough arguments are provided
if [ $# -ne 1 ]; then
    echo "Usage: $0 <commentId>"
    exit 1
fi

gh api repos/$GH_USER/$repo/pulls/comments/$commentId -X DELETE
