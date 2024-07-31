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

## Access Guard

There are several ways to protect the API, such as using keys and tokens. You can specify the mode in the configuration file. If the mode is set to key, users will need the same key to access the API. If the mode is set to token, you will need to use the [Raito-Admin-Panel](https://github.com/nohackjustnoobb/Raito-Admin-Panel) to manage the tokens. Each token is associated with a unique ID representing a user, and each user will need their own token to access the API. The token mode is designed to be used with the CMS enabled, but it can also be used alone. If used alone, the admin panel may warn that it cannot connect to the server; simply ignore the warning and click the token management button.

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
sudo docker compose up -d
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

## Known Issues

There is a known issue where the `mysql.h` and `libpq-fe.h` headers are not able to be included due to an unknown cause. This issue prevents the use of both MySQL and PostgreSQL databases in the application.
