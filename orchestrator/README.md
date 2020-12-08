# Game Server Orchestrator

Starts VMs and game servers on DigitalOcean.

- `cp .env.example .env`
- Set DO_ACCESS_TOKEN in `.env`
- `docker-compose up -d`
- Open localhost:5000

## API

## Get Regions

```sh
curl http://localhost:5000/regions | jq
```

Example response:

```json
{
  "sfo3": "speedtest-sfo3.digitalocean.com",
  "nyc1": "speedtest-nyc1.digitalocean.com",
  "ams3": "speedtest-ams3.digitalocean.com",
  "fra1": "speedtest-fra1.digitalocean.com",
  "sgp1": "speedtest-sgp1.digitalocean.com",
  "blr1": "speedtest-blr1.digitalocean.com"
}
```

## Create Game

```sh
curl -X POST http://localhost:5000/games -d '{"region": "fra1"}' -H "Content-Type: application/json" | jq
```

Make sure region is a valid region.

Example response:

```json
{
  "gameCode": "6DDC10",
  "host": "foo.example.com",
  "port": 2646
}
```

## Get Game

```sh
curl http://localhost:5000/games/:gameCode |jq
```

`:gameCode` is the code you got from the create game call.

Example response:

```json
{
  "gameCode": "6DDC10",
  "host": "foo.example.com",
  "port": 2646
}
```
