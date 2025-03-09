# Web API

## Server and Drivers Infomation

### `/`

Method: `GET`

Parameters: `None`

Headers:

| Key        | Required               | Default | Description                                                                                  |
| ---------- | ---------------------- | ------- | -------------------------------------------------------------------------------------------- |
| Access-Key | Based on `config.json` | None    | If access guard is set in the `config.json`, the correct key is needed to access the server. |

Response Sample:

```json
{
  "availableDrivers": ["EXP1", "EXP2", "EXP3"],
  "version": "1.0.0 (Drogon)"
}
```


### `/driver`

### `/driver/online`

## Manga Related

### `/list`

### `/manga`

### `/chapter`

### `/suggestion`

### `/search`

## CMS Related

TODO

## Others

TODO
