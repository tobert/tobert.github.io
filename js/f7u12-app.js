(function () {
  var moves = [];

  var showdata = function (game) {
    moves[game.sequence] = game.serialize();
    var data = moves.slice(-10).reverse();
    console.log(game.sequence, data);

    var lines = d3.select("#f7u12-json")
      .selectAll("code")
      .data(data, function (d) { return d; });

    lines.enter()
      .append("code")
      .classed("json", true)
      .text(function (d) { return d; });

    lines.exit().remove();
  };

  var move = function (game, dir) {
    var changed = game.move(dir);
    if (!changed) {
      return;
    }

    var new_tile_idx = game.insert();

    showdata(game);

    d3.select("#score-value").text(game.score);
    d3.select("#turn-id-value").text(game.sequence);

    game.last_turn = performance.now();
  };

  var game = new F7U12(4); // 4x4 grid
      game.init(2); // start with 2 tiles
      game.render("#f7u12-container");
      game.make_dpad("#f7u12-dpad");
      game.score = 0;
      game.set_name("Player1");

  showdata(game);

  // jquery swipe plugin
  $("#f7u12-container").swipe({ swipe: function(e, direction) { move(game, direction); } });

  // a simple dpad for testing with the mouse
  ["up", "down", "left", "right"].forEach(function (dir) {
    $("#f7u12-dpad #" + dir).on("click", function () { move(game, dir); });
  });

  // arrow key input
  document.onkeydown = function() {
    switch (window.event.keyCode) {
      case 37: move(game, "left"); break;
      case 38: move(game, "up"); break;
      case 39: move(game, "right"); break;
      case 40: move(game, "down"); break;
    }
  };

})();
