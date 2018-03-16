dkusage
=============

This software calculates the real disk usage of a directory tree.

As it walks the specified directory tree, it gathers the low-level
size data for all directories, links and files, and prints out a
total size in bytes.


Usage
-----

  Usage: dkusage [-d][[-f]|[-t]][-v][-h] [directory_names]
  
  Options:
        -d  do not include files/directories with dot names
        -f  follow symbolic links
        -t  calculate tarfile size
        -v  verbose mode - print filenames
        -h  help - usage


Installing
----------

$ make
$ su 
# make install


Disclaimer
----------

dkusage is free to use, distribute or modify. use at your own risk.
