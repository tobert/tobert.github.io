#!/bin/bash
#
# This script uses inotifywait to watch for changes in my repo's
# src/ directory and runs build-blog.go each time a file changes.
#
# pacman -S inotify-tools # for inotifywait on Arch Linux
# TODO: use https://github.com/cespare/reflex instead

while true
do
	inotifywait -r -e modify -e create -e close .
	rsync -a js css images /srv/http
	go run build-blog.go -pub /srv/http -domain brak.tobert.org -force-idx
done
