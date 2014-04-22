package main

// a quick hack to run tidy over the HTML from the Blogger export
// must be run from the root of my repo, will mangle any html file
// so be careful what gets committed after running!
// usage: go run tidy-html-sources.go

import (
	"bytes"
	"os/exec"
	"io/ioutil"
	"log"
	"os"
	"path"
	"path/filepath"
)

func main() {
	visitor := func(fpath string, f os.FileInfo, err error) error {
		if err != nil {
			log.Fatalf("Encountered an error while loading pages in '%s': %s", fpath, err)
		}

		if f.IsDir() {
			return nil
		}

		// only consider files with the following extensions
		ext := path.Ext(fpath)
		if ext != ".html" {
			return nil
		}

		src, err := ioutil.ReadFile(fpath)
		if err != nil {
			log.Fatalf("Could not read page source file '%s': %s", fpath, err)
		}

		if src[0] != '-' || src[1] != '-' || src[2] != '-' {
			log.Fatalf("Source file '%s' must have '---' as the first 3 characters!", fpath)
		}

		// found the first ---, now find the second one and abstract the YAML for parsing
		end := bytes.Index(src[3:len(src)], []byte("---"))
		yamlBytes := src[4 : end+3] // index was offset by 3, so add it back
		// TODO: possible bug here ... need to check assumption of src offset
		tmplBytes := src[end+7 : len(src)] // second --- is always followed by \n, so 3 + 4 (this is the HTML)

		cmd := exec.Command("/bin/tidy", "-indent", "-utf8", "-asxhtml", "-wrap", "120", "--show-body-only", "yes", "-")
		stdin, _ := cmd.StdinPipe()
		stdout, _ := cmd.StdoutPipe()
		cmd.Start()

		stdin.Write(tmplBytes)
		stdin.Close()

		buf, _ := ioutil.ReadAll(stdout)

		err = cmd.Wait()
		if err != nil {
			log.Printf("/bin/tidy failed: %s\n", err)
		}

		mark := []byte("---\n")

		ioutil.WriteFile(fpath, bytes.Join([][]byte{mark, yamlBytes, mark, buf}, []byte("")), 0644)

		return nil
	}

	err := filepath.Walk("./src", visitor)
	if err != nil {
		log.Fatalf("Could not load page source in ./src: %s", err)
	}
}
