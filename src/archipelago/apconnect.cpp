#include <assert.h>
#include <filesystem>
#include <iostream>
#include <random>

#include "SDL.h"

// In general, the following prefixes are used throughout this file:
// APAll_ is for handling functions common to both local and remote play
// APLocal_ is for handling functions specific to local play (i.e., using an aptyrian file)
// APRemote_ is for handling functions specific to remote play (i.e., connected to an Archipelago server)
// Archipelago_ is for public functions that the C side of the code calls

// Not functional, apclient uses implicit conversions.
//#define JSON_USE_IMPLICIT_CONVERSIONS 0

// Uncomment to see server<->client comms from APClient
//#define APCLIENT_DEBUG

// Silence warnings from apclient/wswrap
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#elif defined(_MSC_VER)
#pragma warning (push)
#pragma warning (disable:4100)
#pragma warning (disable:4127)
#pragma warning (disable:4191)
#pragma warning (disable:4355)
#pragma warning (disable:4371)
#pragma warning (disable:4582)
#pragma warning (disable:4583)
#pragma warning (disable:4619)
#pragma warning (disable:4623)
#pragma warning (disable:4625)
#pragma warning (disable:4626)
#pragma warning (disable:4643)
#pragma warning (disable:4800)
#pragma warning (disable:4840)
#pragma warning (disable:4996)
#pragma warning (disable:5026)
#pragma warning (disable:5027)
#pragma warning (disable:5031)
#pragma warning (disable:5039)
#pragma warning (disable:5204)
#pragma warning (disable:5220)
#pragma warning (disable:5267)
#endif

// For some reason, when building x86, websocketpp seems to think that some C++11 features
// are not available. Since we require C++17, this is obviously incorrect, so we tell
// websocketpp to skip its tests and assume the entirety of C++11 is available.
#define _WEBSOCKETPP_CPP11_STRICT_

#include <apclient.hpp>

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#elif defined(_MSC_VER)
#pragma warning (pop)
#endif

extern "C" {
	// Function definitions that go out C side need to not be mangled by C++
	#include "apconnect.h"

	// List of all AP Items, used for local play
	#include "apitems.h"

	// These are things from C side that we need here.
	extern const char *opentyrian_str; // opentyr.h
	extern const char *opentyrian_version; // opentyr.h
	extern bool tyrian2000detected; // opentyr.h
	extern const int font_ascii[256]; // fonthand.h

	// These functions are defined from the C side (apmsg.c), and we use them
	// to communicate information to the player while in game.
	void apmsg_enqueue(const char *);
	void apmsg_playSFX(archipelago_sound_t);
	void apmsg_setSpeed(Uint8 speed);
	void apmsg_cleanQueue(void);

	// These functions are needed for savegame support.
	const char *get_user_directory(void); // config.h
}

using nlohmann::json;

// Settings package received from server.
archipelago_settings_t APSeedSettings;

// Local options.
archipelago_options_t APOptions;

static std::string connection_slot_name = "";
static std::string connection_server_address = "";
static std::string connection_password = "";

static std::string clientUUID = "";

// True if we're in a game (local or remote)
// Used to save data if something happens
static bool gameInProgress = false;

// ----------------------------------------------------------------------------

// Base ID for all location/item checks, Archipelago side.
// Add to all outgoing ID numbers and remove from all incoming ones.
#define ARCHIPELAGO_BASE_ID 20031000

// Should match TyrianWorld.aptyrian_net_version
#define APTYRIAN_NET_VERSION 2

static std::unique_ptr<APClient> ap;

// ----------------------------------------------------------------------------
// Data -- Always Reloaded From Server / File
// ----------------------------------------------------------------------------

apupdatereq_t APUpdateRequest; // When C++ side thinks C needs to redraw something

// ------------------------------------------------------------------

// Name of the seed. Used for loading/saving
static std::string multiworldSeedName;

// Local location number (first in region): Number of locations in region.
static std::unordered_map<uint16_t, uint16_t> locationsPerRegion;

// Set of local location IDs.
// Locations in our seed that hold progression items (for us or someone else).
// Filled in by both ParseLocationData and ParseProgressionData.
static std::set<uint16_t> advancementLocations;

// Local location ID: Item ID to be obtained from that location.
// All data about locations in our seed. Only used in local play (no AP server).
static std::unordered_map<uint16_t, uint16_t> allLocationData;

// Local location ID: Cost to buy item.
static std::unordered_map<Uint16, Uint16> shopPrices;

// Local weapon ID: Base cost to upgrade weapon.
static std::unordered_map<Uint16, Uint16> upgradePrices;

// Total number of locations available to check. Used for endgame stats.
static int totalLocationCount = -1;

// ----------------------------------------------------------------------------
// Data -- Stored In Save Game
// ----------------------------------------------------------------------------

// These are C structs that are available C side.
apitem_t APItems; // Weapons, levels; One time collectibles, etc.
apstat_t APStats; // Cash, Armor, Power, etc. Upgradable stats.
applaydata_t APPlayData; // Time, deaths, other tracked things
apitemchoice_t APItemChoices; // APItems chosen to use

// ------------------------------------------------------------------

// Tracks items we've received so far, to ignore duplicates.
int lastItemIndex = -1;

// Set of global location IDs.
// Locations in our seed that we've checked (or had checked for us).
static std::set<int64_t> allLocationsChecked;

// Local location ID: Previously scouted Network Item.
// Scouted information about our shop items when playing remotely.
// We only call for LocationScouts when completing a level for the first time.
static std::unordered_map<Uint16, APClient::NetworkItem> scoutedShopLocations;

// ----------------------------------------------------------------------------

// All characters that can generate in UUIDs.
static const char uuidCharacters[33] = "0123456789ABCDEFGHKLMNPRSTUVWXYZ";

// Generates a new UUID for this client.
void Archipelago_GenerateUUID(void)
{
	std::random_device dev;

	int bitsPerIter = 0;
	for (unsigned int maxval = dev.max(); maxval; maxval >>= 1)
		++bitsPerIter;
	if (bitsPerIter == 0)
		bitsPerIter = 32; // Assuming 32bit max?

	std::stringstream s;
	for (int bits = 0; bits < 128; bits += bitsPerIter)
	{
		if (bits)
			s << '-';

		unsigned int result = dev();
		for (int i = bitsPerIter; i > 0; i -= 5)
		{
			s << uuidCharacters[result & 31];
			result >>= 5;
		}
	}

	clientUUID = s.str();
}

// Reads UUID from given source (config file)
void Archipelago_SetUUID(const char *uuid)
{
	clientUUID = uuid;
}

// Outputs UUID (for saving into config file)
const char *Archipelago_GetUUID(void)
{
	return clientUUID.c_str();
}

// ------------------------------------------------------------------

