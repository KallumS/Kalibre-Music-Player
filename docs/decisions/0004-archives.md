# 0004: Archives through libarchive, read whole into memory

Accepted, 25 November 2026.

libarchive reads ZIP, RAR (4 and 5), 7-Zip, tar and gzip with no outside libraries but zlib (the system's). A location inside an archive is "archive:" + the archive's path + "|" + the entry's. A compressed entry can't be seeked, so it's extracted into memory when opened; a song's size allows it. Tags inside archives are read, never written.
