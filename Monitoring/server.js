"use strict";

const http = require("node:http");
const fs = require("node:fs");
const path = require("node:path");
const { TextDecoder } = require("node:util");

const MAX_BODY_BYTES = 2 * 1024 * 1024;
const OFFLINE_MS = 5000;
const RETENTION_MS = 300000;
const MAX_SOURCES = 64;
const ID = /^[A-Za-z0-9_.:-]{1,128}$/;

class ProtocolError extends Error {
  constructor(message, status = 422) {
    super(message);
    this.status = status;
  }
}

function check(condition, message) {
  if (!condition) throw new ProtocolError(message);
}

function object(value, label) {
  check(value !== null && typeof value === "object" && !Array.isArray(value), `${label}: object required`);
  return value;
}

function identifier(value, label, allowEmpty = false) {
  check(typeof value === "string" && ((allowEmpty && value === "") || ID.test(value)), `${label}: invalid ID`);
  return value;
}

function text(value, label) {
  check(typeof value === "string" && value.length > 0 && value.length <= 160, `${label}: invalid text`);
  return value;
}

function number(value, label, minimum = -Infinity) {
  check(typeof value === "number" && Number.isFinite(value) && value >= minimum, `${label}: invalid number`);
  return value;
}

function integer(value, label, minimum = 0, maximum = Number.MAX_SAFE_INTEGER) {
  check(Number.isSafeInteger(value) && value >= minimum && value <= maximum, `${label}: invalid integer`);
  return value;
}

function boolean(value, label) {
  check(typeof value === "boolean", `${label}: boolean required`);
  return value;
}

function array(value, label, maximum) {
  check(Array.isArray(value) && value.length <= maximum, `${label}: invalid array`);
  return value;
}

function unique(items, label) {
  const seen = new Set();
  for (const item of items) {
    check(!seen.has(item.id), `${label}: duplicate ID ${item.id}`);
    seen.add(item.id);
  }
}

function entity(value, villageIds) {
  object(value, "entity");
  object(value.position, "position");
  const villageId = identifier(value.villageId, "villageId", true);
  check(villageId === "" || villageIds.has(villageId), "entity refers to an unknown village");
  const health = value.health === null ? null : number(value.health, "health", 0);
  const maxHealth = value.maxHealth === null ? null : number(value.maxHealth, "maxHealth", 0);
  check((health === null && maxHealth === null)
    || (health !== null && maxHealth !== null && maxHealth > 0 && health <= maxHealth), "invalid health range");
  return {
    id: identifier(value.id, "entity.id"), villageId,
    kind: identifier(value.kind, "kind"), behavior: identifier(value.behavior, "behavior"),
    state: identifier(value.state, "entity.state"), alive: boolean(value.alive, "alive"),
    position: {
      x: number(value.position.x, "position.x"), y: number(value.position.y, "position.y"),
      z: number(value.position.z, "position.z"),
    },
    health, maxHealth, respawnSeconds: number(value.respawnSeconds, "respawnSeconds", 0),
  };
}

function counts(npcs, monsters) {
  const monsterAlive = monsters.filter(item => item.alive).length;
  return {
    npcCount: npcs.length, npcAlive: npcs.filter(item => item.alive).length,
    monsterAlive, monsterSlots: monsters.length, monsterRespawning: monsters.length - monsterAlive,
  };
}

function validateSnapshot(value) {
  object(value, "snapshot");
  check(value.schemaVersion === 1, "unsupported schemaVersion (expected 1)");
  check(value.type === "snapshot", "unsupported message type");
  const worlds = array(value.worlds, "worlds", 32).map(raw => {
    object(raw, "world");
    const villages = array(raw.villages, "villages", 128).map(village => {
      object(village, "village");
      return { id: identifier(village.id, "village.id"), name: text(village.name, "village.name") };
    });
    unique(villages, "villages");
    const villageIds = new Set(villages.map(village => village.id));
    const npcs = array(raw.npcs, "npcs", 10000).map(item => entity(item, villageIds));
    const monsters = array(raw.monsters, "monsters", 10000).map(item => entity(item, villageIds));
    unique([...npcs, ...monsters], "entities within world");
    return {
      id: identifier(raw.id, "world.id"), name: text(raw.name, "world.name"),
      state: identifier(raw.state, "world.state"), active: boolean(raw.active, "active"),
      width: integer(raw.width, "width", 1, 100000), height: integer(raw.height, "height", 1, 100000),
      kills: integer(raw.kills, "kills"), npcs, monsters, counts: counts(npcs, monsters),
      villages: villages.map(village => ({
        ...village,
        counts: counts(npcs.filter(item => item.villageId === village.id),
          monsters.filter(item => item.villageId === village.id)),
      })),
    };
  });
  unique(worlds, "worlds");
  return {
    schemaVersion: 1, type: "snapshot",
    sourceId: identifier(value.sourceId, "sourceId"), instanceId: identifier(value.instanceId, "instanceId"),
    sequence: integer(value.sequence, "sequence", 1), worlds,
  };
}

class SnapshotStore {
  constructor(now = Date.now) {
    this.now = now;
    this.sources = new Map();
  }

  prune(now) {
    for (const [key, source] of this.sources) {
      if (now - source.receivedMs > RETENTION_MS) this.sources.delete(key);
    }
  }

