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

async function waitForAction(actionId, maxWaitTimeSec) {
  const sleepTimeSec = 8;
  let tries = 0;

  await sleep(1000);

  while (true) {
    if (sleepTimeSec * tries >= maxWaitTimeSec) {
      throw new Error(
        `Too many tries waiting for action ${actionId} (${tries} tries)`
      );
    }
    tries++;

    const { action } = await doClient.actions.getById(actionId);

    if (action.status === "completed") {
      console.log(`Completed action: ${actionId}, tries: ${tries}`);
      return action;
    }

    if (action.status !== "in-progress") {
      return new Error("Failed to create droplet");
    }

    console.log(
      `Waiting for action (${sleepTimeSec} sec): ${actionId}, tries: ${tries}`
    );

    await sleep(sleepTimeSec * 1000);
  }
}

async function waitForVmToBeRunning(vmId, maxWaitTimeSec) {
  const sleepTimeSec = 8;
  let tries = 0;

  await sleep(1000);

  while (true) {
    if (sleepTimeSec * tries >= maxWaitTimeSec) {
      throw new Error(
        `Too many tries waiting for VM to be running vm:${vmId} (${tries} tries)`
      );
    }
    tries++;

    const vm = await db.getVm(vmId);

    if (vm.state === "RUNNING") {
      console.log(
        `Finished waiting for VM to be running: vm:${vmId}, tries: ${tries}`
      );
      return vm;
    }

    if (vm.state === "TERMINATED" || vm.state === "SHUTTING_DOWN") {
      return new Error("Waiting for VM failed, VM ended up stopped");
    }

    console.log(
      `Waiting for ${vm.state} VM (${sleepTimeSec} sec): vm:${vmId}, tries: ${tries}`
    );

    await sleep(sleepTimeSec * 1000);
  }
}

async function getVmImageId(region) {
  if (config.vmImageId) {
    return config.vmImageId;
  }

  if (!config.vmImageName) {
    throw new Error("No image specified");
  }

  const snapshots = await doClient.snapshots.getForDroplets(null, true);

  const snapshot = snapshots.find((s) => s.name === config.vmImageName);

  if (!snapshot) {
    throw new Error(`No image/snapshot with the name ${config.vmImageName}`);
  }

  if (!snapshot.regions.includes(region)) {
    throw new Error(
      `Missing image/snapshot with the name ${config.vmImageName} for the region ${region}`
    );
  }

  config.activeRegions.forEach((activeRegion) => {
    if (!snapshot.regions.includes(activeRegion)) {
      console.error(
        `WARNING: Missing image/snapshot with the name ${config.vmImageName} for the region ${region}`
      );
    }
  });

  return snapshot.id;
}

async function startVm({ region }) {
  const image = await getVmImageId(region);

  // TODO: Save the imageId used
  let vm = await db.setVm({
    region,
    state: "INITAL",
    timeStarted: "NOW()",
  });

  const name = `${config.vmPrefix}${vm.vmId}`;
  console.log(`Starting droplet: ${name}`);

  let result;
  try {
    result = await doClient.droplets.create({
      name,
      region,
      size: config.vmSize,
      image,
      ssh_keys: config.vmSshKeys,
      backups: false,
      ipv6: true,
      monitoring: true,
      volumes: null,
      tags: [...config.vmTags, `vmId:${vm.vmId}`],
    });
  } catch (error) {
    console.log(
      "WARNING: Request to create VM failed, probably didn't start the VM, lets hope so..."
    );
    await shutDownVm(vm);
    throw error;
  }

  vm = await db.setVm({
    ...vm,
    state: "PROVISIONING",
    dropletId: result.droplet.id,
  });

  const actionId = result.links.actions[0].id;

  console.log(`Droplet creation ${name}, Action ID: ${actionId}`);

  try {
    await waitForAction(actionId, 600);
  } catch (error) {
    console.log("WARNING: Droplet creation timed out, trying to stop VM");
    await shutDownVm(vm);
    throw error;
  }

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
  if (vm.dropletId) {
    console.log(
      `Shuting down vm:${vm.vmId} Droplet: ${vm.dropletId} IP: ${vm.ipv4Address}`
    );

    vm = await db.setVm({
      ...vm,
      state: "SHUTTING_DOWN",
    });

    try {
      await doClient.droplets.deleteById(vm.dropletId);
    } catch (error) {
      if (error.message.includes("404")) {
        // Ignore 404, assume droplet already removed
        console.log(
          `WARNING: Droplet ${vm.dropletId} can not be shut down, not found, probably already down`
        );
      } else {
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

// TODO: this should all be a transaction....
async function getOrCreateVm({ region }) {
  const freeVms = await db.getFreeVms({
    region,
    maxGamesOnVm: config.maxGamesOnVm,
  });

  if (freeVms.length > 0) {
    let selectedVm = freeVms.find((vm) => vm.state === "RUNNING") || freeVms[0];

    console.log(
      `Reusing vm:${selectedVm.vmId} for start game request in region ${region}`
    );

    if (selectedVm.state === "PROVISIONING") {
      console.log(
        `Trying to reuse vm:${selectedVm.vmId} that is still PROVISIONING, wating..`
      );
      selectedVm = await waitForVmToBeRunning(selectedVm.vmId, 180);
    }

    return selectedVm;
  }

  // or, start new VM

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
  console.log(
    `Request to start game in region ${region} with version ${version} from ${creatorIpAddress}`
  );

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
