#!/bin/bash
#
# This script uses inotifywait to watch for changes in my repo's
# src/ directory and runs build-blog.go each time a file changes.
#
# pacman -S inotify-tools # for inotifywait on Arch Linux

if [ ! -x tobert.github.io ] ; then
  go build -a
fi

pushcode () {
  rsync -a js css images /srv/http
  ./tobert.github.io -pub /srv/http -domain brak.tobert.org -force-idx
}

pushcode

while true
do
	inotifywait -r -e modify -e create -e close .
	pushcode
done
