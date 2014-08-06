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
 * F7U12: A simple implementation of the 2048 game using D3.
 * Uses performance.now() for timings, might break on older browsers.
 */

// constructor: takes a width
var F7U12 = function (width) {
  this.visible = false;
  this.width = width;
  this.size = width * width;
  this.last = this.size - 1;
  this.sequence = 0;
  this.score = 0;
  this.cells = new Array(this.size);
  this.started = performance.now();
  this.last_turn = this.started;

  for (var i=0; i<this.size; i++) {
    this.cells[i] = 0;
  }
};

F7U12.cell_class = function (d,i) {
  if (d === 0) {
    return "f7u12-cell f7u12-cell-empty";
  } else {
    return "f7u12-cell f7u12-cell-" + d;
  }
};

// used internally in a few .text() calls below
F7U12.print = function (d) {
  if (d > 0) {
    return d;
  } else {
    return "";
  }
};

// makes a copy of the object
F7U12.prototype.clone = function () {
  var out = new F7U12(this.width);
  this.cells.forEach(function (d,i) {
    out.cells[i] = d;
  });
  return out;
};

F7U12.prototype.clear = function (target) {
  d3.select(target).selectAll(".f7u12-grid").remove();
};

// render the game grid
F7U12.prototype.render = function (target) {
  var game = this;
      game.visible = true;

  var pagediv = d3.select(target);
  // not safe across multiple calls ... does it matter?
  game.container = pagediv.append("div").classed("f7u12-grid", true);

  game.cell_divs = game.container.selectAll(".f7u12-cell")
    .data(game.cells)
    .enter()
    .append("div")
      .attr("class", F7U12.cell_class)
      .attr("data-id", function (d,i) { return i; })
      .attr("style", function (d,i) {
         if (i % game.width == 0) { return "clear: both;" }
      })
    .text(F7U12.print);
};

// assuming the grid has been changed on the object with
// game.grid = [...], update the board with those new values
// TODO: d3 transitions
F7U12.prototype.update = function () {
  // cells are only ever updated, never added or removed
  // there is no need to call enter() / exit()
  var game = this;

  if (game.visible) {
    game.cell_divs
      .data(game.cells)
      .attr("classed", F7U12.cell_class)
      .text(F7U12.print);
  }
};

// calculate the from/to coordinates for use in slide() & merge()
// eol is set to true on pairs that wrap around the grid
F7U12.prototype.next = function (dir, i) {
    var out = { from: 0, to: 0, eol: false };

    // DOWN:
    // the cell below i is width cells forward
    if (dir === "down") {
      out.from = i;
      out.to = i + this.width;
      if (out.to >= this.size) {
        out.eol = true;
      }
    }

    // UP:
    // start on the bottom right
    // the cell above from is width cells backward
    else if (dir === "up") {
      out.from = this.last - i;
      out.to = out.from - this.width;
      if (out.to < 0) {
        out.eol = true;
      }
    }

    // RIGHT:
    // the cell to the right is forward one cell
    else if (dir === "right") {
      out.from = i;
      out.to = i + 1;

      if (out.to % this.width === 0) {
        out.eol = true;
      }
    }

    // LEFT:
    // start at the bottom right
    // the cell to the left is backwards one cell
    else if (dir === "left") {
      out.from = this.last - i;
      out.to = out.from - 1;

      if (out.from % this.width === 0) {
        out.eol = true;
      }
    }

    // should never happen normally, most likely a typo in input wiring
    else {
      console.log("BUG: invalid direction '" + dir + "'.");
    }

    return out;
};

// move populated tiles into unpopulated tiles in the given direction
// calls itself recursively until there are no more moves
// return the number of slides completed
F7U12.prototype.slide = function (dir, prev_count) {
  var count = prev_count || 0;
  var game = this;
  var dirty = false;

  var updated = game.cells.map(function (val, i) {
    var idxs = game.next(dir, i);

    if (idxs.eol) {
      return val;
    }

    if (game.cells[idxs.to] == 0 && game.cells[idxs.from] > 0) {
      game.cells[idxs.to] = game.cells[idxs.from];
      game.cells[idxs.from] = 0;
      count++;
      dirty = true;
    }

    return game.cells[i];
  });

  game.update();

  if (dirty) {
    game.slide(dir, count);
  }

  return count;
};

