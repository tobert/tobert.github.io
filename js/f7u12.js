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
 */

/**
 * @fileoverview A simple implementation of the 2048 Depends on d3.js.
 * Also assumes performance.now() is present, which requires a polyfill on Safari.
 * @author atobey@datastax.com (Al Tobey)
 */

/**
 * Creates a new game of 2048.
 * @param {Number} width The width of the grid in cells.
 * @property {Boolean} visible
 * @property {Number} sequence
 * @property {Number} score
 * @property {Array.<Number>} cells values on the grid
 * @constructor
 */
var F7U12 = function (width) {
  // mutable properties
  this.visible = false;
  this.sequence = 0;
  this.score = 0;

  // should not change again after initialization
  this.width = width;
  this.size = width * width;
  this.last = this.size - 1;
  this.started = performance.now();
  this.last_turn = this.started;

  /* initialize all cells to 0 */
  this.cells = new Array(this.size);
  for (var i=0; i<this.size; i++) {
    this.cells[i] = 0;
  }
};

/**
 * Places starting tiles on the board and renders it if this.visibility = true.
 * @param {Number} count number of tiles to place on the board
 * @return {Array.<Number>} indexes of the tiles inserted
 */
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

/**
 * Makes a new F7U12 object with the same grid state. All other values
 * (score, sequence, etc.) are not copied.
 * @return {F7U12}
 */
F7U12.prototype.clone = function () {
  var out = new F7U12(this.width);
  this.cells.forEach(function (d,i) {
    out.cells[i] = d;
  });
  return out;
};

/**
 * Remove an f7u12 grid from the target DOM element.
 * @param {String} CSS selector for the parent DOM element
 */
F7U12.prototype.clear = function (target) {
  d3.select(target).selectAll(".f7u12-grid").remove();
};

/**
 * Append the current f7u12 game grid in the target DOM element.
 * Target DOM elements should be empty. Calling multiple times will append
 * grids, but not replace/overwrite them. Sets game.visible = true.
 * @param {String} CSS selector for the parent DOM element
 */
