#include "ap_integration.h"
// For Numsprites and sprite
#include "build.h"
// For G_AddGroup
#include "common.h"
// For g_scriptNamePtr
#include "common_game.h"
// For ud user defines
#include "global.h"
// For JSON features
#include <json/json.h>
#include <json/reader.h>
// For Menu manipulation
#include "menus.h"
// For victory check
#include "screens.h"
// For sounds
#include "soundefs.h"

uint8_t ap_return_to_menu = 0;

std::string ap_message_str;

// Convenience access to player struct
#define ACTIVE_PLAYER g_player[myconnectindex].ps

static inline ap_location_t safe_location_id(Json::Value& val)
{
    if (val.isInt())
        return AP_SHORT_LOCATION(val.asInt());
    return -1;
}

// Map name in format EYLX[X]
std::string ap_format_map_id(uint8_t level_number, uint8_t volume_number)
{
    if(level_number == 7 && volume_number == 0)
    {
        // user map. Not sure we want to support this in general, but might as well do the right thing here
        return std::string(boardfilename);
    }
    return "E" + std::to_string(volume_number + 1) + "L" + std::to_string(level_number + 1);
}

std::string current_map = "";

static void ap_add_processor_sprite(void)
{
    spritetype *processor;
    short       new_idx;
    new_idx = insertsprite(0, 0);
    if (new_idx < 0 || new_idx >= MAXSPRITES)
    {
        initprintf("Injecting AP Processor sprite failed\n");
        return;
    }
    processor           = &sprite[new_idx];
    processor->cstat    = 0;
    processor->ang      = 0;
    processor->owner    = 0;
    processor->picnum   = AP_PROCESSOR;
    processor->pal      = 0;
    processor->xoffset  = 0;
    processor->yoffset  = 0;
    processor->xvel     = 0;
    processor->yvel     = 0;
    processor->zvel     = 0;
    processor->shade    = 8;
    processor->xrepeat  = 0;
    processor->yrepeat  = 0;
    processor->clipdist = 0;
    processor->extra    = 0;
}

/*
  Patches sprites loaded from a map file based on active shuffle settings.
*/
static void ap_map_patch_sprites(void)
{
    std::string map = current_map;
    Json::Value sprite_locations = ap_game_config["locations"][map]["sprites"];
    int32_t i;
    Json::Value sprite_info;
    int32_t use_sprite;
    int location_id;
    ap_location_t sprite_location;
    std::vector<int32_t> to_delete;
    for (i = 0; i < Numsprites; i++)
    {
        sprite_info = sprite_locations[std::to_string(i)];
        if (!sprite_info["id"].isInt()) continue;  // Not a sprite described in the AP Game Data, ignore it
        location_id = sprite_info["id"].asInt();
        if (location_id < 0)
            use_sprite = 0;
        else
        {
            sprite_location = AP_SHORT_LOCATION(location_id);
            use_sprite  = AP_VALID_LOCATION(sprite_location);
        }
        if (use_sprite)
        {
            // Have a sprite that should become an AP Pickup
            sprite[i].lotag  = sprite_location;
            sprite[i].picnum = AP_LOCATION_PROGRESSION(sprite_location) ? AP_PROG__STATIC : AP_ITEM__STATIC;
            bitmap_set(show2dsprite, i);
        }
        else
        {
            // Unused sprite, set it up for deletion
            to_delete.push_back(i);
        }
    }

    for (auto i: to_delete)
        A_DeleteSprite(i);

    // Inject an AP_PROCESSOR sprite into the map
    ap_add_processor_sprite();
}

void ap_delete_collected_sprites(void)
{
    std::vector<int32_t> to_delete;
    for (int32_t i = 0; i < Numsprites; i++)
    {
        switch(sprite[i].picnum)
        {
        case AP_PROG__STATIC:
        case AP_ITEM__STATIC:
            if (AP_LOCATION_CHECKED(sprite[i].lotag))
                to_delete.push_back(i);
            break;
        }
    }
    for (auto i: to_delete)
        A_DeleteSprite(i);
}

#ifdef AP_DEBUG_ON
/* Helps with finding secrets and spawning cans for location definition */
static void print_debug_level_info()
{
    std::stringstream out;
    out << "Secret sectors for " << current_map << ":\n";
    int32_t i;
    for (i = 0; i < numsectors; i++)
    {
        if (sector[i].lotag == 32767) {
            out << "    " << i << "\n";
        }
    }
    AP_Debugf(out.str().c_str());

    out = std::stringstream();
    out << "Trash cans for " << current_map << ":\n";
    for (i = 0; i < Numsprites; i++)
    {
        if (sprite[i].picnum == CANWITHSOMETHING__STATIC)
        {
            out << "    " << i << " with " << sprite[i].lotag << "\n";
        }
    }
    AP_Debugf(out.str().c_str());

    out = std::stringstream();
    out << "Missed Locations for " << current_map << ":\n";
    for (i = 0; i < Numsprites; i++)
    {
        switch (sprite[i].picnum)
        {
        case CHAINGUNSPRITE__STATIC:
        case RPGSPRITE__STATIC:
        case FREEZESPRITE__STATIC:
        case SHRINKERSPRITE__STATIC:
        case TRIPBOMBSPRITE__STATIC:
        case SHOTGUNSPRITE__STATIC:
        case DEVISTATORSPRITE__STATIC:
        case HBOMBAMMO__STATIC:
        case FIRSTAID__STATIC:
        case SHIELD__STATIC:
        case STEROIDS__STATIC:
        case AIRTANK__STATIC:
        case JETPACK__STATIC:
        case HEATSENSOR__STATIC:
        case ACCESSCARD__STATIC:
        case BOOTS__STATIC:
        case ATOMICHEALTH__STATIC:
        case HOLODUKE__STATIC:
            out << "    " << i << " of type " << sprite[i].picnum << " at x: " << sprite[i].x << " y: " << sprite[i].y;
            if (sprite[i].pal != 0)
                out << " (MP only)";
            out << "\n";
            break;
        }
    }
    AP_Debugf(out.str().c_str());
    OutputDebugString(out.str().c_str());
}

