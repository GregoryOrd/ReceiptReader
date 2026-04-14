#!/bin/bash

echo "Copy GitHub token from the host and export it into the GITHUB_TOKEN environment variable..."
echo 'For example, run if the token was a file in this container it could be exported like: export GITHUB_TOKEN=$(cat /home/devuser/.github_token)'

git clone git@github.com:GregoryOrd/ReceiptReader.git