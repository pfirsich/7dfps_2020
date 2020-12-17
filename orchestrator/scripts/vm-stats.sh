#!/usr/bin/env bash

PGPASSWORD=secret psql -h localhost -U postgres << EOF
	SELECT
		vms.vmId AS "Id",
		vms.region AS "Region",
		vms.state AS "State",
		vms.ipv4Address AS "Ipv4 Address",
		vms.dropletId AS "Droplet Id",
		TO_CHAR(vms.timeStarted, 'yyyy-mm-dd hh:mi') AS "Started",
		TO_CHAR(vms.timeTerminated, 'yyyy-mm-dd hh:mi') AS "Terminated",
		TO_CHAR(AGE(vms.timeTerminated, vms.timeStarted), 'hh:mi:ss') AS "Runtime",
		COUNT(games.gameId) AS "Games Count"
	FROM vms
	LEFT JOIN games ON games.vmid = vms.vmid
	GROUP BY vms.vmId
	ORDER BY vms.timestarted;
EOF
