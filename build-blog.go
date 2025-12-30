package main

/* build-blog.go
 * A simple bespoke static content processor by @MissAmyTobey.
 *
 * This is tailored to my workflow and I do not expect it to be useful
 * to anybody else.
 *
 * License: Creative Commons Attribution 4.0 International
 *
 * Usage: go run build-blog.go [-domain localhost] [-port 80] [-src path] [-pub path] [-force-idx]
 *  note: all flags have defaults specific to my setup
 *        I mostly use the flags with justrun & Apache for local previews.
 */

import (
	"bytes"
	"encoding/json"
	"flag"
	"fmt"
	"log"
	"net/http"
	"net/url"
	"os"
	"path"
	"path/filepath"
	"regexp"
	"slices"
	"strings"
	"text/template"
	"time"

	"github.com/russross/blackfriday"
	"golang.org/x/text/cases"
	"golang.org/x/text/language"
	"gopkg.in/yaml.v3"
)

type Config struct {
	SrcRoot string   // /home/atobey/src/tobert.github.io
	PubRoot string   // /home/atobey/src/tobert.github.io, /srv/www, etc.
	BaseURL *url.URL // http://tobert.github.io
	PageDir string   // src
	SnipDir string   // snippets
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
	Head     string    // additional <head> info (e.g. CSS)
	Date     time.Time // 9999-12-31
	AutoIdx  bool      // default true, when false omit page from automatic page index
	Script   string    // script code to be placed in a <script></script> after the content
	SrcPath  string    // the relative path of the source file
	SrcRel   string    // relative path of the source doc
	PubPath  string    // the path the file will be written to
	PubRel   string    // relative path of the published doc
	PubFull  string    // full permanent path to the doc e.g. https://tobert.github.io/post/2014-01-01-foobar.html
	Dir      string    // the subdirectory, e.g. / for index.html, 'post' for posts
	Type     string    // md html txt xml json
	SyntaxHl bool      // enable syntax highlighting
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

var (
	defaultPath, srcFlag, pubFlag, domainFlag string
	portFlag                                  int
	forceIdxFlag, serveFlag                   bool

	// regex for extracting date from tldr post filenames
	tldrDateRe = regexp.MustCompile(`^\d+-\d+-\d+`)

	// title caser for generating titles
	titleCaser = cases.Title(language.English)
)

func init() {
	defaultPath = path.Join(os.Getenv("HOME"), "src/tobert.github.io")
	flag.StringVar(&domainFlag, "domain", "tobert.github.io", "the domain to use in generated links")
	flag.IntVar(&portFlag, "port", 80, "the HTTP port to put in the URL")
	flag.StringVar(&srcFlag, "src", defaultPath, "where to find the content source")
	flag.StringVar(&pubFlag, "pub", defaultPath, "where to write generated content")
	flag.BoolVar(&forceIdxFlag, "force-idx", false, "forces all pages into the automatic index")
	flag.BoolVar(&serveFlag, "serve", false, "serve content locally, regenerating on each request")
}

func main() {
	flag.Parse()

	if serveFlag {
		serve()
	} else {
		generate()
	}
}

// for each request, generates content as usual then serves the request
// this is wasteful at the moment since it regens on every request but
// should be fine for development for now
func serve() {
	fileHandler := http.FileServer(http.Dir(pubFlag))

	handler := http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		generate()
		fileHandler.ServeHTTP(w, r)
	})

	addr := fmt.Sprintf(":%d", portFlag)
	err := http.ListenAndServe(addr, handler)
	if err != nil {
		log.Fatalf("net.http could not listen on port %d: %s\n", portFlag, err)
	}
}

// generate the site
func generate() {
	baseUrl, err := url.Parse(fmt.Sprintf("https://%s", domainFlag))
	if err != nil {
		log.Fatalf("Could not parse base URL 'https://%s': %s", domainFlag, err)
	}

	if portFlag != 80 {
		baseUrl.Host = fmt.Sprintf("%s:%d", baseUrl.Host, portFlag)
	}

	c := Config{
		SrcRoot: srcFlag,
		PubRoot: pubFlag,
		BaseURL: baseUrl,
		PageDir: "src",
		SnipDir: "snippets",
	}

	snippets := loadSnippets(c)
	pages := findPages(c)
	slices.SortFunc(pages, func(a, b Page) int {
		// sort by date descending (newest first)
		if a.Date.After(b.Date) {
			return -1
		}
		if a.Date.Before(b.Date) {
			return 1
		}
		return 0
	})

	// create another list that only contains pages for the automatic index
	pageAutoIdx := Pages{}
	for _, page := range pages {
		if page.AutoIdx {
			pageAutoIdx = append(pageAutoIdx, page)
		}
	}

	// an index of tag => [ page page page ]
	tagIdx := make(TagPagesIdx)
	for _, page := range pages {
		for _, tag := range page.Tags {
			tagIdx[tag] = append(tagIdx[tag], page)
		}
	}

	// render all pages
	for _, page := range pages {
		td := TmplData{
			Page:     page,
			Config:   c,
			Snippets: snippets,
			Pages:    pageAutoIdx,
			TagIndex: tagIdx,
			Now:      time.Now(),
		}
		renderPage(page, td, snippets)
	}

	js, err := json.MarshalIndent(pages, "", "  ") //.Marshal(pages)
	if err != nil {
		log.Fatalf("JSON marshaling failed: %s\n", err)
	}

	pagesJson := path.Join(c.PubRoot, "pages.json")
	err = os.WriteFile(pagesJson, js, 0644)
	if err != nil {
		log.Fatalf("Saving %s failed: %s\n", pagesJson, err)
	}
}

