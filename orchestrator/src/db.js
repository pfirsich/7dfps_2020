const { Pool } = require("pg");

const pool = new Pool({
  host: process.env.DB_HOST,
  port: process.env.DB_PORT,
  user: process.env.DB_USER,
  password: process.env.DB_PASSWORD,
  database: process.env.DB_NAME,
});

// TODO: Revisit: Kill timouted games
// TODO: Adjust: Min VM runtime 45 min (we are always charged for the hour so what)

pool.on("error", (err, client) => {
  console.error("Unexpected error on idle client", err, client);
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

function vmRowToObject(row) {
  return {
    vmId: row.vmid,
    region: row.region,
    state: row.state,
    ipv4Address: row.ipv4address,
    dropletId: row.dropletid,
    timeStarted: row.timestarted,
    timeTerminated: row.timeterminated,

    // Optional:
    gamesCount: row.gamescount ? Number(row.gamescount) : null,
  };
}

function gameRowToObject(row) {
  return {
    gameId: row.gameid,
    gameCode: row.gamecode,
    creatorIpAddress: row.creatoripaddress,
    host: row.host,
    port: row.port,
    vmId: row.vmid,
    state: row.state,
    version: row.version,
    timeStarted: row.timestarted,
    timeEnded: row.timeended,
  };
}

async function countVms() {
  const res = await query(
    "SELECT COUNT(*) as vmCount FROM vms WHERE state != 'TERMINATED'"
  );

  return Number(res.rows[0].vmcount);
}

async function getFreeVms({ region, maxGamesOnVm }) {
  // Sort descending to always fill VMs first
  const res = await query(
    `SELECT vms.*, COUNT(games.gamecode) AS gamesCount
        FROM vms
        LEFT JOIN (SELECT * FROM games WHERE games.state != 'OVER') as games ON vms.vmId=games.vmId
        WHERE region = $1 AND vms.state = 'RUNNING'
        GROUP BY vms.vmid
        HAVING COUNT(games.gamecode) < $2
        ORDER BY gamesCount DESC`,
    [region, maxGamesOnVm]
  );

  return res.rows.map(vmRowToObject);
}

async function getEmptyVms() {
  // Get VMs that are RUNNING and where no games exits which are still
  // running or just ended less than 20 minutes ago.
  // Excuse my subselects.
  const res = await query(
    `SELECT *
     FROM vms
     WHERE vms.state = 'RUNNING'
         AND NOT EXISTS (
             SELECT vmId
             FROM games
             WHERE games.vmId = vms.vmId
                 AND (games.state != 'OVER'
                     OR (games.state = 'OVER'
                         AND games.timeEnded > NOW() - INTERVAL '1 minutes')))`
  );

  return res.rows.map(vmRowToObject);
}

// TODO: Only set update fields
// current implemtation is a consistency foot gun
async function setVm({
  vmId,
  region,
  state,
  ipv4Address,
  dropletId,
  timeStarted,
  timeTerminated,
}) {
  let sql;
  let values = [
    region,
    state,
    ipv4Address,
    dropletId,
    timeStarted,
    timeTerminated,
  ];

  if (vmId) {
    values = [vmId, ...values];
    sql = `UPDATE vms
               SET region = $2,
                   state = $3,
                   ipv4Address = $4,
                   dropletId = $5,
                   timeStarted = $6,
                   timeTerminated = $7
               WHERE vmId = $1
               RETURNING vmId, region, state, ipv4Address, dropletId, timeStarted, timeTerminated`;
  } else {
    sql = `INSERT INTO vms(region, state, ipv4Address, dropletId, timeStarted, timeTerminated)
               VALUES($1, $2, $3, $4, $5, $6)
               RETURNING vmId, region, state, ipv4Address, dropletId, timeStarted, timeTerminated`;
  }

  const res = await query(sql, values);

  return vmRowToObject(res.rows[0]);
}

async function getGameByGameCode(gameCode) {
  const res = await query(
    "SELECT * FROM games WHERE gameCode = $1 ORDER BY timeStarted DESC",
    [gameCode]
  );

  if (res.rows.length === 0) {
    return null;
  }

  return gameRowToObject(res.rows[0]);
}

async function getGameByGameId(gameId) {
  const res = await query(
    "SELECT * FROM games WHERE gameId = $1 ORDER BY timeStarted DESC",
    [gameId]
  );

  if (res.rows.length === 0) {
    return null;
  }

  return gameRowToObject(res.rows[0]);
}

async function getTimeoutedGames() {
  const res = await query(
    `SELECT * FROM games WHERE state != 'OVER' AND timeStarted < NOW() - INTERVAL '2 hours'`
  );

  return res.rows.map(gameRowToObject);
}

async function setGame({
  gameId,
  gameCode,
  creatorIpAddress,
  host,
  port,
  vmId,
  state,
  version,
  timeStarted,
  timeEnded,
}) {
  let sql;

  let values = [
    gameCode,
    creatorIpAddress,
    host,
    port,
    vmId,
    state,
    version,
    timeStarted,
    timeEnded,
  ];

  if (gameId) {
    values = [gameId, ...values];
    sql = `UPDATE games
               SET gameCode = $2,
                   creatorIpAddress = $3,
                   host = $4,
                   port = $5,
                   vmId = $6,
                   state = $7,
                   version = $8,
                   timeStarted = $9,
                   timeEnded = $10
               WHERE gameId = $1
               RETURNING gameId, gameCode, creatorIpAddress, host, port, vmId, state, version, timeStarted, timeEnded`;
  } else {
    sql = `INSERT INTO games(gameCode, creatorIpAddress, host, port, vmId, state, version, timeStarted, timeEnded)
               VALUES($1, $2, $3, $4, $5, $6, $7, $8, $9)
               RETURNING gameId, gameCode, creatorIpAddress, host, port, vmId, state, version, timeStarted, timeEnded`;
  }

  const res = await query(sql, values);

  return gameRowToObject(res.rows[0]);
}

module.exports = {
  countVms,
  getFreeVms,
  getEmptyVms,
  setVm,
  getGameByGameCode,
  getGameByGameId,
  getTimeoutedGames,
  setGame,
};
