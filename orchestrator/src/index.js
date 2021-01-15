require("dotenv").config();

const path = require("path");
const express = require("express");
const morgan = require("morgan");

const manager = require("./manager");
const geo = require("./geo");
const config = require("./config");

const app = express();

function getClientIp(req) {
  return (
    (req.headers["x-forwarded-for"] || "").split(",").pop().trim() ||
    req.connection.remoteAddress ||
    req.socket.remoteAddress ||
    req.connection.socket.remoteAddress
  );
}

// http logging
app.use(morgan("tiny"));

app.use(express.json());

app.use("/", express.static(path.join(__dirname, "public")));

app.get("/regions", async (req, res) => {
  try {
    const ipAddress = getClientIp(req);
    const regions = await geo.getRegions(ipAddress);

    res.json(regions);
  } catch (error) {
    console.error("Error while getting regions", error);

    res.status(500).json({
      msg: "Internal server error",
    });
  }
});

app.post("/games", async (req, res) => {
  const { region } = req.body;
  const version = req.body.version || "latest";

  if (!geo.isValidRegion(region)) {
    res.status(400).json({ msg: "Invalid region" });
    return;
  }

  if (!config.validVersion.includes(version)) {
    res.status(400).json({ msg: "Invalid version" });
    return;
  }

  try {
    const gameInfo = await manager.startGame({
      region,
      creatorIpAddress: getClientIp(req),
      version,
    });

    console.log("Created Game:", gameInfo);

    res.json({
      gameCode: gameInfo.gameCode,
      host: gameInfo.host,
      port: gameInfo.port,
      version: gameInfo.version,
      timeStarted: gameInfo.timeStarted,
    });
  } catch (error) {
    console.error("Error in create game request", error);

    if (error.full) {
      res.status(503).json({
        msg: "All servers full",
      });
    } else {
      res.status(500).json({
        msg: `Internal server error: ${error.message}`,
      });
    }
  }
});

app.get("/games/:gameCode", async (req, res) => {
  const { gameCode } = req.params;

  try {
    const gameInfo = await manager.lookUpGame(gameCode);

    if (!gameInfo) {
      res.status(404).json({
        msg: "Invalid game code",
      });
    } else if (gameInfo.state === "OVER") {
      res.status(404).json({
        msg: "Game already over",
      });
    } else {
      res.json({
        gameCode: gameInfo.gameCode,
        host: gameInfo.host,
        port: gameInfo.port,
        version: gameInfo.version,
        timeStarted: gameInfo.timeStarted,
      });
    }
  } catch (error) {
    console.error("Error while getting game:", error);

    res.status(500).json({
      msg: "Internal server error",
    });
  }
});

async function init() {
  // Create clean state
  await manager.checkTimeoutedGames();
  await manager.checkVms();

  // Start tick to check timeouts
  setInterval(async () => {
    try {
      await manager.checkTimeoutedGames();
      await manager.checkVms();
    } catch (error) {
      console.error("Error while checking for timeouted games:", error);
    }
  }, 5 * 60 * 1000);

  const port = parseInt(process.env.PORT || "3000", 10);
  // Start server
  app.listen(port, () => {
    console.log(`Listening at http://localhost:${port}`);
  });
}

init().catch((error) => console.error("Error in init:", error));
