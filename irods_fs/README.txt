Module irods_fs
=============


Dependencies
------------
PRODS library from RENCI.

Requirements
------------
You will need in your PHP settings:
* set `allow_url_fopen` to `on`
* fopen wrappers must be enabled
* PHP's curl extension must be installed.

Drupal Settings
---------------

In the configuration for Media > File System (/?q=admin/config/media/file-system), go to "Default download method" and select "iRODS Data Storage" to default iRODS as the storage for downloadable files.
