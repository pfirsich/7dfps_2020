const db = require("./db");
const config = require("./config");
const DigitalOcean = require("do-wrapper").default;

const doClient = new DigitalOcean(process.env.DO_ACCESS_TOKEN);

async function startVm({ region }) {
  const vm = await db.addVm({ region });

  const response = await doClient.droplets.create({
    name: "7dfps-game-vm-" + vm.vmId,
    region,
    size: config.vmSize,
    image: config.vmImage,
    ssh_keys: config.vmSshKeys,
    backups: false,
    ipv6: true,
    user_data: `echo "Hi, I'm a game server, VM ID: ${vm.vmId}"`,
    monitoring: true,
    volumes: null,
    tags: [...config.vmTags, `vmId:${vm.vmId}`],
  });

  console.log(response);

  // response.droplet.name
  // response.droplet.id
  // response.droplet.created_at

  console.log(response.links.actions);

  const { droplet } = await doClient.droplets.getById(response.droplet.id);

  const publicNetwork = droplet.networks.v4.find((n) => n.type === "public");
  const ipv4Address = publicNetwork.ip_address;

  console.log(droplet.networks);

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
  const gameCode = Math.round(Math.random() * config.gameCodeSize)
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
