// Phlog - Team fortress 2 debugging utility

//
// Configuration options
//

const PHLOG_VERSION_MAJOR = 0;
const PHLOG_VERSION_MINOR = 0;
const PHLOG_VERSION_PATCH = 1;

// Max recursion level when converting tables to strings
const PHLOG_TABLE_TO_STRING_MAX_DEPTH = 1;

//
// Utility functions
//

function UTIL_Fmt(fmt, ...) {
    return UTIL_FmtV(fmt, vargv);
}

function UTIL_FmtV(fmt, vargv) {
    local result = "";
    local fmtspec_capture = false;
    local fmtspec_idx = 0;
    foreach (i, c in fmt) {
        if (c == '{' && (i == 0 || fmt[i - 1] != '\\')) {
            fmtspec_capture = true;
        } else if (fmtspec_capture && c == '}') {
            result += UTIL_ToString(vargv[fmtspec_idx++]);
            fmtspec_capture = false;
        } else {
            result += format("%c", c);
        }
    }
    return result;
}

function UTIL_ToString(val, tableDepth = 0) {
    switch (typeof val) {
        // Core types
        case "bool":    { return val ? "true" : "false" } break;
        case "float":   { return format("%f", val); } break;
        case "integer": { return format("%d", val); } break;
        case "string":  { return val; } break;
        // Composite types
        case "array": {
            local middle = ""
            foreach (v in val) {
                if (middle != "") {
                    middle += ", ";
                }
                middle += UTIL_ToString(v);
            }
            return format("[ %s ]", middle);
        } break;
        case "table": {
            if (tableDepth > PHLOG_TABLE_TO_STRING_MAX_DEPTH) {
                return "(table)";
            }
            local middle = "";
            foreach (key, value in val) {
                if (middle != "") {
                    middle += ", ";
                }
                middle += format("%s: %s", key, UTIL_ToString(value, tableDepth + 1));
            }
            return format("{ %s }", middle);
        } break;
        // Catch-all
        default:{
            return format("(%s)", typeof val);
        } break;
    };
}

function UTIL_Error(fmt, ...) {
    UTIL_SayAll(UTIL_Fmt("\x07FF0000[Phlog] {}", UTIL_FmtV(fmt, vargv)));
}

function UTIL_Warn(fmt, ...) {
    UTIL_SayAll(UTIL_Fmt("\x07FFFF00[Phlog] {}", UTIL_FmtV(fmt, vargv)));
}

function UTIL_Msg(fmt, ...) {
    UTIL_SayAll(UTIL_Fmt("\x0700FF00[Phlog] {}", UTIL_FmtV(fmt, vargv)));
}

function UTIL_SayAll(msg) {
    for (local i = 1; i <= MaxClients().tointeger(); ++i) {
        local player = PlayerInstanceFromIndex(i);
        if (player) {
            ClientPrint(player, Constants.EHudNotify.HUD_PRINTTALK, msg);
        }
    }
}

function UTIL_IsStringNumeric(str) {
    foreach (c in str) {
        if (c < '0' || c > '9') {
            return false;
        }
    }
    return true;
}

function UTIL_GetEntityUnderCrosshair(plr) {
    local v1 = plr.EyePosition();
    local v2 = plr.EyePosition() + UTIL_AngleVectors(plr.EyeAngles()).Scale(10000.0);
    local trace = {
        start   = v1,
        end     = v2,
        ignore  = plr,
    };
    if (TraceLineEx(trace)) {
        return trace.enthit;
    }
    return null;
}

function UTIL_Radians(deg) {
    return deg * PI / 180.0;
}

function UTIL_AngleVectors(angle) {
    // https://github.com/ValveSoftware/source-sdk-2013/blob/0d8dceea4310fde5706b3ce1c70609d72a38efdf/mp/src/mathlib/mathlib_base.cpp#L901
    local pitch = UTIL_Radians(angle.x);
    local yaw   = UTIL_Radians(angle.y);
    local sp    = sin(pitch);
    local cp    = cos(pitch);
    local sy    = sin(yaw);
    local cy    = cos(yaw);
    return Vector(cp * cy, cp * sy, -sp);
}