static void APAll_FreshInit(void)
{
	memset(&APItems, 0, sizeof(APItems));
	memset(&APStats, 0, sizeof(APStats));
	memset(&APPlayData, 0, sizeof(APPlayData));
	memset(&APItemChoices, 0, sizeof(APItemChoices));

	allLocationsChecked.clear();
	scoutedShopLocations.clear();
	lastItemIndex = -1;

	APStats.RestGoalEpisodes = APSeedSettings.GoalEpisodes;
	// Initting of item choices is handled when parsing start state,
	// because we don't have knowledge of available items at this point
}

static void APAll_InitMessageQueue(void)
{
	std::stringstream s;
	s << opentyrian_str << " " << opentyrian_version << ", ready to play.";

	std::string versionInfo = s.str();
	apmsg_cleanQueue();
	apmsg_enqueue(versionInfo.c_str());
}

// ------------------------------------------------------------------

void Archipelago_Save(void)
{
	json saveData;

	saveData["APItems"] += APItems.FrontPorts;
	saveData["APItems"] += APItems.RearPorts;
	saveData["APItems"] += APItems.Specials;
	for (int i = 0; i < 5; ++i)
		saveData["APItems"] += APItems.Levels[i];
	for (int i = 0; i < 36; i += 3)
		saveData["APItems"] += (APItems.Sidekicks[i]) | (APItems.Sidekicks[i+1] << 3) | (APItems.Sidekicks[i+2] << 6);

	for (int i = 0; i < 5; ++i)
		saveData["APStats"] += APStats.Clears[i];
	saveData["APStats"] += APStats.RestGoalEpisodes;
	saveData["APStats"] += APStats.PowerMaxLevel;
	saveData["APStats"] += APStats.GeneratorLevel;
	saveData["APStats"] += APStats.ArmorLevel;
	saveData["APStats"] += APStats.ShieldLevel | (APStats.SolarShield ? 0x80 : 0x00);
	saveData["APStats"] += APStats.Cash;
	// QueuedSuperBombs is not saved

	saveData["APPlayData"] += APPlayData.TimeInLevel;
	saveData["APPlayData"] += APPlayData.TimeInMenu;
	saveData["APPlayData"] += APPlayData.Deaths;

	saveData["APItemChoices"] += APItemChoices.FrontPort.Item
		| (APItemChoices.FrontPort.PowerLevel << 10);
	saveData["APItemChoices"] += APItemChoices.RearPort.Item
		| (APItemChoices.RearPort.PowerLevel << 10)
		| (APItemChoices.RearMode2 ? 0x8000 : 0);
	saveData["APItemChoices"] += APItemChoices.Special.Item;
	saveData["APItemChoices"] += APItemChoices.Sidekick[0].Item;
	saveData["APItemChoices"] += APItemChoices.Sidekick[1].Item;

	if (!allLocationsChecked.empty())
		saveData["Checked"] = allLocationsChecked;
	if (lastItemIndex >= 0)
		saveData["Index"] = lastItemIndex;

	for (auto const &[locationID, networkItem] : scoutedShopLocations)
	{
		// We don't save location because that's the key
		std::string locationStr = std::to_string(locationID);
		saveData["Scouts"][locationStr] += networkItem.item;
		saveData["Scouts"][locationStr] += networkItem.player;
		saveData["Scouts"][locationStr] += networkItem.flags;
	}

	std::stringstream saveFileName;
	saveFileName << get_user_directory() << "/AP" << multiworldSeedName << ".sav";

	std::ofstream saveFile(saveFileName.str(), std::ios::trunc | std::ios::binary);
	json::to_msgpack(saveData, saveFile);
}

static bool Archipelago_Load(void)
{
	APAll_FreshInit();

	std::stringstream saveFileName;
	saveFileName << get_user_directory() << "/AP" << multiworldSeedName << ".sav";

	std::ifstream saveFile(saveFileName.str(), std::ios::binary);
	if (!saveFile.is_open())
		return false;

	try
	{
		json saveData = json::from_msgpack(saveFile);

		APItems.FrontPorts = saveData["APItems"].at(0).template get<Uint64>();
		APItems.RearPorts = saveData["APItems"].at(1).template get<Uint64>();
		APItems.Specials = saveData["APItems"].at(2).template get<Uint64>();
		for (int i = 0; i < 5; ++i)
			APItems.Levels[i] = saveData["APItems"].at(3 + i).template get<Uint32>();
		for (int i = 0; i < 12; ++i)
		{
			Uint16 combinedSidekicks = saveData["APItems"].at(8 + i).template get<Uint16>();
			APItems.Sidekicks[0 + i*3] = (combinedSidekicks & 0x3);
			APItems.Sidekicks[1 + i*3] = ((combinedSidekicks >> 3) & 0x3);
			APItems.Sidekicks[2 + i*3] = ((combinedSidekicks >> 6) & 0x3);
		}

		for (int i = 0; i < 5; ++i)
			APStats.Clears[i] = saveData["APStats"].at(i).template get<Uint32>();
		APStats.RestGoalEpisodes = saveData["APStats"].at(5).template get<Uint8>();
		APStats.PowerMaxLevel = saveData["APStats"].at(6).template get<Uint8>();
		APStats.GeneratorLevel = saveData["APStats"].at(7).template get<Uint8>();
		APStats.ArmorLevel = saveData["APStats"].at(8).template get<Uint8>();
		APStats.ShieldLevel = saveData["APStats"].at(9).template get<Uint8>();
		APStats.Cash = saveData["APStats"].at(10).template get<Uint64>();
		if (APStats.ShieldLevel & 0x80)
		{
			APStats.SolarShield = true;
			APStats.ShieldLevel &= 0x7F;
		}

		if (saveData.contains("APPlayData") && saveData["APPlayData"].size() >= 3)
		{			
			APPlayData.TimeInLevel = saveData["APPlayData"].at(0).template get<Uint64>();
			APPlayData.TimeInMenu = saveData["APPlayData"].at(1).template get<Uint64>();
			APPlayData.Deaths = saveData["APPlayData"].at(2).template get<Uint16>();
		}

		APItemChoices.FrontPort.Item = saveData["APItemChoices"].at(0).template get<Uint16>();
		APItemChoices.RearPort.Item = saveData["APItemChoices"].at(1).template get<Uint16>();
		APItemChoices.Special.Item = saveData["APItemChoices"].at(2).template get<Uint16>();
		APItemChoices.Sidekick[0].Item = saveData["APItemChoices"].at(3).template get<Uint16>();
		APItemChoices.Sidekick[1].Item = saveData["APItemChoices"].at(4).template get<Uint16>();

		APItemChoices.FrontPort.PowerLevel = APItemChoices.FrontPort.Item >> 10;
		APItemChoices.FrontPort.Item &= 0x3FF;
		APItemChoices.RearMode2 = (APItemChoices.RearPort.Item & 0x8000) ? true : false;
		APItemChoices.RearPort.PowerLevel = (APItemChoices.RearPort.Item >> 10) & 0xF;
		APItemChoices.RearPort.Item &= 0x3FF;

		if (saveData.contains("Checked"))
			allLocationsChecked.insert(saveData["Checked"].begin(), saveData["Checked"].end());
		if (saveData.contains("Index"))
			lastItemIndex = saveData["Index"].template get<int>();

		if (saveData.contains("Scouts"))
		{
			for (auto & [locationStr, itemJSON] : saveData["Scouts"].items())
			{
				Uint16 locationID = std::stoi(locationStr);

				APClient::NetworkItem newItem;
				newItem.item = itemJSON.at(0).template get<int64_t>();
				newItem.player = itemJSON.at(1).template get<int>();
				newItem.flags = itemJSON.at(2).template get<unsigned>();
				newItem.location = locationID + ARCHIPELAGO_BASE_ID;

				scoutedShopLocations.insert({locationID, newItem});
			}
		}
	}
	catch (...)
	{
		std::cout << "Couldn't load save file " << multiworldSeedName << ".sav; starting new game" << std::endl;

		// Clear everything AGAIN so we start on a fresh slate...
		APAll_FreshInit();
		return false;
	}

	std::cout << "Loaded save file " << multiworldSeedName << ".sav" << std::endl;
	return true;
}

