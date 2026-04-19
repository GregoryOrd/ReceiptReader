#!/bin/bash

repo=ReceiptReader
prNum=2
GH_USER=GregoryOrd
commentId=3107505433
gh api repos/$GH_USER/$repo/pulls/comments/$commentId -X DELETE
