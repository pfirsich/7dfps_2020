const { NodeSSH } = require("node-ssh");

const connections = {};

async function sleep(time) {
  return new Promise((resolve) => {
    setTimeout(resolve, time);
  });
}

function gameCommand(port, gameCode) {
  const cmd = `nc -u -l ${port}`;
  // const cmd = `docker run -p ${port}:${port}/udp -t ${image} build/7dfps server 0.0.0.0 ${port} --exit-after-game --exit-timeout=60 --gamecode=${gameCode}`;

  return `mkdir -p /var/log/7dfps/games && ${cmd} | tee /var/log/7dfps/games/${gameCode}.log`;
}

function spawnGameProcess(ssh, gameInfo, onExitGame) {
  const { port, gameCode, vmId } = gameInfo;

  ssh
    .execCommand(gameCommand(port, gameCode), {
      cwd: "/",
      onStdout(chunk) {
        const str = chunk.toString("utf8");
        str.split("\n").forEach((line) => {
          if (line) {
            console.log(`stdout [vm:${vmId} ${gameCode}]`, line);
          }
        });
      },
      onStderr(chunk) {
        const str = chunk.toString("utf8");
        str.split("\n").forEach((line) => {
          if (line) {
            console.error(`stderr [vm:${vmId} ${gameCode}]`, line);
          }
        });
      },
    })
    .then((result) => {
      console.log(`[vm:${vmId} ${gameCode}] Exit Code: ${result.code}`);

      delete connections[gameInfo.gameId];
      ssh.dispose();
      return onExitGame(gameInfo);
    })
    .catch((error) => {
      console.error("Error in spawned game process:", error);
    });
}

async function waitForShh({ host, port }) {
  const maxTries = 15;
  const waitTimeSec = 5;
  let tries = 0;

  while (true) {
    if (tries >= maxTries) {
      throw new Error(`Too many tries waiting for ssh connection (${tries})`);
    }
    tries++;

    await sleep(waitTimeSec * 1000);

    const ssh = new NodeSSH();

    try {
      await ssh.connect({
        host,
        username: "root",
        privateKey: `${process.env.HOME}/.ssh/id_rsa`,
        passphrase: process.env.SSH_PASSPHRASE,
      });
      console.log(
        `Established ssh connection: ${host}:${port}, tries: ${tries}`
      );

      return ssh;
    } catch (error) {
      ssh.dispose();

      console.log(
        `Waiting ssh connection (${waitTimeSec} sec): ${host}:${port}, tries: ${tries}, error: ${error.message}`
      );
      continue;
    }
  }
}

async function startGameProcess(gameInfo, onExitGame) {
  const { host, port, gameCode, vmId } = gameInfo;
  console.log(`Start game process: vm:${vmId} ${gameCode} ${host} ${port}`);

  const ssh = await waitForShh(gameInfo);

  connections[gameInfo.gameId] = {
    ssh,
    gameId: gameInfo.gameId,
  };

  spawnGameProcess(ssh, gameInfo, onExitGame);
}

function tryClosingConnection(gameId) {
  if (connections[gameId]) {
    connections[gameId].ssh.dispose();
  }
}

function tryClosingAllConnections() {
  const count = Object.values(connections).length;
  if (count > 0) {
    console.log(`Closing ${count} connections before exit`);
  }

  const gameIds = [];
  Object.values(connections).forEach((connection) => {
    connection.ssh.dispose();
    gameIds.push(connection.gameId);
  });

  return gameIds;
}

module.exports = {
  startGameProcess,
  tryClosingConnection,
  tryClosingAllConnections,
};
