#include <assert.h>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <random>
#include <list>
#include <unordered_map>

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

// Never use valijson. It adds a lot of bloat without a tangible benefit.
#define AP_NO_SCHEMA

// Silence warnings from apclient/wswrap
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wpedantic"
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

static std::string ourSlotName = "";
static std::string cx_serverAddress = "";
static std::string cx_serverPassword = "";

static std::string clientUUID = "";

// True if we're in a game (local or remote)
// Used to save data if something happens
static bool gameInProgress = false;

// Silent Item mode is used when initially connecting to a game,
// keeps from spamming sounds on load.
static bool silentItemMode = true;

// ----------------------------------------------------------------------------

// Base ID for all location/item checks, Archipelago side.
// Add to all outgoing ID numbers and remove from all incoming ones.
#define ARCHIPELAGO_BASE_ID 20031000

// Should match TyrianWorld.aptyrian_net_version
#define APTYRIAN_NET_VERSION 6

static APClient::Version targetAPVersion = {0, 6, 0};

static std::unique_ptr<APClient> ap;

// ----------------------------------------------------------------------------
// Data -- Always Reloaded From Server / File
// ----------------------------------------------------------------------------

apupdatereq_t APUpdateRequest; // When C++ side thinks C needs to redraw something

// ------------------------------------------------------------------

// Name of the seed. Used for loading/saving
static std::string multiworldSeedName;

// True if game is in race mode.
static bool isInRaceMode = false;

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

// Set of global location IDs.
// Locations in our seed that we, specifically, have checked.
static std::set<int64_t> localLocationsChecked;

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
	localLocationsChecked.clear();
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

	silentItemMode = true;
}

// ------------------------------------------------------------------

void Archipelago_Save(void)
{
	silentItemMode = false;

	if (multiworldSeedName.empty())
		return; // Debug game, don't save data (it'll get ignored anyway)

	json saveData;

	saveData["Seed"] = multiworldSeedName;
	saveData["APItems"] += APItems.FrontPorts;
	saveData["APItems"] += APItems.RearPorts;
	saveData["APItems"] += APItems.Specials;
	for (int i = 0; i < 5; ++i)
		saveData["APItems"] += APItems.Levels[i];
	saveData["APItems"] += APItems.BonusGames;
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
	saveData["APStats"] += APStats.DataCubes | (APStats.CubeRewardGiven ? 0x8000 : 0x0000);
	// QueuedSuperBombs is not saved

	saveData["APPlayData"] += APPlayData.TimeInLevel;
	saveData["APPlayData"] += APPlayData.TimeInMenu;
	saveData["APPlayData"] += APPlayData.Deaths;
	saveData["APPlayData"] += APPlayData.ExitedLevels;
	saveData["APPlayData"] += APPlayData.TimeInBonus;

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
	if (!localLocationsChecked.empty())
		saveData["LocalChecked"] = localLocationsChecked;
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
	saveFileName << "AP" << multiworldSeedName << "_" << ourSlotName << ".sav";
	std::string saveFileAbsoluteName = get_user_directory() + std::string("/") + saveFileName.str();

	std::ofstream saveFile(saveFileAbsoluteName, std::ios::trunc | std::ios::binary);
	json::to_msgpack(saveData, saveFile);
}

