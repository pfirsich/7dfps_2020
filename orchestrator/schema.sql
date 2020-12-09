CREATE TABLE vms (
    vmId INT GENERATED ALWAYS AS IDENTITY,
    region VARCHAR(16),
    -- TODO: add:
    -- state: provisioning, starting, running, shutdown
    -- ipv4Address
    -- timeStarted
    -- timeEnded
    PRIMARY KEY(vmId)
);

CREATE TABLE games (
    -- NOTE: Game id always unique, game code is only hopefully unique among running games
    gameId INT GENERATED ALWAYS AS IDENTITY,
    gameCode varchar(16),
    creatorIpAddress TEXT,
    host TEXT,
    port INT,
    vmId INT,
    -- TODO: add:
    -- state: starting, running, over
    -- version
    -- timeStarted
    -- timeEnded
    PRIMARY KEY(gameId),
    CONSTRAINT fk_vmID
      FOREIGN KEY(vmId)
	  REFERENCES vms(vmId)
);
