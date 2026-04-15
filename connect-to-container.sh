#!/bin/bash

sudo xhost +local:docker
sudo docker exec -it cpp_qt_dev /bin/bash
