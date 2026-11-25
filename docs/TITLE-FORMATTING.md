# Title Formatting

Patterns turn a track's tags into words, as foobar2000's do. The playlist's columns, Now Playing, renaming and moving files, the converter's file names and the library's browsing all use them.

FIELDS, between % signs:
```
%artist%  %title%  %album%  %album artist%  %date%  %year%  %genre%  %composer%  %performer%  %comment%
%tracknumber% (two digits)  %totaltracks%  %discnumber%  %totaldiscs%
%length%  %length_seconds%  %bitrate%  %samplerate%  %bitspersample%  %channels%  %codec%
%filename%  %filename_ext%  %ext%  %directoryname%  %path%  %filesize%  %filesize_natural%  %archive%
%list_index%  %list_total%
Any other tag by its name: %mood%, %label%, %replaygain_track_gain%.
%artist% falls back to the album artist, composer or performer; %album artist% to the artist; %title% to the file's name.
A field with no value shows "?".
```

[ ]  shows what's inside only if a field in it has a value:  [%album% - ]%title%
' '  shows what's inside as it is:  '[live]'

FUNCTIONS:
```
$if(cond,then[,else])  $if2(a,else)  $if3(a,b,...,else)  $ifequal(a,b,then,else)  $ifgreater(a,b,then,else)  $iflonger(a,n,then,else)
$select(n,a,b,...)  $and(...)  $or(...)  $not(x)  $xor(...)  $greater(a,b)  $longer(a,b)  $strcmp(a,b)  $stricmp(a,b)  $strstr(a,b)
$add(a,b,...)  $sub  $mul  $div  $mod  $min  $max
$upper(s)  $lower(s)  $caps(s)  $caps2(s)  $trim(s)  $len(s)  $left(s,n)  $cut(s,n)  $right(s,n)  $substr(s,from,to)
$pad(s,n[,c])  $pad_right(s,n[,c])  $padcut(s,n)  $num(n,digits)  $replace(s,a,b,...)  $repeat(s,n)  $insert(s,t,n)
$stripprefix(s[,prefixes])  $abbr(s)  $directory(path[,n])  $directory_path(path)  $ext(path)  $filename(path)
$meta(field[,n])  $meta_sep(field,sep[,last])  $meta_num(field)  $meta_test(fields...)  $year(date)
$crlf()  $tab([n])  $char(code)
```

EXAMPLES:
```
%album artist%/[%date% - ]%album%/[%discnumber%-]%tracknumber% - %title%
$if(%track artist%,%track artist% - )%title%
$num(%tracknumber%,3). $caps(%title%)
```


(The same words are in the app under Help.)
