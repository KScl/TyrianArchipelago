#include <assert.h>

// In general, the following prefixes are used throughout this file:
// APAll_ is for handling functions common to both local and remote play
// APLocal_ is for handling functions specific to local play (i.e., using an aptyrian file)
// APRemote_ is for handling functions specific to remote play (i.e., connected to an Archipelago server)
// Archipelago_ is for public functions that the C side of the code calls

// Not functional, apclient uses implicit conversions.
//#define JSON_USE_IMPLICIT_CONVERSIONS 0

// Silence warnings from apclient/wswrap
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <apclient.hpp>
#include <apuuid.hpp>
#pragma GCC diagnostic pop

extern "C" {
	// Function definitions that go out C side need to not be mangled by C++
	#include "apconnect.h"

	// List of all AP Items, used for local play
	#include "apitems.h"

	// TODO Please don't have this here!!
	void JE_drawTextWindowColorful(const char *);

	// These are things from C side that we need here.
	extern bool tyrian2000detected; // opentyr.h
	extern const int font_ascii[256]; // fonthand.h
}

using nlohmann::json;

// Settings package received from server.
archipelago_settings_t ArchipelagoOpts;

std::string connection_slot_name = "";
std::string connection_server_address = "";
std::string connection_password = "";

// ----------------------------------------------------------------------------

// Base ID for all location/item checks, Archipelago side.
// Add to all outgoing ID numbers and remove from all incoming ones.
#define ARCHIPELAGO_BASE_ID 20031000

// Should match TyrianWorld.aptyrian_net_version
#define APTYRIAN_NET_VERSION 1

static std::unique_ptr<APClient> ap;

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
		throw new std::runtime_error("Invalid or corrupt data received.");
	return deobfJSON;
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
			return 1;
		case 503: case 504: case 521: case 721: case 805:
		case 806: case 807: case 808: case 903: case 904:
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
	s << APLocal_ItemNames[localItemID] << "\u001b[0m";
	return s.str();
}

// Creates a string out of an item ID for colorful print.
static std::string APLocal_BuildItemString(Uint16 localItemID, Uint8 flags)
{
	if (localItemID > 999)
		return "";

	std::stringstream s;
	if      (flags & 1) s << "^95"; // Progression
	else if (flags & 2) s << "^83"; // Useful
	else if (flags & 4) s << "^45"; // Trap (none currently in local pool, may change)
	else                s << "^08"; // Filler
	s << APLocal_ItemNames[localItemID] << "^04";
	return s.str();
}

// ----------------------------------------------------------------------------

// Creates a string out of a NetworkItem for colorful print.
static std::string APRemote_BuildItemString(const APClient::NetworkItem &item)
{
	if (!ap)
		return "";

	std::stringstream s;
	if      (item.flags & 1) s << "^95"; // Progression
	else if (item.flags & 2) s << "^83"; // Useful
	else if (item.flags & 4) s << "^45"; // Trap
	else                     s << "^08"; // Filler
	s << ap->get_item_name(item.item, ap->get_player_game(item.player)) << "^04";
	return s.str();
}

// Creates a string out of a location for colorful print.
// There is no local version of this -- there's no need for it, this is primarily used for AP server hints.
static std::string APRemote_BuildLocationString(int64_t locationID, int64_t playerID)
{
	if (!ap)
		return "";

	std::stringstream s;
	s << "^08" << ap->get_location_name(locationID, ap->get_player_game(playerID)) << "^04";
	return s.str();
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
		if (*args.receiving == args.item->player)
			s << "You got your ";
		else
			s << "Received ";

		s << APRemote_BuildItemString(*args.item);

		if (*args.receiving != args.item->player)
			s << " from " << ap->get_player_alias(args.item->player);
	}
	else
		return;

	std::string output = s.str();
	JE_drawTextWindowColorful(output.c_str());
}

// ============================================================================
// Items
// ============================================================================

