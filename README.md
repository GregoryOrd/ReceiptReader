# ReceiptReader

## Docker Container
To be able to run the desktop GUI in the container, we need allow Docker to access our X-Server.
To enable this we run:

    xhost +local:docker

### Git in Docker Container
When trying to run git in the container it will fail because the user in the container does not match the owner of the mounted directory. The error will be something like:

    fatal: detected dubious ownership in repository at '/home/devuser/ReceiptReader'

To fix this we need to run the following command on the host to mark the repo in the container
as a safe directory:

    git config --global --add safe.directory /home/devuser/ReceiptReader