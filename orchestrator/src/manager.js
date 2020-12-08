const db = require("./db");
const config = require("./config");

const gameCodeSize = 2 ** 24;

const DigitalOcean = require("do-wrapper").default;
const doClient = new DigitalOcean(process.env.DO_ACCESS_TOKEN);

async function startVm({ region }) {
  const vm = await db.addVm({ region });

  // TODO: call do

  return vm;
}

// TODO: this should all be one transaction....
async function getOrCreateVm({ region }) {
  const freeVms = await db.getFreeVms({
    region,
    maxGamesOnVm: config.maxGamesOnVm,
  });

  if (freeVms.length > 0) {
    return freeVms[0];
  }

  const vmCount = db.countVms();

  if (vmCount >= config.maxVms) {
    const error = new Error("Max VM count reached");
    error.full = true;
    throw error;
  }

  return startVm({ region });
}

async function startGame({ region }) {
  const gameCode = Math.round(Math.random() * gameCodeSize)
    .toString(16)
    .toUpperCase();

  const vm = await getOrCreateVm({ region });

  // TODO: start game

  const gameInfo = await db.addGame({
    gameCode,
    host: "foo.example.com",
    port: Math.floor(Math.random() * 9000) + 1000,
    vmId: vm.vmId,
  });

  return gameInfo;
}

module.exports = {
  startGame,
};
