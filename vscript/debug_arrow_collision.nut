// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// Think
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

state <- []

function GetCachedArrowIndex(handle) {
    for (local i = 0; i < state.len(); i = i + 1) {
        if (state[i].handle == handle) {
            return i;
        }
    }
    return -1;
}

function DebugArrowCollisionThink() {
    // Remove invalid
    for (local i = 0; i < state.len(); i = i + 1) {
        if (!state[i].handle.IsValid()) {
            state.remove(i);
            i = 0;
        }
    }
    local ent = null
    while (ent = Entities.FindByClassname(ent, "tf_projectile_arrow")) {
        // Check if this arrow is known
        local idx = GetCachedArrowIndex(ent);
        if (idx == -1) {
            // Add it
            info <- {
                handle      = ent,
                last_pos    = Vector(0, 0, 0),
                last_vel    = Vector(0, 0, 0),
            };
            state.append(info)
        }

        idx = GetCachedArrowIndex(ent);
        
        // Render trail
        if (state[idx].last_pos.Length() != 0) {
            DebugDrawLine(state[idx].last_pos, ent.GetOrigin(), 255, 0, 255, true, 3.0);
        }
        state[idx].last_pos = ent.GetOrigin();

        // Detect a hit
        if (ent.GetAbsVelocity().Length() == 0 && state[idx].last_vel.Length() > 0) {
            DebugDrawText(ent.GetOrigin(), "!!! COLLISION !!!", false, 3.0);
        }
        state[idx].last_vel = ent.GetAbsVelocity();


        // if (state.find())
        // local v1 = ent.GetOrigin();
        // local v2 = Vector(v1.x, v1.y, v1.z - 500);
        // local frac = TraceLine(v1, v2, null)
        // v2.z = v1.z - (500 * frac);
        // DebugDrawLine(v1, v2, 0, 255, 0, true, 1.0);
    }
}

// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// Create driver entity
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

local ent = Entities.FindByName(null, "debug_arrow_collision_driver");
if (!ent) {
    ent = SpawnEntityFromTable("logic_script", { targetname = "debug_arrow_collision_driver" });
}

ent.ValidateScriptScope();
ent.GetScriptScope()["UserThink"] <- function() {
    DebugArrowCollisionThink();
    DoEntFire("!self", "CallScriptFunction", "UserThink", 0.01, self, self);
};
DoEntFire("!self", "CallScriptFunction", "UserThink", 0.01, ent, ent);