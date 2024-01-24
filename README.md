# Raito Server (C++ version)

This repository is a submodule of [Raito-Server](https://github.com/nohackjustnoobb/Raito-Server). If you don't know what the server is for, check this repository [Raito-Manga](https://github.com/nohackjustnoobb/Raito-Manga). This module contains 3 drivers including `MHR`, `DM5`,`MHG`.

## Quick Start

1. Create a `config.json` file based on the `config_template.json`.

2. Create a `docker-compose.yml` file like this:

```yml
version: "3.7"

services:
  raito-server:
    image: nohackjustnoobb/raito-server-cpp
    container_name: raito-server
    restart: unless-stopped
    ports:
      - "3000:3000"
    volumes:
      - ./config.json:/app/config.json:ro
```

3. Create the container

```bash
sudo docker-compose up -d
```

## Manual Setup (Not Recommended)

Make sure that you have [Conan](https://conan.io), and `cmake` installed before setting up.

```bash
# 1. Clone this repository
git clone https://github.com/nohackjustnoobb/Raito-Server-Cpp.git
cd Raito-Server-Cpp

# 2. Create and Edit the config file
cp config_template.json config.json
nano config.json

# 3. Install the dependencies
conan profile detect
conan install . --output-folder=build --build=missing

# 4. Build the server
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Release
make

# 5. Run the server
chmod +x Raito-Server
./Raito-Server
```

You can execute the commands one by one or copy all of them at once and create a shell script.