//
// Command parsing
//

class CommandArguments {
    argv    = null;
    argv_i  = null;

    constructor(text) {
        argv    = [];
        argv_i  = 1;

        local arg = "";
        local quote_depth = 0;
        foreach (i, c in text) {
            if (c == ' ' && quote_depth == 0) {
                argv.append(arg);
                arg = "";
            } else {
                if (c == '\'') {
                    local next_non_quote_char = null;
                    for (local j = i; j < text.len(); ++j) {
                        if (text[j] != '\'') {
                            next_non_quote_char = text[j];
                            break;
                        }
                    }
                    if (next_non_quote_char == null || next_non_quote_char == ' ') {
                        --quote_depth;
                    } else {
                        ++quote_depth;
                    }
                } else {
                    arg += format("%c", c);
                }
            }
        }
        if (arg != "") {
            argv.append(arg);
        }
    }

    function match(name) {
        return argv[0] == name;
    }

    function arg_string() {
        if (argv_i >= argv.len()) {
            return null;
        }
        return argv[argv_i++];
    }
}

//
// Entity selectors
//

class EntitySelector {

    valid   = null;

    sel_a   = null;
    sel_c   = null;
    sel_e   = null;
    sel_i   = null;
    sel_p   = null;

    constructor(sel, player, multi = true){
        valid = false;

        // Select by entity index
        if (UTIL_IsStringNumeric(sel)) {
            sel_i = sel.tointeger();
        }
        // Select all players
        else if (sel == "@a" && multi) {
            sel_a = true;
        }
        // Select entity under crosshair
        else if (sel == "@c") {
            sel_c = player;
        }
        // Select all entities
        else if (sel == "@e" && multi) {
            sel_e = true;
        }
        // Select self
        else if (sel == "@p") {
            sel_p = player;
        }

        valid = sel_a || sel_c || sel_e || sel_i || sel_p;
    }

    function evaluate() {
        local result = [];
        if (sel_i != null) {
            for (local ent = Entities.First(); ent; ent = Entities.Next(ent)) {
                if (ent.entindex() == sel_i) {
                    result.append(ent);
                }
            }
        }
        else if (sel_a != null) {
            for (local i = 1; i <= MaxClients().tointeger(); ++i) {
                local player = PlayerInstanceFromIndex(i);
                if (player) {
                    result.append(player);
                }
            }
        }
        else if (sel_c != null) {
            local e = UTIL_GetEntityUnderCrosshair(sel_c);
            if (e) {
                result.append(e);
            }
        }
        else if (sel_e != null) {
            for (local ent = Entities.First(); ent; ent = Entities.Next(ent)) {
                result.append(ent);
            }
        }
        else if (sel_p != null) {
            result.append(sel_p);
        }
        return result;
    }
}

//
// Root commands
//

function CMD_q(cmd, player) {
    local helpstr = "Phlog v{}.{}.{}:\n";
    helpstr += "\tpe?\tEntity commands";
    UTIL_Msg(helpstr, PHLOG_VERSION_MAJOR, PHLOG_VERSION_MINOR, PHLOG_VERSION_PATCH);
}

//
// Entity commands
//

function CMD_e_q(cmd, player) {
    local helpstr1 = "Entity commands:\n";
    helpstr1 += "\tpef\t\tEntity fire input\n";
    helpstr1 += "\tpei\t\tEntity info\n";
    helpstr1 += "\tpek\t\tEntity kill\n";
    helpstr1 += "\tpen\t\tEntity net prop\n";
    UTIL_Msg(helpstr1);
    local helpstr2 = "Entity selectors:\n";
    helpstr2 += "\t<id>\t\tSelect by index\n";
    helpstr2 += "\t@a\t\tSelect all players\n";
    helpstr2 += "\t@c\t\tSelect entity under crosshair\n";
    helpstr2 += "\t@e\t\tSelect all entities\n";
    helpstr2 += "\t@p\t\tSelect the calling player\n";
    UTIL_Msg(helpstr2);

}