// ============================================================================
// Basic Communication
// ============================================================================

static json APAll_DeobfuscateJSONObject(std::string str)
{
	// See TyrianWorld.obfuscate_object
	std::string obfus_chars = "0GTi29}#{K+d O1VYr]en:zP~yAI5(,ZL/)|?.sb4l<MFU3tD6$>wp[f*q%C=o8Emgj;xuXakhW!SNHc-Q7RBJv";
	std::string input_chars = " ABCDEFGHIJKLMNOPQRSTUVWXYZ!?.,;:'\"abcdefghijklmnopqrstuvwxyz#$%*(){}[]1234567890/|\\-+=";

	int offset = 54;
	for (char &c : str)
	{
		int idx = (signed)obfus_chars.find(c) - offset;
		if (idx < 0)
			idx += 87;

		c = input_chars[idx];
		offset += (c & 0xF) + 1;
		if (offset >= 87)
			offset -= 87;
	}

	json deobfJSON = json::parse(str, nullptr, false);
	if (deobfJSON.is_discarded() || deobfJSON.is_string())
		throw std::runtime_error("Invalid or corrupt data received.");
	return deobfJSON;
}

// Cleans up chat by replacing any unknown characters with question marks.
static std::string APRemote_CleanString(std::string input)
{
	for (char &c : input)
	{
		if (font_ascii[(unsigned char)c] == -1 && c != ' ')
			c = '?';
	}
	return input;
}

// Unlike ap->get_player_alias, allows getting from any team
static std::string APRemote_GetPlayerName(int slot, int team = -1)
{
	if (!ap)
		return "Unknown";
	if (slot == 0)
		return "Server";
	if (team == -1)
		team = ap->get_team_number();

	for (auto const& player : ap->get_players())
	{
		if (player.team == team && player.slot == slot)
			return APRemote_CleanString(player.alias);
	}

	return "Unknown";
}

// ============================================================================
// Chat / Game displays
// ============================================================================

static Uint8 APLocal_GetItemFlags(Uint16 localItemID, Uint16 localCheckID)
{
	if (localItemID < 500 || Archipelago_CheckHasProgression(localCheckID))
		return 1;
	else switch (localItemID)
	{
		case 700: case 719: case 900: case 901: case 902:
		case 903: case 904: case 905: case 906: case 907:
			return 1;
		case 503: case 504: case 521: case 721: case 805:
		case 806: case 807: case 808: case 908: case 909:
			return 2;
		default:
			break;
	}
	return 0;
}

static std::string APLocal_BuildANSIString(Uint16 localItemID, Uint8 flags)
{
	if (localItemID > 999)
		return "";

	std::stringstream s;
	if      (flags & 1) s << "\u001b[35;1m"; // Progression
	else if (flags & 2) s << "\u001b[34;1m"; // Useful
	else if (flags & 4) s << "\u001b[31;1m"; // Trap (none currently in local pool, may change)
	else                s << "\u001b[36;1m"; // Filler
	s << apitems_AllNames[localItemID] << "\u001b[0m";
	return s.str();
}

// Creates a string out of an item ID for colorful print.
static std::string APLocal_BuildItemString(Uint16 localItemID, Uint8 flags)
{
	if (localItemID > 999)
		return "";

	std::stringstream s;
	if      (flags & 1) s << "<95"; // Progression
	else if (flags & 2) s << "<83"; // Useful
	else if (flags & 4) s << "<45"; // Trap (none currently in local pool, may change)
	else                s << "<06"; // Filler
	s << apitems_AllNames[localItemID] << ">";
	return s.str();
}

// ----------------------------------------------------------------------------

// Creates a string out of a NetworkItem for colorful print.
static std::string APRemote_BuildItemString(const APClient::NetworkItem &item, int64_t playerID)
{
	if (!ap)
		return "";

	std::string itemName = ap->get_item_name(item.item, ap->get_player_game(playerID));

	std::stringstream s;
	if      (item.flags & 1) s << "<95"; // Progression
	else if (item.flags & 2) s << "<83"; // Useful
	else if (item.flags & 4) s << "<45"; // Trap
	else                     s << "<06"; // Filler
	s << APRemote_CleanString(itemName) << ">";
	return s.str();
}

// Creates a string out of a location for colorful print.
// There is no local version of this -- there's no need for it, this is primarily used for AP server hints.
static std::string APRemote_BuildLocationString(int64_t locationID, int64_t playerID)
{
	if (!ap)
		return "";

	std::string locationName = ap->get_location_name(locationID, ap->get_player_game(playerID));
	return "<06" + APRemote_CleanString(locationName) + ">";
}