static bool Archipelago_Load(void)
{
	APAll_FreshInit();

	std::stringstream saveFileName;
	saveFileName << "AP" << multiworldSeedName << "_" << ourSlotName << ".sav";
	std::string saveFileAbsoluteName = get_user_directory() + std::string("/") +  saveFileName.str();

	std::ifstream saveFile(saveFileAbsoluteName, std::ios::binary);
	if (!saveFile.is_open())
		return false;

	try
	{
		json saveData = json::from_msgpack(saveFile);

		// Make sure seed matches and someone's not just trying to copy files over
		if (multiworldSeedName != saveData.at("Seed"))
			throw std::runtime_error("Save file not for this game");

		APItems.FrontPorts = saveData["APItems"].at(0).template get<Uint64>();
		APItems.RearPorts = saveData["APItems"].at(1).template get<Uint64>();
		APItems.Specials = saveData["APItems"].at(2).template get<Uint64>();
		for (int i = 0; i < 5; ++i)
			APItems.Levels[i] = saveData["APItems"].at(3 + i).template get<Uint32>();
		APItems.BonusGames = saveData["APItems"].at(8).template get<Uint8>();
		for (int i = 0; i < 12; ++i)
		{
			Uint16 combinedSidekicks = saveData["APItems"].at(9 + i).template get<Uint16>();
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
		APStats.DataCubes = saveData["APStats"].at(11).template get<Uint16>();
		if (APStats.ShieldLevel & 0x80)
		{
			APStats.SolarShield = true;
			APStats.ShieldLevel &= 0x7F;
		}
		if (APStats.DataCubes & 0x8000)
		{
			APStats.CubeRewardGiven = true;
			APStats.DataCubes &= 0x7FFF;
		}

		// APPlayData isn't important to care enough if it's the wrong size.
		if (saveData.contains("APPlayData"))
		{
			if (saveData["APPlayData"].size() >= 1)
				APPlayData.TimeInLevel = saveData["APPlayData"].at(0).template get<Uint64>();
			if (saveData["APPlayData"].size() >= 2)
				APPlayData.TimeInMenu = saveData["APPlayData"].at(1).template get<Uint64>();
			if (saveData["APPlayData"].size() >= 3)
				APPlayData.Deaths = saveData["APPlayData"].at(2).template get<Uint16>();
			if (saveData["APPlayData"].size() >= 4)
				APPlayData.ExitedLevels = saveData["APPlayData"].at(3).template get<Uint16>();
			if (saveData["APPlayData"].size() >= 5)
				APPlayData.TimeInBonus = saveData["APPlayData"].at(4).template get<Uint64>();
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
		if (saveData.contains("LocalChecked"))
			localLocationsChecked.insert(saveData["LocalChecked"].begin(), saveData["LocalChecked"].end());
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
		std::cout << "Couldn't load save file " << saveFileName.str() << "; starting new game" << std::endl;

		// Clear everything AGAIN so we start on a fresh slate...
		APAll_FreshInit();
		return false;
	}

	std::cout << "Loaded save file " << saveFileName.str() << std::endl;
	return true;
}

// ============================================================================
// Basic Communication
// ============================================================================

typedef struct {
	uint8_t tyrcode;
	const char *utf8;
	size_t utf8len;
} utf8conv_t;

std::vector<utf8conv_t> UTF8Conversions = {
	{0x80, "\xc3\x87", 2}, // U+00C7 Latin Capital letter C with cedilla
	{0x81, "\xc3\xbc", 2}, // U+00FC Latin Small Letter U with diaeresis
	{0x82, "\xc3\xa9", 2}, // U+00E9 Latin Small Letter E with acute
	{0x83, "\xc3\xa2", 2}, // U+00E2 Latin Small Letter A with circumflex
	{0x84, "\xc3\xa4", 2}, // U+00E4 Latin Small Letter A with diaeresis
	{0x85, "\xc3\xa0", 2}, // U+00E0 Latin Small Letter A with grave
	{0x86, "\xc3\xa5", 2}, // U+00E5 Latin Small Letter A with ring above
	{0x87, "\xc3\xa7", 2}, // U+00E7 Latin Small Letter C with cedilla
	{0x88, "\xc3\xaa", 2}, // U+00EA Latin Small Letter E with circumflex
	{0x89, "\xc3\xab", 2}, // U+00EB Latin Small Letter E with diaeresis
	{0x8A, "\xc3\xa8", 2}, // U+00E8 Latin Small Letter E with grave
	{0x8B, "\xc3\xaf", 2}, // U+00EF Latin Small Letter I with diaeresis
	{0x8C, "\xc3\xae", 2}, // U+00EE Latin Small Letter I with circumflex
	{0x8D, "\xc3\xac", 2}, // U+00EC Latin Small Letter I with grave
	{0x8E, "\xc3\x84", 2}, // U+00C4 Latin Capital letter A with diaeresis
	{0x8F, "\xc3\x85", 2}, // U+00C5 Latin Capital letter A with ring above
	{0x90, "\xc3\x89", 2}, // U+00C9 Latin Capital letter E with acute
	{0x91, "\xc3\xa6", 2}, // U+00E6 Latin Small Letter AE
	{0x92, "\xc3\x86", 2}, // U+00C6 Latin Capital letter AE
	{0x93, "\xc3\xb4", 2}, // U+00F4 Latin Small Letter O with circumflex
	{0x94, "\xc3\xb6", 2}, // U+00F6 Latin Small Letter O with diaeresis
	{0x95, "\xc3\xb2", 2}, // U+00F2 Latin Small Letter O with grave
	{0x96, "\xc3\xbb", 2}, // U+00FB Latin Small Letter U with circumflex
	{0x97, "\xc3\xb9", 2}, // U+00F9 Latin Small Letter U with grave
	{0x98, "\xc3\xbf", 2}, // U+00FF Latin Small Letter Y with diaeresis
	{0x99, "\xc3\x96", 2}, // U+00D6 Latin Capital letter O with diaeresis
	{0x9A, "\xc3\x9c", 2}, // U+00DC Latin Capital Letter U with diaeresis
	{0x9B, "\xc2\xa2", 2}, // U+00A2 Cent sign
	{0x9C, "\xc2\xa3", 2}, // U+00A3 Pound sign
	{0x9D, "\xc2\xa5", 2}, // U+00A5 Yen sign
	{0x9E, "\xe2\x82\xa7", 3}, // U+20A7 Peseta sign
	{0x9F, "\xc6\x92", 2}, // U+0192 Latin Small Letter F with hook
	{0xA0, "\xc3\xa1", 2}, // U+00E1 Latin Small Letter A with acute
	{0xA1, "\xc3\xad", 2}, // U+00ED Latin Small Letter I with acute
	{0xA2, "\xc3\xb3", 2}, // U+00F3 Latin Small Letter O with acute
	{0xA3, "\xc3\xba", 2}, // U+00FA Latin Small Letter U with acute
	{0xA4, "\xc3\xb1", 2}, // U+00F1 Latin Small Letter N with tilde
	{0xA5, "\xc3\x91", 2}, // U+00D1 Latin Capital letter N with tilde
	{0xA6, "\xc2\xaa", 2}, // U+00AA Feminine ordinal indicator
	{0xA7, "\xc2\xba", 2}, // U+00BA Masculine ordinal indicator
	{0xA8, "\xc2\xbf", 2}  // U+00BF Inverted question mark
};

// Cleans and converts the UTF-8 strings from Archipelago.
static std::string APRemote_CleanString(std::string input)
{
	std::string result;
	size_t len = input.length();
	result.reserve(len);

	for (size_t i = 0; i < len; ++i)
	{
		// Handle unicode characters that have sprites
		if ((input[i] & 0xF0) >= 0xC0)
		{
			result += '?';
			for (const utf8conv_t &cvt : UTF8Conversions)
			{
				if (!strncmp(&input[i], cvt.utf8, cvt.utf8len))
				{
					result.back() = (char)cvt.tyrcode;
					break;
				}
			}

			switch (input[i] & 0xF0)
			{
				case 0xF0: ++i; // fall through
				case 0xE0: ++i; // fall through
				default:   ++i; break;
			}
		}
		else if (input[i] == '\r' || input[i] == '\n' || input[i] == ' ')
			result += input[i];
		else if (font_ascii[(uint8_t)input[i]] != -1)
			result += input[i];
		else
			result += '?';
	}
	return result;
}

// ------------------------------------------------------------------

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

static Uint8 APLocal_GetItemFlags(Uint16 localItemID)
{
	if (localItemID < 500) // All Levels
		return 3; // Progression + Useful
	else if (localItemID < 700) // All Front and Rear Weapons
		return 1; // Progression
	else switch (localItemID)
	{
		case 700: // Repulsor
		case 719: // Invulnerability
		case 900: // Advanced MR-12
		case 901: // Gencore Custom MR-12
		case 902: // Standard MicroFusion
		case 903: // Advanced MicroFusion
		case 904: // Gravitron Pulse-Wave
		case 905: // Progressive Generator
		case 906: // Maximum Power Up
		case 907: // Armor Up
		case 911: // Data Cube
			return 1; // Progression
		case 704: // Ice Beam
		case 710: // Banana Bomb
		case 721: // SDF Main Gun
		case 805: // MegaMissile
		case 806: // Atom Bombs
		case 807: // Phoenix Device
		case 808: // Plasma Storm
		case 819: // MicroSol FrontBlaster
		case 821: // BattleShip-Class Firebomb
		case 822: // Protron Cannon Indigo
		case 824: // Protron Cannon Tangerine
		case 825: // MicroSol FrontBlaster II
		case 826: // Beno Wallop Beam
		case 827: // Beno Protron System -B-
		case 831: // Flying Punch
		case 908: // Shield Up
		case 909: // Solar Shields
		case 999: // 1,000,000 Credits
			return 2; // Useful
		default:
			return 0; // Filler
	}
}

static std::string APLocal_BuildANSIString(Uint16 localItemID, Uint8 flags)
{
	if (localItemID > 999)
		return "";

	std::stringstream s;
	if ((flags & 3) == 3) s << "\u001b[35;1m"; // Progression + Useful
	else if (flags & 1)   s << "\u001b[35;1m"; // Progression
	else if (flags & 2)   s << "\u001b[34;1m"; // Useful
	else if (flags & 4)   s << "\u001b[31;1m"; // Trap (none currently in local pool, may change)
	else                  s << "\u001b[36;1m"; // Filler
	s << apitems_AllNames[localItemID] << "\u001b[0m";
	return s.str();
}

// Creates a string out of an item ID for colorful print.
static std::string APLocal_BuildItemString(Uint16 localItemID, Uint8 flags)
{
	if (localItemID > 999)
		return "";

	std::stringstream s;
	if ((flags & 3) == 3) s << "<25"; // Progression + Useful
	else if (flags & 1)   s << "<55"; // Progression
	else if (flags & 2)   s << "<35"; // Useful
	else if (flags & 4)   s << "<45"; // Trap (none currently in local pool, may change)
	else                  s << "<06"; // Filler
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
	if ((item.flags & 3) == 3) s << "<25"; // Progression + Useful
	else if (item.flags & 1)   s << "<55"; // Progression
	else if (item.flags & 2)   s << "<35"; // Useful
	else if (item.flags & 4)   s << "<45"; // Trap
	else                       s << "<06"; // Filler
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
	}
	// A hint that pertains to us was received (ignore original message contents)
	else if (args.type == "Hint")
	{
		s << APRemote_GetPlayerName(*args.receiving) << "'s ";
		s << APRemote_BuildItemString(*args.item, *args.receiving) << " is at\n";
		s << APRemote_BuildLocationString(args.item->location, args.item->player);
		s << "\nin " << APRemote_GetPlayerName(args.item->player) << "'s world. ";
		if (*args.found)
			s << "<25(found)";
		else
			s << "<45(not found)";
	}
	// Someone joined or left the game (ignore original message contents)
	else if (args.type == "Join" || args.type == "Part")
	{
		s << "<04" << APRemote_GetPlayerName(*args.slot, *args.team);
		if (args.type == "Join")
		{
			s << " [<08";
			s << APRemote_CleanString(ap->get_player_game(*args.slot));
			s << "<04]";
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

// ----------------------------------------------------------------------------

void Archipelago_ChatMessage(const char *userMessage)
{
	std::string text = userMessage;

	if (text.find_first_not_of(' ') == std::string::npos)
		return; // Do not send a string consisting only of whitespace

	if (text[0] == '/')
		return; // Do not send commands that start with a backslash (for future local command use)

	if (ap)
		ap->Say(text);
}

// ============================================================================
// Items
// ============================================================================

static const int remoteCashItemValues[] =
	{50, 75, 100, 150, 200, 300, 375, 500, 750, 800, 1000, 2000, 5000, 7500, 10000, 20000, 40000, 75000, 100000, 1000000};

// Quick lookup table of unlock items for goal levels, used for Data Cube Hunt
// (when data cube target reached, it "gives" these items to the player)
static const int goalLevelQuickLookup[5] = {15, 111, 211, 317, 406};

// Based on item ID, adjusts APItems or APStats appropriately to mark it as acquired.
// Whether it uses local or global item IDs is determined by is_local
static void APAll_ResolveItem(int64_t item, bool is_local)
{
	if (!is_local) // Subtract the AP Base ID from remote item IDs
		item -= ARCHIPELAGO_BASE_ID;

	if (item < 0 || item > 999)
		return; // May have received an AP Global item (e.g. Nothing, at ID -1), just ignore

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
	else if (item == 911) ++APStats.DataCubes;
	else if (item == 920) APItems.BonusGames |= 1;
	else if (item == 921) APItems.BonusGames |= 2;
	else if (item == 922) APItems.BonusGames |= 4;
	else if (item >= 980) APStats.Cash += remoteCashItemValues[item - 980];

	if (!silentItemMode)
		apmsg_playSFX(item >= 980 ? APSFX_RECEIVE_MONEY : APSFX_RECEIVE_ITEM);
}

// Sends goal levels to the local player in data cube hunt mode, if enough data cubes collected
static void APAll_CheckDataCubeCount(void)
{
	// Not in cube hunt mode? Or reward already given?
	if (APSeedSettings.DataCubesNeeded <= 0 || APStats.CubeRewardGiven)
		return;

	if (APStats.DataCubes >= APSeedSettings.DataCubesNeeded)
	{
		bool origSilent = silentItemMode;
		silentItemMode = true;

		for (int episode = 0; episode < 5; ++episode)
		{
			if (!AP_BITSET(APSeedSettings.GoalEpisodes, episode))
				continue;

			// This is technically "locally" handled even in remote play
			const int localItemID = goalLevelQuickLookup[episode];
			const Uint8 flags = APLocal_GetItemFlags(localItemID);

			APAll_ResolveItem(localItemID, true);
			std::string output = "You got your " + APLocal_BuildItemString(localItemID, flags);
			apmsg_enqueue(output.c_str());
		}
		silentItemMode = origSilent;

		// Execute once only.
		APStats.CubeRewardGiven = true;
	}
}

// Parses "StartState" structure. In all JSON blobs.
// Contains all the stuff we start with in a curated form.
static void APAll_ParseStartState(json &j)
{
	if (j.contains("Items"))
	{
		for (auto const &item : j["Items"])
			APAll_ResolveItem(item.template get<Sint16>(), true);
	}
	APStats.ArmorLevel = 10 + (j.value<int>("Armor", 0) * 2);
	APStats.ShieldLevel = 10 + (j.value<int>("Shield", 0) * 2);
	APStats.PowerMaxLevel = j.value<int>("Power", 0);
	APStats.GeneratorLevel = 1 + j.value<int>("Generator", 0);
	APStats.SolarShield = j.value<bool>("SolarShield", false);
	APStats.Cash = j.value<int64_t>("Credits", 0);
	APStats.DataCubes = j.value<int64_t>("DataCubes", 0);

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

	// It's entirely possible for someone to start_inventory enough data cubes to reach their target,
	// so we have to be ready to handle that case.
	APAll_CheckDataCubeCount();
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
		Uint16 localItemID = 0;

		std::string contents = location.value().template get<std::string>();
		if (contents[0] == '!')
		{
			advancementLocations.insert({locationID});
			contents = contents.substr(1);
		}
		localItemID = std::stoi(contents);
		allLocationData.emplace(locationID, localItemID);
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
	Uint16 localItemID = allLocationData[locationID];
	APAll_ResolveItem(localItemID, true);

	Uint8 flags = APLocal_GetItemFlags(localItemID);
	std::string output = "You got your " + APLocal_BuildItemString(localItemID, flags);
	apmsg_enqueue(output.c_str());

	std::cout << "You got your "
	          << APLocal_BuildANSIString(localItemID, flags)
	          << " (" << lastItemIndex + 1 << "/" << totalLocationCount << ")" << std::endl;

	// Check for data cubes after every receive.
	APAll_CheckDataCubeCount();
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
			APAll_ResolveItem(item.item, false);
		}
	}

	// Check for data cube hunt completion after every receive.
	APAll_CheckDataCubeCount();
}

// ----------------------------------------------------------------------------

void Archipelago_ResendAllChecks()
{
	if (!ap || allLocationsChecked.size() == 0)
		return;

	std::list<int64_t> dataList(allLocationsChecked.begin(), allLocationsChecked.end());
	ap->LocationChecks(dataList);
}

void Archipelago_SendCheck(int checkID)
{
	checkID += ARCHIPELAGO_BASE_ID;
	allLocationsChecked.insert({checkID});
	localLocationsChecked.insert({checkID});

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

int Archipelago_GetTotalAnyoneCheckedCount(void)
{
	return allLocationsChecked.size();
}

int Archipelago_GetTotalWeCheckedCount(void)
{
	return localLocationsChecked.size();
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
		Uint16 localItemID = location.value().template get<Uint16>();
		shopPrices.emplace(locationID, localItemID);
	}
}

static void APAll_ParseWeaponCost(json &j)
{
	upgradePrices.clear();

	for (auto &item : j.items())
	{
		Uint16 localItemID = std::stoi(item.key());
		Uint16 itemPrice = item.value().template get<Uint16>();
		upgradePrices.emplace(localItemID, itemPrice);
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

		if (APSeedSettings.ShopMode == SHOP_MODE_HIDDEN)
		{
			// Hidden shop contents mode (local or remote, doesn't matter)
			itemName = "Archipelago Item " + std::to_string(i);
			shopItemBuffer[i].Icon = 9003;
		}
		else if (!ap)
		{
			// Local game, we have the info already
			Uint16 localItemID = allLocationData[shopStartID + i];
			itemName = apitems_AllNames[localItemID];
			shopItemBuffer[i].Icon = apitems_AllIcons[localItemID];
		}
		else if (scoutedShopLocations.count(shopStartID + i) == 1)
		{
			// Remote game, previously scouted location from the server
			int64_t playerID = scoutedShopLocations[shopStartID + i].player;
			int64_t itemID = scoutedShopLocations[shopStartID + i].item;

			itemName = ap->get_item_name(itemID, ap->get_player_game(playerID));

			if (playerID != ap->get_player_number())
			{
				std::string playerName = APRemote_GetPlayerName(playerID);
				strncpy(shopItemBuffer[i].PlayerName, playerName.c_str(), 40 - 1);
				shopItemBuffer[i].PlayerName[40 - 1] = 0;
			}

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
			itemName = "Unknown Item " + std::to_string(i);
			shopItemBuffer[i].Icon = 9003;
		}

		strncpy(shopItemBuffer[i].ItemName, itemName.c_str(), 128);

		// If it's not null-terminated, then the name's too long to fit.
		if (shopItemBuffer[i].ItemName[127])
		{
			// Add the null terminator back, and then an ellipsis to indicate truncation.
			shopItemBuffer[i].ItemName[127] = '\0';
			shopItemBuffer[i].ItemName[126] = '.';
			shopItemBuffer[i].ItemName[125] = '.';
			shopItemBuffer[i].ItemName[124] = '.';
		}
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

unsigned int Archipelago_GetUpgradeCost(Uint16 localItemID, int powerLevel)
{
	if (upgradePrices.count(localItemID) != 1)
		return 0;

	const int upgradeMultiplier[11] = {0, 1, 3, 6, 10, 15, 21, 28, 36, 45, 55};
	return upgradePrices[localItemID] * upgradeMultiplier[powerLevel];
}

unsigned int Archipelago_GetTotalUpgradeCost(Uint16 localItemID, int powerLevel)
{
	if (upgradePrices.count(localItemID) != 1 || powerLevel > 10)
		return 0;

	const int upgradeMultiplier[11] = {0, 1, 4, 10, 20, 35, 56, 84, 120, 165, 220};
	return upgradePrices[localItemID] * upgradeMultiplier[powerLevel];
}

// ============================================================================
// Other Gameplay Elements
// ============================================================================

aptwiddlelist_t APTwiddles; // Street Fighter-like codes, for Twiddle randomizer

// ----------------------------------------------------------------------------

static void APAll_ParseTwiddleData(json& j)
{
	memset(&APTwiddles, 0, sizeof(APTwiddles));

	for (auto &twiddle : j)
	{
		aptwiddle_t *ThisTwiddle = &APTwiddles.Code[APTwiddles.Count++];

		std::string twiddleName = twiddle.at("Name").template get<std::string>();
		strncpy(ThisTwiddle->Name, twiddleName.c_str(), 32 - 1);
		ThisTwiddle->Name[32 - 1] = 0;

		int i = 0;
		for (auto &command : twiddle.at("Command"))
		{
			ThisTwiddle->Command[i] = command.template get<Uint8>();
			if (++i >= 7)
				break;
		}
		for (; i < 8; ++i)
			ThisTwiddle->Command[i] = twiddle.at("Action").template get<Uint8>() + 100;
		ThisTwiddle->Cost = twiddle.at("Cost").template get<Uint8>();
	}
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
		" took one bullet too many."
		" can confirm:  Sudden decompression sucks.",
		" should've kept their shields up."
	},
	{ // DAMAGE_CONTACT
		" experienced a hull breach.",
		" tried to use ramming speed.",
		" disagreed with something stronger than them.",
		" should've tried dodging."
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

		std::string output = "<45";
		if (bounceJSON["data"].contains("cause"))
		{
			std::string message = bounceJSON["data"]["cause"].template get<std::string>();
			output += APRemote_CleanString(message);
		}
		if (output.find_first_not_of(' ', 3) == std::string::npos)
		{
			// Handle a whitespace-only cause like a nonexistant one
			output = "<45Killed by ";
			if (bounceJSON["data"].contains("source"))
			{
				std::string playerName = bounceJSON["data"]["source"].template get<std::string>();
				output += APRemote_CleanString(playerName);
			}
			else
				output += "DeathLink";
		}
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
	std::string cause = ourSlotName + death_messages[source][entry];
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

std::string localErrorString;

// Starts a game with a predefined "save" with all items and levels unlocked.
// Returns NULL on success, or error string on failure.
const char *Archipelago_StartDebugGame(void)
{
	if (gameInProgress || ap)
		return "Game already in progress.";

	ourSlotName = "local";
	APAll_InitMessageQueue();

	memset(&APSeedSettings, 0, sizeof(APSeedSettings));
	APSeedSettings.Tyrian2000Mode = tyrian2000detected;
	APSeedSettings.PlayEpisodes = tyrian2000detected ? 0x1F : 0xF;
	APSeedSettings.GoalEpisodes = tyrian2000detected ? 0x1F : 0xF;
	APSeedSettings.Difficulty = 2;
	APSeedSettings.SpecialMenu = true;

	APAll_FreshInit();

	// Grant the player everything unconditionally.
	APItems.FrontPorts = -1;
	APItems.RearPorts = -1;
	APItems.Specials = -1;
	APItems.BonusGames = -1;
	for (int i = 0; i < 5; ++i)
		APItems.Levels[i] = -1;
	for (int i = 0; i < 36; ++i)
		APItems.Sidekicks[i] = 2;

	APStats.PowerMaxLevel = 10;
	APStats.GeneratorLevel = 6;
	APStats.ArmorLevel = 28;
	APStats.ShieldLevel = 28;
	APItemChoices.FrontPort.Item = 500;

	totalLocationCount = 0;

	gameInProgress = true;
	isInRaceMode = false;

	APOptions.EnableDeathLink = false; // No point if you're alone
	APOptions.ArchipelagoRadar = true;
	APOptions.PracticeMode = true;

	return NULL;
}

// Starts a game from a local aptyrian file.
// Returns NULL on success, or error string on failure.
const char *Archipelago_StartLocalGame(FILE *file)
{
	if (gameInProgress || ap)
		return "Game already in progress.";

	APAll_InitMessageQueue();
	try
	{
		json aptyrianJSON = json::parse(file);

		if (aptyrianJSON.at("NetVersion") != APTYRIAN_NET_VERSION)
			throw std::runtime_error("Version mismatch. You may need to update APTyrian.");

		APSeedSettings.Tyrian2000Mode = aptyrianJSON.at("Settings").at("RequireT2K").template get<bool>();

		if (!tyrian2000detected && APSeedSettings.Tyrian2000Mode)
			throw std::runtime_error("This seed requires data from Tyrian 2000.");

		multiworldSeedName = aptyrianJSON.at("Seed").template get<std::string>();

		// Settings required to be present
		APSeedSettings.PlayEpisodes = aptyrianJSON["Settings"].at("Episodes").template get<unsigned int>();
		APSeedSettings.GoalEpisodes = aptyrianJSON["Settings"].at("Goal").template get<unsigned int>();
		APSeedSettings.Difficulty = aptyrianJSON["Settings"].at("Difficulty").template get<int>();

		// Optional settings
		APSeedSettings.DataCubesNeeded = aptyrianJSON["Settings"].value<int>("DataCubesNeeded", 0);
		APSeedSettings.HardContact = aptyrianJSON["Settings"].value<bool>("HardContact", false);
		APSeedSettings.ExcessArmor = aptyrianJSON["Settings"].value<bool>("ExcessArmor", false);
		APSeedSettings.ForceGameSpeed = aptyrianJSON["Settings"].value<int>("GameSpeed", 0);

		APSeedSettings.ShopMode = aptyrianJSON["Settings"].value<int>("ShopMode", SHOP_MODE_NONE);
		APSeedSettings.SpecialMenu = aptyrianJSON["Settings"].value<bool>("SpecialMenu", false);
		APSeedSettings.Christmas = aptyrianJSON["Settings"].value<bool>("Christmas", false);
		APSeedSettings.DeathLink = false; // No reason to enable in local play

		std::string obfuscated;
		json resultJSON;

		obfuscated = aptyrianJSON.value<std::string>("TwiddleData", "?6"); // Optional
		resultJSON = APAll_DeobfuscateJSONObject(obfuscated);
		APAll_ParseTwiddleData(resultJSON);

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
		ourSlotName = "local";
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
		localErrorString = e.what();
		std::cout << "Can't start game: " << localErrorString << std::endl;
		return localErrorString.c_str();
	}
	catch (...)
	{
		// Slot data JSON wasn't formed like expected to be
		localErrorString = "Invalid or corrupt data in file.";
		std::cout << "Can't start game: " << localErrorString << std::endl;
		return localErrorString.c_str();
	}

	// Init other data (can't fail)
	APLocal_InitLocationsPerRegion();
	gameInProgress = true;
	isInRaceMode = false;

	APOptions.EnableDeathLink = false; // No point if you're alone
	APOptions.ArchipelagoRadar = true;
	APOptions.PracticeMode = false;

	return NULL;
}

// ============================================================================
// Remote Play : Server I/O
// ============================================================================

static archipelago_connectionstat_t connection_stat = APCONN_NOT_CONNECTED;
static bool connection_ever_made = false; // Disables timeout
static std::string connection_error_desc = "";

#if SDL_VERSION_ATLEAST(2, 0, 18)
static uint64_t connection_start_time; // For initial connection timeout
#else
static uint32_t connection_start_time; // 32-bit for older SDL2 versions
#endif

// ----------------------------------------------------------------------------

// Call when a fatal connection error has occurred and the connection should be closed.
// Stuff like receiving a slot refusal, or getting invalid data.
static void APRemote_FatalError(std::string reason)
{
	connection_stat = APCONN_FATAL_ERROR;
	connection_error_desc = reason;

	// This is ... a conundrum. We are in the middle of a game, got disconnected from the AP server,
	// and in the process of reconnecting, have received a refusal to connect to slot or some other error
	// that demands we stop. We can't proceed past this point, so we show an error dialog and close.
	if (gameInProgress)
	{
		Archipelago_Save();

		std::stringstream s;
		s << "While attempting to reconnect to the Archipelago server, a fatal error occurred:" << std::endl;
		s << reason << std::endl;
		s << std::endl;
		s << "The game is now in an unrecoverable state, and will be closed." << std::endl;
		s << "Your progress has been automatically saved." << std::endl;
		std::string full_error_text = s.str();

		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Fatal Error", full_error_text.c_str(), NULL);
		exit(1);
	}
}

// *** Callback for APClient's "OnSocketDisconnected" ***
static void APRemote_CB_Disconnect()
{
	connection_stat = APCONN_CONNECTING;
	if (gameInProgress)
		apmsg_enqueue("<45Disconnected");
}

// *** Callback for APClient's "OnSocketError" ***
static void APRemote_CB_Error(const std::string& error)
{
	connection_error_desc = error;
	connection_stat = APCONN_CONNECTING;
	if (gameInProgress)
		apmsg_enqueue(("<45" + error).c_str());
}

// *** Callback for "RoomInfo" ***
static void APRemote_CB_RoomInfo()
{
	if (ap->has_password() && cx_serverPassword.empty())
	{
		connection_stat = APCONN_NEED_PASSWORD;
	}
	else
	{
		connection_stat = APCONN_TENTATIVE;
		ap->ConnectSlot(ourSlotName, cx_serverPassword, 0b011, {}, targetAPVersion);
	}
}

// *** Callback for "Retrieved" ***
static void APRemote_CB_Retrieved(const std::map<std::string, json>& keys)
{
	if (keys.count("_read_race_mode") && keys.at("_read_race_mode").is_number())
		isInRaceMode = (keys.at("_read_race_mode").template get<int>() != 0);
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
			all_reasons += "Slot '" + ourSlotName + "' does not exist.";
		else if (reason == "InvalidGame")
			all_reasons += "Slot '" + ourSlotName + "' is not playing Tyrian.";
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
	if (gameInProgress) // Reconnected mid game -- our old data should be fine.
	{
		// Except it's _NOT_ fine, if the seed doesn't match.
		// We need to check for this, because the Archipelago server can potentially close our currently running game
		// and then open up an entirely different game on the same port, in extreme circumstances.
		std::string newSeedName = ap->get_seed();
		if (newSeedName != multiworldSeedName)
		{
			APRemote_FatalError("Game seed mismatch after reconnecting.");
			return;
		}
		apmsg_enqueue("<25Reconnected to Archipelago server.");
	}
	else // Initial connection to the server.
	{
		APAll_InitMessageQueue();
		try
		{
			if (slot_data.at("NetVersion") != APTYRIAN_NET_VERSION)
				throw std::runtime_error("Version mismatch. You may need to update APTyrian.");

			APSeedSettings.Tyrian2000Mode = slot_data.at("Settings").at("RequireT2K").template get<bool>();

			if (!tyrian2000detected && APSeedSettings.Tyrian2000Mode)
				throw std::runtime_error("Slot '" + ourSlotName + "' requires data from Tyrian 2000.");

			multiworldSeedName = ap->get_seed();

			// Settings required to be present
			APSeedSettings.PlayEpisodes = slot_data["Settings"].at("Episodes").template get<unsigned int>();
			APSeedSettings.GoalEpisodes = slot_data["Settings"].at("Goal").template get<unsigned int>();
			APSeedSettings.Difficulty = slot_data["Settings"].at("Difficulty").template get<int>();

			// Optional settings
			APSeedSettings.DataCubesNeeded = slot_data["Settings"].value<int>("DataCubesNeeded", 0);
			APSeedSettings.HardContact = slot_data["Settings"].value<bool>("HardContact", false);
			APSeedSettings.ExcessArmor = slot_data["Settings"].value<bool>("ExcessArmor", false);
			APSeedSettings.ForceGameSpeed = slot_data["Settings"].value<int>("GameSpeed", 0);

			APSeedSettings.ShopMode = slot_data["Settings"].value<int>("ShopMode", SHOP_MODE_NONE);
			APSeedSettings.SpecialMenu = slot_data["Settings"].value<bool>("SpecialMenu", false);
			APSeedSettings.Christmas = slot_data["Settings"].value<bool>("Christmas", false);
			APSeedSettings.DeathLink = slot_data["Settings"].value<bool>("DeathLink", false);

			totalLocationCount = slot_data.at("LocationMax").template get<int>();

			std::string obfuscated;
			json resultJSON;

			obfuscated = slot_data.value<std::string>("TwiddleData", "?6"); // Optional
			resultJSON = APAll_DeobfuscateJSONObject(obfuscated);
			APAll_ParseTwiddleData(resultJSON);

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

			// When initially connecting, enable DeathLink client side if the game has it set.
			APOptions.EnableDeathLink = APSeedSettings.DeathLink;

			// Init other data.
			locationsPerRegion.clear(); // Will be properly init later (after data package is received)
			gameInProgress = true;

			// Check race mode status from server. Until proven otherwise, assume race mode.
			isInRaceMode = true;
			ap->Get({"_read_race_mode"});
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
	}

	std::cout << "Successfully connected." << std::endl;
	connection_stat = APCONN_READY;
	connection_ever_made = true;

	// Update tags if we need to.
	if (APSeedSettings.DeathLink && APOptions.EnableDeathLink)
		Archipelago_UpdateDeathLinkState();

	// Resend all checks to make sure server is up to date.
	Archipelago_ResendAllChecks();
}

// ----------------------------------------------------------------------------

void Archipelago_SetPassword(const char *password)
{
	if (gameInProgress)
		return;

	cx_serverPassword = password;
	if (ap && connection_stat == APCONN_NEED_PASSWORD)
		APRemote_CB_RoomInfo();
}

void Archipelago_Connect(const char *slot_name, const char *address, const char *password)
{
	if (gameInProgress)
		return;

	ourSlotName = (slot_name) ? slot_name : "";
	cx_serverAddress = (address) ? address : "";
	cx_serverPassword = (password) ? password : "";

	if (cx_serverAddress.empty())
		return APRemote_FatalError("Please provide a server address to connect to.");
	if (ourSlotName.empty())
		return APRemote_FatalError("Please provide a slot name.");

	std::cout << "Connecting to Archipelago server ("
	          << ourSlotName      << "@"
	          << cx_serverAddress << ")" << std::endl;
	ap.reset();

	APOptions.EnableDeathLink = true; // Only relevant if DeathLink was turned on in the YAML
	APOptions.ArchipelagoRadar = true;
	APOptions.PracticeMode = false;

	std::string cert = std::filesystem::exists("./cacert.pem") ? "./cacert.pem" : "";
	ap.reset(new APClient(clientUUID, "Tyrian", cx_serverAddress));

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
	ap->set_retrieved_handler(APRemote_CB_Retrieved);

	connection_stat = APCONN_CONNECTING;
	connection_error_desc = "";
	connection_ever_made = false;

#if SDL_VERSION_ATLEAST(2, 0, 18)
	connection_start_time = SDL_GetTicks64();
#else
	connection_start_time = SDL_GetTicks();
#endif
}

void Archipelago_Poll(void)
{
	if (!ap)
		return;

	if (connection_stat == APCONN_FATAL_ERROR)
	{
		// Fatal error. Disconnect from the Archipelago server and don't retry.
		Archipelago_Disconnect();
	}
	else if (connection_ever_made)
	{
		// Momentary connection errors are fine. Just keep retrying.
		ap->poll();
	}
	else
	{
#if SDL_VERSION_ATLEAST(2, 0, 18)
		if (connection_stat == APCONN_CONNECTING && SDL_GetTicks64() > connection_start_time + 10000)
#else
		if (connection_stat == APCONN_CONNECTING && !SDL_TICKS_PASSED(SDL_GetTicks(), connection_start_time + 10000))
#endif
		{
			// Connection timeout, disconnect from the server and don't retry further.
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

bool Archipelago_IsRacing(void)
{
	return isInRaceMode;
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
