// ── NHL Live Watchface  ·  PebbleKit JS ───────────────────────────────────
// Keys must match #define KEY_* in main.c exactly
var KEY_AWAY_ABBR    = 1;
var KEY_HOME_ABBR    = 2;
var KEY_AWAY_SCORE   = 3;
var KEY_HOME_SCORE   = 4;
var KEY_PERIOD       = 5;
var KEY_PERIOD_TIME  = 6;
var KEY_STATUS       = 10;
var KEY_TEAM_IDX     = 11;
var KEY_START_TIME   = 12;
var KEY_AWAY_WINS    = 13;
var KEY_AWAY_LOSSES  = 14;
var KEY_HOME_WINS    = 15;
var KEY_HOME_LOSSES  = 16;
var KEY_VIBRATE      = 17;
var KEY_AWAY_SHOTS   = 18;
var KEY_HOME_SHOTS   = 19;
var KEY_LAST_GOAL    = 20;
var KEY_AWAY_SKATERS = 21;
var KEY_HOME_SKATERS = 22;
var KEY_PENALTY_SECS = 23;
var KEY_NEXT_GAME    = 24;
var KEY_BATTERY_BAR  = 25;
var KEY_TICKER       = 26;
var KEY_TZ_OFFSET    = 31;
var KEY_TICKER_SPEED = 32;

// NHL Web API — free, no key required
var SCHEDULE_URL    = "https://api-web.nhle.com/v1/schedule/now";
var GAMECENTER_URL  = "https://api-web.nhle.com/v1/gamecenter";
var STANDINGS_URL   = "https://api-web.nhle.com/v1/standings/now";
var CONFIG_URL      = "https://brooks2564.github.io/Pebble-NHL-Live/nhl-config.html";

// Must match TEAM_ABBR[] order in main.c exactly (index = KEY_TEAM_IDX value)
var TEAMS = [
  { abbr: "ANA", name: "Ducks"        },  //  0
  { abbr: "BOS", name: "Bruins"       },  //  1
  { abbr: "BUF", name: "Sabres"       },  //  2
  { abbr: "CAR", name: "Hurricanes"   },  //  3
  { abbr: "CBJ", name: "Blue Jackets" },  //  4
  { abbr: "CGY", name: "Flames"       },  //  5
  { abbr: "CHI", name: "Blackhawks"   },  //  6
  { abbr: "COL", name: "Avalanche"    },  //  7
  { abbr: "DAL", name: "Stars"        },  //  8
  { abbr: "DET", name: "Red Wings"    },  //  9
  { abbr: "EDM", name: "Oilers"       },  // 10
  { abbr: "FLA", name: "Panthers"     },  // 11
  { abbr: "LAK", name: "Kings"        },  // 12
  { abbr: "MIN", name: "Wild"         },  // 13
  { abbr: "MTL", name: "Canadiens"    },  // 14
  { abbr: "NJD", name: "Devils"       },  // 15
  { abbr: "NSH", name: "Predators"    },  // 16
  { abbr: "NYI", name: "Islanders"    },  // 17
  { abbr: "NYR", name: "Rangers"      },  // 18
  { abbr: "OTT", name: "Senators"     },  // 19
  { abbr: "PHI", name: "Flyers"       },  // 20
  { abbr: "PIT", name: "Penguins"     },  // 21
  { abbr: "SEA", name: "Kraken"       },  // 22
  { abbr: "SJS", name: "Sharks"       },  // 23
  { abbr: "STL", name: "Blues"        },  // 24
  { abbr: "TBL", name: "Lightning"    },  // 25
  { abbr: "TOR", name: "Maple Leafs"  },  // 26
  { abbr: "UTA", name: "Hockey Club"  },  // 27
  { abbr: "VAN", name: "Canucks"      },  // 28
  { abbr: "VGK", name: "Golden Knts"  },  // 29
  { abbr: "WSH", name: "Capitals"     },  // 30
  { abbr: "WPG", name: "Jets"         }   // 31
];

// ── Saved state ────────────────────────────────────────────────────────────
var gTeamIdx    = parseInt(localStorage.getItem("teamIdx")   || "26"); // TOR default
var gVibrate    = localStorage.getItem("vibrate")    !== "0";
var gBatteryBar = localStorage.getItem("batteryBar") !== "0";
var gTzOffset   = parseInt(localStorage.getItem("tzOffset")  || "-5");
// Ticker speed stored as STRING to avoid Pebble JS number truncation.
// Pebble's + operator and parseInt truncate multi-digit numbers to 3 chars.
// JSON.stringify is the only safe number→string conversion.
function validSpeedStr(s) {
  return s === "5000" || s === "10000" || s === "30000" || s === "60000";
}
var SPEED_NUM    = {"5000":5000,"10000":10000,"30000":30000,"60000":60000};
var _rawSpd      = localStorage.getItem("tickerSpeed");
var gTickerSpeed = validSpeedStr(_rawSpd) ? _rawSpd : "5000"; // STRING

