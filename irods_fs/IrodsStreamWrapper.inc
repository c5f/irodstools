<?php
/**
 * @file
 * Drupal stream wrapper implementation for iRODS
 */

/**
 * Implements DrupalStreamWrapperInterface to provide an iRODS wrapper with
 * the rods:// prefix
 */
class IrodsStreamWrapper implements DrupalStreamWrapperInterface
{
    /**
     *
     * @var mixed
     */
    private $config = array();

    /**
    * current postion of the file or dir
    *
    * @var int
    */
    private $postion;

    /**
     * Name of the directory/collection specified in the URI to opendir().
     *
     * @var ProdsDir
     */
    private $dir;

    /**
     * Name of the file specified in the URI to fopen().
     *
     * @var ProdsFile
     */
    private $file;

    /**
     * @var string    Referenced as "rods://..."
     */
    protected $uri;

    /**
     * @var string 	The domain on which the iRODS server is.
     */
    protected $domain;

    /**
     * @var array
     *   Default map for determining file mime types
     */
     protected static $mapping = null;


    /**
     * Object constructor
     *
     * Sets the uri
     */
    public function __construct()
    {
        $this->uri = $this->getBaseURI();
    }

    /**
     * Object destructor
     *
     * Called just prior to stream_flush()
     */
    public function __destruct()
    {

    }


    /**
     * Sets the stream resource URI.
     *
     * URIs are formatted as "rods://username:password@domain:1247/managed_images/"
     *
     * This allows you to set the URI. Generally is only called by the factory
     * method.
     *
     * @param $uri 	A string containing the URI that should be used for this instance.
     *
     * @return  Returns the current URI of the instance.
     */
    public function setUri($uri) {
        $this->uri = $uri;
    }


    /**
     * Returns the stream resource URI.
     *
     * URIs are formatted as "rods://username:password@domain:1247/managed_images/"
     *
     * @return  Returns the current URI of the instance.
     */
    public function getUri()
    {
        return $this->uri;
    }


    /**
     * Gets the path that the wrapper is responsible for.
     *
     * @return  String  specifying the path.
     */
    public function getDirectoryPath() {
        return $this->domain;
    }


    /**
     * Returns a web accessible URL for the resource.
     *
     * This function should return a URL that can be embedded in a web page
     * and accessed from a browser.
     *
     * @return
     *   Returns a string containing a web accessible URL for the resource.
     */
    public function getExternalUrl($uri = NULL)
    {
        /* There could be security problems with this if we include the password in the url */
        return $this->getBaseURI($uri);
    }


    /**
     * Determine a file's media type
     *
     * Uses Drupal's mimetype mappings. Returns 'application/octet-stream' if
     * no match is found.
     *
     *  @return
     *   Returns a string representing the file's MIME type
     */
    public static function getMimeType($uri, $mapping = NULL)
    {
        /* Load the default file map */
        if ( ! isset(self::$mapping) ) {
            include_once DRUPAL_ROOT . '/includes/file.mimetypes.inc';

            self::$mapping = file_mimetype_mapping();
        }

        $extension = '';
        $file_parts = explode('.', basename($uri));

        /* Remove the first part: a full filename should not match an extension */
        array_shift($file_parts);

        /* Iterate over the file parts, trying to find a match.
         * For my.awesome.image.jpeg, we try:
         *   - jpeg
         *   - image.jpeg, and
         *   - awesome.image.jpeg
         */
        while ( $additional_part = array_pop($file_parts) ) {
            $extension = strtolower($additional_part . ($extension ? '.' . $extension : ''));

            if ( isset(self::$mapping['extensions'][$extension]) ) {
                return self::$mapping['mimetypes'][self::$mapping['extensions'][$extension]];
            }
        }

        return 'application/octet-stream';
    }


