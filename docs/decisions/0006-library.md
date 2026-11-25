# 0006: The library: watched folders, queries, facets

Accepted, 25 November 2026.

The library is the tracks in the folders chosen, kept as one JSON file and searched in memory (fast enough for tens of thousands of tracks). The scanner reads only files whose modification time or size changed. On the Mac, FSEvents says which folders changed; elsewhere a timer looks every five minutes. Searching takes foobar2000's query syntax, or plain words. Browsing is facets: three columns, each any field, each narrowing the next; what's chosen fills a Library Selection playlist, as foobar's Facets does.
