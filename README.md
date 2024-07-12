# Raito Server

If you don't know what the server is for, check this repository [Raito-Manga](https://github.com/nohackjustnoobb/Raito-Manga). This module contains 3 drivers including `MHR`, `DM5`,`MHG`.

## Drivers Information

The driver is responsible for handling how the data is fetched. Each source has its own driver. There are two types of drivers: passive and active. A passive driver fetches data only when the client is requesting it. Active drivers fetch data regardless of client requests. You can write your driver by extending the `BaseDriver` or `ActiveDriver` classes. Check out the provided driver as an example. Feel free to create a pull request.

_The drivers are ordered based on the recommended level._

### MHR - 漫畫人 / 漫畫社

This driver fetches the manga information from the reverse-engineered mobile API server. Its source is the same as `DM5` but faster in responding.

Official link: [click here](https://www.manhuaren.com)

### MHG - 漫畫櫃

This driver scraped the manga information from the website. It is slow and extremely unstable due to its strict request limit. However, it has the most up-to-date manga.

Official link: [click here](https://tw.manhuagui.com)

### DM5 - 漫畫人 (Website)

This driver scraped the manga information from the website. Its source is the same as `MHR` but stabler, as the website won't update frequently.

Official link: [click here](https://www.dm5.com)

## Content management system (CMS)

This server can also be used as a CMS server by enabling the CMS in the configuration file. To access the management system, use the front-end from [Raito-Admin-Panel](https://github.com/nohackjustnoobb/Raito-Admin-Panel).

## Quick Start

1. Create a `config.json` file based on the `config_template.json`.

2. Create a `docker-compose.yml` file like this:

```yml
version: "3.7"

services:
  raito-server:
    image: nohackjustnoobb/raito-server
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
git clone https://github.com/nohackjustnoobb/Raito-Server.git
cd Raito-Server

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