  accept(raw) {
    // Validate the entire message before replacing the last good state.
    const snapshot = validateSnapshot(raw);
    const now = this.now();
    this.prune(now);
    const key = JSON.stringify([snapshot.sourceId, snapshot.instanceId]);
    const previous = this.sources.get(key);
    if (previous && snapshot.sequence <= previous.snapshot.sequence) return false;
    if (!previous && this.sources.size >= MAX_SOURCES) {
      const offline = [...this.sources.entries()]
        .filter(([, source]) => now - source.receivedMs > OFFLINE_MS)
        .sort((a, b) => a[1].receivedMs - b[1].receivedMs);
      if (!offline.length) throw new ProtocolError("too many live sources", 503);
      this.sources.delete(offline[0][0]);
    }
    this.sources.set(key, { snapshot, receivedMs: now });
    return true;
  }

  state() {
    const now = this.now();
    this.prune(now);
    const sources = [...this.sources.values()].map(({ snapshot, receivedMs }) => ({
      ...snapshot, receivedAt: new Date(receivedMs).toISOString(),
      ageSeconds: Math.max(0, now - receivedMs) / 1000,
      online: now - receivedMs <= OFFLINE_MS,
    }));
    sources.sort((a, b) => a.sourceId.localeCompare(b.sourceId) || a.instanceId.localeCompare(b.instanceId));
    return { schemaVersion: 1, serverTime: new Date(now).toISOString(), offlineSeconds: OFFLINE_MS / 1000, sources };
  }
}

function reply(response, status, body, contentType = "application/json; charset=utf-8") {
  const content = typeof body === "string" ? body : JSON.stringify(body);
  response.writeHead(status, {
    "Content-Type": contentType, "Content-Length": Buffer.byteLength(content),
    "Cache-Control": "no-store", "X-Content-Type-Options": "nosniff",
    "Content-Security-Policy": "default-src 'self'; script-src 'self'; style-src 'self'; connect-src 'self'; frame-ancestors 'none'",
  });
  response.end(content);
}

function createHandler(store = new SnapshotStore()) {
  const assets = new Map([
    ["/", ["index.html", "text/html; charset=utf-8"]],
    ["/index.html", ["index.html", "text/html; charset=utf-8"]],
    ["/dashboard.js", ["dashboard.js", "text/javascript; charset=utf-8"]],
    ["/dashboard.css", ["dashboard.css", "text/css; charset=utf-8"]],
  ]);
  return async (request, response) => {
    try {
      // Loopback binding plus Host/Origin checks keep this a local PC service.
      const host = request.headers.host || "";
      if (!/^(127\.0\.0\.1|localhost):\d+$/.test(host)) {
        reply(response, 403, { error: "local Host required" });
        return;
      }
      const origin = request.headers.origin;
      if (origin && origin !== `http://${host}`) {
        reply(response, 403, { error: "same origin required" });
        return;
      }
      const url = new URL(request.url, `http://${host}`);
      if (request.method === "GET" && url.pathname === "/api/state") {
        reply(response, 200, store.state());
      } else if (request.method === "GET" && url.pathname === "/api/health") {
        reply(response, 200, { status: "ok", schemaVersion: 1 });
      } else if (request.method === "GET" && assets.has(url.pathname)) {
        const [file, type] = assets.get(url.pathname);
        reply(response, 200, fs.readFileSync(path.join(__dirname, file), "utf8"), type);
      } else if (request.method === "POST" && url.pathname === "/api/snapshot") {
        if ((request.headers["content-type"] || "").split(";", 1)[0].trim().toLowerCase() !== "application/json") {
          throw new ProtocolError("Content-Type must be application/json", 415);
        }
        if (request.headers["content-encoding"] && request.headers["content-encoding"] !== "identity") {
          throw new ProtocolError("compressed bodies are unsupported", 415);
        }
        if (Number(request.headers["content-length"]) > MAX_BODY_BYTES) {
          throw new ProtocolError("snapshot exceeds 2 MiB", 413);
        }
        let size = 0;
        const chunks = [];
        for await (const chunk of request) {
          size += chunk.length;
          if (size > MAX_BODY_BYTES) throw new ProtocolError("snapshot exceeds 2 MiB", 413);
          chunks.push(chunk);
        }
        let raw;
        try {
          raw = JSON.parse(new TextDecoder("utf-8", { fatal: true }).decode(Buffer.concat(chunks)));
        } catch {
          throw new ProtocolError("invalid UTF-8 JSON", 400);
        }
        reply(response, 200, { accepted: store.accept(raw) });
      } else {
        reply(response, 404, { error: "route not found" });
      }
    } catch (error) {
      if (response.destroyed || response.writableEnded) return;
      if (!(error instanceof ProtocolError)) console.error(error);
      reply(response, error instanceof ProtocolError ? error.status : 500,
        { error: error instanceof ProtocolError ? error.message : "internal error" });
    }
  };
}

if (require.main === module) {
  const args = process.argv.slice(2);
  if (args.length > 1 || (args[0] && !/^\d+$/.test(args[0]))) {
    console.error("Usage: node Monitoring/server.js [port]");
    process.exitCode = 1;
  } else {
    const port = Number(args[0] || 8765);
    if (!Number.isInteger(port) || port < 1 || port > 65535) {
      console.error("Port must be between 1 and 65535.");
      process.exitCode = 1;
    } else {
      const server = http.createServer(createHandler());
      server.requestTimeout = 5000;
      server.headersTimeout = 5000;
      server.setTimeout(5000);
      server.on("error", error => {
        console.error(`Monitor could not start: ${error.message}`);
        process.exitCode = 1;
      });
      server.listen(port, "127.0.0.1", () => console.log(`Monitor: http://127.0.0.1:${port}`));
    }
  }
}

module.exports = { validateSnapshot, SnapshotStore, createHandler, ProtocolError, OFFLINE_MS, RETENTION_MS };
