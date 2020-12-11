CREATE TYPE vmState AS ENUM('INITAL', 'PROVISIONING', 'RUNNING', 'SHUTTING_DOWN', 'TERMINATED');
CREATE TYPE gameState AS ENUM('PROVISIONING', 'STARTING', 'RUNNING', 'OVER');

CREATE TABLE vms (
    vmId INT GENERATED ALWAYS AS IDENTITY,
    region VARCHAR(16) NOT NULL,
    state vmState NOT NULL,
    ipv4Address VARCHAR(16),
    dropletId VARCHAR(32),
    timeStarted TIMESTAMP NOT NULL,
    timeTerminated TIMESTAMP,
    PRIMARY KEY(vmId)
);

CREATE TABLE games (
    -- NOTE: Game id always unique, game code is only hopefully unique among running games
    gameId INT GENERATED ALWAYS AS IDENTITY,
    gameCode VARCHAR(16) NOT NULL,
    creatorIpAddress TEXT NOT NULL,
    host TEXT,
    port INT,
    vmId INT,
    state gameState NOT NULL,
    version TEXT,
    timeStarted TIMESTAMP NOT NULL,
    timeEnded TIMESTAMP,
    PRIMARY KEY(gameId),
    CONSTRAINT fk_vmID
      FOREIGN KEY(vmId)
	  REFERENCES vms(vmId)
);