// *** Callback for "PrintJSON" ***
// Prints messages to the console. Also enqueues messages for game display in certain cases.
static void APRemote_CB_ReceivePrint(const APClient::PrintJSONArgs &args)
{
	// Let APClient log the message in full to the console, no matter what.
	std::cout << ap->render_json(args.data, APClient::RenderFormat::ANSI) << std::endl;

	std::stringstream s;
	if (args.type == "ItemSend")
	{
		// We are receiving an item
		if (*args.receiving == ap->get_player_number())
		{
			if (*args.receiving == args.item->player) // From (and to) ourselves
				s << "You got your ";
			else // Someone else sent us something
				s << "Received ";

			s << APRemote_BuildItemString(*args.item, *args.receiving);

			if (*args.receiving != args.item->player)
				s << " from " << APRemote_GetPlayerName(args.item->player);

			apmsg_playSFX(args.item->item >= ARCHIPELAGO_BASE_ID+980
				? APSFX_RECEIVE_MONEY : APSFX_RECEIVE_ITEM);
		}
		// We are sending an item to another person
		else if (args.item->player == ap->get_player_number())
		{
			s << "Sent " << APRemote_BuildItemString(*args.item, *args.receiving);
			s << " to " << APRemote_GetPlayerName(*args.receiving);
		}
		else return; // We don't care about items that don't pertain to us
	}
	// We are getting an item from the server via !getitem (ignore original message contents)
	else if (args.type == "ItemCheat")
	{
		if (*args.receiving != ap->get_player_number())
			return; // Don't care if someone else used !getitem

		s << "Received " << APRemote_BuildItemString(*args.item, *args.receiving);
		s << " from [Server]";

		apmsg_playSFX(args.item->item >= ARCHIPELAGO_BASE_ID+980
			? APSFX_RECEIVE_MONEY : APSFX_RECEIVE_ITEM);
	}
	// A hint that pertains to us was received (ignore original message contents)
	else if (args.type == "Hint")
	{
		s << APRemote_GetPlayerName(*args.receiving) << "'s ";
		s << APRemote_BuildItemString(*args.item, *args.receiving) << " is at\n";
		s << APRemote_BuildLocationString(args.item->location, args.item->player);
		s << "\nin " << APRemote_GetPlayerName(args.item->player) << "'s world. ";
		if (*args.found)
			s << "<C5(found)";
		else
			s << "<45(not found)";
	}
	// Someone joined or left the game (ignore original message contents)
	else if (args.type == "Join" || args.type == "Part")
	{
		s << APRemote_GetPlayerName(*args.slot, *args.team);
		if (args.type == "Join")
		{
			s << " [" << ap->get_player_game(*args.slot) << "]";
			s << " joined the game";
		}
		else
			s << " left the game";
	}
	// Most other types that we don't handle in any special way
	else if (args.type != "Tutorial" && args.type != "TagsChanged")
	{
		std::string message = ap->render_json(args.data, APClient::RenderFormat::TEXT);
		s << APRemote_CleanString(message);

		if (args.type == "Countdown" || args.type == "Chat" || args.type == "ServerChat")
			apmsg_playSFX(APSFX_CHAT);
		if (args.type == "Countdown")
			apmsg_setSpeed(8);
	}
	// Don't handle tutorial text.
	else return;

	std::string output = s.str();
	apmsg_enqueue(output.c_str());
}

// ============================================================================
// Items
// ============================================================================

static const int remoteCashItemValues[] =
	{50, 75, 100, 150, 200, 300, 375, 500, 750, 800, 1000, 2000, 5000, 7500, 10000, 20000, 40000, 75000, 100000, 1000000};

// Based on item ID, adjusts APItems or APStats appropriately to mark it as acquired.
// Uses local IDs, but is called in both local and remote play.
static void APAll_ResolveItem(int item)
{
	// Automatically subtract the AP Base ID if we notice it.
	if (item >= ARCHIPELAGO_BASE_ID)
		item -= ARCHIPELAGO_BASE_ID;

	assert(item >= 0 && item <= 999);
	if (item < 0 || item > 999)
		return;

	// Items (weapons, levels, etc.)
	if      (item < 100) APItems.Levels[0]  |= (Uint32)1 << (item );
	else if (item < 200) APItems.Levels[1]  |= (Uint32)1 << (item - 100);
	else if (item < 300) APItems.Levels[2]  |= (Uint32)1 << (item - 200);
	else if (item < 400) APItems.Levels[3]  |= (Uint32)1 << (item - 300);
	else if (item < 500) APItems.Levels[4]  |= (Uint32)1 << (item - 400);
	else if (item < 600) APItems.FrontPorts |= (Uint64)1 << (item - 500);
	else if (item < 700) APItems.RearPorts  |= (Uint64)1 << (item - 600);
	else if (item < 800) APItems.Specials   |= (Uint64)1 << (item - 700);
	else if (item < 836 && APItems.Sidekicks[item-800] < 2) ++APItems.Sidekicks[item-800];

	// Armor, shield, money... statistic-affecting things
	else if (item == 900 && APStats.GeneratorLevel < 2) APStats.GeneratorLevel = 2;
	else if (item == 901 && APStats.GeneratorLevel < 3) APStats.GeneratorLevel = 3;
	else if (item == 902 && APStats.GeneratorLevel < 4) APStats.GeneratorLevel = 4;
	else if (item == 903 && APStats.GeneratorLevel < 5) APStats.GeneratorLevel = 5;
	else if (item == 904 && APStats.GeneratorLevel < 6) APStats.GeneratorLevel = 6;
	else if (item == 905 && APStats.GeneratorLevel < 6) ++APStats.GeneratorLevel;
	else if (item == 906 && APStats.PowerMaxLevel < 10) ++APStats.PowerMaxLevel;
	else if (item == 907)
	{
		APStats.ArmorLevel += 2;
		if (APStats.ArmorLevel > 28)
			APStats.ArmorLevel = 28;

		APUpdateRequest.Armor += 2;
	}
	else if (item == 908)
	{
		APStats.ShieldLevel += 2;
		if (APStats.ShieldLevel > 28)
			APStats.ShieldLevel = 28;

		APUpdateRequest.Shield += 2;
	}

	else if (item == 909) APStats.SolarShield = true;
	else if (item == 910) ++APStats.QueuedSuperBombs;
	else if (item >= 980) APStats.Cash += remoteCashItemValues[item - 980];

	std::cout << "Item resolved: " << item << std::endl;
}

// Parses "StartState" structure. In all JSON blobs.
// Contains all the stuff we start with in a curated form.
static void APAll_ParseStartState(json &j)
{
	if (j.contains("Items"))
	{
		for (auto const &item : j["Items"])
			APAll_ResolveItem(item.template get<Sint16>());
	}
	APStats.ArmorLevel = 10 + (j.value<int>("Armor", 0) * 2);
	APStats.ShieldLevel = 10 + (j.value<int>("Shield", 0) * 2);
	APStats.PowerMaxLevel = j.value<int>("Power", 0);
	APStats.GeneratorLevel = 1 + j.value<int>("Generator", 0);
	APStats.SolarShield = j.value<bool>("SolarShield", false);
	APStats.Cash = j.value<int64_t>("Credits", 0);

	// Default the player's front port to whatever they've started with.
	// If they started with multiple (via starting_inventory), use the first one in ID order.
	for (int i = 0; i < 64; ++i)
	{
		if (APItems.FrontPorts & (1ull << i))
		{
			APItemChoices.FrontPort.Item = 500 + i;
			break;
		}
	}

	// If the Specials menu isn't enabled, then "choose" the first special we see automatically.
	// Either this loop won't find anything (if specials are off), or it'll find whatever special
	// the seed designated the player to start with, and force them on that.
	if (!APSeedSettings.SpecialMenu)
	{
		for (int i = 0; i < 64; ++i)
		{
			if (APItems.Specials & (1ull << i))
			{
				APItemChoices.Special.Item = 700 + i;
				break;
			}
		}
	}
}

