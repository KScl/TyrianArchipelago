#include <apclient.hpp>
#include <apuuid.hpp>

extern "C" {
	// Function definitions that go out C side need to not be mangled by C++
	#include "apconnect.h"
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

static std::unique_ptr<APClient> ap;

// ============================================================================
// Checks and Items
// ============================================================================

// Locations in our seed that we've checked (or had checked for us)
static std::set<int64_t> allLocationsChecked;

// Locations across the multiworld that have had our items
static std::set<int64_t> allOurItemLocations;

// ----------------------------------------------------------------------------

static void APSocket_RemoteLocationCheck(const std::list <int64_t>& locations)
{
	allLocationsChecked.insert(locations.begin(), locations.end());
}

static void APSocket_ReceiveItem(const std::list<APClient::NetworkItem>& items)
{
	for (auto const &item : items)
	{
		if (allOurItemLocations.count(item.location) == 1)
			continue; // Already handled the item from this location, ignore
		allOurItemLocations.insert({item.location});

		std::cout << ap->get_item_name(item.item) << std::endl;
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

	if (ap)
		ap->LocationChecks({checkID});
}

bool Archipelago_WasChecked(int checkID)
{
	return allLocationsChecked.count(checkID + ARCHIPELAGO_BASE_ID) == 1;
}

// ============================================================================
// DeathLink
// ============================================================================

// Set to true in bounce handler if someone else has died.
// Set to false by the game at start of stage.
bool apDeathLinkReceived;

static double lastBounceTime; // Required to filter out own deathlinks

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

void APSocket_Bounce(const json& bounceJson)
{
	std::cout << bounceJson << std::endl;
	if (lastBounceTime == bounceJson["data"].at("time"))
		return; // Ignore own deaths

	apDeathLinkReceived = true;
}

// ----------------------------------------------------------------------------

// Sends death out to the multiworld.
void Archipelago_SendDeathLink(damagetype_t source)
{
	if (!ap)
		return;

	if (apDeathLinkReceived || source == DAMAGE_DEATHLINK)
		return; // Prevent chained deathlinks

	int entry = rand() % death_messages[source].size();
	std::string cause = connection_slot_name + death_messages[source][entry];
	lastBounceTime = ap->get_server_time();

	json bounceData = { {"time", lastBounceTime}, {"cause", cause}, {"source", ap->get_slot()} };
	ap->Bounce(bounceData, {}, {}, {"DeathLink"});
}

// ============================================================================
// Server I/O
// ============================================================================

static archipelago_connectionstat_t connection_stat = APCONN_NOT_CONNECTED;
static bool connection_ever_made = false; // More retry attempts if we know connection worked in past
static int connection_errors = 0;
static std::string connection_error_desc = "";

// ----------------------------------------------------------------------------

static void APSocket_Disconnect()
{
	connection_stat = APCONN_CONNECTING;
}

static void APSocket_Error(const std::string& error)
{
	++connection_errors;
	connection_error_desc = error;
	connection_stat = APCONN_CONNECTING;
}

static void APSocket_RoomInfo()
{
	connection_stat = APCONN_TENTATIVE;

	std::list<std::string> tags;
	tags.push_back("DeathLink");
	ap->ConnectSlot(connection_slot_name, connection_password, 0b011, tags, {0,4,4});
}

static void APSocket_SlotRefused(const std::list<std::string>& reasons)
{
	connection_stat = APCONN_NOT_CONNECTED;
	connection_errors = 999;
	connection_error_desc = "";

	for (auto const &reason : reasons)
	{
		if (connection_error_desc != "")
			connection_error_desc += " ";

		if (reason == "InvalidSlot")
			connection_error_desc += "Slot '" + connection_slot_name + "' does not exist.";
		else if (reason == "InvalidGame")
			connection_error_desc += "Slot '" + connection_slot_name + "' is not playing Tyrian.";
		else if (reason == "IncompatibleVersion")
			connection_error_desc += "Server version incompatible with this client.";
		else if (reason == "InvalidPassword")
			connection_error_desc += "Password missing or incorrect.";
		else if (reason == "InvalidItemsHandling")
			connection_error_desc += "Internal misconfiguration.";
		else
			connection_error_desc += "Unknown reason.";
	}
	std::cout << "Received refusal from Archipelago server: " << connection_error_desc << std::endl;
}

static void APSocket_SlotConnected(const json& slot_data)
{
	std::cout << slot_data << std::endl;
	ArchipelagoOpts.tyrian_2000_support_wanted = slot_data["Settings"].at("Tyrian2000");
	ArchipelagoOpts.game_difficulty = slot_data["Settings"].at("Difficulty");
	ArchipelagoOpts.hard_contact = slot_data["Settings"].at("HardContact");
	ArchipelagoOpts.excess_armor = slot_data["Settings"].at("ExcessArmor");
	ArchipelagoOpts.show_twiddle_inputs = slot_data["Settings"].at("TwiddleCmds");

	ArchipelagoOpts.archipelago_radar = slot_data["Settings"].at("APRadar");
	ArchipelagoOpts.christmas = slot_data["Settings"].at("Christmas");

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

	ap.reset(new APClient(uuid, "Tyrian", connection_server_address));

	// Checks and Items
	ap->set_items_received_handler(APSocket_ReceiveItem);
	ap->set_location_checked_handler(APSocket_RemoteLocationCheck);

	// DeathLink
	ap->set_bounced_handler(APSocket_Bounce);

	// Server IO
    ap->set_socket_disconnected_handler(APSocket_Disconnect);
    ap->set_socket_error_handler(APSocket_Error);
	ap->set_room_info_handler(APSocket_RoomInfo);
	ap->set_slot_refused_handler(APSocket_SlotRefused);
	ap->set_slot_connected_handler(APSocket_SlotConnected);

    ap->set_print_handler([](const std::string& msg) {
        printf("%s\n", msg.c_str());
    });
    ap->set_print_json_handler([](const std::list<APClient::TextNode>& msg) {
        printf("%s\n", ap->render_json(msg, APClient::RenderFormat::ANSI).c_str());
    });
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
	if (!ap)
		return;

	printf("Disconnecting from Archipelago server.\n");
	connection_stat = APCONN_NOT_CONNECTED;
	ap.reset();

	// Disconnect is only called when we're leaving the multiworld server, so invalidate all stored data.
	allLocationsChecked.clear();
	allOurItemLocations.clear();
}

void Archipelago_DisconnectWithError(const char *error_msg)
{
	connection_error_desc = error_msg;
	Archipelago_Disconnect();
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
