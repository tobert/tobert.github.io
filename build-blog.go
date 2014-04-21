package main

/* build-blog.go
 * A simple bespoke static content processor by @AlTobey.
 *
 * This is tailored to my workflow and I do not expect it to be useful
 * to anybody else.
 *
 * License: Creative Commons Attribution 4.0 International
 *
 * Usage: go run build-blog.go
 */

import (
	"bytes"
	"github.com/russross/blackfriday"
	"gopkg.in/yaml.v1"
	"io/ioutil"
	"log"
	"net/url"
	"os"
	"path"
	"path/filepath"
	"sort"
	"strings"
	"text/template"
	"time"
)

type Config struct {
	RepoRoot string   // /home/atobye/src/tobert.github.io
	SiteURL  *url.URL // http://tobert.github.io
	PageDir  string   // src
	SnipDir  string   // snippets
}

type Snippet struct {
	Id      string // based on filename with path & extension removed
	SrcPath string // /home/atobey/src/tobert.github.io/snippets/header.html
	src     string // raw data
	tmpl    *template.Template
}

// a map of Id => Snippet{}
type Snippets map[string]Snippet

type Page struct {
	Id       string    // why-i-wrote-slobber, used to generate permalinks
	Title    string    // <title>{{ .Title }}</title>
	Abstract string    // A quick overview of the post for RSS
	Tags     []string  // ["slobber", "golang"]
	PubDate  string    // the string value, will be converted to Date after
	Date     time.Time // 9999-12-31
	Draft    bool      // don't render pages with draft: true
	SrcPath  string    // the relative path of the source file
	SrcRel   string    // relative path of the source doc
	PubPath  string    // the path the file will be written to
	PubRel   string    // relative path of the published doc
	Dir      string    // the subdirectory, e.g. / for index.html, 'post' for posts
	Type     string    // md html txt xml json
	src      string    // raw data
}

type Pages []Page // sort interface methods below
type TagPagesIdx map[string][]Page

type TmplData struct {
	Page     Page
	Config   Config
	Snippets Snippets
	Pages    Pages
	TagIndex TagPagesIdx
	Now      time.Time
}

func main() {
	// I may or may not make this stuff configurable someday
	root := path.Join(os.Getenv("HOME"), "src/tobert.github.io")
	siteUrl, err := url.Parse("http://tobert.github.io")
	if err != nil {
		panic("could not parse domain")
	}

	c := Config{
		root,
		siteUrl,
		"src",
		"snippets",
	}

	snippets := loadSnippets(c)
	pages := findPages(c)
	sort.Sort(pages)

	// an index of tag => [ page page page ]
	tagIdx := make(TagPagesIdx)

	for _, page := range pages {
		for _, tag := range page.Tags {
			tagIdx[tag] = append(tagIdx[tag], page)
		}
	}

	// render pages
	for _, page := range pages {
		td := TmplData{page, c, snippets, pages, tagIdx, time.Now()}

		// parse the page template
		tmpl, err := template.New(page.Id).Parse(page.src)
		if err != nil {
			log.Fatalf("Template parsing of page file '%s' failed: %s", page.SrcPath, err)
		}

		// load snippets too, names are basename $file
		for _, s := range snippets {
			_, err = tmpl.ParseFiles(s.SrcPath)
			if err != nil {
				log.Fatalf("Snippet parsing failed on '%s': %s\n", s.SrcPath, err)
			}
		}

		// open file for write
		fd, err := os.OpenFile(page.PubPath, os.O_CREATE|os.O_WRONLY|os.O_TRUNC, 0644)
		if err != nil {
			log.Fatalf("Could not open '%s' for write: %s\n", page.PubPath, err)
		}
		defer fd.Close()

		// text/template supports referencing other templates, but that would be silly
		// since I want this on every html page
		if page.Type == "html" || page.Type == "md" {
			err = snippets["header"].tmpl.Execute(fd, td)
			if err != nil {
				log.Fatalf("Failed to render header template: %s\n", err)
			}
		}

		// index.html is the only special page, it has its own container
		// everything else gets a standard container from a snippet
		if path.Base(page.SrcRel) != "index.html" {
			err = snippets["container-top"].tmpl.Execute(fd, td)
			if err != nil {
				log.Fatalf("Failed to render container-top snippet: %s\n", err)
			}
		}

		// everything in the source directory is treated as a template
		// run the template into a buffer instead of the file so it can
		// be processed with blackfriday or similar before output
		var buf bytes.Buffer
		err = tmpl.Execute(&buf, td)
		if err != nil {
			log.Fatalf("Failed to render template '%s': %s\n", page.SrcRel, err)
		}

		// process Markdown, write everything else directly to the file
		if page.Type == "md" {
			output := blackfriday.MarkdownCommon(buf.Bytes())
			_, err = fd.Write(output)
		} else {
			_, err = fd.Write(buf.Bytes())
		}
		if err != nil {
			log.Fatalf("Error writing content to file '%s': %s'\n", page.PubPath, err)
		}

		// close the container snippet
		if path.Base(page.SrcRel) != "index.html" {
			err = snippets["container-bottom"].tmpl.Execute(fd, td)
			if err != nil {
				log.Fatalf("Failed to render container-bottom snippet: %s\n", err)
			}
		}

		// add the footer to the file
		if page.Type == "html" || page.Type == "md" {
			err = snippets["footer"].tmpl.Execute(fd, td)
			if err != nil {
				log.Fatalf("Failed to render footer template: %s\n", err)
			}
		}

		log.Printf("OK Wrote %s to %s\n", strings.TrimLeft(page.SrcRel, "/"), strings.TrimLeft(page.PubRel, "/"))
	}
}