// ── Utility ────────────────────────────────────────────────────────────────
function todayDateStr() {
  var d  = new Date();
  var mm = d.getMonth() + 1;
  var dd = d.getDate();
  return d.getFullYear() + "-" +
    (mm < 10 ? "0" + mm : mm) + "-" +
    (dd < 10 ? "0" + dd : dd);
}

function formatStartTime(isoStr) {
  if (!isoStr) return "";
  try {
    var d = new Date(isoStr);
    if (isNaN(d.getTime())) return "";
    var h = d.getHours(), m = d.getMinutes();
    var ampm = h >= 12 ? "PM" : "AM";
    h = h % 12; if (h === 0) h = 12;
    return h + ":" + (m < 10 ? "0" + m : m) + " " + ampm;
  } catch(e) { return ""; }
}

// Parse "W-L-OT" record string into {wins, losses}
function parseRecord(recStr) {
  var parts = (recStr || "0-0-0").split("-");
  return {
    wins:   parseInt(parts[0]) || 0,
    losses: parseInt(parts[1]) || 0
  };
}

// Parse penalty time string "M:SS" into total seconds
function penaltyTimeToSecs(timeStr) {
  if (!timeStr) return 0;
  try {
    var parts = timeStr.split(":");
    if (parts.length < 2) return 0;
    return (parseInt(parts[0]) || 0) * 60 + (parseInt(parts[1]) || 0);
  } catch(e) { return 0; }
}

// Parse situation code (4 chars: [homeGoalie][homeSkaters][awaySkaters][awayGoalie])
// e.g. "1551" = even, "1451" = home PP (home 5, away 4)
// Returns {homeSkaters, awaySkaters} (skaters only, not goalie)
function parseSituationCode(code) {
  // Format: [awayGoalie][awaySkaters][homeSkaters][homeGoalie]
  if (!code || code.length < 4) return { homeSkaters: 5, awaySkaters: 5 };
  var a = parseInt(code[1]) || 5;  // code[1] = away skaters
  var h = parseInt(code[2]) || 5;  // code[2] = home skaters
  if (h < 3 || h > 6) h = 5;
  if (a < 3 || a > 6) a = 5;
  return { homeSkaters: h, awaySkaters: a };
}

// Convert NHL API gameState to our internal status string
function apiStateToStatus(gameState) {
  if (!gameState) return "off";
  var s = gameState.toUpperCase();
  if (s === "LIVE" || s === "CRIT") return "live";
  if (s === "FINAL" || s === "OVER" || s === "OFF") return "final";
  if (s === "FUT" || s === "PRE")   return "pre";
  return "off";
}

// Period number + type → display period index (1-based, 4=OT, 5=SO)
function periodIndex(periodNum, periodType) {
  if (!periodNum) return 1;
  var n = parseInt(periodNum) || 1;
  if ((periodType || "").toUpperCase() === "SO") return 5;
  if ((periodType || "").toUpperCase() === "OT" || n > 3) return 4;
  return n;
}

// ── Ticker builder ─────────────────────────────────────────────────────────
function buildTicker(games, myAbbr) {
  var parts = [];
  for (var i = 0; i < games.length; i++) {
    var g    = games[i];
    var away = (g.awayTeam && g.awayTeam.abbrev) || "";
    var home = (g.homeTeam && g.homeTeam.abbrev) || "";
    if (away === myAbbr || home === myAbbr) continue;

    var status = apiStateToStatus(g.gameState);
    if (status === "off") continue;

    var entry = "";
    if (status === "pre") {
      var t = formatStartTime(g.startTimeUTC || "");
      entry = away + " vs " + home + (t ? " " + t : "");
    } else if (status === "final") {
      entry = away + " " + ((g.awayTeam && g.awayTeam.score) || 0) + " - " +
              home + " " + ((g.homeTeam && g.homeTeam.score) || 0) + " F";
    } else {
      // live
      var per = g.period || "";
      entry = away + " " + ((g.awayTeam && g.awayTeam.score) || 0) + " - " +
              home + " " + ((g.homeTeam && g.homeTeam.score) || 0) +
              " P" + per;
    }
    if (entry.length > 21) entry = entry.substring(0, 21);
    parts.push(entry);
  }
  return parts.join("|");
}