// renderPage writes a single page to disk, handling all template rendering
func renderPage(page Page, td TmplData, snippets Snippets) {
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

	// make sure the target directory exists
	err = os.MkdirAll(path.Dir(page.PubPath), 0755)
	if err != nil {
		log.Fatalf("Could not create target directory '%s': %s\n", path.Dir(page.PubPath), err)
	}

	// open file for write
	fd, err := os.OpenFile(page.PubPath, os.O_CREATE|os.O_WRONLY|os.O_TRUNC, 0644)
	if err != nil {
		log.Fatalf("Could not open '%s' for write: %s\n", page.PubPath, err)
	}
	defer fd.Close() // now safe: one file per function call

	// text/template supports referencing other templates, but that would be silly
	// since I want this on every html page
	if page.Type == "html" || page.Type == "md" || page.Type == "sh" {
		err = snippets["header"].tmpl.Execute(fd, td)
		if err != nil {
			log.Fatalf("Failed to render header template: %s\n", err)
		}

		// index.html is the only special page, it has its own container
		// everything else gets a standard container from a snippet
		if path.Base(page.SrcRel) != "index.html" {
			err = snippets["container-top"].tmpl.Execute(fd, td)
			if err != nil {
				log.Fatalf("Failed to render container-top snippet: %s\n", err)
			}
		}

		// .sh files are plain shell scripts that get posted as a page
		// and follow most of the 'tldr' rules (and code)
		if page.Type == "sh" {
			err = snippets["tldr-sh-top"].tmpl.Execute(fd, td)
			if err != nil {
				log.Fatalf("Failed to render tldr-sh-top snippet: %s\n", err)
			}
		}
	}

	// everything in the source directory is considered a template
	var buf bytes.Buffer
	err = tmpl.Execute(&buf, td)
	if err != nil {
		log.Fatalf("Failed to render template '%s': %s\n", page.SrcRel, err)
	}

	if page.Type == "md" {
		output := markdown([]byte(page.src))
		_, err = fd.Write(output)
	} else {
		_, err = fd.Write(buf.Bytes())
	}
	if err != nil {
		log.Fatalf("Error writing content to file '%s': %s'\n", page.PubPath, err)
	}

	if page.Type == "html" || page.Type == "md" || page.Type == "sh" {
		if page.Type == "sh" {
			err = snippets["tldr-sh-bot"].tmpl.Execute(fd, td)
			if err != nil {
				log.Fatalf("Failed to render tldr-sh-bot snippet: %s\n", err)
			}
		}

		// close the container snippet
		if path.Base(page.SrcRel) != "index.html" {
			err = snippets["container-bottom"].tmpl.Execute(fd, td)
			if err != nil {
				log.Fatalf("Failed to render container-bottom snippet: %s\n", err)
			}
		}

		// add the footer to the file
		err = snippets["footer"].tmpl.Execute(fd, td)
		if err != nil {
			log.Fatalf("Failed to render footer template: %s\n", err)
		}
	}

	log.Printf("OK Wrote %s to %s\n", strings.TrimLeft(page.SrcRel, "/"), strings.TrimLeft(page.PubRel, "/"))
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

			src, err := os.ReadFile(fpath)
			if err != nil {
				log.Fatalf("Could not read snippet source file '%s': %s", fpath, err)
			}

			srcStr := string(src)

			tmpl, err := template.New(id).Parse(srcStr)
			if err != nil {
				log.Fatalf("Error parsing snippet '%s' as template: %s\n", fpath, err)
			}

			snippets[id] = Snippet{
				Id:      id,
				SrcPath: fpath,
				src:     srcStr,
				tmpl:    tmpl,
			}
		}

		return nil
	}

	dir := path.Join(c.SrcRoot, c.SnipDir)
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
		if ext != ".md" && ext != ".html" && ext != ".txt" && ext != ".xml" && ext != ".sh" {
			return nil
		}

		page := Page{
			AutoIdx: true,
			Type:    ext[1:],
		}

		// these variables are used below to build paths in the Page struct
		dname, fname := path.Split(fpath)
		subpath := strings.TrimPrefix(dname, path.Join(c.SrcRoot, c.PageDir))
		page.Id = strings.TrimSuffix(fname, ext)

		src, err := os.ReadFile(fpath)
		if err != nil {
			log.Fatalf("Could not read page source file '%s': %s", fpath, err)
		}

		if subpath == "/tldr/" {
			// "tldr" posts are shorter and have no required front matter
			// the dates are expected to be at the front of the filename and are removed
			// from the title
			date := tldrDateRe.FindString(page.Id)
			if date != "" {
				page.Date, err = time.Parse("2006-01-02", date)
				if err != nil {
					log.Fatalf("Parsing of date '%s' in file '%s' failed:\n\t%s\n", date, page.Id, err)
				}
				name := tldrDateRe.ReplaceAllString(page.Id, "")
				page.Id = strings.TrimLeft(name, "-")
			} else {
				log.Fatalf("tldr post '%s' does not seem to have a date prefix", page.Id)
			}

			title := strings.ReplaceAll(page.Id, "-", " ")
			page.Title = fmt.Sprintf("TL;DR: %s", titleCaser.String(title))
			page.Tags = strings.Split(page.Id, "-")

			page.src = string(src)

			if ext == ".sh" {
				page.SyntaxHl = true // make sure rainbow js & css get loaded (see snippets)
			}
		} else {
			// all other pages have YAML "front matter" that is parsed for metadata
			if len(src) < 4 || !bytes.HasPrefix(src, []byte("---")) {
				log.Fatalf("Source file '%s' must start with '---'!", fpath)
			}

			// found the first ---, now find the second one and extract the YAML for parsing
			end := bytes.Index(src[3:], []byte("\n---"))
			if end == -1 {
				log.Fatalf("Source file '%s' is missing closing '---' for front matter!", fpath)
			}

			yamlBytes := src[4 : end+3] // skip opening "---\n", end is relative to src[3:]
			// find where content starts (after closing --- and its newline)
			contentStart := end + 3 + 4 // position after "\n---"
			if contentStart < len(src) && src[contentStart] == '\n' {
				contentStart++ // skip the newline after closing ---
			}
			if contentStart > len(src) {
				contentStart = len(src)
			}
			page.src = string(src[contentStart:])

			// parse the YAML data
			err = yaml.Unmarshal(yamlBytes, &page)
			if err != nil {
				log.Fatalf("Failed to parse YAML front matter from '%s': %s\n", fpath, err)
			}

			// convert pubdate -> date, which is required to be RFC3339 format
			page.Date, err = time.Parse(time.RFC3339, page.PubDate)
			if err != nil {
				log.Fatalf("Parsing of date '%s' in file '%s' failed:\n\t%s\n", page.PubDate, fpath, err)
			}
		}

		fparts := []string{page.Id}
		// markdown will get rendered to HTML, everything goes as-is
		if ext == ".md" || ext == ".sh" {
			fparts = append(fparts, ".html")
		} else {
			fparts = append(fparts, ext)
		}

		page.SrcPath = fpath
		page.SrcRel = path.Join(subpath, fname) // will include leading /
		if page.PubRel == "" {
			page.PubRel = path.Join(subpath, strings.Join(fparts, ""))
		}
		page.PubFull = fmt.Sprintf("%s%s", c.BaseURL.String(), page.PubRel)
		page.PubPath = path.Join(c.PubRoot, subpath, strings.Join(fparts, ""))
		page.Dir = strings.Trim(subpath, "/")
		if page.Dir == "" {
			page.Dir = "/"
		}

		// don't index files in /pages/ by default
		if subpath == "/pages/" {
			page.AutoIdx = false
		}

		pages = append(pages, page)

		return nil
	}

	dir := path.Join(c.SrcRoot, c.PageDir)
	err := filepath.Walk(dir, visitor)
	if err != nil {
		log.Fatalf("Could not load page source in '%s': %s", dir, err)
	}

	return pages
}

// pass custom flags to the blackfriday md->html renderer
func markdown(input []byte) []byte {
	flags := 0
	flags |= blackfriday.HTML_USE_XHTML
	r := blackfriday.HtmlRenderer(flags, "", "")

	ext := 0
	ext |= blackfriday.EXTENSION_NO_INTRA_EMPHASIS
	ext |= blackfriday.EXTENSION_TABLES
	ext |= blackfriday.EXTENSION_SPACE_HEADERS
	ext |= blackfriday.EXTENSION_FOOTNOTES
	ext |= blackfriday.EXTENSION_FENCED_CODE
	ext |= blackfriday.EXTENSION_STRIKETHROUGH
	ext |= blackfriday.EXTENSION_AUTOLINK

	return blackfriday.Markdown(input, r, ext)
}