    /**
     * Returns the local writable target of the resource within the stream.
     *
     * This function should be used in place of calls to realpath() or similar
     * functions when attempting to determine the location of a file. While
     * functions like realpath() may return the location of a read-only file, this
     * method may return a URI or path suitable for writing that is completely
     * separate from the URI used for reading.
     *
     * @param $uri
     *   Optional URI.
     *
     * @return
     *   Returns a string representing a location suitable for writing a file,
     *   or FALSE if unable to write to the file such as with read-only streams.
     */
    protected function getTarget($uri = NULL)
    {
        if ( ! isset($uri) ) {
            $uri = $this->uri;
        }

        list($scheme, $target) = explode('://', $uri, 2);

        /* Remove erroneous leading or trailing forward-slashes and backslashes. */
        return trim($target, '\/');
    }


    /**
     * Changes permissions of the resource.
     *
     * @TODO Do we even need chmod in iRODS??
     *
     * @param $mode
     *   Integer value for the permissions. Consult PHP chmod() documentation
     *   for more information.
     *
     * @return TRUE on success or FALSE on failure.
     */
    public function chmod($mode)
    {
        return TRUE;
    }


    /**
     * Returns canonical, absolute path of the resource.
     *
     * @return str
     */
    public function realpath()
    {
        return $this->getLocalPath();
    }


    /**
     * Gets the name of the directory from a given path.
     *
     * This method is usually accessed through drupal_dirname(), which wraps
     * around the normal PHP dirname() function, which does not support stream
     * wrappers.
     *
     * @param $uri
     *   An optional URI.
     *
     * @return String containing the directory name, or FALSE if not applicable.
     *
     * @see drupal_dirname()
     */
    public function dirname($uri = NULL)
    {
        list($scheme, $target) = explode('://', $uri, 2);

        $target  = $this->getTarget($uri);
        $dirname = dirname($target);

        if ($dirname == '.') {
            $dirname = '';
        }

           return $scheme . '://' . $dirname;
      }


    /**
     * Support for fopen(), file_get_contents(), file_put_contents() etc.
     *
     * @param $uri
     *   A string containing the URI to the file to open.
     * @param $mode
     *   The file mode ("r", "wb" etc.).
     * @param $options
     *   A bit mask of STREAM_USE_PATH and STREAM_REPORT_ERRORS.
     * @param $opened_path
     *   A string containing the path actually opened.
     *
     * @return TRUE if file was opened successfully.
     *
     * @see http://php.net/manual/en/streamwrapper.stream-open.php
     */
    public function stream_open($uri, $mode = 'w+', $options, &$opened_path)
    {
        $full_uri = $this->uri . str_replace('rods://', '', $uri);
            // $this->uri already has a trailing slash

        /* parse the url to pass to RODSAccount and ProdsFile, since
         * ProdsFile::fromUri() will not have all the needed permission rules
         * firing on it; explicitly creating a RODSAccount then ProdsFile
         * does work...
         */
        $url_parts = parse_url($full_uri);

        try {
            $this->account = new RODSAccount(
                $url_parts['host'], $url_parts['port'], $url_parts['user'], $url_parts['pass']
            );
            $this->file = new ProdsFile($this->account, $url_parts['path']);

            //Or, this replaces the above 4 lines: $this->file = ProdsFile::fromUri($full_uri);
            $this->file->open($mode);

            /* set the file's position */
            $this->position = 0;

            return TRUE;

        } catch (Exception $e) {
            //@TODO use Drupal's native error handling

            return FALSE;
        }
    }


    /**
     * fread() and fgets() handler.
     *
     * @param $count 	Maximum number of bytes to be read.
     * @return string|bool 	String which was read, or FALSE is failed
     *
     * @see http://php.net/manual/en/streamwrapper.stream-read.php
     */
    public function stream_read($count)
    {
        if ( in_array($this->file->getOpenMode(), array('w', 'a', 'x')) ) {
            return FALSE;
        }

        try {
            $ret = $this->file->read($count);
            $this->position = $this->file->tell();

            return $ret;
        } catch (Exception $e) {
            trigger_error("Got an exception: $e", E_USER_WARNING);

            return FALSE;
        }
    }


