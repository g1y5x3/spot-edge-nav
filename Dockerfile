FROM osrf/ros:humble-desktop-full

USER root

RUN apt-get update && \
    apt-get install -y \
        sudo \
        tmux \
        python3-pip \
        iputils-ping \
        python3-colcon-common-extensions \
        python3-serial \
        mesa-utils \
        libglpk-dev \
        liburdfdom-dev \
        liboctomap-dev \
        libassimp-dev \
        libcgal-dev \
        libpcap-dev \
        pcl-tools \
        zstd \
        ros-humble-pinocchio \
        ros-humble-grid-map \
        ros-humble-hpp-fcl \
        ros-humble-teleop-twist-keyboard \
        ros-humble-pcl-ros \
        ros-humble-pcl-conversions \
        ros-humble-tf2-eigen \
        ros-humble-rmw-zenoh-cpp \
        ros-humble-rosbag2-storage-mcap \
    && rm -rf /var/lib/apt/lists/*

RUN python3 -m pip install --upgrade --no-cache-dir \
        bosdyn-client \
        bosdyn-mission \
        bosdyn-choreography-client \
        bosdyn-orbit

# User setup
ARG UNAME=rosuser
ARG UID=1000
ARG GID=1000

# 3. Create user AND grant sudo privileges
RUN groupadd -g $GID $UNAME && \
    useradd -u $UID -g $GID -m -s /bin/bash $UNAME && \
    usermod -aG sudo,video,dialout,plugdev $UNAME && \
    echo "$UNAME ALL=(ALL) NOPASSWD:ALL" > /etc/sudoers.d/$UNAME && \
    chmod 0440 /etc/sudoers.d/$UNAME

# Switch to the new user
USER $UNAME

# Set user-specific environment variables
ENV HOME /home/$UNAME

# Automatically source ROS 2 when opening a new shell
RUN echo "source /opt/ros/humble/setup.bash" >> /home/$UNAME/.bashrc

# Enable vi mode in tmux
RUN echo "set -g mode-keys vi" >> /home/$UNAME/.tmux.conf

# Set the working directory
WORKDIR /home/$UNAME/ros2_ws
