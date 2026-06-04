const { spawn } = require("node:child_process");

const codexExe = process.argv[2];
if (!codexExe) {
  console.error("missing codex executable path");
  process.exit(2);
}

const child = spawn(codexExe, ["app-server", "--listen", "stdio://"], {
  stdio: ["pipe", "pipe", "pipe"],
  windowsHide: true,
});

let stdout = "";
let stderr = "";
let nextId = 1;
const pending = new Map();

function send(method, params) {
  const id = nextId++;
  const message = params === undefined ? { id, method } : { id, method, params };
  child.stdin.write(`${JSON.stringify(message)}\n`, "utf8");
  return new Promise((resolve, reject) => {
    pending.set(id, { resolve, reject });
  });
}

function notify(method, params) {
  const message = params === undefined ? { method } : { method, params };
  child.stdin.write(`${JSON.stringify(message)}\n`, "utf8");
}

function fail(message) {
  try {
    child.kill("SIGKILL");
  } catch {}
  console.error(message);
  process.exit(3);
}

child.stdout.on("data", (chunk) => {
  stdout += chunk.toString("utf8");
  for (;;) {
    const newline = stdout.indexOf("\n");
    if (newline < 0) {
      break;
    }
    const line = stdout.slice(0, newline).trim();
    stdout = stdout.slice(newline + 1);
    if (!line) {
      continue;
    }

    let message;
    try {
      message = JSON.parse(line);
    } catch {
      continue;
    }

    if (message.id === undefined || !pending.has(message.id)) {
      continue;
    }

    const waiter = pending.get(message.id);
    pending.delete(message.id);
    if (message.error) {
      waiter.reject(new Error(message.error.message || JSON.stringify(message.error)));
    } else {
      waiter.resolve(message.result);
    }
  }
});

child.stderr.on("data", (chunk) => {
  stderr += chunk.toString("utf8");
});

child.on("exit", (code) => {
  if (pending.size > 0) {
    for (const waiter of pending.values()) {
      waiter.reject(new Error(`codex app-server exited before response; code=${code}; stderr=${stderr}`));
    }
    pending.clear();
  }
});

const timeout = setTimeout(() => {
  fail(`codex app-server timeout; stderr=${stderr}`);
}, 45000);

function shutdownAndExit(code) {
  try {
    child.stdin.end();
  } catch {}
  setTimeout(() => {
    try {
      child.kill();
    } catch {}
    process.exit(code);
  }, 50);
}

(async () => {
  try {
    await send("initialize", {
      clientInfo: {
        name: "strategic-nexus-usage-probe",
        title: "Strategic Nexus Usage Probe",
        version: "0.1",
      },
      capabilities: {
        experimentalApi: true,
        requestAttestation: false,
      },
    });
    notify("initialized");
    const result = await send("account/rateLimits/read");
    clearTimeout(timeout);
    process.stdout.write(`${JSON.stringify(result)}\n`, "utf8", () => {
      shutdownAndExit(0);
    });
  } catch (error) {
    clearTimeout(timeout);
    fail(error && error.message ? error.message : String(error));
  }
})();