// Prints a level template json for filling in location names
static void print_level_template(void)
{
    Json::Value out = Json::arrayValue;

    for (unsigned int i = 0; i < Numsprites; i++)
    {
        Json::Value loc = Json::objectValue; 
        switch (sprite[i].picnum)
        {
        case CHAINGUNSPRITE__STATIC:
        case RPGSPRITE__STATIC:
        case FREEZESPRITE__STATIC:
        case SHRINKERSPRITE__STATIC:
        case TRIPBOMBSPRITE__STATIC:
        case SHOTGUNSPRITE__STATIC:
        case DEVISTATORSPRITE__STATIC:
        case HBOMBAMMO__STATIC:
        case FIRSTAID__STATIC:
        case SHIELD__STATIC:
        case STEROIDS__STATIC:
        case AIRTANK__STATIC:
        case JETPACK__STATIC:
        case HEATSENSOR__STATIC:
        case BOOTS__STATIC:
        case ATOMICHEALTH__STATIC:
        case HOLODUKE__STATIC:
        case MONK__STATIC:
        case CANWITHSOMETHING2__STATIC:
        case ACCESSCARD__STATIC:
            loc["name"] = std::to_string(i) +" ";
            loc["id"] = i;
            loc["type"] = "sprite";
            if(sprite[i].pal != 0 && sprite[i].picnum != ACCESSCARD__STATIC)
            {
                loc["name"] = "MP " + loc["name"].asString();
                loc["mp"] = Json::booleanValue;
                loc["mp"] = true;
            }
            break;
        }

        bool has_relevant_content = false;
        switch (sprite[i].lotag)
        {
        case CHAINGUNSPRITE__STATIC:
        case RPGSPRITE__STATIC:
        case FREEZESPRITE__STATIC:
        case SHRINKERSPRITE__STATIC:
        case TRIPBOMBSPRITE__STATIC:
        case SHOTGUNSPRITE__STATIC:
        case DEVISTATORSPRITE__STATIC:
        case HBOMBAMMO__STATIC:
        case FIRSTAID__STATIC:
        case SHIELD__STATIC:
        case STEROIDS__STATIC:
        case AIRTANK__STATIC:
        case JETPACK__STATIC:
        case HEATSENSOR__STATIC:
        case BOOTS__STATIC:
        case ATOMICHEALTH__STATIC:
        case ACCESSCARD__STATIC:
        case HOLODUKE__STATIC:
            has_relevant_content = true;
            break;
        }

        bool store = true;
        switch (sprite[i].picnum)
        {
        case CHAINGUNSPRITE__STATIC:
            loc["name"] = loc["name"].asString() + "Chaingun";
            break;
        case RPGSPRITE__STATIC:
            loc["name"] = loc["name"].asString() + "RPG";
            break;
        case FREEZESPRITE__STATIC:
            loc["name"] = loc["name"].asString() + "Freezethrower";
            break;
        case SHRINKERSPRITE__STATIC:
            loc["name"] = loc["name"].asString() + "Shrinker";
            break;
        case TRIPBOMBSPRITE__STATIC:
            loc["name"] = loc["name"].asString() + "Tripbomb";
            break;
        case SHOTGUNSPRITE__STATIC:
            loc["name"] = loc["name"].asString() + "Shotgun";
            break;
        case DEVISTATORSPRITE__STATIC:
            loc["name"] = loc["name"].asString() + "Devastator";
            break;
        case HBOMBAMMO__STATIC:
            loc["name"] = loc["name"].asString() + "Pipebombs";
            break;
        case FIRSTAID__STATIC:
            loc["name"] = loc["name"].asString() + "Medkit";
            break;
        case SHIELD__STATIC:
            loc["name"] = loc["name"].asString() + "Armor";
            break;
        case STEROIDS__STATIC:
            loc["name"] = loc["name"].asString() + "Steroids";
            break;
        case AIRTANK__STATIC:
            loc["name"] = loc["name"].asString() + "Scuba Gear";
            break;
        case JETPACK__STATIC:
            loc["name"] = loc["name"].asString() + "Jetpack";
            break;
        case HEATSENSOR__STATIC:
            loc["name"] = loc["name"].asString() + "Night Vision Goggles";
            break;
        case ACCESSCARD__STATIC:
            loc["name"] = loc["name"].asString() + (sprite[i].pal == 0 ? "Blue " : (sprite[i].pal == 21 ? "Red " : "Yellow ")) + "Key Card";
            break;
        case BOOTS__STATIC:
            loc["name"] = loc["name"].asString() + "Protective Boots";
            break;
        case ATOMICHEALTH__STATIC:
            loc["name"] = loc["name"].asString() + "Atomic Health";
            break;
        case HOLODUKE__STATIC:
            loc["name"] = loc["name"].asString() + "Holo Duke";
            break;
        case MONK__STATIC:
            if (!has_relevant_content)
            {
                store = false;
                break;
            }
            loc["name"] = loc["name"].asString() + "Monk";
            loc["sprite_type"] = "monk";
            break;
        case CANWITHSOMETHING2__STATIC:
            if (!has_relevant_content)
            {
                store = false;
                break;
            }
            loc["name"] = loc["name"].asString() + "Trash Can";
            loc["sprite_type"] = "trashcan";
            break;
        default:
            store = false;
        }
        if (store)
            out.append(loc);
    }

    uint16_t secretcnt = 1;
    for (unsigned int i = 0; i < numsectors; i++)
    {
        if (sector[i].lotag == 32767)
        {
            Json::Value loc = Json::objectValue;
            loc["name"] = "Secret " + std::to_string(secretcnt);
            secretcnt++;
            loc["id"] = i;
            loc["type"] = "sector";
            out.append(loc);
        }
    }

    Json::Value loc = Json::objectValue;
    loc["name"] = "Exit";
    loc["id"] = 0;
    loc["type"] = "exit";
    out.append(loc);

    Json::FastWriter writer;
    OutputDebugString(writer.write(out).c_str());
}
#endif

/*
  Marks all secret sectors corresponding to already checked
  locations as already collected.
*/
static void ap_mark_known_secret_sectors(bool from_save)
{
    int32_t i;
    Json::Value cur_secret_locations = ap_game_config["locations"][current_map]["sectors"];
    Json::Value sector_info;
    for (i = 0; i < numsectors; i++)
    {
        if (sector[i].lotag == 32767)
        {
            // Secret sector, check if it is a valid location for the AP seed and has been collected already
            sector_info = cur_secret_locations[std::to_string(i)];
            ap_location_t location_id = safe_location_id(sector_info["id"]);
            if (location_id < 0)
                continue;
            if AP_LOCATION_CHECKED(location_id)
            {
                // Already have this secret, disable the sector and mark it as collected
                sector[i].lotag = 0;
                ACTIVE_PLAYER->secret_rooms++;
                // Also increase the max secret count for this one as the sector will no longer be tallied when
                // the map is processed further
                // Save files already have a valid max tally, so we can skip this when loading a save
                if (!from_save)
                    ACTIVE_PLAYER->max_secret_rooms++;
            }
        }
    }
}

