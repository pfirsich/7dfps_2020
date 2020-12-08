require("dotenv").config();

const express = require("express");

const morgan = require("morgan");
const manager = require("./manager");
const db = require("./db");
const regions = require("./regions");

const app = express();

// http logging
app.use(morgan("tiny"));

app.use(express.json());

const port = parseInt(process.env.PORT || "3000", 10);

app.get("/", (req, res) => {
  res.json({
    msg: "Hi!",
  });
});

app.get("/regions", (req, res) => {
  res.json(regions);
});

app.post("/games", async (req, res) => {
  let { region } = req.body;

  if (!region) {
    res.status(400).json({ msg: "Invalid JSON" });
    return;
  }

  if (!regions[region]) {
    res.status(400).json({ msg: "Invalid region" });
    return;
  }

  try {
    const gameInfo = await manager.startGame({ region });
    console.log("Created Game:", gameInfo);

    res.json({
      gameCode: gameInfo.gameCode,
      host: gameInfo.host,
      port: gameInfo.port,
    });
  } catch (error) {
    console.error(error);

    if (error.full) {
      res.status(503).json({
        msg: "All servers full",
      });
    } else {
      res.status(500).json({
        msg: "Internal server error",
      });
    }
  }
});

app.get("/games/:gameCode", async (req, res) => {
  const { gameCode } = req.params;

  try {
    const gameInfo = await db.getGame(gameCode);

    if (!gameInfo) {
      res.status(404).json({
        msg: "Invalid game code",
      });
    } else {
      res.json({
        gameCode: gameInfo.gameCode,
        host: gameInfo.host,
        port: gameInfo.port,
      });
    }
  } catch (error) {
    console.error(error);

    res.status(500).json({
      msg: "Internal server error",
    });
  }
});

app.listen(port, () => {
  console.log(`Listening at http://localhost:${port}`);
});
