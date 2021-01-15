const { NodeSSH } = require("node-ssh");

const config = require("./config");

const connections = {};

async function sleep(time) {
  return new Promise((resolve) => {
    setTimeout(resolve, time);
  });
}

// TODO: Move to config
function gameCommand(port, gameCode, version) {
  return `
  set -e
  mkdir -p /var/log/arbitrary-complexity/games
  log=/var/log/arbitrary-complexity/games/${gameCode}.log
  touch $log

  cd /home/morel/arbitrary-complexity
  if test -d "${version}"; then
    cd "${version}"
  fi

  echo "Starting game ${gameCode} with version ${version}" >> $log
  echo "PWD:" >> $log
  pwd >> $log

  ./complexity server 0.0.0.0 ${port} --exit-after-game --exit-timeout=60 --gamecode=${gameCode} | tee -a $log
  `;
}

function spawnGameProcess(ssh, gameInfo, onExitGame) {
  const { port, gameCode, vmId } = gameInfo;

  ssh
    .execCommand(gameCommand(port, gameCode, gameInfo.version), {
      cwd: "/",
      onStdout(chunk) {
        const str = chunk.toString("utf8");
        str.split("\n").forEach((line) => {
          if (line) {
            console.log(`[vm:${vmId} ${gameCode}]`, line);
          }
        });
      },
      onStderr(chunk) {
        const str = chunk.toString("utf8");
        str.split("\n").forEach((line) => {
          if (line) {
            console.error(`[vm:${vmId} ${gameCode}]`, line);
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

async function waitForShh({ host, port }, maxWaitTime) {
  const sleepTimeSec = 5;
  let tries = 0;

  while (true) {
    if (sleepTimeSec * tries >= maxWaitTime) {
      throw new Error(
        `Too many tries waiting for ssh connection to ${host}:${port} (tries: ${tries})`
      );
    }
    tries++;

    const ssh = new NodeSSH();

    try {
      await ssh.connect({
        host,
        username: config.sshUser,
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
        `Waiting for ssh connection (${sleepTimeSec} sec): ${host}:${port}, tries: ${tries}, error: ${error.message}`
      );

      await sleep(sleepTimeSec * 1000);
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