// entity fire
function CMD_ef(cmd, player) {
    local a_sel = cmd.arg_string();
    local a_act = cmd.arg_string();
    local a_val = cmd.arg_string();
    if (!a_sel || !a_act) {
        UTIL_Error("Usage: !pef <entity> <action> [value]");
        return;
    }
    local sel = EntitySelector(a_sel, player);
    if (!sel.valid) {
        UTIL_Error("Entity selector \"{}\" is invalid", a_sel);
        return;
    }
    local targets = sel.evaluate();
    if (targets.len() == 0) {
        UTIL_Error("No entity could be found with selector \"{}\"", a_sel);
        return;
    }
    foreach (target in targets) {
        EntFireByHandle(target, a_act, a_val, -1.0, player, player);
    }
}

// entity info
function CMD_ei(cmd, player) {
    local a_sel = cmd.arg_string();
    if (!a_sel) {
        UTIL_Error("Usage: !pei <entity>");
        return;
    }
    local sel = EntitySelector(a_sel, player);
    if (!sel.valid) {
        UTIL_Error("Entity selector \"{}\" is invalid", a_sel);
        return;
    }
    local targets = sel.evaluate();
    if (targets.len() == 0) {
        UTIL_Error("No entity could be found with selector \"{}\"", a_sel);
        return;
    }
    foreach (target in targets) {
        UTIL_Msg("[#{}] {}", target.GetEntityIndex(), target.GetClassname());
    }
}

// entity kill
function CMD_ek(cmd, player) {
    local a_sel = cmd.arg_string();
    if (!a_sel) {
        UTIL_Error("Usage: !pek <entity>");
        return;
    }
    local sel = EntitySelector(a_sel, player);
    if (!sel.valid) {
        UTIL_Error("Entity selector \"{}\" is invalid", a_sel);
        return;
    }
    local targets = sel.evaluate();
    if (targets.len() == 0) {
        UTIL_Error("No entity could be found with selector \"{}\"", a_sel);
        return;
    }
    foreach (target in targets) {
        target.Kill();
    }
}

// entity netprop
function CMD_en(cmd, player) {
    local a_sel     = cmd.arg_string();
    local a_prop    = cmd.arg_string();
    if (!a_sel || !a_prop) {
        UTIL_Error("Usage: !pen <entity> <property>");
        return;
    }
    local sel = EntitySelector(a_sel, player);
    if (!sel.valid) {
        UTIL_Error("Entity selector \"{}\" is invalid", a_sel);
        return;
    }
    local targets = sel.evaluate();
    if (targets.len() == 0) {
        UTIL_Error("No entity could be found with selector \"{}\"", a_sel);
        return;
    }
    foreach (target in targets) {
        if (!NetProps.HasProp(target, a_prop)) {
            UTIL_Warn("Class {} has no property \"{}\"", target.GetClassname(), a_prop);
            continue;
        }
        local val = null;
        switch (NetProps.GetPropType(target, a_prop)) {
            case "float":   { val = UTIL_Fmt("{}", NetProps.GetPropFloat(target, a_prop)); } break;
            case "integer": { val = UTIL_Fmt("{}", NetProps.GetPropInt(target, a_prop)); } break;
        };
        UTIL_Msg("[#{}] {}.{}: {}", target.GetEntityIndex(), target.GetClassname(), a_prop, val);
    }
}

function OnGameEvent_player_say(msg) {
    local player = GetPlayerFromUserID(msg.userid);
    if (!player) {
        UTIL_Error("Message issued by invalid player {}", msg.userid);
        return;
    }
    local args = CommandArguments(msg.text);
    if (args.match("!p?")) { CMD_q(args, player); }
    // Entity commands
    else if (args.match("!pe?")) { CMD_e_q(args, player); }
    else if (args.match("!pef")) { CMD_ef(args, player); }
    else if (args.match("!pei")) { CMD_ei(args, player); }
    else if (args.match("!pek")) { CMD_ek(args, player); }
    else if (args.match("!pen")) { CMD_en(args, player); }

}
__CollectGameEventCallbacks(this);

UTIL_Msg("Loaded v{}.{}.{}", PHLOG_VERSION_MAJOR, PHLOG_VERSION_MINOR, PHLOG_VERSION_PATCH);