    /**
     * fwrite() handler.
     *
     * @param $data string  The content to write to the file
     * @return int|bool 	Number of bytes written, or FALSE if failed
     *
     * @see http://php.net/manual/en/streamwrapper.stream-write.php
     */
    public function stream_write($data)
    {
        try {
            $bytes_written = $this->file->write($data);

            /* The new position after the bytes were written */
            $this->position = $this->file->tell();

            //$this->file->close();

            return $bytes_written;

        } catch (Exception $e) {
            trigger_error("Got an exception: $e", E_USER_WARNING);

            return FALSE;
        }
    }


    /**
     * Support for fclose().
     *
     * @return bool
     *
     * @see http://php.net/manual/en/streamwrapper.stream-close.php
     */
    public function stream_close()
    {
        try {
            /* this is absolutely essential, else for a new file, iRODS will not
             * assign the RODSAdmin permission */
            //if ( is_resource($this->file) && $this->file instanceof ProdsFile) {}
            $this->file->close();

            /* Deallocate any resources */
            $this->position = 0;
            $this->file = null;
            $this->dir = null;

            /* Built-in PHP stream_close returns void, but Drupal needs it be TRUE */
            return TRUE;

        } catch (Exception $e) {
            trigger_error("Got an exception: $e", E_USER_WARNING);

            return FALSE;
        }
    }


    /**
     * Support for flock().
     *
     * @param $operation
     *   One of the following:
     *   - LOCK_SH to acquire a shared lock (reader).
     *   - LOCK_EX to acquire an exclusive lock (writer).
     *   - LOCK_UN to release a lock (shared or exclusive).
     *   - LOCK_NB if you don't want flock() to block while locking (not
     *     supported on Windows).
     *
     * @return bool 	For now, its always FALSE, but might supposed to be TRUE?
     *
     * @see http://php.net/manual/en/streamwrapper.stream-lock.php
     */
    public function stream_lock($operation)
    {
        /* irods locking is at the Database level, not at the resource-level */
        return FALSE;
            //@TODO maybe return TRUE so we can simulate it...?
    }


    /**
     * ftell() handler.
     *
     * @return int 	The current offset in bytes from the beginning of file.
     *
     * @see http://php.net/manual/en/streamwrapper.stream-tell.php
     */
    public function stream_tell()
    {
        return $this->position;
    }


    /**
     * fstat() handler.
     *
     * return mixed|bool 	An array with file status, or FALSE in case of an
     *  					error - see fstat() for a description of this array
     *
     * @see http://php.net/manual/streamwrapper.stream-stat.php
     */
    public function stream_stat()
    {
        try {
            $stats = $this->file->getStats();

            return array (
                -1, -1, -1, -1, -1, -1, $stats->size, time (), $stats->mtime, $stats->ctime, -1, -1,
                'dev' => -1,
                'ino' => -1,
                'mode' => -1,
                'nlink' => -1,
                'uid' => -1,
                'gid' => -1,
                'rdev' => -1,
                'size' => $stats->size,
                'atime' => time (),
                'mtime' => $stats->mtime,
                'ctime' => $stats->ctime,
                'blksize' => -1,
                'blocks' => -1,
            );
        } catch (Exception $e) {
            trigger_error("Got an exception:$e", E_USER_WARNING);

            return array (
                -1, -1, -1, -1, -1, -1, -1, time (), time (), time (), -1, -1,
                'dev' => -1,
                'ino' => -1,
                'mode' => -1,
                'nlink' => -1,
                'uid' => -1,
                'gid' => -1,
                'rdev' => -1,
                'size' => -1,
                'atime' => time (),
                'mtime' => time (),
                'ctime' => time (),
                'blksize' => -1,
                'blocks' => -1,
            );
        }
    }


