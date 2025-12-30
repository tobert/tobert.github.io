# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Overview

This is a bespoke static blog generator written in Go. It processes Markdown and HTML source files with YAML front matter into a static website hosted on GitHub Pages.

## Build Commands

```bash
# Generate the site (one-time build)
go run build-blog.go

# Build and serve locally with auto-regeneration on each request
go run build-blog.go -serve -port 8080 -domain localhost

# Local development with file watching (requires inotify-tools)
./dev.sh
```

### Build Flags

- `-domain` - Domain for generated links (default: tobert.github.io)
- `-port` - HTTP port for URLs (default: 80)
- `-src` - Source directory path
- `-pub` - Output directory path
- `-serve` - Run local server that regenerates on each request
- `-force-idx` - Force all pages into the automatic index

## Content Architecture

### Directory Structure

- `src/post/` - Blog posts with full YAML front matter (included in auto-index)
- `src/pages/` - Static pages (excluded from auto-index by default)
- `src/tldr/` - Short posts with minimal front matter (date prefix in filename)
- `snippets/` - HTML template fragments (header, footer, containers)

### Front Matter Format

Regular posts require YAML front matter:

```yaml
---
id: "2024-01-01-post-slug"
title: "Post Title"
abstract: "Brief description for RSS"
tags: ["tag1", "tag2"]
pubdate: 2024-01-01T00:00:00Z
---
```

TL;DR posts use filename-based metadata: `src/tldr/YYYY-MM-DD-title-words.md` - the date is extracted from the filename prefix and the title is derived from the remaining hyphenated words.

### Template System

Pages are Go `text/template` files. Available template data:
- `.Page` - Current page metadata
- `.Pages` - All auto-indexed pages (sorted by date, newest first)
- `.Config` - Site configuration
- `.Snippets` - Loaded snippet templates
- `.TagIndex` - Map of tag → pages

Snippets are automatically loaded and can be referenced by basename (e.g., `header.html` → `header`).

### Supported File Types

- `.md` - Markdown (rendered to HTML via blackfriday)
- `.html` - HTML templates
- `.sh` - Shell scripts (displayed with syntax highlighting)
- `.txt`, `.xml` - Passed through as-is
