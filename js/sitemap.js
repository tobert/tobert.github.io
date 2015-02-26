(function() {
    // for collecting the nodes from the json page data
    var nodes = {
        "/": [],
        "tldr": [],
        "post": [],
        "pages": []
    };

    // in the d3 tree format
    var d3nodes = {
        "name": "/",
        "parent": null,
        "children": [],
        "depth": 0
    };

    d3.json("/pages.json", function(err, jsondata) { // ends at end of file basically - TODO: fix indenting
        jsondata.forEach(function(d) {
            var parts = d.PubRel.split(/\/+/);

            // only 2 levels are handled at the moment
            if (parts.length > 2) {
                // pages, tldrs, and posts
                nodes[parts[1]].push({
                    "name": d.Id,
                    "parent": parts[1],
                    "children": null,
                    "depth": 2
                });
            } else {
                // index.html etc.
                nodes["/"].push({
                    "name": d.Id,
                    "parent": "/",
                    "children": null,
                    "depth": 1
                });
            }
        });

        d3.keys(nodes).forEach(function(key) {
            d3nodes["children"].push({
                "name": key,
                "parent": "/",
                "children": nodes[key],
                "depth": 2
            });
        });
        console.log(d3nodes);

        // ************** Generate the tree diagram  *****************
        var margin = {
            top: 20,
            right: 120,
            bottom: 20,
            left: 120
        };
        var width = 960 - margin.right - margin.left;
        var height = 500 - margin.top - margin.bottom;
        var i = 0; // for generating ids

        var tree = d3.layout.tree()
            .size([height, width]);

        var diagonal = d3.svg.diagonal()
            .projection(function(d) {
                return [d.y, d.x];
            });

        var svg = d3.select("#sitemap-container").append("svg")
            .attr("width", width + margin.right + margin.left)
            .attr("height", height + margin.top + margin.bottom)
            .append("g")
            .attr("transform", "translate(" + margin.left + "," + margin.top + ")");

        function update(source) {

            // Compute the new tree layout.
            var nodes = tree.nodes(source).reverse(),
                links = tree.links(nodes);

            // Normalize for fixed-depth.
            nodes.forEach(function(d) {
                d.y = d.depth * 180;
            });

            // Declare the nodes.
            var node = svg.selectAll("g.node")
                .data(nodes, function(d) {
                    return d.id || (d.id = ++i);
                });

            // Enter the nodes.
            var nodeEnter = node.enter().append("g")
                .attr("class", "node")
                .attr("transform", function(d) {
                    return "translate(" + d.y + "," + d.x + ")";
                });

            nodeEnter.append("circle")
                .attr("r", 10)
                .style("fill", "#fff");

            nodeEnter.append("text")
                .attr("x", function(d) {
                    return d.children || d._children ? -13 : 13;
                })
                .attr("dy", ".35em")
                .attr("text-anchor", function(d) {
                    return d.children || d._children ? "end" : "start";
                })
                .text(function(d) {
                    return d.name;
                })
                .style("fill-opacity", 1);

            // Declare the linksâ€¦
            var link = svg.selectAll("path.link")
                .data(links, function(d) {
                    return d.target.id;
                });

            // Enter the links.
            link.enter().insert("path", "g")
                .attr("class", "link")
                .attr("d", diagonal);
        }

        update(d3nodes);
    });
})();