    /**
     * fseek() handler.
     *
     * @param $offset int 	Number of bytes to seek
     * @param $whence bit 	One of SEEK_SET, SEEK_CUR, or SEEK_END
     * return bool 	TRUE on success; FALSE on failure
     *
     * @see http://php.net/manual/en/streamwrapper.stream-seek.php
     */
    public function stream_seek($offset, $whence)
    {
        try {
            $this->file->seek($offset, $whence);
            // @TODO shouldn't we update $this->position accordingly?

            return TRUE;

        } catch (Exception $e) {
            trigger_error("Got an exception: $e", E_USER_WARNING);

            return FALSE;
        }
        /*
        switch($whence) {
          case SEEK_CUR:
            // Set position to current location plus $offset
            $new_position = $this->position + $offset;
            break;
          case SEEK_END:
            // Set position to eof plus $offset
            $new_position = $this->object_size + $offset;
            break;
          case SEEK_SET:
          default:
            // Set position equal to $offset
            $new_position = $offset;
            break;
        }

        $ret = ($new_position >= 0 && $new_position <= $this->object_size);
        if ($ret) {
          $this->position = $new_position;
        }
        return $ret;
        */
    }


    /**
     * fflush() handler.  "Please Note: This method must be called for any
     * changes to be committed to the repository."
     *
     * However, the PRODS streamer seems to do nothing and writes directly,
     * instead of caching the stream to a buffer, so...
     *
     * @return boolean
     */
    public function stream_flush()
    {
        /* Built-in PHP stream_flush returns void, but Drupal needs it be TRUE */
        return TRUE;
    }


    /**
     * feof() handler.
     *
     * @return bool 	TRUE if end-of-file has been reached.
     *
     * @see http://php.net/manual/en/streamwrapper.stream-eof.php
     */
    public function stream_eof()
    {
        try {
            $stats = $this->file->getStats();

            return $this->position >= $stats->size;

        } catch (Exception $e) {
            trigger_error("Got an exception: $e", E_USER_WARNING);

            return TRUE;
        }
    }


    /**
     * Support for unlink().
     *
     * @param $uri 	A string containing the URI to the resource to delete.
     *
     * @return bool TRUE if resource was successfully deleted, FALSE on failure
     *
     * @see http://php.net/manual/streamwrapper.unlink.php
     */
    public function unlink($uri)
    {
        /* ProdsFile::unlink seems to return nothing, so wrap in a try/catch */
        try {
            /* unlink over streams don't call stream_open() first,
             * so we need to init $this->file here.
             * See: https://bugs.php.net/bug.php?id=40459
             * this is fixed in PHP 5.4.7, and later (Sept, 2012), per PHP Changelog
             */
            $this->file = ProdsFile::fromURI($uri);

            $this->file->unlink();

        } catch (Exception $e) {
            trigger_error("Got an exception: $e", E_USER_WARNING);

            return FALSE;
        }

        return TRUE;
    }


    /**
     * Handle rename(). Not supported by the PRODS library at this time?
     *
     * @param $from_uri 	The URI to the file to rename.
     * @param $to_uri 		The new URI for file.
     *
     * @return 				TRUE if file was successfully renamed.
     *
     * @see http://php.net/manual/streamwrapper.rename.php
     */
    public function rename($from_uri, $to_uri)
    {
        /* __construct is not called prior to this method.
        * See: https://bugs.php.net/bug.php?id=40459 */

        return FALSE;
    }


    /**
     * opendir() handler.
     *
     * (non-PHPdoc)
     * @see StreamWrapperInterface::dir_opendir()
     */
    public function dir_opendir($path, $options)
    {
        try {
          $this->dir=ProdsDir::fromURI($path, TRUE);

          return TRUE;

        } catch (Exception $e) {
          trigger_error("Got an exception:$e", E_USER_WARNING);

          return FALSE;
        }
    }


    /**
     * readdir() handler.
     *
     * @return bool
     */
    public function dir_readdir()
    {
        try {
            $child=$this->dir->getNextChild();

            if ($child === FALSE) {
                return false;
            }

            return $child->getName();

        } catch (Exception $e) {
            trigger_error("Got an exception:$e", E_USER_WARNING);

            return FALSE;
        }
    }


    /**
     * rewinddir() handler.
     *
     * @return bool
     */
    public function dir_rewinddir()
    {
        try {
            $this->dir->rewind();

            return TRUE;

        } catch (Exception $e) {
            trigger_error("Got an exception:$e", E_USER_WARNING);

            return FALSE;
        }
    }


