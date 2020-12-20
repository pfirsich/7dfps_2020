const DigitalOcean = require("do-wrapper").default;
const exitHook = require("async-exit-hook");

const db = require("./db");
const ssh = require("./ssh");
const config = require("./config");

const doClient = new DigitalOcean(process.env.DO_ACCESS_TOKEN);

function randomRange(min, max) {
  return Math.floor(Math.random() * (max - min)) + min;
}

async function sleep(time) {
  return new Promise((resolve) => {
    setTimeout(resolve, time);
  });
}

async function waitForAction(actionId) {
  const waitTimeSec = 5;
  const maxTries = 20;
  let tries = 0;

  while (true) {
    tries++;
    if (tries >= maxTries) {
      throw new Error(`Too many tries waiting for action (${tries} tries)`);
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
  let vm = await db.setVm({
    region,
    state: "INITAL",
    timeStarted: "NOW()",
  });

  const name = `${config.vmPrefix}${vm.vmId}`;
  console.log(`Starting droplet: ${name}`);

  const result = await doClient.droplets.create({
    name,
    region,
    size: config.vmSize,
    image: config.vmImage,
    ssh_keys: config.vmSshKeys,
    backups: false,
    ipv6: true,
    // user_data doesn't work for some reason
    // user_data: initScript(vm),
    monitoring: true,
    volumes: null,
    tags: [...config.vmTags, `vmId:${vm.vmId}`],
  });

  vm = await db.setVm({
    ...vm,
    state: "PROVISIONING",
    dropletId: result.droplet.id,
  });

  const actionId = result.links.actions[0].id;

  console.log(`Droplet creation ${name}, Action ID: ${actionId}`);

  await waitForAction(actionId);

  const { droplet } = await doClient.droplets.getById(result.droplet.id);

  const publicNetwork = droplet.networks.v4.find((n) => n.type === "public");
  const ipv4Address = publicNetwork.ip_address;

  vm = await db.setVm({
    ...vm,
    state: "RUNNING",
    ipv4Address,
  });

  console.log("Droplet created with IP:", ipv4Address);

  return vm;
}

async function shutDownVm(vm) {
  console.log(
    `Shuting down vm:${vm.vmId} Droplet: ${vm.dropletId} IP: ${vm.ipv4Address}`
  );

  if (vm.dropletId) {
    vm = await db.setVm({
      ...vm,
      state: "SHUTTING_DOWN",
    });

    try {
      await doClient.droplets.deleteById(vm.dropletId);
    } catch (error) {
      // Ignore 404, assume droplet already removed
      if (!error.message.includes("404")) {
        throw error;
      }
    }
  }

  vm = await db.setVm({
    ...vm,
    state: "TERMINATED",
    timeTerminated: "NOW()",
  });

  console.log(`Terminated vm:${vm.vmId}`);
}

async function checkVms() {
  console.log("Checking if any VMs can be shut down");
  const emptyVms = await db.getEmptyVms();

  // Shut down all VMs in parallel
  await Promise.all(emptyVms.map(shutDownVm));
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

async function onExitGame(gameInfo) {
  console.log(`Game ended ${gameInfo.gameCode} (id: ${gameInfo.gameId})`);

  gameInfo = await db.setGame({
    ...gameInfo,
    state: "OVER",
    timeEnded: "NOW()",
  });

  // We do not need to check VMs here since they are only terminated some time after the game ends
  // await checkVms();
}

async function startGame({ region, creatorIpAddress, version }) {
  const gameCodeMaxChars = config.gameCodeSize.toString(16).length;
  const gameCode = Math.round(Math.random() * config.gameCodeSize)
    .toString(16)
    .padStart(gameCodeMaxChars, "0")
    .toUpperCase();

  let gameInfo = await db.setGame({
    gameCode,
    state: "PROVISIONING",
    creatorIpAddress,
    version,
    timeStarted: "NOW()",
  });

  try {
    const vm = await getOrCreateVm({ region });

    const port = randomRange(32768, 60999);
    const host = vm.ipv4Address;

    gameInfo = await db.setGame({
      ...gameInfo,
      state: "STARTING",
      host,
      port,
      vmId: vm.vmId,
    });

    if (!host) {
      throw new Error("No IP for vm");
    }

    await ssh.startGameProcess(gameInfo, onExitGame);

    gameInfo = await db.setGame({
      ...gameInfo,
      state: "RUNNING",
    });
  } catch (error) {
    await onExitGame(gameInfo);

    throw error;
  }

  return gameInfo;
}

function lookUpGame(gameCode) {
  return db.getGameByGameCode(gameCode);
}

async function checkTimeoutedGames() {
  console.log("Checking for timeouted games");
  const timeoutedGames = await db.getTimeoutedGames();

  for (const gameInfo of timeoutedGames) {
    console.log(`Game timeouted ${gameInfo.gameCode} (id: ${gameInfo.gameId})`);
    ssh.tryClosingConnection(gameInfo.gameId);
    await onExitGame(gameInfo);
  }
}

exitHook(async (callback) => {
  console.log(); // New line after ^C
  console.log();
  console.log("===========================");
  console.log("Cleaning up, please wait...");
  console.log("===========================");
  console.log();

  try {
    const gameIds = ssh.tryClosingAllConnections();

    // Closing the ssh connection will also end the game but it won't
    // wait for it so we have to close the game here just to make sure.
    for (const gameId of gameIds) {
      const gameInfo = await db.getGameByGameId(gameId);
      await onExitGame(gameInfo);
    }
  } catch (error) {
    console.error("Error in clean up:", error);
  }

  callback();
});

module.exports = {
  startGame,
  checkTimeoutedGames,
  checkVms,
  lookUpGame,
};