/*
  Load correct game data based on configured AP settings
*/
void ap_startup(void)
{
    // [AP] Get connection settings from user defs
    ap_connection_settings.mode = AP_DISABLED;
    if (Bstrlen(ud.setup.ap_user) > 0)
    {
        ap_connection_settings.mode = AP_SERVER;
        ap_connection_settings.player = ud.setup.ap_user;
        ap_connection_settings.ip = ud.setup.ap_server;
        ap_connection_settings.password = ud.setup.ap_pass;
        AP_Printf("Connecting to AP Server " + std::string(ud.setup.ap_server) + " as " + std::string(ud.setup.ap_user));
    }
    if (ap_connection_settings.mode == AP_DISABLED)
    {
        AP_Printf("No AP connection settings defined; Launching vanilla game.");
        return;
    }
}

Json::Value read_json_from_grp(const char* filename)
{
    int kFile = kopen4loadfrommod(filename, 0);

    if (kFile == -1)  // JBF: was 0
    {
        return NULL;
    }

    int const kFileLen = kfilelength(kFile);
    char     *mptr     = (char *)Xmalloc(kFileLen + 1);
    mptr[kFileLen]     = 0;
    kread(kFile, mptr, kFileLen);
    kclose(kFile);

    Json::Value ret;
    Json::Reader reader;

    reader.parse(std::string(mptr, kFileLen), ret);
    Xfree(mptr);

    return ret;
}

std::string ap_episode_names[MAXVOLUMES] = { "" };
std::vector<uint8_t> ap_active_episodes;
std::vector<std::vector<uint8_t>> ap_active_levels;
std::map<std::string, Json::Value> ap_level_data;

void ap_parse_levels()
{
    ap_level_data.clear();
    ap_active_episodes.clear();
    ap_active_levels.clear();

    // Iterate over all episodes defined in the game config
    // JSON iteration can be out of order, use flags to keep things sequential
    Json::Value tmp_levels[MAXVOLUMES][MAXLEVELS] = { 0 };

    for (std::string ep_id : ap_game_config["episodes"].getMemberNames())
    {
        uint8_t volume = ap_game_config["episodes"][ep_id]["volumenum"].asInt();
        ap_episode_names[volume] = ap_game_config["episodes"][ep_id]["name"].asString();
        for (std::string lev_id : ap_game_config["episodes"][ep_id]["levels"].getMemberNames())
        {
            uint8_t levelnum = ap_game_config["episodes"][ep_id]["levels"][lev_id]["levelnum"].asInt();
            ap_net_id_t unlock = ap_game_config["episodes"][ep_id]["levels"][lev_id]["unlock"].asInt64();
            if (AP_IsLevelUsed(unlock))
            {
                ap_level_data[ap_format_map_id(levelnum, volume)] = ap_game_config["episodes"][ep_id]["levels"][lev_id];
            }
        }
    }

    // Now agregate and insert in correct order
    for(int i=0; i < MAXVOLUMES; i++) {
        std::vector<uint8_t> episode_active_levels;
        for (int j = 0; j < MAXLEVELS; j++) {
            if (ap_level_data.count(ap_format_map_id(j, i))) {
                episode_active_levels.push_back(j);
            }
        }

        if (episode_active_levels.size())
        {
            ap_active_episodes.push_back(i);
        }
        ap_active_levels.push_back(episode_active_levels);
    }

}

void ap_fix_spawns(void)
{
    // Tune blimp spawn items to not include progression stuff
    g_blimpSpawnItems[0] = RPGAMMO;
    g_blimpSpawnItems[1] = DEVISTATORAMMO;
    g_blimpSpawnItems[2] = DEVISTATORAMMO;
    g_blimpSpawnItems[3] = RPGAMMO;
    g_blimpSpawnItems[4] = RPGAMMO;
    g_blimpSpawnItems[5] = DEVISTATORAMMO;
    g_blimpSpawnItems[6] = DEVISTATORAMMO;
    g_blimpSpawnItems[7] = SIXPAK;
    g_blimpSpawnItems[8] = SIXPAK;
    g_blimpSpawnItems[9] = RPGAMMO;
    g_blimpSpawnItems[10] = RPGAMMO;
    g_blimpSpawnItems[11] = SIXPAK;
    g_blimpSpawnItems[12] = RPGAMMO;
    g_blimpSpawnItems[13] = FREEZEAMMO;
    g_blimpSpawnItems[14] = FREEZEAMMO;
}

ap_connection_settings_t ap_connection_settings = {AP_DISABLED, "", "", "", ""};

void ap_initialize(void)
{
    if (ap_connection_settings.mode == AP_DISABLED) return;
    Json::Value game_ap_config = read_json_from_grp("ap_config.json");
    if (game_ap_config.isNull()) return;

    ap_connection_settings.game = game_ap_config["game"].asCString();
    AP_Initialize(game_ap_config, ap_connection_settings);

    if (AP)
    {
        // Additional initializations after the archipelago setup is done
        ap_parse_levels();
        // Fix spawns
        ap_fix_spawns();
    }
}

// Safe check if an item is scoped to a level
static inline bool item_for_level(Json::Value& info, uint8_t level, uint8_t volume)
{
    return (info["levelnum"].isInt() && (info["levelnum"].asInt() == level)) && (info["volumenum"].isInt() && (info["volumenum"].asInt() == volume));
}

static inline bool item_for_current_level(Json::Value& info)
{
    return item_for_level(info, ud.level_number, ud.volume_number);
}

static inline int64_t json_get_int(Json::Value& val, int64_t def)
{
    return val.isInt() ? val.asInt() : def;
}

// Track inventory unlock state. There might be a better solution to this?
static uint8_t inv_available[GET_MAX];
static uint16_t inv_capacity[GET_MAX];
static uint16_t inv_max_capacity[GET_MAX];
static std::map<std::string, uint8_t> ability_unlocks;

