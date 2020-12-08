const { Pool } = require("pg");

const pool = new Pool({
  host: process.env.DB_HOST,
  port: process.env.DB_PORT,
  user: process.env.DB_USER,
  password: process.env.DB_PASSWORD,
  database: process.env.DB_NAME,
});

pool.on("error", (err, client) => {
  console.error("Unexpected error on idle client", err);
  console.error("Exiting for good messure");
  process.exit(1);
});

function query(...args) {
  return new Promise((resolve, reject) => {
    pool.query(...args, (err, res) => {
      if (err) {
        reject(err);
        return;
      }
      resolve(res);
    });
  });
}

async function countVms() {
  const res = await query("SELECT COUNT(*) as vmCount FROM vms");

  return res.rows[0].vmcount;
}

async function getFreeVms({ region, maxGamesOnVm }) {
  // Sort descending to always fill VMs first
  const res = await query(
    `SELECT vms.vmid, vms.region, COUNT(games.gamecode) AS gamesCount
        FROM vms
        JOIN games ON vms.vmId=games.vmId
        WHERE region = $1
        GROUP BY vms.vmid
        HAVING COUNT(games.gamecode) < $2
        ORDER BY gamesCount DESC`,
    [region, maxGamesOnVm]
  );

  return res.rows.map((row) => ({
    vmId: row.vmid,
    region: row.region,
    gamesCount: Number(row.gamescount),
  }));
}

async function addVm({ region }) {
  const res = await query(
    "INSERT INTO vms(region) VALUES($1) RETURNING vmId, region",
    [region]
  );

  const [row] = res.rows;

  return {
    vmId: row.vmid,
    region: row.region,
  };
}

async function getGame(gameCode) {
  const res = await query("SELECT * FROM games WHERE gameCode = $1", [
    gameCode,
  ]);

  if (res.rows.length === 0) {
    return null;
  }

  const [row] = res.rows;

  return {
    gameCode: row.gamecode,
    host: row.host,
    port: row.port,
    vmId: row.vmid,
  };
}

async function addGame({ gameCode, host, port, vmId }) {
  const res = await query(
    "INSERT INTO games(gameCode, host, port, vmId) VALUES($1, $2, $3, $4) RETURNING gameCode, host, port, vmId",
    [gameCode, host, port, vmId]
  );

  const [row] = res.rows;

  return {
    gameCode: row.gamecode,
    host: row.host,
    port: row.port,
    vmId: row.vmid,
  };
}

module.exports = {
  countVms,
  getFreeVms,
  addVm,
  getGame,
  addGame,
};
