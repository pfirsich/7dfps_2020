module.exports = {
  maxVms: 8,

  // TOOD: set to 4 or something
  maxGamesOnVm: 4,

  gameCodeSize: 2 ** 24,

  vmSize: "s-1vcpu-1gb",
  vmImage: "centos-8-x64",
  // vmImage: "ubuntu-16-04-x32",
  vmSshKeys: [
    "a3:68:a6:33:c8:d9:4b:b4:e1:cd:91:d7:72:01:69:01",
    "3e:b7:e7:14:f0:c7:0d:49:7a:f3:5b:7b:65:b4:d3:0f",
  ],
  vmTags: ["7dfps-game-vm"],
};
