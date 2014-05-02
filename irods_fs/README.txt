Module irods_fs
=============


Dependencies
------------
PRODS library from RENCI. Installed in sites/all/libraries.

Requirements
------------
You will need in your PHP settings:
* set `allow_url_fopen` to `on`
* fopen wrappers must be enabled

Drupal Settings
---------------

In the configuration for Media > File System (/?q=admin/config/media/file-system), go to "Default download method" and select "iRODS Data Storage" to default iRODS as the storage for downloadable files.
