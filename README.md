SMS Server 3
============

This is a fork of the official SMS Server 3.

I've ported on GIT and refactored a lot.

# REDIS

I need to put my SMS into REDIS and use PUB/SUB for sending, bypassing the filesystem. This allow to use a
`SMS Server 3` easily through a stable communication protocol as Redis is.

Keep in touch!

## Build

You can disable REDIS disabling the flag into `Makefile`