// loads all snippet files in Config.SnipSrcPath into memory
func loadSnippets(c Config) Snippets {
	snippets := make(Snippets)

	visitor := func(fpath string, f os.FileInfo, err error) error {
		if err != nil {
			log.Fatalf("Encountered an error while loading snippets in '%s': %s", fpath, err)
		}

		fname := path.Base(fpath)
		ext := path.Ext(fname)
		if ext == ".md" || ext == ".html" || ext == ".txt" || ext == ".xml" {
			id := strings.TrimSuffix(fname, ext)

			src, err := ioutil.ReadFile(fpath)
			if err != nil {
				log.Fatalf("Could not read snippet source file '%s': %s", fpath, err)
			}

			srcStr := string(src)

			tmpl, err := template.New(id).Parse(srcStr)
			if err != nil {
				log.Fatalf("Error parsing snippet '%s' as template: %s\n", fpath, err)
			}

			snip := Snippet{id, fpath, srcStr, tmpl}

			snippets[id] = snip
		}

		return nil
	}

	dir := path.Join(c.RepoRoot, c.SnipDir)
	err := filepath.Walk(dir, visitor)
	if err != nil {
		log.Fatalf("Could not load snippets in '%s': %s", dir, err)
	}

	return snippets
}

// find all page files, loading the whole file to extract the YAML block for metadata
// all files in the 'source' directory must have a YAML block between --- delimiters
// e.g.
// ---
// foo: "bar"
// ---
func findPages(c Config) (pages Pages) {
	visitor := func(fpath string, f os.FileInfo, err error) error {
		if err != nil {
			log.Fatalf("Encountered an error while loading pages in '%s': %s", fpath, err)
		}

		if f.IsDir() {
			return nil
		}

		// only consider files with the following extensions
		ext := path.Ext(fpath)
		if ext != ".md" && ext != ".html" && ext != ".txt" && ext != ".xml" {
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
		yamlBytes := src[3 : end+3] // index was offset by 3, so add it back
		// TODO: possible bug here ... need to check assumption of src offset
		tmplBytes := src[end+7 : len(src)] // second --- is always followed by \n, so 3 + 4

		page := Page{
			Type:  ext[1:len(ext)],
			src:   string(tmplBytes),
		}
		err = yaml.Unmarshal(yamlBytes, &page)

		if page.Id == "" {
			log.Fatalf("Parsing of date '%s' in file '%s' failed:\n\tid: is required!\n", page.PubDate, fpath)
		}

		// these variables are used below to build paths in the Page struct
		dname, fname := path.Split(fpath)
		subpath := strings.TrimPrefix(dname, path.Join(c.RepoRoot, c.PageDir))
		fparts := []string{page.Id}
		// markdown will get rendered to HTML, everything goes as-is
		if ext == ".md" {
			fparts = append(fparts, ".html")
		} else {
			fparts = append(fparts, ext)
		}

		page.SrcPath = fpath
		page.SrcRel = path.Join(subpath, fname) // will include leading /
		page.PubRel = path.Join(subpath, strings.Join(fparts, ""))
		page.PubPath = path.Join(c.RepoRoot, subpath, strings.Join(fparts, ""))
		page.Dir = strings.Trim(subpath, "/")
		if page.Dir == "" {
			page.Dir = "/"
		}

		// now convert pubdate -> date, which is required to be RFC3339 format
		page.Date, err = time.Parse(time.RFC3339, page.PubDate)
		if err != nil {
			log.Fatalf("Parsing of date '%s' in file '%s' failed:\n\t%s\n", page.PubDate, fpath, err)
		}

		// leave drafts out of the list so they don't get rendered/published
		if !page.Draft {
			pages = append(pages, page)
		}

		return nil
	}

	dir := path.Join(c.RepoRoot, c.PageDir)
	err := filepath.Walk(dir, visitor)
	if err != nil {
		log.Fatalf("Could not load page source in '%s': %s", dir, err)
	}

	return pages
}

func (pl Pages) Len() int {
	return len(pl)
}
func (pl Pages) Less(i, j int) bool {
	return pl[i].Date.After(pl[j].Date)
}
func (pl Pages) Swap(i, j int) {
	pl[i], pl[j] = pl[j], pl[i]
}