// These are C structs that are available C side.
apitem_t APItems; // Weapons, levels; One time collectibles, etc.
apstat_t APStats; // Cash, Armor, Power, etc. Upgradable stats.

// ----------------------------------------------------------------------------

static const int remoteCashItemValues[] =
	{50, 75, 100, 150, 200, 300, 375, 500, 750, 800, 1000, 2000, 5000, 7500, 10000, 20000, 40000, 750000, 100000, 1000000};

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
	else if (item < 836) ++APItems.Sidekicks[item - 800];

	// Armor, shield, money... statistic-affecting things
	else if (item == 900) ++APStats.PowerMaxLevel;
	else if (item == 901) ++APStats.GeneratorLevel;
	else if (item == 902) APStats.ArmorLevel += 2;
	else if (item == 903) APStats.ShieldLevel += 2;
	else if (item == 904) APStats.SolarShield = true;
	else if (item == 904) ++APStats.QueuedSuperBombs;
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
	APStats.GeneratorLevel = j.value<int>("Generator", 0);
	APStats.SolarShield = j.value<bool>("SolarShield", false);
	APStats.Cash = j.value<int64_t>("Credits", 0);
}

// ============================================================================
// Outgoing and Incoming Locations and Checks
// ============================================================================

// Locations in our seed that hold progression items (for us or someone else).
// Filled in by both ParseLocationData and ParseProgressionData.
// This is local to only us, so uses local IDs (without ARCHIPELAGO_BASE_ID added).
static std::set<uint16_t> advancementLocations;

// All data about locations in our seed. Only used in local play (no AP server).
// As such, it also uses local IDs.
static std::unordered_map<uint16_t, uint16_t> allLocationData;

// Locations in our seed that we've checked (or had checked for us)
// This is filled with stuff directly from AP, so it uses global IDs (even when playing locally)
static std::set<int64_t> allLocationsChecked;

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

// Called in local games only to handle the results of a location check.
static void APLocal_ReceiveItem(int64_t locationID)
{
	if (allLocationData.count(locationID - ARCHIPELAGO_BASE_ID) == 0)
		return;

	Uint16 itemID = allLocationData[locationID - ARCHIPELAGO_BASE_ID];
	APAll_ResolveItem(itemID);

	Uint8 flags = APLocal_GetItemFlags(itemID, locationID);
	std::string output = "You got your " + APLocal_BuildItemString(itemID, flags);
	JE_drawTextWindowColorful(output.c_str());

	std::cout << "You got your "
	          << APLocal_BuildANSIString(itemID, flags)
	          << " (" << allLocationsChecked.size() << "/" << allLocationData.size() << ")"
	          << std::endl;
}

// ----------------------------------------------------------------------------
// Locations across the multiworld that have had our items
static std::set<int64_t> allOurItemLocations;

// Parses "ProgressionData" structure. Only present in remote slot_data.
// Contains a list of all locations that are known to have progression items, and nothing more.
static void APRemote_ParseProgressionData(json &j)
{
	advancementLocations.clear();
	advancementLocations.insert(j.begin(), j.end());
}

// *** Callback for APClient's "OnLocationChecked" ***
// Keeps a local copy of all checked locations, used to suppress items from spawning
// if they've already been collected in the seed
static void APRemote_CB_RemoteLocationCheck(const std::list <int64_t>& locations)
{
	allLocationsChecked.insert(locations.begin(), locations.end());
}

