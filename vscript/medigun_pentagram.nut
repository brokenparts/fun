// Commands for replicating on itemtest:
//
// mp_tournament 1
// bot -class medic -name med1 -team red
// bot -class medic -name med2 -team red
// bot -class medic -name med3 -team red
// bot -class medic -name med4 -team red

// do bot_mimic 1, switch to medi gun, bot_mimic 0

// bot_teleport med1 24 -243 -116 0 -90 0
// bot_teleport med2 11 -508 -116 0 135 0
// bot_teleport med3 -130 -320 -116 0 -15 0
// bot_teleport med4 117 -387 -116 0 -157 0

// bot_forceattack 1

local PENTAGRAM_DEBUG = true;

// Helper: Recursively build a list of players chained together by medi beams
::CheckForMedicPentagram_BuildBeamChainRecursive <- function(player, max_depth) {
  local result = [];
  if (player.GetPlayerClass() == Constants.ETFClass.TF_CLASS_MEDIC) {
    result.append(player);
    local patient = player.GetHealTarget();
    if (patient && max_depth > 1) {
      result.extend(CheckForMedicPentagram_BuildBeamChainRecursive(patient, max_depth - 1));
    }
  } 
  return result;
}

// Helper: 2D vector cross product. Used as an edge function
::CheckForMedicPentagram_OrientLines <- function(p1, p2, test) {
  return (test.y - p1.y) * (p2.x - p1.x) > (p2.y - p1.y) * (test.x - p1.x);
}

// Helper: Check if 2D line segments intersect. Z is ignored.
::CheckForMedicPentagram_IntersectLines <- function(l1p1, l1p2, l2p1, l2p2) {
  return CheckForMedicPentagram_OrientLines(l1p1, l2p1, l2p2) != CheckForMedicPentagram_OrientLines(l1p2, l2p1, l2p2) && CheckForMedicPentagram_OrientLines(l1p1, l1p2, l2p1) != CheckForMedicPentagram_OrientLines(l1p1, l1p2, l2p2);
}

::CheckForMedicPentagram <- function() {
  for (local i = 1; i <= MaxClients().tointeger(); ++i) {
    local player = PlayerInstanceFromIndex(i);
    if (!player) {
      continue;
    }
    // Build a list of players chained together by medi beams
    local chain = CheckForMedicPentagram_BuildBeamChainRecursive(player, 6);
    if (chain.len() != 6) {
      continue;
    }
    // Start and end point should match
    if (chain[0].entindex() != chain[5].entindex()) {
      continue;
    }
    chain = chain.slice(0, 5);
    // Verify that the line segments from player to player form a pentagon
    local chain_points = chain.map(function(player) {
      local origin = player.GetOrigin();
      return Vector(origin.x, origin.y, 0.0);
    });
    if (0) {
      local timer = floor(Time() * 4);
      local z = GetListenServerHost().GetOrigin().z;
      local p1 = chain_points[0]; p1.z = z;
      local p2 = chain_points[1]; p2.z = z;
      DebugDrawLine(p1, p2, 0x00, 0xFF, 0x00, false, 0.25);
      local p1 = chain_points[(2 + timer) % 5]; p1.z = z;
      local p2 = chain_points[(3 + timer) % 5]; p2.z = z;
      DebugDrawLine(p1, p2, 0xFF, 0xFF, 0x00, false, 0.25);
    }
    // Each line segment should intersect two other line segments
    for (local j = 0; j < 5; ++j) {
      local intersection_count = 0;
      local l1p1 = chain_points[j];
      local l1p2 = chain_points[(j + 1) % 5];
      for (local k = 0; k < 5; ++k) {
        if (j == k || k + 1 == j) {
          continue;
        }
        local l2p1 = chain_points[k];
        local l2p2 = chain_points[(k + 1) % 5];
        if (CheckForMedicPentagram_IntersectLines(l1p1, l1p2, l2p1, l2p2)) {
          ++intersection_count;
        }
      }
      if (intersection_count != 2) {
        if (PENTAGRAM_DEBUG) {
          local z = GetListenServerHost().GetOrigin().z;
          local p1 = l1p1; p1.z = z;
          local p2 = l1p2; p2.z = z;
          DebugDrawLine(p1, p2, 0xFF, 0xFF, 0x00, false, 0.25);
        }
        printl("Pentagram failed, intersection count: " + intersection_count)
        return false;
      }
    }
    return true;
  }
  return false;
}
if (PENTAGRAM_DEBUG) {
  // Call this every second or something. I have no clue how expensive the check is but it might be bad. I don't know.
  if (CheckForMedicPentagram()) {
    DebugDrawScreenTextLine(0.05, 0.05, 0, "Pentagram: YES", 0x00, 0xFF, 0x00, 0xFF, 0.25);
  } else {
    DebugDrawScreenTextLine(0.05, 0.05, 0, "Pentagram: NO", 0x00, 0xFF, 0x00, 0xFF, 0.25);
  }
}