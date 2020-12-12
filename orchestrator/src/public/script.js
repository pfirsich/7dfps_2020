function onError(e) {
  console.error(e);

  document.getElementById("screenWait").innerText = e.message;

  document.getElementById("screenWait").style.display = "block";
  document.getElementById("screenForm").style.display = "none";
  document.getElementById("screenDone").style.display = "none";
}

async function submit(event) {
  event.preventDefault();

  console.log("Hi");

  const timeStarted = Date.now();
  const update = () => {
    const took = Date.now() - timeStarted;

    let progress = took / (60 * 1000);
    if (progress > 0.6) {
      progress = (progress - 0.6) * 0.8 + 0.6;
    }
    if (progress > 0.5) {
      progress = (progress - 0.5) * 0.2 + 0.5;
    }
    if (progress > 0.98) {
      progress = 0.98;
    }

    document.getElementById("screenWait").innerHTML = `
        Starting server: ${(progress * 100).toFixed(2)}%<br>
        <br>
        <br>
        (this my take up to 60 seconds)
      `;
  };

  const interval = setInterval(update, 431);
  update();

  document.getElementById("screenWait").style.display = "block";
  document.getElementById("screenForm").style.display = "none";
  document.getElementById("screenDone").style.display = "none";

  const response = await fetch("/games", {
    method: "POST",
    headers: {
      "Content-Type": "application/json",
    },
    body: JSON.stringify({
      version: event.target.version.value,
      region: event.target.region.value,
    }),
  });

  clearInterval(interval);

  const json = await response.json();

  if (json.msg) {
    throw new Error(json.msg);
  }

  document.getElementById("screenDone").innerHTML = `
    <h3>Game Code: ${json.gameCode}</h3>

    Host: ${json.host} <br>
    Port: ${json.port} <br>

    <br><br>
    ===========
    <br><br>
    Version: ${json.version} <br>
    Time Started: ${json.timeStarted} <br>
  `;

  document.getElementById("screenWait").style.display = "none";
  document.getElementById("screenForm").style.display = "none";
  document.getElementById("screenDone").style.display = "block";
}

async function init() {
  const response = await fetch("/regions");
  const regions = await response.json();

  document.getElementById("regions").innerHTML = Object.entries(regions).map(
    ([key, value]) => `
        <option value="${key}">${value.name}</option>
      `
  );

  document.getElementById("screenWait").style.display = "none";
  document.getElementById("screenForm").style.display = "block";
  document.getElementById("screenDone").style.display = "none";

  document.getElementById("form").addEventListener("submit", (event) => {
    submit(event).catch(onError);
  });
}

init().catch(onError);