void force_set_player_weapon(uint8_t weaponnum)
{
    if (ACTIVE_PLAYER->curr_weapon == weaponnum) return;  // Already equipped
    if (!(ACTIVE_PLAYER->gotweapon & (1 << weaponnum))) weaponnum = PISTOL_WEAPON__STATIC;  // Weapon not unlocked, switch to pistol

    switch(weaponnum)
    {
    case SHRINKER_WEAPON__STATIC:
        ACTIVE_PLAYER->subweapon &= ~(1<<GROW_WEAPON);
        break;
    case GROW_WEAPON__STATIC:
        ACTIVE_PLAYER->subweapon |= (1<<GROW_WEAPON);
        break;
    }

    if (RR && weaponnum == HANDBOMB_WEAPON)
        ACTIVE_PLAYER->last_weapon = -1;

    ACTIVE_PLAYER->random_club_frame = 0;

    if (ACTIVE_PLAYER->holster_weapon == 0)
    {
        ACTIVE_PLAYER->weapon_pos = -1;
        ACTIVE_PLAYER->last_weapon = ACTIVE_PLAYER->curr_weapon;
    }
    else
    {
        ACTIVE_PLAYER->weapon_pos = WEAPON_POS_RAISE;
        ACTIVE_PLAYER->holster_weapon = 0;
        ACTIVE_PLAYER->last_weapon = -1;
    }

    ACTIVE_PLAYER->kickback_pic = 0;
    ACTIVE_PLAYER->curr_weapon = weaponnum;
    if (REALITY)
        ACTIVE_PLAYER->dn64_372 = weaponnum;

    switch (DYNAMICWEAPONMAP(weaponnum))
    {
    case SLINGBLADE_WEAPON__STATIC:
    case KNEE_WEAPON__STATIC:
    case TRIPBOMB_WEAPON__STATIC:
    case HANDREMOTE_WEAPON__STATIC:
    case HANDBOMB_WEAPON__STATIC:     break;
    case SHOTGUN_WEAPON__STATIC:      A_PlaySound(REALITY ? 131 : SHOTGUN_COCK, ACTIVE_PLAYER->i); break;
    case PISTOL_WEAPON__STATIC:       A_PlaySound(REALITY ? 4 : INSERT_CLIP, ACTIVE_PLAYER->i); break;
    default:      A_PlaySound(RR ? EJECT_CLIP : (REALITY ? 176 : SELECT_WEAPON), ACTIVE_PLAYER->i); break;
    }
}

void force_set_inventory_item(uint8_t invnum)
{
    if (ACTIVE_PLAYER->inv_amount[invnum] > 0)
        // No idea why displaying armor messes up?
        if (inv_to_icon[invnum] != ICON_NONE && inv_to_icon[invnum] != ICON_SHIELD)
            ACTIVE_PLAYER->inven_icon = inv_to_icon[invnum];
    else
        P_SelectNextInvItem(ACTIVE_PLAYER);
}