// ============================================================================
// Outgoing and Incoming Locations and Checks
// ============================================================================

// Parses "LocationData" structure. Only present in local play files.
// Contains all locations and their contents; progression locations are marked with an "!".
static void APLocal_ParseLocationData(json &j)
{
	advancementLocations.clear();
	allLocationData.clear();

	for (auto &location : j.items())
	{
		Uint16 locationID = std::stoi(location.key());
		Uint16 itemID = 0;

		std::string contents = location.value().template get<std::string>();
		if (contents[0] == '!')
		{
			advancementLocations.insert({locationID});
			contents = contents.substr(1);
		}
		itemID = std::stoi(contents);
		allLocationData.emplace(locationID, itemID);
	}
}

static void APLocal_InitLocationsPerRegion(void)
{
	locationsPerRegion.clear();
	for (uint16_t i = 0; i < 2000; i += 10)
	{
		uint16_t total = 0;
		for (; total < 10; ++total)
		{
			if (allLocationData.count(i + total) == 0)
				break;
		}
		if (total > 0)
			locationsPerRegion.insert({i, total});
	}
}

// Called in local games only to handle the results of a location check.
static void APLocal_ReceiveItem(int64_t locationID)
{
	locationID -= ARCHIPELAGO_BASE_ID;
	if (allLocationData.count(locationID) == 0)
		return;

	++lastItemIndex;
	Uint16 itemID = allLocationData[locationID];
	APAll_ResolveItem(itemID);

	Uint8 flags = APLocal_GetItemFlags(itemID, locationID);
	std::string output = "You got your " + APLocal_BuildItemString(itemID, flags);
	apmsg_enqueue(output.c_str());
	apmsg_playSFX(itemID >= 980 ? APSFX_RECEIVE_MONEY : APSFX_RECEIVE_ITEM);

	std::cout << "You got your "
	          << APLocal_BuildANSIString(itemID, flags)
	          << " (" << lastItemIndex + 1 << "/" << totalLocationCount << ")" << std::endl;
}

// ----------------------------------------------------------------------------

// Parses "ProgressionData" structure. Only present in remote slot_data.
// Contains a list of all locations that are known to have progression items, and nothing more.
static void APRemote_ParseProgressionData(json &j)
{
	advancementLocations.clear();
	advancementLocations.insert(j.begin(), j.end());
}

static void APRemote_InitLocationsPerRegion(void)
{
	if (!ap)
		return;

	locationsPerRegion.clear();

	// For regular locations we have to kludge together a list by checking if location names exist
	for (uint16_t i = 0; i < 1000; i += 10)
	{
		uint16_t total = 0;
		for (; total < 10; ++total)
		{
			if (ap->get_location_name(i + total + ARCHIPELAGO_BASE_ID, "Tyrian") == "Unknown")
				break;
		}
		if (total > 0)
			locationsPerRegion.insert({i, total});
	}
	// For shops we can just check shop data.
	for (uint16_t i = 1000; i < 2000; i += 10)
	{
		uint16_t total = 0;
		for (; total < 10; ++total)
		{
			if (shopPrices.count(i + total) == 0)
				break;
		}
		if (total > 0)
			locationsPerRegion.insert({i, total});
	}
}

// *** Callback for APClient's "OnLocationChecked" ***
// Keeps a local copy of all checked locations, used to suppress items from spawning
// if they've already been collected in the seed
static void APRemote_CB_RemoteLocationCheck(const std::list<int64_t>& locations)
{
	allLocationsChecked.insert(locations.begin(), locations.end());
}

// *** Callback for "ReceiveItems" ***
// Handles new items sent to us from the AP server.
static void APRemote_CB_ReceiveItem(const std::list<APClient::NetworkItem>& items)
{
	for (auto const &item : items)
	{
		// Messaging is handled when we receive the PrintJSON.
		if (item.index > lastItemIndex)
		{
			lastItemIndex = item.index;
			APAll_ResolveItem(item.item);
		}
	}
}

// ----------------------------------------------------------------------------

void Archipelago_ResendAllChecks()
{
	if (!ap || allLocationsChecked.size() == 0)
		return;

	std::list dataList(allLocationsChecked.begin(), allLocationsChecked.end());
	ap->LocationChecks(dataList);
}

void Archipelago_SendCheck(int checkID)
{
	checkID += ARCHIPELAGO_BASE_ID;
	allLocationsChecked.insert({checkID});

	if (ap) // Send item to AP server
		ap->LocationChecks({checkID});
	else // Local play, give item to ourselves
		APLocal_ReceiveItem(checkID);
}

bool Archipelago_WasChecked(int checkID)
{
	// Check data comes from the AP server and so uses the global ID number
	return allLocationsChecked.count(checkID + ARCHIPELAGO_BASE_ID) == 1;
}

bool Archipelago_CheckHasProgression(int checkID)
{
	// This is entirely local to us, so don't add ARCHIPELAGO_BASE_ID
	return advancementLocations.count(checkID) == 1;
}

int Archipelago_GetRegionCheckCount(int firstCheckID)
{
	// We can't immediately init locations per region if the data package wasn't cached.
	// We have to wait until we first need it, and then fill it in.
	if (ap && locationsPerRegion.empty())
		APRemote_InitLocationsPerRegion();

	if (locationsPerRegion.count(firstCheckID) == 0)
		return 0;
	return locationsPerRegion[firstCheckID];
}

// Returns bitmask of checked locations.
Uint32 Archipelago_GetRegionWasCheckedList(int firstCheckID)
{
	Uint32 checkList = 0;
	int checkTotal = Archipelago_GetRegionCheckCount(firstCheckID);

	for (int i = 0; i < checkTotal; ++i)
	{
		if (allLocationsChecked.count(firstCheckID + i + ARCHIPELAGO_BASE_ID) == 1)
			checkList |= (1 << i);
	}
	return checkList;
}

int Archipelago_GetTotalCheckCount(void)
{
	return totalLocationCount;
}

int Archipelago_GetTotalWasCheckedCount(void)
{
	return allLocationsChecked.size();
}

// ============================================================================
// Shops & Upgrades (Spending Money)
// ============================================================================

