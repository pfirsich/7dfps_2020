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

function onError(e) {
  console.error(e);

  document.getElementById("screenWait").innerText = e;

  document.getElementById("screenWait").style.display = "block";
  document.getElementById("screenForm").style.display = "none";
  document.getElementById("screenDone").style.display = "none";
}

function fallbackCopyTextToClipboard(text) {
  const textArea = document.createElement("textarea");
  textArea.value = text;

  // Avoid scrolling to bottom
  textArea.style.top = "0";
  textArea.style.left = "0";
  textArea.style.position = "fixed";

  document.body.appendChild(textArea);
  textArea.focus();
  textArea.select();

  try {
    const successful = document.execCommand("copy");
    const msg = successful ? "successful" : "unsuccessful";
    console.log(`Fallback: Copying text command was ${msg}`);
  } catch (err) {
    console.error("Fallback: Oops, unable to copy", err);
  }

  document.body.removeChild(textArea);
}

function copyTextToClipboard(text) {
  if (!navigator.clipboard) {
    fallbackCopyTextToClipboard(text);
    return;
  }
  navigator.clipboard.writeText(text).then(
    () => {
      console.log("Async: Copying to clipboard was successful!");
    },
    (err) => {
      console.error("Async: Could not copy text: ", err);
    }
  );
}

async function submit(event) {
  event.preventDefault();

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
        (this may take up to 60 seconds)
      `;
  };

  update();

  document.getElementById("screenWait").style.display = "block";
  document.getElementById("screenForm").style.display = "none";
  document.getElementById("screenDone").style.display = "none";

  const body = {
    version: event.target.version && event.target.version.value,
    region: event.target.region && event.target.region.value,
  };

  if (!event.target.version) {
    delete body.version;
  }

  let interval;
  let gameInfo;
  try {
    interval = setInterval(update, 431);
    gameInfo = await fetchJson(
      "/games",
      {
        method: "POST",
        headers: {
          "Content-Type": "application/json",
        },
        body: JSON.stringify(body),
      },
      (json) => {
        if (json.msg) {
          throw new Error(json.msg);
        }
      }
    );
  } finally {
    clearInterval(interval);
  }

  const gameString = `${gameInfo.host}:${gameInfo.port}:${gameInfo.gameCode}`;
  const copyGameString = () => {
    copyTextToClipboard(gameString);
  };

  document.getElementById("screenDone").innerHTML = `
    <h2>Game Code: </h2>
    <input type="text" id="inputHost" readonly value="${gameString}" style="width: 380px;">
    <button type="button" id="copy">Copy</button>

    <br><br>
    Share this code to play together.
  `;

  document.getElementById("copy").addEventListener("click", copyGameString);
  document
    .getElementById("inputHost")
    .addEventListener("click", copyGameString);

  document.getElementById("screenWait").style.display = "none";
  document.getElementById("screenForm").style.display = "none";
  document.getElementById("screenDone").style.display = "block";
}

async function init() {
  const regions = await fetchJson("/regions", null, (json) => {
    if (json.msg) {
      throw new Error(json.msg);
    }
  });

  const options = Object.entries(regions);
  options.sort((a, b) => a[1].estimatedRtt - b[1].estimatedRtt);

  document.getElementById("regions").innerHTML = options
    .map(([key, value]) => {
      let text = value.name;

      // if (value.distance) {
      //   text += ", ~";
      //   text += value.distance;
      //   text += "km";
      // }

      if (value.estimatedRtt) {
        text += ", ~";
        text += value.estimatedRtt;
        text += "ms ping";
      }

      return `  <option value="${key}">${text}</option>`;
    })
    .join("\n");

  document.getElementById("screenWait").style.display = "none";
  document.getElementById("screenForm").style.display = "block";
  document.getElementById("screenDone").style.display = "none";

  document.getElementById("form").addEventListener("submit", (event) => {
    submit(event).catch(onError);
  });
}

init().catch(onError);
