# Raito Server Documentation

To use the API, refer to the [Web API](#web-api) section. If you're interested in writing your own driver, see the [App API](#app-api) section.

## Web API

### Server and Drivers Information

| Endpoint                                  | Description                         |
| ----------------------------------------- | ----------------------------------- |
| [/](app_api.md)                           | Retrieve server information         |
| [/driver](app_api.md#driver)              | Retrieve driver information         |
| [/driver/online](app_api.md#driveronline) | Check if the driver is still online |

### Manga Related

| Endpoint                             | Description                                    |
| ------------------------------------ | ---------------------------------------------- |
| [/list](app_api.md#list)             | Retrieve a list of manga                       |
| [/manga](app_api.md#manga)           | Retrieve information for specific manga        |
| [/chapter](app_api.md#chapter)       | Retrieve URLs for a specific chapter           |
| [/suggestion](app_api.md#suggestion) | Retrieve search suggestions based on a keyword |
| [/search](app_api.md#search)         | Search for manga using a specific keyword      |

### CMS Related

| Endpoint                                         | Description                            |
| ------------------------------------------------ | -------------------------------------- |
| [/admin/manga/edit](app_api.md#adminmangaedit)   | Edit or create manga                   |
| [/admin/chapter/edit](app_api.md#adminmangaedit) | Edit or create chapters                |
| [/admin/image/edit](app_api.md#adminimageedit)   | Edit or add images to manga            |
| [/admin/download](app_api.md#admindownload)      | Download manga as a ZIP file           |
| [/admin/upload](app_api.md#adminupload)          | Upload manga using a `.raito.zip` file |

_The endpoints below function similarly to the ones listed above, except they don't require specifying a driver. They will always use the self-hosted source._
| Endpoint | Description |
| ----------------------------------------------- | -------------------------------------------------------------------------- |
| [/admin](app_api.md#admin) | Retrieve information about the self-hosted source |
| [/admin/list](app_api.md#adminlist) | Retrieve a list of manga from the self-hosted source |
| [/admin/manga](app_api.md#adminmanga) | Retrieve information for specific manga from the self-hosted source |
| [/admin/chapter](app_api.md#adminchapter) | Retrieve URLs for a specific chapter from the self-hosted source |
| [/admin/suggestion](app_api.md#adminsuggestion) | Retrieve search suggestions based on a keyword from the self-hosted source |
| [/admin/search](app_api.md#adminsearch) | Search for manga using a keyword in the self-hosted source |

### Others

| Endpoint                              | Description                                                          |
| ------------------------------------- | -------------------------------------------------------------------- |
| [/admin/token](app_api.md#admintoken) | Manage user access tokens                                            |
| [/share](app_api.md#share)            | Generate a shareable link to preview manga description and thumbnail |
| [/image](app_api.md#image)            | Image proxy                                                          |

## App API

TODO