    /**
     * closedir() handler.
     *
     * @return bool
     */
    public function dir_closedir()
    {
        try {
            $this->dir->rewind();

            return TRUE;

        } catch (Exception $e) {
            trigger_error("Got an exception: $e", E_USER_WARNING);

            return FALSE;
        }
    }


    /**
     * Support for mkdir().
     *
     * @param $uri string 			A string containing the URI to the
     * 								directory to create.
     * @param $mode 				Permission flags - see mkdir().
     * @param $options bit mask 	One of STREAM_REPORT_ERRORS and
     * 								STREAM_MKDIR_RECURSIVE.
     *
     * @return bool 				TRUE if directory was successfully created
     *
     * @see http://php.net/manual/streamwrapper.mkdir.php
     */
    public function mkdir($uri, $mode, $options)
    {
        /* PHP won't not call stream_open() first for mkdir() and others,
         * so we need to init $this->dir here. */
        $uri = $this->getBaseURI() . str_replace('rods://', '', $uri);
            // $full_uri already has a trailing slash

        /* get the new dir name */
        $new_directory = basename(parse_url($uri, PHP_URL_PATH));
        $parent_uri = str_replace($new_directory, '', $uri);

        /* instantiate the parent dir; call PRODS to create the new dir */
        $this->dir = ProdsDir::fromURI($parent_uri, true);

        try {
            /* ProdsDir::mkdir returns an instance of ProdsDir,
             * thus the awkward reassignment... */
            $this->dir = $this->dir->mkdir($new_directory);

            /* $recursive = (bool) ($options & STREAM_MKDIR_RECURSIVE);
             * $localpath = $this->getLocalPath($uri); */
            return TRUE;

        } catch (Exception $e) {
            return FALSE;
        }
    }


    /**
     * Support for rmdir().
     *
     * @param $uri string 		the URI to the directory to delete
     * @param $options bit mask 	STREAM_REPORT_ERRORS
     *
     * @return bool 				TRUE if directory was successfully removed
     *
     * @see http://php.net/manual/streamwrapper.rmdir.php
     */
    public function rmdir($uri, $options)
    {
        /* PHP won't not call stream_open() first for mkdir() and others,
         * so we need to init $this->dir here.
         * See: https://bugs.php.net/bug.php?id=40459 */
        $uri = $this->getBaseURI() . str_replace('rods://', '', $uri);
            // $full_uri already has a trailing slash

        /* instantiate the parent dir; call PRODS to create the new dir */
        $this->dir = ProdsDir::fromURI($uri, true);

        try {
            /* ProdsDir::rmdir() returns void */
            $void = $this->dir->rmdir(TRUE, FALSE);

            return TRUE;

        } catch (Exception $e) {
            return FALSE;
        }

        /*
        if ($options & STREAM_REPORT_ERRORS) {

        }
        else {

        }*/
    }


    /**
     * Return the local filesystem path.
     *
     * @param $uri
     *   Optional URI, supplied when doing a move or rename.
     */
    protected function getLocalPath($uri = NULL)
    {
        /* set backup defaults */
        $root = '/iplant/home/';
        $username = 'cldb';

        if ( variable_get('irods_username', 0) ) {
            $username = variable_get($variable_name);

            /* make sure its not empty */
            if ( ! strlen($username) > 0) {
                $username = 'cldb';
            }
        }

        if ( variable_get('irods_root', 0) ) {
            $root = variable_get('irods_root');

            /* make sure its not empty */
            if ( ! strlen('irods_root') > 0) {
                $root = '/iplant/home/';
            }
        }

        return
            $root . $username . '/'
            . trim(str_replace('rods://', '', $uri), '/');
    }