// ── Next game finder ───────────────────────────────────────────────────────
// Scan gameWeek days after today to find the team's next scheduled game
function findNextGame(gameWeek, myAbbr, today) {
  for (var d = 0; d < gameWeek.length; d++) {
    var day = gameWeek[d];
    if ((day.date || "") <= today) continue;
    var gs = day.games || [];
    for (var i = 0; i < gs.length; i++) {
      var g    = gs[i];
      var away = (g.awayTeam && g.awayTeam.abbrev) || "";
      var home = (g.homeTeam && g.homeTeam.abbrev) || "";
      if (away === myAbbr || home === myAbbr) {
        var opp = (away === myAbbr) ? home : away;
        var t   = formatStartTime(g.startTimeUTC || "");
        var entry = (day.date || "").substring(5) + " " + opp; // e.g. "01-22 MTL"
        if (t) entry += " " + t;
        if (entry.length > 19) entry = entry.substring(0, 19);
        return entry;
      }
    }
  }
  return "";
}

// ── Gamecenter fetch (last goal) ───────────────────────────────────────────
function fetchGamecenterLanding(gameId, callback) {
  var url = GAMECENTER_URL + "/" + gameId + "/landing";
  console.log("[NHL] Fetching gamecenter: " + gameId);
  var xhr = new XMLHttpRequest();
  xhr.open("GET", url, true);
  xhr.setRequestHeader("Accept", "application/json");
  xhr.onload = function() {
    if (xhr.status !== 200) { callback(null); return; }
    try { callback(JSON.parse(xhr.responseText)); }
    catch(e) { console.log("[NHL] Gamecenter parse error: " + e); callback(null); }
  };
  xhr.onerror = function() { callback(null); };
  xhr.send();
}

// Fetch standings and return {awayWins, awayLosses, homeWins, homeLosses}
function fetchStandings(awayAbbr, homeAbbr, callback) {
  var xhr = new XMLHttpRequest();
  xhr.open("GET", STANDINGS_URL, true);
  xhr.setRequestHeader("Accept", "application/json");
  xhr.onload = function() {
    if (xhr.status !== 200) { callback(null); return; }
    try {
      var list = JSON.parse(xhr.responseText).standings || [];
      var r = { awayWins:0, awayLosses:0, homeWins:0, homeLosses:0 };
      for (var i = 0; i < list.length; i++) {
        var t    = list[i];
        var abbr = (t.teamAbbrev && t.teamAbbrev.default) || "";
        if (abbr === awayAbbr) { r.awayWins = t.wins||0; r.awayLosses = t.losses||0; }
        if (abbr === homeAbbr) { r.homeWins = t.wins||0; r.homeLosses = t.losses||0; }
      }
      console.log("[NHL] standings " + awayAbbr + " " + r.awayWins + "-" + r.awayLosses +
                  "  " + homeAbbr + " " + r.homeWins + "-" + r.homeLosses);
      callback(r);
    } catch(e) { console.log("[NHL] standings error: " + e); callback(null); }
  };
  xhr.onerror = function() { callback(null); };
  xhr.send();
}