F7U12.prototype.render = function (target) {
  var game = this;

  // TODO: this is probably an unnecessary side-effect and should be removed
  game.visible = true;

  var pagediv = d3.select(target);
  // multiple calls to render() on the same target will cause additional
  // grids to be rendered
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

/**
 * Update a grid in-place. Modifies the DOM unless {@code this.visible = false}.
 */
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

/**
 * Calculate the from/to coordinates for use in slide() & merge()
 * Probably only useful for internal use by merge() and slide().
 * @param {String} dir up/down/left/right
 * @param {Number} i index of the cell in {@code this.cells} array
 * @return {Number} index of the next cell in the given direction, -1 if at end of col/row
 */
F7U12.prototype.next = function (dir, i) {
    var next = -1;

    // DOWN:
    // the cell below i is width cells forward
    if (dir === "down") {
      next = i + this.width;
      if (next >= this.size) {
        return -1;
      }
    }

    // UP:
    // the cell above i is width cells back
    else if (dir === "up") {
      next = i - this.width;
      if (next < 0) {
        return -1;
      }
    }

    // RIGHT:
    // the cell to the right is forward one cell
    else if (dir === "right") {
      next = i + 1;

      if (next % this.width === 0) {
        return -1;
      }
    }

    // LEFT:
    // the cell to the left is backwards one cell
    else if (dir === "left") {
      next = i - 1;

      if (i % this.width === 0) {
        return -1;
      }
    }

    // should never happen normally, most likely a typo in input wiring
    else {
      console.log("BUG: invalid direction '" + dir + "'.");
    }

    return next;
};

/**
 * Moves tiles in the requested direction.
 * Does not merge tiles. Does not modify DOM. Game state is updated.
 * @param {String} dir up/down/left/right
 * @return {Number} the number of cells moved
 */
F7U12.prototype.slide = function (dir) {
  return this._slide(dir, 0);
};

/**
 * Slide tiles in the requested direction. Recurses until there are no more moves.
 * @see slide
 * @param {String} dir up/down/left/right
 * @param {Number} count number of cells moved
 * @return {Number} the number of cells moved
 *
 * Compares values in the given direction using this.next to walk rows or
 * columns as necessary.
 */
F7U12.prototype._slide = function (dir, count) {
  var game = this;
  var dirty = false;

  var updated = game.cells.map(function (val, i) {
    var next = game.next(dir, i);

    // negative values mean end of row/column, nothing to do
    if (next < 0) {
      return val;
    }

    if (game.cells[next] == 0 && game.cells[i] > 0) {
      game.cells[next] = game.cells[i];
      game.cells[i] = 0;
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

/**
 * Merge adjacent cells with the same value in the direction specified. Does
 * not modify DOM. Game state is updated.
 * @param {String} dir up/down/left/right
 * @return {Array.<Number>} list of indices of cells merged
 */
F7U12.prototype.merge = function (dir) {
  var game = this;
  var score = 0;

  // track which cells have been updated and only merge them once per move
  var merged = new Array(game.size);
  merged.forEach(function (v,i) { merged[i] = 0; }); // initialize

  var updated = game.cells.map(function (val, i) {
    var next = game.next(dir, i);

    // negative values mean end of row/column, nothing to do
    if (next < 0) {
      return val;
    }

    if (game.cells[i] == game.cells[next] && !merged[i]) {
      game.cells[next] = game.cells[next] + game.cells[i];
      game.cells[i] = 0;
      score += game.cells[next];
      merged[next] = game.cells[next]; // don't merge cells twice
    }

    return game.cells[i];
  });

  game.score += score;

  game.update();

  return merged.filter(function (val) {
    if (val > 0) {
      return true;
    } else {
      return false;
    }
  });
};

/**
 * Performs slide() and merge() and updates game state. Does not insert
 * the new piece, insert() must be called afterwards if that's what you want.
 * @param {String} dir up/down/left/right
 * @return {Boolean} true if the board changed, false if it didn't
 */
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

/**
 * Similar to move() without side-effects.
 * @nosideeffects
 * @param {String} dir up/down/left/right
 * @return {Boolean} true if the board changed, false if it didn't
 */
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

/**
 * Test whether or not the game is over.
 * @nosideeffects
 * @return {Boolean} true if game is over. false if it's possible to move.
 */
F7U12.prototype.over = function () {
  var game = this;

  // probably slow on big boards, negligible on 4x4 grids
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

/**
 * Counts how many empty cells (value = 0) are present on the board.
 * @nosideeffects
 * @return {Number} count of empty cells on the board
 */
F7U12.prototype.empty_cells = function () {
  return this.cells.reduce(function (pv, cv) {
    if (cv == 0) {
      return pv + 1;
    } else {
      return pv;
    }
  });
};

/**
 * Randomly insert a 2 or 4 on the grid. Not weighted. Writes the new tile to
 * this.cells, updates this.latest_tile_idx and this.latest_tile_value.
 * @return {Number} index of the new tile
 */
F7U12.prototype.insert = function () {
  var game = this;

  var i = game.random_empty_tile();

  // take the first value and assign 2 or 4 at random
  game.cells[i] = d3.shuffle([2,4])[0];

  // display the new grid (if visible)
  game.update();

  // serialize() saves these values, handy for replays, etc.
  game.latest_tile_idx = i;
  game.latest_tile_value = game.cells[i];

  return i;
};

/**
 * Find a random empty tile.
 * @nosideeffects
 * @return {Number} index of an empty (value = 0) tile
 */
F7U12.prototype.random_empty_tile = function () {
  var game = this;

  // make a list of all empty cells' (value = 0) indexes
  var available = game.cells.map(function (val, i) {
    if (val == 0) {
      return i;
    }
  }).filter(function (val) { return val != undefined; });

  // shuffle the empty index list
  d3.shuffle(available);

  // return the index where the tile should be inserted
  return available[0];
};

/**
 * Serializes game state to JSON.
 * @nosideeffects
 * @return {String} json representation of the game state
 * @property game_id   {String} hex UUID, defaults to all zeroes if not set
 * @property turn_id   {Number} number of moves since game start (this.sequence)
 * @property offset_ms {Number} time elapsed since the start of the game
 * @property turn_ms   {Number} time elapsed since the previous move
 * @property player    {String} player name, if set, default of empty string
 * @property score     {Number} the current score
 * @property tile_val  {Number} the value of the tile added most recently
 * @property tile_idx  {Number} the index of the tile added most recently
 * @property dir       {Number} the direction moved most recently
 * @property state     {Array.<Number>} all values currently on the grid
 */
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

/** @ignore this is an internal utility function to generate CSS class names */
F7U12.cell_class = function (d,i) {
  if (d === 0) {
    return "f7u12-cell f7u12-cell-empty";
  } else {
    return "f7u12-cell f7u12-cell-" + d;
  }
};

/** @ignore this is an internal utility function to populate cells with numbers */
F7U12.print = function (d) {
  if (d > 0) {
    return d;
  } else {
    return "";
  }
};

// vim: et ts=2 sw=2 ai smarttab