/* 
  Apply whatever item we just got to our current game state 

  Upgrade only provides just the unlock, but no ammo/capacity. This is used
  when loading savegames.
*/
static void ap_get_item(ap_net_id_t item_id, bool silent, bool is_new)
{
    Json::Value item_info = ap_item_info[item_id];
    // Check if we have a dynamic override for the item in our seed slot data
    if (!ap_game_settings["dynamic"][std::to_string(item_id)].isNull())
        item_info = ap_game_settings["dynamic"][std::to_string(item_id)];
    bool notify = !(silent || item_info["silent"].asBool());

    // Store counts for stateful items
    if (is_new && item_info["persistent"].asBool())
    {
        uint16_t count = 0;
        if (AP_HasItem(item_id))
        {
            count = ap_game_state.persistent[item_id];
        }
        // Increment
        count++;
        // Set to 1 if it's a unique item
        if (item_info["unique"].asBool())
            count = 1;
        ap_game_state.persistent[item_id] = count;
    }

    std::string item_type = item_info["type"].asString();
    // Poor man's switch
    if (item_type == "progressive")
    {
        // Add to our progressive counter and check how many we have now
        uint16_t prog_count = AP_ProgressiveItem(item_id);
        // And apply whatever item we have next in the queue
        if (item_info["items"].isArray() && item_info["items"].size() > 0)
        {
            // Repeat the last entry if we have more copies
            uint16_t idx = (item_info["items"].size() < prog_count ? item_info["items"].size() : prog_count) - 1;
            ap_net_id_t next_item = AP_NET_ID(json_get_int(item_info["items"][idx], 0));
            ap_get_item(next_item, silent, false);
        }
    }
    else if (item_type == "key" && item_for_current_level(item_info))
    {
        // Key is for current level, apply
        // Lower 3 bits match the flags we have on access cards
        uint8_t key_flag = item_info["flags"].asInt();
        ACTIVE_PLAYER->got_access |= (key_flag & 0x7);
        // Remaining flags are for RR keys
        for (uint8_t i = 0; i < 5; i++)
        {
            if (key_flag & (1 << (i + 2)))
                ACTIVE_PLAYER->keys[i] = 1;
        }
    }
    else if (item_type == "automap" && item_for_current_level(item_info))
    {
        // Enable all sectors
        ud.showallmap = 1;
        Bmemset(show2dsector, 0xFF, 512);
    }
    else if (item_type == "weapon")
    {
        int64_t weaponnum = json_get_int(item_info["weaponnum"], 0);
        if (weaponnum >= MAX_WEAPONS) return;  // Limit to valid weapons
        bool had_weapon = ACTIVE_PLAYER->gotweapon & (1 << weaponnum);
        ACTIVE_PLAYER->gotweapon |= (1 << weaponnum);
        // If it's a new unlock, switch to it
        if (notify && !had_weapon)
        {
            force_set_player_weapon(weaponnum);
        }
        P_AddAmmo(ACTIVE_PLAYER, json_get_int(item_info["weaponnum"], 0), json_get_int(item_info["ammo"], 0));
    }
    else if (item_type == "ammo" || item_type == "maxammo")
    {
        int64_t weaponnum = json_get_int(item_info["weaponnum"], 0);
        if (weaponnum >= MAX_WEAPONS) return;  // Limit to valid weapons
        ACTIVE_PLAYER->max_ammo_amount[weaponnum] += json_get_int(item_info["capacity"], 0);
        P_AddAmmo(ACTIVE_PLAYER, weaponnum, json_get_int(item_info["ammo"], 0));
    }
    else if (item_type == "inventory")
    {
        int64_t invnum = json_get_int(item_info["invnum"], -1);
        if (invnum < 0 || invnum >= GET_MAX) return;  // Limit to valid slots

        // Add capacity
        ACTIVE_PLAYER->inv_amount[invnum] += json_get_int(item_info["capacity"], 0);
        inv_max_capacity[invnum] += json_get_int(item_info["capacity"], 0);
        if (inv_available[invnum] == 0)
        {
            // Also use stored min capacity
            ACTIVE_PLAYER->inv_amount[invnum] += inv_capacity[invnum];
            inv_max_capacity[invnum] += inv_capacity[invnum];
            inv_capacity[invnum] = 0;
        }
        // Mark as unlocked
        inv_available[invnum] = 1;

        // Saturate
        int64_t max_capacity = json_get_int(item_info["max_capacity"], -1);
        // Special case, armor has a dynamic max value tracked already
        // Armor should usually be provided with an "armor" type item instead, but no
        // Reason not to support this
        if (invnum == GET_SHIELD)
            max_capacity = ACTIVE_PLAYER->max_shield_amount;
        if (max_capacity >= 0 && ACTIVE_PLAYER->inv_amount[invnum] > max_capacity)
            ACTIVE_PLAYER->inv_amount[invnum] = max_capacity;

        // And display item
        if (notify)
            force_set_inventory_item(invnum);
    }
    else if (item_type == "invcapacity")
    {
        int64_t invnum = json_get_int(item_info["invnum"], -1);
        if (invnum < 0 || invnum >= GET_MAX) return;  // Limit to valid slots

        // If the item is not unlocked yet, just add it to the min capacity
        if (!inv_available[invnum])
            inv_capacity[invnum] += json_get_int(item_info["capacity"], 0);
        else
        {
            // Inventory item unlocked, just increase capacity
            ACTIVE_PLAYER->inv_amount[invnum] += json_get_int(item_info["capacity"], 0);
            inv_max_capacity[invnum] += json_get_int(item_info["capacity"], 0);
            // Saturate
            int64_t max_capacity = json_get_int(item_info["max_capacity"], -1);
            if (max_capacity >= 0 && ACTIVE_PLAYER->inv_amount[invnum] > max_capacity)
                ACTIVE_PLAYER->inv_amount[invnum] = max_capacity;

            // And display item
            if (notify)
                ACTIVE_PLAYER->inven_icon = inv_to_icon[invnum];
        }
    }
    else if (item_type == "ability")
    {
        if (item_info["enables"].isString())
            ability_unlocks[item_info["enables"].asString()] = 1;
    }
    else if (item_type == "health")
    {
        uint16_t capacity = json_get_int(item_info["capacity"], 0);
        ACTIVE_PLAYER->max_player_health += capacity;
        uint16_t healing = json_get_int(item_info["heal"], 0);
        // Non standard max health, like for atomic health
        bool overheal = item_info["overheal"].asBool();
        uint16_t max_health = overheal ? (2 * ACTIVE_PLAYER->max_player_health) : ACTIVE_PLAYER->max_player_health;
        if (sprite[ACTIVE_PLAYER->i].extra < max_health)
        {
            sprite[ACTIVE_PLAYER->i].extra += healing;
            if (sprite[ACTIVE_PLAYER->i].extra > max_health)
                sprite[ACTIVE_PLAYER->i].extra = max_health;
        }
    }
    else if (item_type == "armor")
    {
        uint16_t capacity = json_get_int(item_info["maxcapacity"], 0);
        ACTIVE_PLAYER->max_shield_amount += capacity;
        uint16_t new_armor = json_get_int(item_info["capacity"], 0);
        if (ACTIVE_PLAYER->inv_amount[GET_SHIELD] < ACTIVE_PLAYER->max_shield_amount)
        {
            ACTIVE_PLAYER->inv_amount[GET_SHIELD] += new_armor;
            if (ACTIVE_PLAYER->inv_amount[GET_SHIELD] > ACTIVE_PLAYER->max_shield_amount)
                ACTIVE_PLAYER->inv_amount[GET_SHIELD] = ACTIVE_PLAYER->max_shield_amount;
        }
    }
    // Do not retrigger traps that the server has sent us before
    else if (item_type == "trap" && !silent)
    {
        Json::Value& trap_state = ap_game_state.dynamic_player["traps"][std::to_string(item_id)];
        if (!trap_state.isObject())
        {
            // New trap type, initialize
            trap_state["id"] = Json::UInt64(item_id);
            trap_state["count"] = Json::UInt64(1);
            trap_state["remaining"] = Json::UInt64(0);
            trap_state["grace"] = Json::UInt64(0);
        }
        else
        {
            // Just increment the queue trap count
            trap_state["count"] = trap_state["count"].asInt() + 1;
        }
    }
}

void process_message_queue()
{
    // Handle messages from the AP Server
    AP_ProcessMessages();

    // ToDo use a better dedicated system, this is at least easy to force into the engine/layout right now
    // Override all quotes that are not AP related
    if ((ACTIVE_PLAYER->ftq != AP_MESSAGE_QUOTE) && ap_message_queue.size() > 0)
    {
        // Pop oldest message
        ap_message_str = ap_message_queue[0];
        ap_message_queue.erase(ap_message_queue.begin());
        // And trigger a new quote
        ACTIVE_PLAYER->ftq = 0;
        P_DoQuote(AP_MESSAGE_QUOTE, ACTIVE_PLAYER);
        OSD_Printf("%s%s\n", AP_OSDTEXT_NORMAL, ap_message_str.c_str());
    }
}

static double credits_trap_end = 0;
int ap_velocity_modifier = 1;
int32_t pshrinking_label_code = 0;

