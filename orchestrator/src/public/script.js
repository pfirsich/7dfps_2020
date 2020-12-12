function onError(e) {
  console.error(e);

  document.getElementById("screenWait").innerText = e.message;

  document.getElementById("screenWait").style.display = "block";
  document.getElementById("screenForm").style.display = "none";
  document.getElementById("screenDone").style.display = "none";
}

function fallbackCopyTextToClipboard(text) {
  var textArea = document.createElement("textarea");
  textArea.value = text;

  // Avoid scrolling to bottom
  textArea.style.top = "0";
  textArea.style.left = "0";
  textArea.style.position = "fixed";

  document.body.appendChild(textArea);
  textArea.focus();
  textArea.select();

  try {
    var successful = document.execCommand("copy");
    var msg = successful ? "successful" : "unsuccessful";
    console.log("Fallback: Copying text command was " + msg);
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
    function () {
      console.log("Async: Copying to clipboard was successful!");
    },
    function (err) {
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

    <h2>Game Code: </h2>
    <input type="text" id="inputHost" readonly value="${json.host}:${json.port}:${json.gameCode}">  <button type="button" id="copy">Copy</button>

    <br><br>
    Share this code to play together.
  `;

  document.getElementById("copy").addEventListener("click", () => {
    copyTextToClipboard(`${json.host}:${json.port}:${json.gameCode}`);
  });
  document.getElementById("inputHost").addEventListener("click", () => {
    copyTextToClipboard(`${json.host}:${json.port}:${json.gameCode}`);
  });

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
