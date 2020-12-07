const express = require("express");
const morgan = require("morgan");
const manager = require("./manager");
const db = require("./db");

const app = express();

// http logging
app.use(morgan("tiny"));

const port = parseInt(process.env.PORT || "3000", 10);

app.get("/", (req, res) => {
  res.json({
    msg: "Hi!",
  });
});

app.get("/regions", (req, res) => {
  res.json({
    nyc1: "speedtest-nyc1.digitalocean.com",
    nyc2: "speedtest-nyc2.digitalocean.com",
    nyc3: "speedtest-nyc3.digitalocean.com",
    tor1: "speedtest-tor1.digitalocean.com",
    ams2: "speedtest-ams2.digitalocean.com",
    ams3: "speedtest-ams3.digitalocean.com",
    sfo1: "speedtest-sfo1.digitalocean.com",
    sfo2: "speedtest-sfo2.digitalocean.com",
    sgp1: "speedtest-sgp1.digitalocean.com",
    lon1: "speedtest-lon1.digitalocean.com",
    fra1: "speedtest-fra1.digitalocean.com",
    blr1: "speedtest-blr1.digitalocean.com",
  });
});

app.post("/games", async (req, res) => {
  try {
    const gameInfo = await manager.startGame();
    res.json(gameInfo);
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
      res.json(gameInfo);
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
