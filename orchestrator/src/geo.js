const fetch = require("node-fetch");

const regions = require("./regions");
const config = require("./config");

async function fetchJson(url, options, validate) {
  const response = await fetch(url, options);
  let json;
  let jsonError;
  try {
    json = await response.json();
  } catch (error) {
    jsonError = error;
  }

  if (json && validate) {
    validate(json);
    return json;
  }

  if (!response.ok) {
    throw new Error(`Unexpected Status: ${response.status}`);
  }

  if (json) {
    return json;
  }

  throw jsonError;
}

async function getIpGeo(ipAddress) {
  const { data } = await fetchJson(
    `https://tools.keycdn.com/geo.json?host=${ipAddress}`
  );

  if (!data.geo.latitude) {
    throw new Error(`No location for IP: ${ipAddress}`);
  }

  return {
    lat: data.geo.latitude,
    lon: data.geo.longitude,
    name: `${data.geo.city}, ${data.geo.country_name}`,
  };
}

// Geo distance in KM
function distanceBetween(a, b) {
  const degreesToRadians = Math.PI / 180;
  const aLat = (a.lat || a[0] || 0) * degreesToRadians;
  const bLat = (b.lat || b[0] || 0) * degreesToRadians;
  const dLon =
    Math.abs((b.lon || b[1] || 0) - (a.lon || a[1] || 0)) * degreesToRadians;

  return (
    Math.atan2(
      Math.sqrt(
        Math.pow(Math.cos(bLat) * Math.sin(dLon), 2.0) +
          Math.pow(
            Math.cos(aLat) * Math.sin(bLat) -
              Math.sin(aLat) * Math.cos(bLat) * Math.cos(dLon),
            2.0
          )
      ),
      Math.sin(aLat) * Math.sin(bLat) +
        Math.cos(aLat) * Math.cos(bLat) * Math.cos(dLon)
    ) * 6372.8
  );
}

async function getRegions(ipAddress) {
  let ipGeo;

  try {
    ipGeo = await getIpGeo(ipAddress);
  } catch (error) {
    // Don't fail if external API down
    console.error(error);
  }

  const data = {};
  Object.entries(regions).forEach(([key, region]) => {
    if (ipGeo && region.geo) {
      const distance = Math.round(distanceBetween(ipGeo, region.geo));
      // Google: 1000km to light seconds
      const kmToLightMs = 0.00333564;
      const minRtt = Math.ceil(distance * 2 * kmToLightMs);
      const estimatedRtt = minRtt * 2 + 8;

      region = {
        ...region,
        distance,
        minRtt,
        estimatedRtt,
      };
    }

    if (config.activeRegions.includes(key)) {
      data[key] = region;
    }
  });

  return data;
}

function isValidRegion(key) {
  return config.activeRegions.includes(key) && key in regions;
}

module.exports = {
  getRegions,
  isValidRegion,
};