// merge cells with matching numbers
// returns a list of indices of cells merged
F7U12.prototype.merge = function (dir) {
  var game = this;
  var score = 0;

  // track which cells have been updated and only merge them once per move
  var merged = new Array(game.size);
  merged.forEach(function (v,i) { merged[i] = 0; });

  var updated = game.cells.map(function (val, i) {
    var idxs = game.next(dir, i);

    if (idxs.eol) {
      return val;
    }

    if (game.cells[idxs.from] == game.cells[idxs.to] && !merged[idxs.from]) {
      game.cells[idxs.from] = 0;
      game.cells[idxs.to] = game.cells[idxs.to] * 2;
      score += game.cells[idxs.to];
      merged[idxs.to] = game.cells[idxs.to]; // return the combined value
    }

    return game.cells[i];
  });

  game.score += score;

  game.update();

  return merged.filter(function (val) { if (val > 0) { return true; } else { return false; } });
};

// collapse tiles, move, collapse again
// returns true if the board changed, false otherwise
// does NOT insert a new piece, you must call insert()
F7U12.prototype.move = function (dir) {
  var game = this;

  var slides = game.slide(dir);
  var merged = game.merge(dir);
  slides += game.slide(dir);

  if (slides > 0 || merged.length > 0) {
    game.sequence++;

    // update game metrics
    var now = performance.now();
    game.turn_ms = now - game.last_turn;
    game.offset_ms = now - game.started;
    game.last_turn = now;
    game.latest_dir = dir;

    return true;
  }

  return false;
};

// returns true if moving <dir> will change the board
F7U12.prototype.can_move = function (dir) {
  // make a clone of the game and throw it away
  var game = this.clone();

  // same code as move() but without the metrics
  var slides = game.slide(dir);
  var merged = game.merge(dir);
  slides += game.slide(dir);

  if (slides > 0 || merged.length > 0) {
    return true;
  }

  return false;
};

// probably slow, esp on bigger boards, but good enough for now
F7U12.prototype.over = function () {
  var game = this;

  if (game.can_move("up")) {
     return false;
  } else if (game.can_move("down")) {
     return false;
  } else if (game.can_move("left")) {
     return false;
  } else if (game.can_move("right")) {
     return false;
  } else {
     return true;
  }
};

// returns the number of empty cells on the board
F7U12.prototype.empty_cells = function () {
  return this.cells.reduce(function (pv, cv) {
      if (cv == 0) {
          return pv + 1;
      } else {
          return pv;
      }
  });
};

// randomly insert a 2 or 4 on the grid
F7U12.prototype.insert = function () {
  var game = this;

  // make a list of all empty cells' (value = 0) indexes
  var available = game.cells.map(function (val, i) {
    if (val == 0) {
      return i;
    }
  }).filter(function (val) { return val != undefined; });

  // shuffle the empty index list
  d3.shuffle(available);
  // take the first value and assign 2 or 4 at random
  game.cells[available[0]] = d3.shuffle([2,4])[0];

  // display the new grid (if visible)
  game.update();

  // serialize() saves these values, handy for replays, etc.
  game.latest_tile_idx = available[0];
  game.latest_tile_value = game.cells[available[0]];

  // return the index of the inserted tile
  return available[0];
};

// set the name of the game / player, an arbitrary string
F7U12.prototype.set_name = function (name) {
  this.name = name;
};

// grabs the latest game state stored in the game object and returns JSON
// since F7U12 doesn't implement score directly, it will always
// be 0 unless you set it.
F7U12.prototype.serialize = function () {
  var game = this;

  var out = {
    "game_id":   game.uuid || "00000000-0000-0000-0000-000000000000",
    "turn_id":   game.sequence,
    "offset_ms": game.offset_ms,
    "turn_ms":   game.turn_ms,
    "player":    game.name,
    "score":     game.score,
    "tile_val":  game.latest_tile_value,
    "tile_idx":  game.latest_tile_idx,
    "dir":       game.latest_dir,
    "state":     game.cells
  };

  return JSON.stringify(out);
};

// render the board with count values on it
// returns the indexes of the tiles inserted
F7U12.prototype.init = function (count) {
  var tiles = [];
  for (var i=0; i<count; i++) {
    tiles.push(this.insert());
  }

  if (this.visible) {
    this.update();
  }

  return tiles;
};
