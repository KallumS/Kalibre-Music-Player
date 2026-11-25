# Search Queries

Type words to find tracks with every word somewhere in their tags or file name:
```
beatles abbey
```

Or a query, with keywords in capitals, as foobar2000's:
```
artist HAS beatles                    the field holds these words
genre IS jazz                         the field is exactly this (any one of its values)
date AFTER 1969  /  date BEFORE 1980
rating GREATER 3  /  LESS  /  EQUAL
comment MISSING  /  comment PRESENT
added DURING LAST 2 WEEKS             MINUTES, HOURS, DAYS, WEEKS
%album artist% IS %artist%            a pattern compared with a pattern
* HAS live                            any field
NOT, AND, OR and ( ) combine them:
genre IS jazz AND (date BEFORE 1970 OR artist HAS davis)
ALL                                   everything
... SORT BY %date%  /  SORT DESCENDING BY %rating%
```

A query that can't be read is taken as words. Autoplaylists (File > New Autoplaylist) hold what a query finds, and follow the library.


(The same words are in the app under Help.)
