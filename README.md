# Raito Server (C++ version)

TODO: description

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
