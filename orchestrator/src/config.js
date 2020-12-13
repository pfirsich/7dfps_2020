module.exports = {
  gameNameSlug: "7dfps",

  maxVms: 8,

  maxGamesOnVm: 10,

  gameCodeSize: (2 ** 24) - 1,

  // sshUser: "morel",
  sshUser: "root",

  vmSize: "s-1vcpu-1gb",
  // vmImage: "centos-8-x64",
  // vmImage: "ubuntu-16-04-x32",

  vmImage: "75019393", // 7dfps-vm-1607740173248

  vmSshKeys: [
    // Dev
    "a3:68:a6:33:c8:d9:4b:b4:e1:cd:91:d7:72:01:69:01",
    "3e:b7:e7:14:f0:c7:0d:49:7a:f3:5b:7b:65:b4:d3:0f",

    // Orchestrator Key
    "46:17:b4:bd:7d:bf:f1:4e:40:de:e3:d8:77:a3:bf:41",
  ],
  vmTags: ["7dfps-game-vm"],
  vmPrefix: "7dfps-game-vm-",
};