void ap_handle_trap(Json::Value& trap_info, bool triggered)
{
    if (triggered)
        AP_QueueMessage("^10" + trap_info["name"].asString() + "^12 triggered!");

    // Process supported trap types
    std::string trap_type = trap_info["trap"].asString();
    if (trap_type == "celebrate")
    {
        // 1 in 25 chance to start a new fist pump each frame
        if (ACTIVE_PLAYER->fist_incs == 0 && (krand2() % 25 == 0))
        {
            ACTIVE_PLAYER->fist_incs = 1;
            switch (krand2()%6)
            {
            case 0:
                A_PlaySound(BONUS_SPEECH1, ACTIVE_PLAYER->i);
                break;
            case 1:
                A_PlaySound(BONUS_SPEECH4, ACTIVE_PLAYER->i);
                break;
            case 2:
                A_PlaySound(JIBBED_ACTOR6, ACTIVE_PLAYER->i);
                break;
            case 3:
                A_PlaySound(JIBBED_ACTOR5, ACTIVE_PLAYER->i);
                break;
            case 4:
                A_PlaySound(JIBBED_ACTOR2, ACTIVE_PLAYER->i);
                break;
            case 5:
                A_PlaySound(YIPPEE1, ACTIVE_PLAYER->i);
                break;
            }
        }
    }
    else if (trap_type == "credits" && triggered)
    {
        // Go look at the credits for some time
        credits_trap_end = timerGetFractionalTicks() + trap_info["duration"].asDouble()*1000/REALGAMETICSPERSEC;
    }
    else if (trap_type == "hyperspeed")
    {
        // Good Luck
        ap_velocity_modifier = 6;
    }
    else if (trap_type == "death" && triggered)
    {
        P_QuickKill(ACTIVE_PLAYER);
    }
    else if (trap_type == "shrink")
    {
        // All of the shrinking code is handled in the game CON script, try to inject stuff to there
        if (actor[ACTIVE_PLAYER->i].t_data[1] != pshrinking_label_code)
        {
            actor[ACTIVE_PLAYER->i].t_data[0] = 0;
            actor[ACTIVE_PLAYER->i].t_data[1] = pshrinking_label_code;
        }
        else if (actor[ACTIVE_PLAYER->i].t_data[0] > 268)
        {
            actor[ACTIVE_PLAYER->i].t_data[0] = 268;
        }
    }
    else if (trap_type == "spawn" && triggered)
    {
        int i, j;
        int sprite_id = trap_info["sprite"].asInt();
        for (i = 0; i < trap_info["amount"].asInt(); i++)
        {
            j = A_Spawn(ACTIVE_PLAYER->i, sprite_id);
            // ToDo find a valid nearby location instead of a random nearby spot
            sprite[j].x += (krand2() % 8192) - 4096;
            sprite[j].y += (krand2() % 8192) - 4096;
            // Battlelords need to be spawned with a specirfic pallette to not be the boss one
            if (sprite_id == BOSS1__STATIC || sprite_id == BOSS1STAYPUT__STATIC)
            {
                sprite[j].pal = 21;
            }
        }
    }
}

// Called from an actor in game, ensures we are in a valid game state and an in game tic has expired since last call
void ap_process_game_tic(void)
{
    // Ensure we don't have a velocity modifier unless something actively changes it for the current tic
    ap_velocity_modifier = 1;

    // Check for items in our queue to process
    while (!ap_game_state.ap_item_queue.empty())
    {
        auto queue_item = ap_game_state.ap_item_queue.front();
        ap_game_state.ap_item_queue.erase(ap_game_state.ap_item_queue.begin());
        ap_get_item(queue_item.first, !queue_item.second, true);
    }

    // Check for outstanding or active traps
    for (std::string trap_str : ap_game_state.dynamic_player["traps"].getMemberNames())
    {
        // Fetch relevant trap information for this trap type
        Json::Value& trap_state = ap_game_state.dynamic_player["traps"][trap_str];
        ap_net_id_t trap_id = trap_state["id"].asUInt64();
        Json::Value& trap_info = ap_item_info[trap_id];
        if (!ap_game_settings["dynamic"][trap_str].isNull())
            trap_info = ap_game_settings["dynamic"][trap_str];

        bool triggered = false;
        bool active = false;
        uint32_t remaining = trap_state["remaining"].asUInt64();
        if (remaining > 1)
        {
            remaining--;
            trap_state["remaining"] = remaining;
            active = true;
        }
        else if (remaining == 1)
        {
            // Disable trap and set a grace period
            trap_state["remaining"] = 0;
            trap_state["grace"] = trap_info["grace"];
            active = false;
        }
        
        if (!active)
        {
            uint32_t grace = trap_state["grace"].asUInt64();
            uint32_t count = trap_state["count"].asUInt64();
            if (grace > 0)
            {
                grace--;
                trap_state["grace"] = grace;
            }
            else if (count > 0)
            {
                // Trigger new trap instance
                trap_state["remaining"] = trap_info["duration"];
                count--;
                trap_state["count"] = count;
                triggered = true;
                active = true;
            }
        }

        if (active)
        {
            ap_handle_trap(trap_info, triggered);
        }
    }

    // Check if we have regenerated a steroids charge
    // Once every 20 seconds
    if (ACTIVE_PLAYER->player_par % (20 * REALGAMETICSPERSEC) == 0)
    {
        // Only increment if there is space for a full additional charge
        uint16_t steroid_amount = ACTIVE_PLAYER->inv_amount[GET_STEROIDS] + ap_steroids_duration();
        if (steroid_amount <= inv_max_capacity[GET_STEROIDS])
            ACTIVE_PLAYER->inv_amount[GET_STEROIDS] = steroid_amount;
    }

    // Process outstanding messages to quote at the player
    process_message_queue();
}

