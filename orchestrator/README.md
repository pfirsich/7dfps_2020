# Game Server Orchestrator

Starts VMs and game servers on DigitalOcean.

## Getting Started

- `cp .env.example .env`
- Set `DO_ACCESS_TOKEN` in `.env`
- `docker-compose up -d`
- Open localhost:5000

## API

### Get Regions

```sh
curl http://localhost:5000/regions | jq
```

Example response:

```json
{
  "sfo3": {
    "pingTest": "speedtest-sfo3.digitalocean.com",
    "name": "San Francisco, United States 3"
  },
  "nyc1": {
    "pingTest": "speedtest-nyc1.digitalocean.com",
    "name": "New York City, United States 1"
  },
  "fra1": {
    "pingTest": "speedtest-fra1.digitalocean.com",
    "name": "Frankfurt, Germany 1"
  },
  "sgp1": {
    "pingTest": "speedtest-sgp1.digitalocean.com",
    "name": "Singapore 1"
  }
}
```

### Create Game

```sh
curl -X POST http://localhost:5000/games -d '{"region": "fra1", "version": "stable"}' -H "Content-Type: application/json" | jq
```

Make sure `region` is a valid region.
Optionally: `version` is can be one of `jam`, `stable`, `dev`.

Example response:

```json
{
  "gameCode": "6D257A",
  "host": "138.197.179.34",
  "port": 36400,
  "version": "stable",
  "timeStarted": "2020-12-11T10:36:51.048Z"
}
```

### Get Game

```sh
curl http://localhost:5000/games/:gameCode |jq
```

`:gameCode` is the code you got from the create game call.

Example response:

```json
{
  "gameCode": "6D257A",
  "host": "138.197.179.34",
  "port": 36400,
  "version": "stable",
  "timeStarted": "2020-12-11T10:36:51.048Z"
}
```
