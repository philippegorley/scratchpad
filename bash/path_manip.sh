file=/path/to/a/file.txt
echo ${file%.*} # remove ext
echo ${file%%.*} # remove all ext
echo ${file%/*} # dirname $file
echo ${file#/path/to} # remove prefix /path/to
echo ${file##*/} # basename $file