// This is called from the main game loop to ensure these checks happen globally
bool ap_process_periodic(void)
{
    bool reset_game = false;

    // Force HUD settings. Not nice, clean up in the future when we can support custom hud on all settings
    ud.screen_size = 4;
    ud.althud = 1;

    // Peek item queue and process stuff we already can while not in game
    if (!(ACTIVE_PLAYER->gm & MODE_GAME))
    {
        for (auto iter = ap_game_state.ap_item_queue.begin(); iter != ap_game_state.ap_item_queue.end();)
        {
            Json::Value item_info = ap_item_info[iter->first];

            // If it's a silent item or a map or key unlock or trap, process them outside the game
            // already. Handling traps here ensures we don't lose them during sessions
            std::string item_type = item_info["type"].asString();
            if (!iter->second || item_type == "map" || item_type == "trap" || item_type == "key")
            {
                ap_get_item(iter->first, !iter->second, true);
                // And then remove the entry from the queue
                iter = ap_game_state.ap_item_queue.erase(iter);
            }
            else iter++;
        }
    }

    // Handle credits trap, as we can't count on in-game tics in the menu
    if (credits_trap_end > timerGetFractionalTicks())
    {
        switch(m_currentMenu->menuID)
        {
        case MENU_CREDITS:
        case MENU_CREDITS2:
        case MENU_CREDITS3:
        case MENU_CREDITS4:
        case MENU_CREDITS5:
        case MENU_CREDITS6:
        case MENU_CREDITS7:
        case MENU_CREDITS8:
        case MENU_CREDITS9:
        case MENU_CREDITS10:
        case MENU_CREDITS11:
        case MENU_CREDITS12:
        case MENU_CREDITS13:
        case MENU_CREDITS14:
        case MENU_CREDITS15:
        case MENU_CREDITS16:
        case MENU_CREDITS17:
        case MENU_CREDITS18:
        case MENU_CREDITS19:
        case MENU_CREDITS20:
        case MENU_CREDITS21:
        case MENU_CREDITS22:
        case MENU_CREDITS23:
        case MENU_CREDITS24:
        case MENU_CREDITS25:
        case MENU_CREDITS26:
        case MENU_CREDITS27:
        case MENU_CREDITS28:
        case MENU_CREDITS29:
        case MENU_CREDITS30:
        case MENU_CREDITS31:
        case MENU_CREDITS32:
        case MENU_CREDITS33:
            // good boy
            break;
        default:
            Menu_Change(MENU_CREDITS);
            break;
        }
        if(!(ACTIVE_PLAYER->gm & MODE_MENU))
            Menu_Open(myconnectindex);
    }

    // Sync our save status, this only does something if there are changes
    // So it's save to call it every game tic
    AP_SyncProgress();

    // Check if we have reached all goals
    if (AP_CheckVictory())
    {
        uint8_t old_volumenum = ud.volume_number;
        ud.volume_number = 2;
        ud.eog = 1;
        G_BonusScreen(0);
        ap_return_to_menu = 0;
        G_BackToMenu();
        reset_game = true;
        ud.volume_number = old_volumenum;
    }

    return reset_game;
}

/* Configures the default inventory state */
static void ap_set_default_inv(void)
{
    // Clear inventory info
    for (uint8_t i = 0; i < GET_MAX; i++)
    {
        inv_capacity[i] = 0;
        inv_max_capacity[i] = 0;
        inv_available[i] = 0;
        ACTIVE_PLAYER->inv_amount[i] = 0;
    }
    // Clear ammo info
    for (uint8_t i = 0; i < MAX_WEAPONS; i++)
    {
        ACTIVE_PLAYER->ammo_amount[i] = 0;
    }

    // Set default max ammo amounts
    ACTIVE_PLAYER->max_ammo_amount[PISTOL_WEAPON__STATIC] = json_get_int(ap_game_settings["maximum"]["pistol"], 120);
    ACTIVE_PLAYER->max_ammo_amount[SHOTGUN_WEAPON__STATIC] = json_get_int(ap_game_settings["maximum"]["shotgun"], 20);
    ACTIVE_PLAYER->max_ammo_amount[CHAINGUN_WEAPON__STATIC] = json_get_int(ap_game_settings["maximum"]["chaingun"], 100);
    ACTIVE_PLAYER->max_ammo_amount[RPG_WEAPON__STATIC] = json_get_int(ap_game_settings["maximum"]["rpg"], 5);
    ACTIVE_PLAYER->max_ammo_amount[HANDBOMB_WEAPON__STATIC] = json_get_int(ap_game_settings["maximum"]["pipebomb"], 5);
    ACTIVE_PLAYER->max_ammo_amount[SHRINKER_WEAPON__STATIC] = json_get_int(ap_game_settings["maximum"]["shrinker"], 5);
    ACTIVE_PLAYER->max_ammo_amount[DEVISTATOR_WEAPON__STATIC] = json_get_int(ap_game_settings["maximum"]["devastator"], 25);
    ACTIVE_PLAYER->max_ammo_amount[TRIPBOMB_WEAPON__STATIC] = json_get_int(ap_game_settings["maximum"]["tripmine"], 3);
    ACTIVE_PLAYER->max_ammo_amount[FREEZE_WEAPON__STATIC] = json_get_int(ap_game_settings["maximum"]["freezethrower"], 50);
    ACTIVE_PLAYER->max_ammo_amount[GROW_WEAPON__STATIC] = json_get_int(ap_game_settings["maximum"]["expander"], 15);
    ACTIVE_PLAYER->max_player_health = json_get_int(ap_game_settings["maximum"]["health"], 100);
    ACTIVE_PLAYER->max_shield_amount = json_get_int(ap_game_settings["maximum"]["armor"], 100);
    // Always have mighty foot and pistol
    ACTIVE_PLAYER->gotweapon = 3;
    // Always have pistol ammo, and a bit more since we might have no other weapons
    ACTIVE_PLAYER->ammo_amount[PISTOL_WEAPON__STATIC] = 96;
    if (ACTIVE_PLAYER->ammo_amount[PISTOL_WEAPON__STATIC] > ACTIVE_PLAYER->max_ammo_amount[PISTOL_WEAPON__STATIC])
        ACTIVE_PLAYER->ammo_amount[PISTOL_WEAPON__STATIC] = ACTIVE_PLAYER->max_ammo_amount[PISTOL_WEAPON__STATIC];

    ability_unlocks.clear();
    if (!ap_game_settings["lock"]["crouch"].asBool())
        ability_unlocks["crouch"] = true;
    if (!ap_game_settings["lock"]["dive"].asBool())
        ability_unlocks["dive"] = true;
    if (!ap_game_settings["lock"]["jump"].asBool())
        ability_unlocks["jump"] = true;
    if (!ap_game_settings["lock"]["run"].asBool())
        ability_unlocks["run"] = true;
    if (!ap_game_settings["lock"]["open"].asBool())
        ability_unlocks["open"] = true;
    if (!ap_game_settings["lock"]["use"].asBool())
        ability_unlocks["use"] = true;
}