// Extract last goal, SOG, clock, situation, and records from gamecenter landing data
function extractGamecenterData(data) {
  var result = { lastGoal: "", awaySog: 0, homeSog: 0, perTime: "",
                 awayWins: -1, awayLosses: -1, homeWins: -1, homeLosses: -1,
                 situationCode: "", penaltySecs: -1 };
  if (!data) return result;

  // Last goal scorer
  try {
    var scoring = (data.summary && data.summary.scoring) || [];
    var lastGoal = null;
    for (var p = 0; p < scoring.length; p++) {
      var goals = scoring[p].goals || [];
      for (var g = 0; g < goals.length; g++) lastGoal = goals[g];
    }
    if (lastGoal) {
      var scorer = "";
      if (lastGoal.firstName && lastGoal.lastName) {
        scorer = (lastGoal.firstName.default || "") + " " + (lastGoal.lastName.default || "");
      } else if (lastGoal.name && lastGoal.name.default) {
        scorer = lastGoal.name.default;
      }
      scorer = scorer.trim();
      var goalNum = lastGoal.goalsToDate || lastGoal.total || 0;
      result.lastGoal = scorer + (goalNum ? " (" + goalNum + ")" : "");
      if (result.lastGoal.length > 19) result.lastGoal = result.lastGoal.substring(0, 19);
    }
  } catch(e) { console.log("[NHL] lastGoal error: " + e); }

  // Shots on goal — direct sog field on team objects (gamecenter)
  try {
    var at2 = data.awayTeam || {};
    var ht2 = data.homeTeam || {};
    result.awaySog = parseInt(at2.sog) || 0;
    result.homeSog = parseInt(ht2.sog) || 0;
    // Fallback: teamGameStats array
    if (!result.awaySog && !result.homeSog) {
      var stats = (data.summary && data.summary.teamGameStats) || [];
      for (var i = 0; i < stats.length; i++) {
        if ((stats[i].category || "").toLowerCase() === "sog") {
          result.awaySog = parseInt(stats[i].awayValue) || 0;
          result.homeSog = parseInt(stats[i].homeValue) || 0;
          break;
        }
      }
    }
  } catch(e) { console.log("[NHL] SOG error: " + e); }

  // Period clock
  try {
    if (data.clock) {
      result.perTime = data.clock.inIntermission ? "INT" : (data.clock.timeRemaining || "");
    }
  } catch(e) { console.log("[NHL] clock error: " + e); }

  // Situation (power play)
  try {
    if (data.situation) {
      result.situationCode = data.situation.situationCode || "";
      result.penaltySecs   = penaltyTimeToSecs(data.situation.timeRemaining || "");
    }
  } catch(e) { console.log("[NHL] situation error: " + e); }

  return result;
}

// ── Main game fetch ────────────────────────────────────────────────────────
function fetchGameData(teamIdx) {
  var abbr = TEAMS[teamIdx].abbr;
  console.log("[NHL] Fetching for " + abbr);
  var xhr = new XMLHttpRequest();
  xhr.open("GET", SCHEDULE_URL, true);
  xhr.setRequestHeader("Accept", "application/json");
  xhr.onload = function() {
    if (xhr.status !== 200) {
      console.log("[NHL] API error: " + xhr.status);
      sendOffMessage();
      return;
    }
    try {
      var data     = JSON.parse(xhr.responseText);
      var gameWeek = data.gameWeek || [];
      processGameWeek(gameWeek, abbr);
    } catch(e) {
      console.log("[NHL] Parse error: " + e);
      sendOffMessage();
    }
  };
  xhr.onerror = function() { sendOffMessage(); };
  xhr.send();
}

