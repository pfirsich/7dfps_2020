const db = require("./db");
const ssh = require("./ssh");
const config = require("./config");
const DigitalOcean = require("do-wrapper").default;

const doClient = new DigitalOcean(process.env.DO_ACCESS_TOKEN);

function randomRange(min, max) {
  return Math.floor(Math.random() * (max - min)) + min;
}

async function sleep(time) {
  return new Promise((resolve) => {
    setTimeout(resolve, time);
  });
}

function initScript(vm) {
  return `
  mkdir -p /var/log/7dfps/;
  mkdir -p /usr/share/7dfps/;

  echo ${vm.vmId} > /usr/share/7dfps/vmId;

  yum install -y nc docker | tee /var/log/7dfps/install.log;
  `;
}

async function waitForAction(actionId) {
  const waitTimeSec = 5;
  const maxTries = 15;
  let tries = 0;

  while (true) {
    tries++;
    if (tries >= maxTries) {
      throw new Error(`Too many tries waiting for action ${tries}`);
    }

    await sleep(waitTimeSec * 1000);

    const { action } = await doClient.actions.getById(actionId);

    if (action.status === "completed") {
      console.log(`Completed action: ${actionId}, tries: ${tries}`);
      return action;
    }

    if (action.status !== "in-progress") {
      return new Error("Failed to create droplet");
    }

    console.log(
      `Waiting for action (${waitTimeSec} sec): ${actionId}, tries: ${tries}`
    );
  }
}

async function startVm({ region }) {
  const vm = await db.addVm({ region });

  const name = "7dfps-game-vm-" + vm.vmId;

  console.log(`Starting droplet: ${name}`);

  const response = await doClient.droplets.create({
    name,
    region,
    size: config.vmSize,
    image: config.vmImage,
    ssh_keys: config.vmSshKeys,
    backups: false,
    ipv6: true,
    user_data: initScript(vm),
    monitoring: true,
    volumes: null,
    tags: [...config.vmTags, `vmId:${vm.vmId}`],
  });

  const actionId = response.links.actions[0].id;

  console.log(`Droplet creation ${name}, Action ID: ${actionId}`);

  await waitForAction(actionId);

  const { droplet } = await doClient.droplets.getById(response.droplet.id);

  const publicNetwork = droplet.networks.v4.find((n) => n.type === "public");
  const ipv4Address = publicNetwork.ip_address;

  console.log("Droplet created with IP:", ipv4Address);

  return {
    ...vm,

    // TODO: Write this to DB
    name: droplet.name,
    id: droplet.id,
    createdAt: droplet.created_at,
    ipv4Address,
  };
}

// TODO: this should all be one transaction....
async function getOrCreateVm({ region }) {
  const freeVms = await db.getFreeVms({
    region,
    maxGamesOnVm: config.maxGamesOnVm,
  });

  if (freeVms.length > 0) {
    console.log(`Reusing vm:${freeVms[0].vmId}`);
    return freeVms[0];
  }

  const vmCount = await db.countVms();

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

  const port = randomRange(32768, 60999);
  const host = vm.ipv4Address || "207.154.216.224";

  if (!host) {
    throw new Error("No IP for vm");
  }

  await ssh.startGameProcess({
    gameCode,
    host,
    port,
    vmId: vm.vmId,
  });

  const gameInfo = await db.addGame({
    gameCode,
    host,
    port,
    vmId: vm.vmId,
  });

  return gameInfo;
}

module.exports = {
  startGame,
};