void ap_load_dynamic_player_data()
{
    Json::Value player_data = ap_game_state.dynamic_player["player"];

    int health = json_get_int(player_data["health"], -1);
    // Player died, reset to max HP
    if(health <= 0 || health > ACTIVE_PLAYER->max_player_health) health = ACTIVE_PLAYER->max_player_health;
    // Give at least some minimum halth to the player on entering a level
    if (health < 30) health = 30;

    sprite[ACTIVE_PLAYER->i].extra = health;
    ACTIVE_PLAYER->last_extra = health;

    // Upgrade inventory capacity if we had more left over
    for (uint8_t i = 0; i < GET_MAX; i++)
    {
        int inv_capacity = json_get_int(player_data["inv"][i], -1);
        if (inv_capacity > ACTIVE_PLAYER->inv_amount[i])
            ACTIVE_PLAYER->inv_amount[i] = inv_capacity;
    }

    // Upgrade ammo count if we had more left over
    for (uint8_t i = 0; i < MAX_WEAPONS; i++)
    {
        int ammo_capacity = json_get_int(player_data["ammo"][i], -1);
        if (ammo_capacity > ACTIVE_PLAYER->ammo_amount[i])
            ACTIVE_PLAYER->ammo_amount[i] = ammo_capacity;
    }

    int curr_weapon = json_get_int(player_data["curr_weapon"], -1);
    if (curr_weapon >= 0)
        force_set_player_weapon(curr_weapon);

    int selected_inv = json_get_int(player_data["selected_inv"], -1);
    if (selected_inv >= 0)
        force_set_inventory_item(selected_inv);

    ACTIVE_PLAYER->player_par = json_get_int(player_data["time"], 0);
}

void ap_store_dynamic_player_data(void)
{
    Json::Value new_player_data = Json::Value();

    // Only store data if we are actually in game mode
    if (!(ACTIVE_PLAYER->gm & (MODE_EOL | MODE_GAME))) return;

    new_player_data["health"] = Json::Int(sprite[ACTIVE_PLAYER->i].extra);
    bool player_died = sprite[ACTIVE_PLAYER->i].extra <= 0 ? true : false;
    for (uint8_t i = 0; i < GET_MAX; i++)
    {
        // Only store non progressive inventory values
        switch(i)
        {
        case GET_SHIELD:
        case GET_HOLODUKE:
        case GET_HEATS:
        case GET_FIRSTAID:
        case GET_BOOTS:
            new_player_data["inv"][i] = Json::Int(player_died ? 0 : ACTIVE_PLAYER->inv_amount[i]);
            break;
        }
    }

    // Upgrade ammo count if we had more left over
    for (uint8_t i = 0; i < MAX_WEAPONS; i++)
    {
        new_player_data["ammo"][i] = Json::Int(player_died ? 0 : ACTIVE_PLAYER->ammo_amount[i]);
    }

    new_player_data["curr_weapon"] = Json::Int(ACTIVE_PLAYER->curr_weapon);
    new_player_data["selected_inv"] = Json::Int(icon_to_inv[ACTIVE_PLAYER->inven_icon]);

    new_player_data["time"] = Json::Int(ACTIVE_PLAYER->player_par);

    ap_game_state.dynamic_player["player"] = new_player_data;
    // Mark our save state as to be synced
    ap_game_state.need_sync = true;
}

void ap_sync_inventory()
{
    // Reset the count of each progressive item we've processed
    ap_game_state.progressive.clear();

    ap_set_default_inv();

    // Apply state for all persistent items we have unlocked
    for (auto item_pair : ap_game_state.persistent)
    {
        for(unsigned int i = 0; i < item_pair.second; i++)
            ap_get_item(item_pair.first, true, false);
    }

    // Restore dynamic data like HP and ammo from last map
    ap_load_dynamic_player_data();
}

void ap_on_map_load(void)
{
    current_map = ap_format_map_id(ud.level_number, ud.volume_number);

#ifdef AP_DEBUG_ON
    print_level_template();
#endif

    ap_map_patch_sprites();

#ifdef AP_DEBUG_ON
    print_debug_level_info();
#endif

    ap_mark_known_secret_sectors(false);
}

void ap_on_save_load(void)
{
    current_map = ap_format_map_id(ud.level_number, ud.volume_number);

    ap_mark_known_secret_sectors(true);
    ap_sync_inventory();
    ap_delete_collected_sprites();
}

void ap_check_secret(int16_t sectornum)
{
    AP_CheckLocation(safe_location_id(ap_game_config["locations"][current_map]["sectors"][std::to_string(sectornum)]["id"]));
}

void ap_level_end(void)
{
    ap_store_dynamic_player_data();
    // Return to menu after beating a level
    ACTIVE_PLAYER->gm = 0;
    Menu_Open(myconnectindex);
    Menu_Change(MENU_AP_LEVEL);
    ap_return_to_menu = 1;
}

void ap_check_exit(int16_t exitnum)
{
    ap_location_t exit_location = safe_location_id(ap_game_config["locations"][current_map]["exits"][std::to_string(exitnum)]["id"]);
    // Might not have a secret exit defined, so in this case treat as regular exit
    if (exit_location < 0)
        exit_location = safe_location_id(ap_game_config["locations"][current_map]["exits"][std::to_string(0)]["id"]);
    AP_CheckLocation(exit_location);
}

void ap_select_level(uint8_t i)
{
    ud.m_level_number = ap_active_levels[ud.m_volume_number][i];
}

void ap_shutdown(void)
{
    AP_LibShutdown();
}

bool ap_can_dive()
{
    return (ACTIVE_PLAYER->inv_amount[GET_SCUBA] > 0) || ability_unlocks.count("dive");
}

bool ap_can_jump()
{
    return ability_unlocks.count("jump");
}

bool ap_can_crouch()
{
    return ability_unlocks.count("crouch");
}

bool ap_can_run()
{
    return ability_unlocks.count("run");
}

bool ap_can_open()
{
    return ability_unlocks.count("open");
}

bool ap_can_use()
{
    return ability_unlocks.count("use");
}

bool ap_can_save()
{
    return (!AP || !ap_game_settings["no_save"].asBool());
}

void ap_remaining_items(uint16_t* collected, uint16_t* total)
{
    for (std::string sprite_str : ap_game_config["locations"][current_map]["sprites"].getMemberNames())
    {
        ap_location_t pickup_loc = safe_location_id(ap_game_config["locations"][current_map]["sprites"][sprite_str]["id"]);
        if (pickup_loc > 0 && AP_LOCATION_CHECK_MASK(pickup_loc, (AP_LOC_PICKUP | AP_LOC_USED)))
        {
            (*total)++;
            if (AP_LOCATION_CHECKED(pickup_loc)) (*collected)++;
        }
    }
}

void ap_con_hook(void)
{
    const char* shrink_buf = "PSHRINKING";
    int32_t i = hash_find(&h_labels, shrink_buf);
    if (i >= 0)
        pshrinking_label_code = labelcode[i];
}

uint16_t ap_steroids_duration(void)
{
    uint16_t duration = ap_game_settings["steroids_duration"].asUInt();
    if (duration == 0)
        // default value
        duration = 40;
    return duration;
}