function processGameWeek(gameWeek, abbr) {
  var today    = todayDateStr();
  var allGames = [];
  var myGame   = null;

  // Find today's day block and my team's game
  for (var d = 0; d < gameWeek.length; d++) {
    var day = gameWeek[d];
    var gs  = day.games || [];
    if ((day.date || "") === today) {
      allGames = gs;
      for (var i = 0; i < gs.length; i++) {
        var g    = gs[i];
        var away = (g.awayTeam && g.awayTeam.abbrev) || "";
        var home = (g.homeTeam && g.homeTeam.abbrev) || "";
        if (away === abbr || home === abbr) { myGame = g; break; }
      }
    }
  }

  var ticker   = buildTicker(allGames, abbr);
  var nextGame = "";

  if (!myGame) {
    // No game today — find next one
    nextGame = findNextGame(gameWeek, abbr, today);
    sendOffMessage(ticker, nextGame);
    return;
  }

  var status   = apiStateToStatus(myGame.gameState);
  var awayTeam = myGame.awayTeam || {};
  var homeTeam = myGame.homeTeam || {};

  console.log("[NHL] gameType=" + myGame.gameType + " status=" + status);

  // Records: playoffs (gameType=3) may return series record "0-0" — try season wins/losses fields too
  var awayWins = 0, awayLosses = 0, homeWins = 0, homeLosses = 0;
  if (myGame.gameType === 3) {
    // Playoff series wins (seriesWins / seriesLosses if available)
    awayWins   = awayTeam.seriesWins   || 0;
    awayLosses = awayTeam.seriesLosses || 0;
    homeWins   = homeTeam.seriesWins   || 0;
    homeLosses = homeTeam.seriesLosses || 0;
  } else {
    var awayRec = parseRecord(awayTeam.record || "");
    var homeRec = parseRecord(homeTeam.record || "");
    awayWins   = awayRec.wins;
    awayLosses = awayRec.losses;
    homeWins   = homeRec.wins;
    homeLosses = homeRec.losses;
  }

  // SOG: try schedule API fields first
  var awaySog = awayTeam.sog || awayTeam.shotsOnGoal || 0;
  var homeSog = homeTeam.sog || homeTeam.shotsOnGoal || 0;

  // Situation / power play
  var sit         = myGame.situation || {};
  var sitCode     = sit.situationCode || "1551";
  var skaters     = parseSituationCode(sitCode);
  var penaltySecs = penaltyTimeToSecs(sit.timeRemaining || "");

  // Period info (use periodDescriptor.number as primary; schedule clock as fallback)
  var pd      = myGame.periodDescriptor || {};
  var perNum  = pd.number || myGame.period || 1;
  var perIdx  = periodIndex(perNum, pd.periodType);
  var perTime = "";
  if (myGame.clock) {
    perTime = myGame.clock.inIntermission ? "INT" : (myGame.clock.timeRemaining || "");
  }

  if (status === "final") {
    nextGame = findNextGame(gameWeek, abbr, today);
  }

  var msg = {};
  msg[KEY_AWAY_ABBR]    = awayTeam.abbrev  || "---";
  msg[KEY_HOME_ABBR]    = homeTeam.abbrev  || "---";
  msg[KEY_AWAY_SCORE]   = awayTeam.score   || 0;
  msg[KEY_HOME_SCORE]   = homeTeam.score   || 0;
  msg[KEY_PERIOD]       = perIdx;
  msg[KEY_PERIOD_TIME]  = perTime;
  msg[KEY_STATUS]       = status;
  msg[KEY_START_TIME]   = formatStartTime(myGame.startTimeUTC || "");
  msg[KEY_AWAY_WINS]    = awayWins;
  msg[KEY_AWAY_LOSSES]  = awayLosses;
  msg[KEY_HOME_WINS]    = homeWins;
  msg[KEY_HOME_LOSSES]  = homeLosses;
  msg[KEY_VIBRATE]      = gVibrate    ? 1 : 0;
  msg[KEY_AWAY_SHOTS]   = awaySog;
  msg[KEY_HOME_SHOTS]   = homeSog;
  msg[KEY_LAST_GOAL]    = "";
  msg[KEY_AWAY_SKATERS] = skaters.awaySkaters;
  msg[KEY_HOME_SKATERS] = skaters.homeSkaters;
  msg[KEY_PENALTY_SECS] = penaltySecs;
  msg[KEY_NEXT_GAME]    = nextGame;
  msg[KEY_BATTERY_BAR]  = gBatteryBar ? 1 : 0;
  msg[KEY_TICKER]       = ticker;

  // For live or final games: fetch gamecenter (SOG, last goal, clock) + standings (records) in parallel
  if ((status === "live" || status === "final") && myGame.id) {
    var gcDone = false, stDone = false, gcResult = null, stResult = null;
    function trySend() {
      if (!gcDone || !stDone) return;
      var gc = extractGamecenterData(gcResult);
      msg[KEY_LAST_GOAL]  = gc.lastGoal;
      if (gc.awaySog > 0) msg[KEY_AWAY_SHOTS]  = gc.awaySog;
      if (gc.homeSog > 0) msg[KEY_HOME_SHOTS]  = gc.homeSog;
      if (gc.perTime)     msg[KEY_PERIOD_TIME] = gc.perTime;
      if (gc.situationCode) {
        var sk = parseSituationCode(gc.situationCode);
        msg[KEY_AWAY_SKATERS] = sk.awaySkaters;
        msg[KEY_HOME_SKATERS] = sk.homeSkaters;
      }
      if (gc.penaltySecs >= 0) msg[KEY_PENALTY_SECS] = gc.penaltySecs;
      if (stResult) {
        msg[KEY_AWAY_WINS]   = stResult.awayWins;
        msg[KEY_AWAY_LOSSES] = stResult.awayLosses;
        msg[KEY_HOME_WINS]   = stResult.homeWins;
        msg[KEY_HOME_LOSSES] = stResult.homeLosses;
      }
      sendMessage(msg);
    }
    fetchGamecenterLanding(myGame.id, function(data) { gcResult = data; gcDone = true; trySend(); });
    fetchStandings(awayTeam.abbrev || "", homeTeam.abbrev || "", function(r) { stResult = r; stDone = true; trySend(); });
    return;
  }

  sendMessage(msg);
}

function sendOffMessage(ticker, nextGame) {
  var msg = {};
  msg[KEY_STATUS]    = "off";
  msg[KEY_NEXT_GAME] = nextGame || "";
  msg[KEY_TICKER]    = ticker   || "";
  sendMessage(msg);
}

