CREATE TABLE vms (
    vmId INT GENERATED ALWAYS AS IDENTITY,
    region VARCHAR(16),
    PRIMARY KEY(vmId)
);

CREATE TABLE games (
    gameCode varchar(16),
    host text,
    port int,
    vmId INT,
    PRIMARY KEY(gameCode),
    CONSTRAINT fk_vmID
      FOREIGN KEY(vmId)
	  REFERENCES vms(vmId)
);
