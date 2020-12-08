const db = require("./db");

const gameCodeSize = 2 ** 24;

async function getOrCreateVm({ region }) {
  // list vms in region

  // check if max vm count reached

  // start vm

  const vm = await db.addVm({ region });

  return vm;
}

async function startGame({ region }) {
  const gameCode = Math.round(Math.random() * gameCodeSize)
    .toString(16)
    .toUpperCase();

  const vm = await getOrCreateVm({ region });

  // start game

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