// *** Callback for "ReceiveItems" ***
// Handles new items sent to us from the AP server.
static void APRemote_CB_ReceiveItem(const std::list<APClient::NetworkItem>& items)
{
	for (auto const &item : items)
	{
		if (allOurItemLocations.count(item.location) == 1)
			continue; // Already handled the item from this location, ignore
		allOurItemLocations.insert({item.location});

		// Messaging is handled when we receive the PrintJSON.
		APAll_ResolveItem(item.item);
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

// ============================================================================
// DeathLink
// ============================================================================

// Set to true in bounce handler if someone else has died.
// Set to false by the game at start of stage.
bool apDeathLinkReceived;

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

void APRemote_CB_Bounce(const json& bounceJson)
{
	if (!ArchipelagoOpts.deathlink)
		return;

	std::cout << bounceJson << std::endl;
	if (lastBounceTime == bounceJson["data"].at("time"))
		return; // Ignore own deaths

	apDeathLinkReceived = true;

	std::string output = "^45" + bounceJson["data"].at("cause").template get<std::string>();
	JE_drawTextWindowColorful(output.c_str());
}

// ----------------------------------------------------------------------------

// Sends death out to the multiworld.
void Archipelago_SendDeathLink(damagetype_t source)
{
	if (!ap || !ArchipelagoOpts.deathlink)
		return;

	if (apDeathLinkReceived || source == DAMAGE_DEATHLINK)
		return; // Prevent chained deathlinks

	int entry = rand() % death_messages[source].size();
	std::string cause = connection_slot_name + death_messages[source][entry];
	lastBounceTime = ap->get_server_time();

	json bounceData = { {"time", lastBounceTime}, {"cause", cause}, {"source", ap->get_slot()} };
	ap->Bounce(bounceData, {}, {}, {"DeathLink"});

	JE_drawTextWindowColorful("^45Sending death to your friends...");
}

// ============================================================================
// Local Play : File I/O
// ============================================================================

bool Archipelago_StartLocalGame(FILE *file)
{
	if (ap) // Excuse me? No? Not if we're playing online already
		return false;

	try
	{
		json aptyrianJSON = json::parse(file);

		if (aptyrianJSON.at("NetVersion") > APTYRIAN_NET_VERSION)
			throw new std::runtime_error("Version mismatch. You need to update APTyrian.");

		ArchipelagoOpts.game_difficulty = aptyrianJSON["Settings"].at("Difficulty").template get<int>();
		ArchipelagoOpts.hard_contact = aptyrianJSON["Settings"].at("HardContact").template get<bool>();
		ArchipelagoOpts.excess_armor = aptyrianJSON["Settings"].at("ExcessArmor").template get<bool>();
		ArchipelagoOpts.show_twiddle_inputs = aptyrianJSON["Settings"].at("ShowTwiddles").template get<bool>();
		ArchipelagoOpts.archipelago_radar = aptyrianJSON["Settings"].at("APRadar").template get<bool>();
		ArchipelagoOpts.christmas = aptyrianJSON["Settings"].at("Christmas").template get<bool>();
		ArchipelagoOpts.deathlink = false;

		std::string obfuscated = aptyrianJSON.at("StartState").template get<std::string>();
		json resultJSON = APAll_DeobfuscateJSONObject(obfuscated);
		APAll_ParseStartState(resultJSON);

		obfuscated = aptyrianJSON.at("LocationData").template get<std::string>();
		resultJSON = APAll_DeobfuscateJSONObject(obfuscated);
		APLocal_ParseLocationData(resultJSON);
	}
	catch (const std::exception&)
	{
		return false;
	}
	return true;
}

// ============================================================================
// Remote Play : Server I/O
// ============================================================================

static archipelago_connectionstat_t connection_stat = APCONN_NOT_CONNECTED;
static bool connection_ever_made = false; // More retry attempts if we know connection worked in past
static int connection_errors = 0;
static std::string connection_error_desc = "";

// ----------------------------------------------------------------------------

// Call when a fatal connection error has occurred and the connection should be closed.
// Stuff like receiving a slot refusal, or getting invalid data.
static void APRemote_FatalError(std::string reason)
{
	connection_stat = APCONN_NOT_CONNECTED;
	connection_errors = 999;
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
	++connection_errors;
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
	std::cout << slot_data << std::endl;
	try
	{
		if (slot_data.at("NetVersion") > APTYRIAN_NET_VERSION)
			throw new std::runtime_error("Version mismatch. You need to update APTyrian.");

		if (!tyrian2000detected && slot_data.at("Settings").at("RequireT2K") == true)
			throw new std::runtime_error("Slot '" + connection_slot_name + "' requires data from Tyrian 2000.");

		ArchipelagoOpts.game_difficulty = slot_data["Settings"].at("Difficulty").template get<int>();
		ArchipelagoOpts.hard_contact = slot_data["Settings"].at("HardContact").template get<bool>();
		ArchipelagoOpts.excess_armor = slot_data["Settings"].at("ExcessArmor").template get<bool>();
		ArchipelagoOpts.show_twiddle_inputs = slot_data["Settings"].at("ShowTwiddles").template get<bool>();
		ArchipelagoOpts.archipelago_radar = slot_data["Settings"].at("APRadar").template get<bool>();
		ArchipelagoOpts.christmas = slot_data["Settings"].at("Christmas").template get<bool>();
		ArchipelagoOpts.deathlink = slot_data["Settings"].at("DeathLink").template get<bool>();

		std::string obfuscated = slot_data.at("StartState").template get<std::string>();
		json resultJSON = APAll_DeobfuscateJSONObject(obfuscated);
		APAll_ParseStartState(resultJSON);

		obfuscated = slot_data.at("ProgressionData").template get<std::string>();
		resultJSON = APAll_DeobfuscateJSONObject(obfuscated);
		APRemote_ParseProgressionData(resultJSON);
	}
	catch (const json::out_of_range&)
	{
		// Slot data JSON wasn't formed like expected to be
		APRemote_FatalError("Invalid or corrupt data received.");
		return;
	}
	catch (const std::runtime_error& e)
	{
		APRemote_FatalError(e.what());
		return;
	}

	std::cout << "Successfully connected." << std::endl;
	connection_stat = APCONN_READY;
	connection_errors = 0;
	connection_ever_made = true;

	// Resend all checks to make sure server is up to date.
	Archipelago_ResendAllChecks();
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
	std::cout << "Connecting to Archipelago server ("
	          << connection_slot_name      << "@"
	          << connection_server_address << ")" << std::endl;
	ap.reset();
	std::string uuid = ap_get_uuid("test.uuid");
	std::cout << uuid << std::endl;

	connection_stat = APCONN_CONNECTING;
	connection_errors = 0;
	connection_error_desc = "";
	connection_ever_made = false;

	if (connection_server_address.empty())
		return APRemote_FatalError("Please provide a server address to connect to.");
	if (connection_slot_name.empty())
		return APRemote_FatalError("Please provide a slot name.");

	ap.reset(new APClient(uuid, "Tyrian", connection_server_address));

	// Chat and other communications
	ap->set_print_json_handler(APRemote_CB_ReceivePrint);

	// Checks and Items
	ap->set_items_received_handler(APRemote_CB_ReceiveItem);
	ap->set_location_checked_handler(APRemote_CB_RemoteLocationCheck);

	// DeathLink
	ap->set_bounced_handler(APRemote_CB_Bounce);

	// Server IO
    ap->set_socket_disconnected_handler(APRemote_CB_Disconnect);
    ap->set_socket_error_handler(APRemote_CB_Error);
	ap->set_room_info_handler(APRemote_CB_RoomInfo);
	ap->set_slot_refused_handler(APRemote_CB_SlotRefused);
	ap->set_slot_connected_handler(APRemote_CB_SlotConnected);
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
		// Only tolerate two errors before giving up entirely
		if (connection_errors >= 2)
			Archipelago_Disconnect();
		else
			ap->poll();
	}
}

void Archipelago_Disconnect(void)
{
	// Disconnect is only called when we're leaving the multiworld server (or ending a local game),
	// so invalidate all stored data.
	allLocationsChecked.clear();
	allOurItemLocations.clear();

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
