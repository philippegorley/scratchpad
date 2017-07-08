file=/path/to/a/file.txt
echo ${file%.*} # remove ext
echo ${FILE%%.*} # remove all ext
echo ${FILE%/*} # dirname $file
echo ${FILE#/path/to} # remove prefix /path/to
echo ${FILE##*/} # basename $file

