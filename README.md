# ReceiptReader

## Web and desktop UI

The browser UI is built with Vue 3, TypeScript, and Vite. The REST server serves the
same built UI and API for browsers and the Qt WebEngine desktop wrapper.

1. Build the frontend and C++ targets (requires Node.js/npm and Qt 6 WebEngineWidgets):

       ./scripts/build

2. Start the server:

       ./scripts/run-server

   Open `http://localhost:53000/` in a browser. To override defaults, set
   `RECEIPTREADER_PORT`, `RECEIPTREADER_DB`, or `RECEIPTREADER_WEB_ROOT`, or pass
   server arguments to `run-server`.
3. To launch the desktop wrapper separately, with the server already running:

       ./scripts/run-desktop-gui

   Set `RECEIPTREADER_URL` to connect the desktop wrapper to a different server.
   Stop either or both background processes with `./scripts/stop`. Run tests with
   `./scripts/run-test`.

The server listens on all network interfaces so a phone on the same LAN can open
`http://<server-lan-address>:53000/`. The API currently has no authentication; only
use LAN access on a trusted network and do not expose the server to the public internet.
The desktop wrapper connects to an already-running server; it does not start one.

## Docker

Build the container image and start the web application/API server with persistent
database storage:

    docker build -t receipt-reader .
    docker run --detach --name receipt-reader \
      --publish 53000:53000 \
      --volume receipt-reader-data:/data \
      receipt-reader

Open `http://localhost:53000/`. Set `RECEIPTREADER_PORT` to change the server port
(and publish that same container port). This image runs the server and serves the
web UI; the Qt desktop webview is not included. As with the non-container setup,
the API has no authentication, so expose it only on a trusted network.

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