function sendMessage(dict) {
  // Send TICKER in a separate message to avoid 512-byte inbox overflow.
  var ticker = dict[KEY_TICKER];
  delete dict[KEY_TICKER];

  Pebble.sendAppMessage(dict,
    function() {
      console.log("[NHL] Message sent OK");
      if (ticker !== undefined) {
        var tm = {};
        tm[KEY_TICKER] = ticker;
        Pebble.sendAppMessage(tm,
          function()  { console.log("[NHL] Ticker sent OK"); },
          function(e) { console.log("[NHL] Ticker NACK: " + JSON.stringify(e)); }
        );
      }
    },
    function(e) { console.log("[NHL] NACK: " + JSON.stringify(e)); }
  );
}

// ── Pebble events ──────────────────────────────────────────────────────────
Pebble.addEventListener("ready", function() {
  console.log("[NHL] Ready – team: " + TEAMS[gTeamIdx].abbr);
  fetchGameData(gTeamIdx);
});

Pebble.addEventListener("appmessage", function(e) {
  var msg = e.payload;
  var idx = parseInt(msg[KEY_TEAM_IDX]);
  if (!isNaN(idx) && idx >= 0 && idx < TEAMS.length) {
    gTeamIdx = idx;
    localStorage.setItem("teamIdx", String(gTeamIdx));
  }
  fetchGameData(gTeamIdx);
});

// ── Settings ───────────────────────────────────────────────────────────────
Pebble.addEventListener("showConfiguration", function() {
  // gTickerSpeed is a string ("5000", "10000", etc.) — safe to concatenate directly
  var url = CONFIG_URL + "?v=1" + "#" + gTeamIdx +
    "|" + (gVibrate    ? "1" : "0") +
    "|" + (gBatteryBar ? "1" : "0") +
    "|" + gTzOffset +
    "|" + gTickerSpeed;
  console.log("[NHL] showConfiguration url: " + url);
  Pebble.openURL(url);
});

Pebble.addEventListener("webviewclosed", function(e) {
  console.log("[NHL] webviewclosed response: " + e.response);
  if (!e.response) return;
  try {
    var cfg = JSON.parse(decodeURIComponent(e.response));
    console.log("[NHL] webviewclosed cfg: " + JSON.stringify(cfg));
    var idx = parseInt(cfg.teamIdx);
    if (isNaN(idx) || idx < 0 || idx >= TEAMS.length) return;

    gTeamIdx    = idx;
    gVibrate    = cfg.vibrate    === 1 || cfg.vibrate    === true || cfg.vibrate    === "1";
    gBatteryBar = cfg.batteryBar === 1 || cfg.batteryBar === true || cfg.batteryBar === "1";
    gTzOffset   = parseInt(cfg.tzOffset) || -5;
    // cfg.tickerSpeed is a number. Convert via JSON.stringify (safe against truncation).
    var spdStr = JSON.stringify(cfg.tickerSpeed);
    gTickerSpeed = validSpeedStr(spdStr) ? spdStr : "5000";

    localStorage.setItem("teamIdx",     String(gTeamIdx));
    localStorage.setItem("vibrate",     gVibrate    ? "1" : "0");
    localStorage.setItem("batteryBar",  gBatteryBar ? "1" : "0");
    localStorage.setItem("tzOffset",    String(gTzOffset));
    localStorage.setItem("tickerSpeed", gTickerSpeed);

    console.log("[NHL] Settings – team: " + TEAMS[gTeamIdx].abbr +
      " vibrate: " + gVibrate + " battery: " + gBatteryBar +
      " tz: " + gTzOffset + " tickerSpeed: " + gTickerSpeed);

    var settingsMsg = {};
    settingsMsg[KEY_TEAM_IDX]     = gTeamIdx;
    settingsMsg[KEY_VIBRATE]      = gVibrate    ? 1 : 0;
    settingsMsg[KEY_BATTERY_BAR]  = gBatteryBar ? 1 : 0;
    settingsMsg[KEY_TZ_OFFSET]    = gTzOffset;
    settingsMsg[KEY_TICKER_SPEED] = SPEED_NUM[gTickerSpeed] || 5000;
    Pebble.sendAppMessage(settingsMsg,
      function() { fetchGameData(gTeamIdx); },
      function() { fetchGameData(gTeamIdx); }
    );
  } catch(ex) {
    console.log("[NHL] webviewclosed error: " + ex);
  }
});
