#!/usr/bin/env bash

PGPASSWORD=secret psql -h localhost -U postgres << EOF
	SELECT
		games.gameId as "Id",
		CONCAT(
			games.host,
			':',
			games.port,
			':',
			games.gameCode
		) as "Game Code",
		games.creatorIpAddress as "IP Creator",
		games.state as "State",
		games.version as "Version",
		vms.region as "Region",
		TO_CHAR(games.timeStarted, 'yyyy-mm-dd hh:mi') AS "Started",
		TO_CHAR(games.timeEnded, 'yyyy-mm-dd hh:mi') AS "Ended",
		TO_CHAR(AGE(games.timeEnded, games.timeStarted), 'hh:mi:ss') AS "Duration"
	FROM games
	LEFT JOIN vms ON games.vmid = vms.vmid
	ORDER BY games.timestarted;
EOF