static void APAll_ParseShopData(json &j)
{
	shopPrices.clear();

	for (auto &location : j.items())
	{
		Uint16 locationID = std::stoi(location.key());
		Uint16 itemID = location.value().template get<Uint16>();
		shopPrices.emplace(locationID, itemID);
	}
}

static void APAll_ParseWeaponCost(json &j)
{
	upgradePrices.clear();

	for (auto &item : j.items())
	{
		Uint16 itemID = std::stoi(item.key());
		Uint16 itemPrice = item.value().template get<Uint16>();
		upgradePrices.emplace(itemID, itemPrice);
	}
}

// ----------------------------------------------------------------------------

static void APRemote_CB_ScoutResults(const std::list<APClient::NetworkItem>& items)
{
	for (auto const &item : items)
		scoutedShopLocations.insert({item.location - ARCHIPELAGO_BASE_ID, item});
}

// ----------------------------------------------------------------------------

static shopitem_t shopItemBuffer[5];

int Archipelago_GetShopItems(int shopStartID, shopitem_t **shopItems)
{
	memset(shopItemBuffer, 0, sizeof(shopItemBuffer));

	int i;
	for (i = 0; i < 5; ++i)
	{
		if (shopPrices.count(shopStartID + i) == 0)
			break;
		shopItemBuffer[i].LocationID = shopStartID + i;
		shopItemBuffer[i].Cost = shopPrices[shopStartID + i];

		std::string itemName;

		if (APSeedSettings.ShopMenu == 2)
		{
			// Hidden shop contents mode (local or remote, doesn't matter)
			itemName = "Archipelago Item " + i;
			shopItemBuffer[i].Icon = 9003;
		}
		else if (!ap)
		{
			// Local game, we have the info already
			Uint16 itemID = allLocationData[shopStartID + i];
			itemName = apitems_AllNames[itemID];
			shopItemBuffer[i].Icon = apitems_AllIcons[itemID];
		}
		else if (scoutedShopLocations.count(shopStartID + i) == 1)
		{
			// Remote game, previously scouted location from the server
			int playerID = scoutedShopLocations[shopStartID + i].player;
			int itemID = scoutedShopLocations[shopStartID + i].item;

			itemName = ap->get_item_name(itemID, ap->get_player_game(playerID));

			// We don't show player name, but we used to...
			//if (playerID != ap->get_player_number())
			//	playerName = APRemote_GetPlayerName(playerID) + "'s";

			if (itemID >= ARCHIPELAGO_BASE_ID && itemID <= ARCHIPELAGO_BASE_ID+999)
				shopItemBuffer[i].Icon = apitems_AllIcons[itemID - ARCHIPELAGO_BASE_ID];
			else if (scoutedShopLocations[shopStartID + i].flags & 1)
				shopItemBuffer[i].Icon = 9007;
			else
				shopItemBuffer[i].Icon = 9003;
		}
		else
		{
			// We don't have the scouted info, for some reason
			itemName = "Unknown Item " + i;
			shopItemBuffer[i].Icon = 9003;
		}

		strncpy(shopItemBuffer[i].ItemName, itemName.c_str(), 40 - 1);
		shopItemBuffer[i].ItemName[39] = 0;
	}
	*shopItems = (i == 0) ? NULL : shopItemBuffer;
	return i;
}

// Only relevant for local; sends out a LocationScouts request to get
// the contents of our shops, when they're unlocked
void Archipelago_ScoutShopItems(int shopStartID)
{
	if (!ap)
		return;

	std::list<int64_t> scoutRequests;
	for (int i = 0; i < 5; ++i)
	{
		if (shopPrices.count(shopStartID + i) == 0)
			break;
		scoutRequests.emplace_back(shopStartID + i + ARCHIPELAGO_BASE_ID);
	}

	if (!scoutRequests.empty())
		ap->LocationScouts(scoutRequests);
}

unsigned int Archipelago_GetUpgradeCost(int itemID, int powerLevel)
{
	if (upgradePrices.count(itemID) != 1)
		return 0;

	const int upgradeMultiplier[11] = {0, 1, 3, 6, 10, 15, 21, 28, 36, 45, 55};
	return upgradePrices[itemID] * upgradeMultiplier[powerLevel];
}

unsigned int Archipelago_GetTotalUpgradeCost(int itemID, int powerLevel)
{
	if (upgradePrices.count(itemID) != 1 || powerLevel > 10)
		return 0;

	const int upgradeMultiplier[11] = {0, 1, 4, 10, 20, 35, 56, 84, 120, 165, 220};
	return upgradePrices[itemID] * upgradeMultiplier[powerLevel];
}

// ============================================================================
// DeathLink
// ============================================================================

// Set to true in bounce handler if someone else has died.
// Set to false by the game at start of stage.
bool APDeathLinkReceived;

static double lastBounceTime; // Required to filter out own deathlinks.

std::vector<std::string> death_messages[] = {
	{}, // DAMAGE_DEATHLINK (unused)
	{ // DAMAGE_BULLET
		" went out in a blaze of glory.",
		" experienced sudden decompression.",
		" took one bullet too many."
	},
	{ // DAMAGE_CONTACT
		" experienced a hull breach.",
		" tried to use ramming speed.",
		" disagreed with something stronger than them."
	}
};

// ----------------------------------------------------------------------------

static void APRemote_CB_Bounce(const json& bounceJSON)
{
	// Only care about DeathLink-tagged bounces (for now)
	if (!APSeedSettings.DeathLink || !APOptions.EnableDeathLink
		|| !bounceJSON.contains("tags") || !bounceJSON.contains("data"))
		return;

	auto tags = bounceJSON["tags"];
	if (std::find(tags.begin(), tags.end(), "DeathLink") != tags.end())
	{
		if (lastBounceTime == bounceJSON["data"].at("time"))
			return; // Ignore own deaths

		APDeathLinkReceived = true;

		std::string output = "<45" + bounceJSON["data"].at("cause").template get<std::string>();
		apmsg_enqueue(output.c_str());
	}
}

// ----------------------------------------------------------------------------

// Sends death out to the multiworld.
void Archipelago_SendDeathLink(damagetype_t source)
{
	if (!ap || !APSeedSettings.DeathLink || !APOptions.EnableDeathLink)
		return;

	if (APDeathLinkReceived || source == DAMAGE_DEATHLINK)
		return; // Prevent chained deathlinks

	int entry = rand() % death_messages[source].size();
	std::string cause = connection_slot_name + death_messages[source][entry];
	lastBounceTime = ap->get_server_time();

	json bounceData = { {"time", lastBounceTime}, {"cause", cause}, {"source", ap->get_slot()} };
	ap->Bounce(bounceData, {}, {}, {"DeathLink"});

	apmsg_enqueue("<45Sending death to your friends...");
}

