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
  "fra1": {
    "name": "Frankfurt, Germany",
    "pingTest": "speedtest-fra1.digitalocean.com",
    "geo": { "lat": 50.111729, "lon": 8.683914 },
    "distance": 153,
    "minRtt": 2,
    "estimatedRtt": 12
  },
  "sfo3": {
    "name": "San Francisco, United States",
    "pingTest": "speedtest-sfo3.digitalocean.com",
    "geo": { "lat": 37.759291, "lon": -122.445495 },
    "distance": 8992,
    "minRtt": 60,
    "estimatedRtt": 128
  },
  "nyc1": {
    "name": "New York City, United States",
    "pingTest": "speedtest-nyc1.digitalocean.com",
    "geo": { "lat": 40.598202, "lon": -74.177587 },
    "distance": 6075,
    "minRtt": 41,
    "estimatedRtt": 90
  },
  "sgp1": {
    "name": "Singapore",
    "pingTest": "speedtest-sgp1.digitalocean.com",
    "geo": { "lat": 1.334565, "lon": 103.84425 },
    "distance": 10377,
    "minRtt": 70,
    "estimatedRtt": 148
  }
}
```

Uses the [IP Location Finder by KeyCDN](https://tools.keycdn.com/geo) to find the closest server. If this fails, `distance`, `minRtt`, and `estimatedRtt` may not be present.

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
