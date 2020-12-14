# Orchestrator Ops

## Deployment

- Clone repo
- Pull
- `docker-compose up -d --build`

## Game Deployment

- Create a Droplet in DigitalOcean where you can start game servers
- Copy your game in the directory with the right version and make sure it works with the script in `ssh.js`
- Create a snapshot of the Droplet in DigitalOcean
- Go to images > snapshots in DigitalOcean and add the snapshot to all supported regions
- Open the dev tools, reload, check the network tap, search for the `snapshot` XHR request, copy the _id_ of the image
- Update the `vmImage` in the `config.js`

## Monitoring

Check logs

```sh
docker-compose logs -f app
```

Check SQL

```sh
psql -h localhost -U postgres
pw: secret
```

```sql
-- See VMs and runtime
select *, age(timeterminated, timestarted) from vms;

-- See games
select * from games;
```