// Enables or disables DeathLink locally.
// Call after changing APOptions.EnableDeathLink
void Archipelago_UpdateDeathLinkState(void)
{
	if (!APSeedSettings.DeathLink)
	{
		APOptions.EnableDeathLink = false;
		return;
	}

	if (!ap)
		return;

	std::list<std::string> tags;
	if (APOptions.EnableDeathLink)
		tags.push_back("DeathLink");
	ap->ConnectUpdate(std::nullopt, tags);
}

// ============================================================================
// Local Play : File I/O
// ============================================================================

bool Archipelago_StartLocalGame(FILE *file)
{
	if (gameInProgress || ap)
		return false;

	APAll_InitMessageQueue();
	try
	{
		json aptyrianJSON = json::parse(file);

		if (aptyrianJSON.at("NetVersion") != APTYRIAN_NET_VERSION)
			throw std::runtime_error("Version mismatch. You may need to update APTyrian.");

		if (!tyrian2000detected && aptyrianJSON.at("Settings").at("RequireT2K") == true)
			throw std::runtime_error("This seed requires data from Tyrian 2000.");

		multiworldSeedName = aptyrianJSON.at("Seed").template get<std::string>();

		APSeedSettings.PlayEpisodes = aptyrianJSON["Settings"].at("Episodes").template get<int>();
		APSeedSettings.GoalEpisodes = aptyrianJSON["Settings"].at("Goal").template get<int>();
		APSeedSettings.Difficulty = aptyrianJSON["Settings"].at("Difficulty").template get<int>();

		APSeedSettings.ShopMenu = aptyrianJSON["Settings"].at("ShopMenu").template get<int>();
		APSeedSettings.SpecialMenu = aptyrianJSON["Settings"].at("SpecialMenu").template get<bool>();
		APSeedSettings.HardContact = aptyrianJSON["Settings"].at("HardContact").template get<bool>();
		APSeedSettings.ExcessArmor = aptyrianJSON["Settings"].at("ExcessArmor").template get<bool>();
		APSeedSettings.TwiddleInputs = aptyrianJSON["Settings"].at("ShowTwiddles").template get<bool>();
		APSeedSettings.ArchipelagoRadar = aptyrianJSON["Settings"].at("APRadar").template get<bool>();
		APSeedSettings.Christmas = aptyrianJSON["Settings"].at("Christmas").template get<bool>();
		APSeedSettings.DeathLink = false;

		std::string obfuscated;
		json resultJSON;

		obfuscated = aptyrianJSON.at("ShopData").template get<std::string>();
		resultJSON = APAll_DeobfuscateJSONObject(obfuscated);
		APAll_ParseShopData(resultJSON);

		obfuscated = aptyrianJSON.at("WeaponCost").template get<std::string>();
		resultJSON = APAll_DeobfuscateJSONObject(obfuscated);
		APAll_ParseWeaponCost(resultJSON);

		obfuscated = aptyrianJSON.at("LocationData").template get<std::string>();
		resultJSON = APAll_DeobfuscateJSONObject(obfuscated);
		APLocal_ParseLocationData(resultJSON);

		totalLocationCount = allLocationData.size();

		// Attempt to load old game first
		if (!Archipelago_Load())
		{
			// Not able to load, load start state and start new game
			obfuscated = aptyrianJSON.at("StartState").template get<std::string>();
			resultJSON = APAll_DeobfuscateJSONObject(obfuscated);
			APAll_ParseStartState(resultJSON);			
		}
	}
	catch (const std::runtime_error& e)
	{
		std::cout << "Can't start game: " << e.what() << std::endl;
		return false;
	}
	catch (...)
	{
		// Slot data JSON wasn't formed like expected to be
		std::cout << "Can't start game: Invalid or corrupt data received." << std::endl;
		return false;
	}

	// Init other data (can't fail)
	APLocal_InitLocationsPerRegion();
	gameInProgress = true;
	return true;
}

// ============================================================================
// Remote Play : Server I/O
// ============================================================================

static archipelago_connectionstat_t connection_stat = APCONN_NOT_CONNECTED;
static bool connection_ever_made = false; // Disables timeout
static uint64_t connection_start_time; // For initial connection timeout
static std::string connection_error_desc = "";

// ----------------------------------------------------------------------------

// Call when a fatal connection error has occurred and the connection should be closed.
// Stuff like receiving a slot refusal, or getting invalid data.
static void APRemote_FatalError(std::string reason)
{
	connection_stat = APCONN_NOT_CONNECTED;
	connection_error_desc = reason;
}

// *** Callback for APClient's "OnSocketDisconnected" ***
static void APRemote_CB_Disconnect()
{
	connection_stat = APCONN_CONNECTING;
}

// *** Callback for APClient's "OnSocketError" ***
static void APRemote_CB_Error(const std::string& error)
{
	connection_error_desc = error;
	connection_stat = APCONN_CONNECTING;
}

// *** Callback for "RoomInfo" ***
static void APRemote_CB_RoomInfo()
{
	connection_stat = APCONN_TENTATIVE;
	ap->ConnectSlot(connection_slot_name, connection_password, 0b011, {}, {0,4,4});
}

// *** Callback for "ConnectionRefused" ***
static void APRemote_CB_SlotRefused(const std::list<std::string>& reasons)
{
	std::string all_reasons = "";
	for (auto const &reason : reasons)
	{
		if (all_reasons != "")
			all_reasons += " ";

		if (reason == "InvalidSlot")
			all_reasons += "Slot '" + connection_slot_name + "' does not exist.";
		else if (reason == "InvalidGame")
			all_reasons += "Slot '" + connection_slot_name + "' is not playing Tyrian.";
		else if (reason == "IncompatibleVersion")
			all_reasons += "Server version incompatible with this client.";
		else if (reason == "InvalidPassword")
			all_reasons += "Password missing or incorrect.";
		else if (reason == "InvalidItemsHandling")
			all_reasons += "Internal misconfiguration.";
		else
			all_reasons += "Unknown reason.";
	}
	std::cout << "Received refusal from Archipelago server: " << all_reasons << std::endl;
	APRemote_FatalError(all_reasons);
}

