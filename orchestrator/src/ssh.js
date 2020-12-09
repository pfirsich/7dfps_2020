const { NodeSSH } = require("node-ssh");

async function sleep(time) {
  return new Promise((resolve) => {
    setTimeout(resolve, time);
  });
}

function gameCommand({ port, gameCode }) {
  const cmd = `nc -u -l ${port}`;
  // const cmd = `7dfps server 0.0.0.0 ${port} --exit-after-game --exit-timeout=60 --gamecode=${gameCode}`;

  return `mkdir -p /var/log/7dfps/games && ${cmd} | tee /var/log/7dfps/games/${gameCode}.log`;
}

async function spwanGameProcess(ssh, { port, gameCode, vmId }) {
  const result = await ssh.execCommand(gameCommand({ port, gameCode }), {
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
  });

  console.log(`[vm:${vmId} ${gameCode}] Exit Code: ${result.code}`);

  ssh.dispose();
}

async function waitForShh({ host, port }) {
  const maxTries = 50;
  const waitTimeSec = 5;
  let tries = 0;

  while (true) {
    if (tries >= maxTries) {
      throw new Error(`Too many tries waiting for ssh connection (${tries})`);
    }
    tries++;

    await sleep(waitTimeSec * 1000);

    let ssh = new NodeSSH();

    try {
      await ssh.connect({
        host: host,
        username: "root",
        privateKey: process.env.HOME + "/.ssh/id_rsa",
        passphrase: process.env.SSH_PASSPHRASE,
      });
      console.log(`Connection established: ${host}:${port}, tries: ${tries}`);

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

async function startGameProcess({ host, port, gameCode, vmId }) {
  console.log(`Start game process: vm:${vmId} ${gameCode} ${host} ${port}`);

  const ssh = await waitForShh({ host, port, gameCode, vmId });

  spwanGameProcess(ssh, { port, gameCode, vmId });
}

module.exports = {
  startGameProcess,
};
