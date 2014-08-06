/*
 * Copyright 2014 Albert P. Tobey <atobey@datastax.com> @AlTobey
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * A simple DPAD for the F7U12 version of 2048.
 *
 * requires: D3 3.x
 */

F7U12.dpad_data = [
  [["",     "" ], ["up",   "⇧"], ["",      "" ]],
  [["left", "⇦"], ["",     "" ], ["right", "⇨"]],
  [["",     "" ], ["down", "⇩"], ["",      "" ]]
];

// creates a dpad next to the grid inside the same container
// events can be wired by selecting #up, #down, #left, #right
// or .dpad-up, .dpad-down, .dpad-left, .dpad-right
F7U12.prototype.make_dpad = function(target) {
  var game = this;

  var tbody = d3.select(target)
    .append("table")
    .attr("id", target + "-dpad")
    .attr("class", "dpad");

  var tr = tbody.selectAll("tr")
    .data(F7U12.dpad_data)
    .enter()
    .append("tr");

  var td = tr.selectAll("td")
    .data(function (d) { return d; })
    .enter()
    .append("td")
    .attr("id", function (d) { if (d[0] != "") { return d[0]; } else { return null; } })
    .attr("class", function (d) { if (d[0] != "") { return "arrow dpad-" + d[0]; } })
    .text(function (d) { return d[1]; });
};
