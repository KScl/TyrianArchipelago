#include <apclient.hpp>
#include <apuuid.hpp>

extern "C" {
	// Function definitions that go out C side need to not be mangled by C++
	#include "apconnect.h"
}

using nlohmann::json;

// Settings package received from server.
archipelago_settings_t ArchipelagoOpts;

std::string slot_name = "";
std::string connection_password = "";

// ----------------------------------------------------------------------------

static std::unique_ptr<APClient> ap;
static archipelago_connectionstat_t connection_stat = APCONN_NOT_CONNECTED;
static bool connection_ever_made = false; // More retry attempts if we know connection worked in past
static int connection_errors = 0;
static std::string connection_error_desc = "";

static void APSocketDisconnect()
{
	connection_stat = APCONN_CONNECTING;
}

static void APSocketError(const std::string& error)
{
	++connection_errors;
	connection_error_desc = error;
	connection_stat = APCONN_CONNECTING;
}

static void APSocketRoomInfo()
{
	connection_stat = APCONN_TENTATIVE;

	std::list<std::string> tags;
	tags.push_back("DeathLink");
	ap->ConnectSlot(slot_name, connection_password, 0b111, tags, {0,4,4});
}

static void APSocketSlotRefused(const std::list<std::string>& reasons)
{
	connection_stat = APCONN_NOT_CONNECTED;
	connection_errors = 999;
	connection_error_desc = "";

	for (auto const &reason : reasons)
	{
		if (connection_error_desc != "")
			connection_error_desc += " ";

		if (reason == "InvalidSlot")
			connection_error_desc += "Slot '" + slot_name + "' does not exist.";
		else if (reason == "InvalidGame")
			connection_error_desc += "Slot '" + slot_name + "' is not playing Tyrian.";
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

static void APSocketSlotConnected(const json& slot_data)
{
	std::cout << "Successfully connected." << std::endl;
	connection_stat = APCONN_READY;
	connection_errors = 0;
	connection_ever_made = true;
}

// ----------------------------------------------------------------------------

void Archipelago_Connect()
{
	printf("Connecting to Archipelago server.\n");
	ap.reset();
	std::string uuid = ap_get_uuid("test.uuid");
	std::cout << uuid << std::endl;

	connection_stat = APCONN_CONNECTING;
	connection_errors = 0;
	connection_error_desc = "";
	connection_ever_made = false;

	slot_name = "KS";
	connection_password = "";
	ap.reset(new APClient(uuid, "Tyrian", "ws://0.0.0.0:38281"));

    ap->set_socket_disconnected_handler(APSocketDisconnect);
    ap->set_socket_error_handler(APSocketError);
	ap->set_room_info_handler(APSocketRoomInfo);
	ap->set_slot_refused_handler(APSocketSlotRefused);
}

void Archipelago_Poll()
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

void Archipelago_Disconnect()
{
	if (!ap)
		return;

	printf("Disconnecting from Archipelago server.\n");
	connection_stat = APCONN_NOT_CONNECTED;
	ap.reset();
}

archipelago_connectionstat_t Archipelago_ConnectionStatus()
{
	if (!ap)
		return APCONN_NOT_CONNECTED;
	return connection_stat;
}

const char* Archipelago_GetConnectionError()
{
	return connection_error_desc.c_str();
}

// ----------------------------------------------------------------------------
// DeathLink
// ----------------------------------------------------------------------------

// Set to true in bounce handler if someone else has died.
// Set to false by the game at start of stage.
bool apDeathLinkReceived;

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

// Sends death out to the multiworld.
void Archipelago_SendDeathLink(damagetype_t source)
{
	if (apDeathLinkReceived || source == DAMAGE_DEATHLINK)
		return; // Prevent chained deathlinks

	int entry = rand() % death_messages[source].size();
	std::string cause = slot_name + death_messages[source][entry];

	std::cout << "Death sent: " << cause << std::endl;
}