// *** Callback for "Connected" ***
// This is where we get our slot_data and thus where most of the magic happens.
static void APRemote_CB_SlotConnected(const json& slot_data)
{
	APAll_InitMessageQueue();
	try
	{
		if (slot_data.at("NetVersion") != APTYRIAN_NET_VERSION)
			throw std::runtime_error("Version mismatch. You may need to update APTyrian.");

		if (!tyrian2000detected && slot_data.at("Settings").at("RequireT2K") == true)
			throw std::runtime_error("Slot '" + connection_slot_name + "' requires data from Tyrian 2000.");

		multiworldSeedName = slot_data.at("Seed").template get<std::string>();

		APSeedSettings.PlayEpisodes = slot_data["Settings"].at("Episodes").template get<int>();
		APSeedSettings.GoalEpisodes = slot_data["Settings"].at("Goal").template get<int>();
		APSeedSettings.Difficulty = slot_data["Settings"].at("Difficulty").template get<int>();

		APSeedSettings.ShopMenu = slot_data["Settings"].at("ShopMenu").template get<int>();
		APSeedSettings.SpecialMenu = slot_data["Settings"].at("SpecialMenu").template get<bool>();
		APSeedSettings.HardContact = slot_data["Settings"].at("HardContact").template get<bool>();
		APSeedSettings.ExcessArmor = slot_data["Settings"].at("ExcessArmor").template get<bool>();
		APSeedSettings.TwiddleInputs = slot_data["Settings"].at("ShowTwiddles").template get<bool>();
		APSeedSettings.ArchipelagoRadar = slot_data["Settings"].at("APRadar").template get<bool>();
		APSeedSettings.Christmas = slot_data["Settings"].at("Christmas").template get<bool>();
		APSeedSettings.DeathLink = slot_data["Settings"].at("DeathLink").template get<bool>();

		totalLocationCount = slot_data.at("LocationMax").template get<int>();

		std::string obfuscated;
		json resultJSON;

		obfuscated = slot_data.at("ShopData").template get<std::string>();
		resultJSON = APAll_DeobfuscateJSONObject(obfuscated);
		APAll_ParseShopData(resultJSON);

		obfuscated = slot_data.at("WeaponCost").template get<std::string>();
		resultJSON = APAll_DeobfuscateJSONObject(obfuscated);
		APAll_ParseWeaponCost(resultJSON);

		obfuscated = slot_data.at("ProgressionData").template get<std::string>();
		resultJSON = APAll_DeobfuscateJSONObject(obfuscated);
		APRemote_ParseProgressionData(resultJSON);

		// Attempt to load old game first
		if (!Archipelago_Load())
		{
			// Not able to load, load start state and start new game
			obfuscated = slot_data.at("StartState").template get<std::string>();
			resultJSON = APAll_DeobfuscateJSONObject(obfuscated);
			APAll_ParseStartState(resultJSON);
		}

		// If all goals are already completed, pre-emptively change our status to "Goal Complete"
		if (!APStats.RestGoalEpisodes)
			ap->StatusUpdate(APClient::ClientStatus::GOAL);
	}
	catch (std::runtime_error& e)
	{
		APRemote_FatalError(e.what());
		return;
	}
	catch (...)
	{
		// Slot data JSON wasn't formed like expected to be
		APRemote_FatalError("Invalid or corrupt data received.");
		return;
	}

	std::cout << "Successfully connected." << std::endl;
	connection_stat = APCONN_READY;
	connection_ever_made = true;

	// Update tags if we need to.
	if (APSeedSettings.DeathLink && APOptions.EnableDeathLink)
		Archipelago_UpdateDeathLinkState();

	// Resend all checks to make sure server is up to date.
	Archipelago_ResendAllChecks();

	// Init other data.
	locationsPerRegion.clear(); // Will be properly init later (after data package is received)
	gameInProgress = true;
}

// ----------------------------------------------------------------------------

void Archipelago_SetDefaultConnectionDetails(const char *address)
{
	std::string cppAddress = address;
	std::string::size_type at_sign = cppAddress.find("@");

	connection_slot_name = cppAddress.substr(0, at_sign);
	connection_server_address = cppAddress.substr(at_sign + 1);

	std::cout << "Will connect to Archipelago server ("
	          << connection_slot_name      << "@"
	          << connection_server_address << ")" << std::endl;
}

void Archipelago_SetDefaultConnectionPassword(const char *connectionPassword)
{
	connection_password = connectionPassword;
}

void Archipelago_Connect(void)
{
	if (gameInProgress)
		return;

	std::cout << "Connecting to Archipelago server ("
	          << connection_slot_name      << "@"
	          << connection_server_address << ")" << std::endl;
	ap.reset();

	APOptions.EnableDeathLink = true;

	if (connection_server_address.empty())
		return APRemote_FatalError("Please provide a server address to connect to.");
	if (connection_slot_name.empty())
		return APRemote_FatalError("Please provide a slot name.");

	std::string cert = std::filesystem::exists("./cacert.pem") ? "./cacert.pem" : "";
	std::cout << cert << std::endl;
	ap.reset(new APClient(clientUUID, "Tyrian", connection_server_address));

	// Chat and other communications
	ap->set_print_json_handler(APRemote_CB_ReceivePrint);

	// Checks and Items
	ap->set_items_received_handler(APRemote_CB_ReceiveItem);
	ap->set_location_checked_handler(APRemote_CB_RemoteLocationCheck);
	ap->set_location_info_handler(APRemote_CB_ScoutResults);

	// DeathLink
	ap->set_bounced_handler(APRemote_CB_Bounce);

	// Server IO
    ap->set_socket_disconnected_handler(APRemote_CB_Disconnect);
    ap->set_socket_error_handler(APRemote_CB_Error);
	ap->set_room_info_handler(APRemote_CB_RoomInfo);
	ap->set_slot_refused_handler(APRemote_CB_SlotRefused);
	ap->set_slot_connected_handler(APRemote_CB_SlotConnected);

	connection_stat = APCONN_CONNECTING;
	connection_error_desc = "";
	connection_ever_made = false;
	connection_start_time = SDL_GetTicks64();
}

void Archipelago_Poll(void)
{
	if (!ap)
		return;

	if (connection_ever_made)
	{
		// Just keep retrying.
		ap->poll();
	}
	else
	{
		if (connection_stat == APCONN_NOT_CONNECTED // Fatal error occurred
			|| (connection_stat == APCONN_CONNECTING && SDL_GetTicks64() > connection_start_time + 10000))
		{
			// Fatal error or connection timeout, disconnect and remove game info
			Archipelago_Disconnect();
		}
		else
			ap->poll();
	}
}

void Archipelago_Disconnect(void)
{
	// Save before we disconnect!
	if (gameInProgress)
		Archipelago_Save();
	gameInProgress = false;

	if (!ap)
		return;

	printf("Disconnecting from Archipelago server.\n");
	connection_stat = APCONN_NOT_CONNECTED;
	ap.reset();
}

archipelago_connectionstat_t Archipelago_ConnectionStatus(void)
{
	if (!ap)
		return APCONN_NOT_CONNECTED;
	return connection_stat;
}

const char* Archipelago_GetConnectionError(void)
{
	return connection_error_desc.c_str();
}

void Archipelago_GoalComplete(void)
{
	if (!ap)
	{
		std::cout << "Your goal has been completed." << std::endl;
		return;
	}

	ap->StatusUpdate(APClient::ClientStatus::GOAL);
}
