#!/bin/bash
# pacman -S inotify-tools # for inotifywait

while true
do
	inotifywait -r -e modify -e create -e close .
	rsync -a js css images /srv/http
	go run build-blog.go -pub /srv/http -domain brak.tobert.org -force-idx
done