    /**
     * Support for stat().
     *
     * @param $uri string 	 The URI to get information about.
     * @param $flags bitmask One of STREAM_URL_STAT_LINK and STREAM_URL_STAT_QUIET
     *
     * @return mixed 		 Array with file status, or FALSE if failure
     *
     * @see http://php.net/manual/en/streamwrapper.url-stat.php
     * @see http://php.net/manual/en/files.f-stat.php
     */
    public function url_stat($uri, $flags)
    {
        /* __construct is NOT called prior to this function! */
        $full_uri = $this->getBaseURI() . str_replace('rods://', '', $uri);
            // $this->uri already has a trailing slash

        try {
            /* stat over streams doesn't call stream_open() first,
             * so we need to init $this->file here. */
            $this->dir = ProdsDir::fromURI($full_uri, true);
            $stats = $this->dir->getStats();

            /* 040000 is vital; else it never works */
            return array (
                -1, -1, 040755, -1, -1, -1, $stats->size, time (), $stats->mtime, $stats->ctime, -1, -1,
                'dev' => -1,
                'ino' => -1,
                'mode' => 040755,
                'nlink' => -1,
                'uid' => -1,
                'gid' => -1,
                'rdev' => -1,
                'size' => $stats->size,
                'atime' => time (),
                'mtime' => $stats->mtime,
                'ctime' => $stats->ctime,
                'blksize' => -1,
                'blocks' => -1,
            );
        }
        catch(Exception $e) {
            /* try to see if its a file instead. */
            try {
                /* stat over streams won't call stream_open() first,
                 * so we need to init $this->file here. */
                $this->file = ProdsFile::fromURI($full_uri, true);
                $stats = $this->file->getStats();

                return array (
                    -1, -1, 0100644, -1, -1, -1, $stats->size, time (), $stats->mtime, $stats->ctime, -1, -1,
                    'dev' => -1,
                    'ino' => -1,
                    'mode' => 0100644,
                    'nlink' => -1,
                    'uid' => -1,
                    'gid' => -1,
                    'rdev' => -1,
                    'size' => $stats->size,
                    'atime' => time (),
                    'mtime' => $stats->mtime,
                    'ctime' => $stats->ctime,
                    'blksize' => -1,
                    'blocks' => -1,
                );
            }
            catch (Exception $e) {
                trigger_error("Got an exception:$e", E_USER_WARNING);

                return array (
                    -1, -1, -1, -1, -1, -1, -1, time (), time (), time (), -1, -1,
                    'dev' => -1,
                    'ino' => -1,
                    'mode' => -1,
                    'nlink' => -1,
                    'uid' => -1,
                    'gid' => -1,
                    'rdev' => -1,
                    'size' => -1,
                    'atime' => time (),
                    'mtime' => time (),
                    'ctime' => time (),
                    'blksize' => -1,
                    'blocks' => -1,
                );
            }
        }
    }


    /**
     * Construct the full URI to the iRODS server from config
     *
     * URI is as such:
     * "rods://$user:$password@data.iplantcollaborative.org:1247/iplant/home/$user/"
     *
     * @return string 	The base URI to the actual iRODS server.
     */
    protected function getBaseURI()
    {
        /* Defaults */
        $irods = array();

        $irods['schema'] 	= 'rods://';
        $irods['username'] 	= '';
        $irods['password'] 	= '';
        $irods['host'] 	    = 'data.iplantcollaborative.org';
        $irods['port'] 		= '1247';
        $irods['root'] 		= 'iplant/home/' . $irods['username'];

        /* if the domain is set in the site's config, get it; otherwise use the
         * built-in defaults above */
        foreach ($irods as $variable) {
            /* variables names are same as the array key names, but prefixed
             * with 'irods_' in the config tables in the database.  */
            $variable_name = 'irods_' . $variable;
            if ( variable_get($variable_name, 0) ) {
                $variable_value = variable_get($variable_name);
                /* make sure its not empty */
                if ( strlen($variable_value) > 0) {
                    $irods[$variable] = $variable_value;
                }
            }
        }

        /* set the class variable for other functions to reference */
        $base_uri =
            $irods['schema'] . $irods['username'] . ':' . $irods['password'] . '@' .
            $irods['host'] . ':' . $irods['port'] . '/' . $irods['root'] . '/';

        return $base_uri;
    }

}
