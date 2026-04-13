# ReceiptReader

## Docker Container
To be able to run the desktop GUI in the container, we need allow Docker to access our X-Server.
To enable this we run:

    xhost +local:docker

### Git in Docker Container
To get git working in the container, we create volumes for the following files on the
host into the container. For the permissions to work correctly, we need to make sure that
the files on the host have the correct permissions and belong to the 1001 user id. 

    ../.docker_gitconfig
    ../.docker_ssh_key
    ../.docker_ssh_key.pub

The .docker_gitconfig file can be a copy of the hosts ~/.gitconfig.
The .docker_ssh_key and .docker_ssh_key.pub files should be created on the host 
with ssh-keygen. 

    chown 1001:1001 .docker_gitconfig
    chown 1001:1001 .docker_ssh_key
    chown 1001:1001 .docker_ssh_key.pub