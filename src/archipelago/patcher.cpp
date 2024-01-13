
#include <cstdarg>
#include <iostream>
#include <list>

#include <nlohmann/json.hpp>
#include <zlib.h>

extern "C" {
	// Function definitions that go out C side need to not be mangled by C++
	#include "patcher.h"
}

using nlohmann::json;
using namespace nlohmann::literals;

static json patchData;

static std::string crc32str(Uint32 crc)
{
	std::stringstream s;
	s << std::setfill('0') << std::setw(8) << std::hex << crc;
	return s.str();
}

bool Patcher_SystemInit(void)
{
	bool ready = false;

	FILE *patchJson = fopen("patch.jsonc", "r");
	if (patchJson != NULL)
	{
		try
		{
			patchData = json::parse(patchJson, nullptr, true, true);
			ready = true;
		}
		catch (const json::parse_error& e)
		{
			std::cout << e.what() << std::endl;
		}
		fclose(patchJson);
	}
	else
	{
		std::cout << "Patch file not found." << std::endl;
	}
	return ready;
}

// ----------------------------------------------------------------------------

// JSON blob of the currently loaded patch, nullptr if there isn't one.
static json loadedPatch = nullptr;

// List of all event numbers that the patch modifies.
// Used to speed up patch processing.
static std::list<int> patchEvents;

// 
static int eventCount = 0;

static void jsonEventToGameEvent(json &j, Uint16 x)
{
	eventRec[x].eventtime = j.at("time");
	eventRec[x].eventtype = j.at("event");

	// By default, since so many events use it
	eventRec[x].eventdat4 = j.value<Uint8> ("linknum", 0);

	// Sint16 eventdat, eventdat2;
	// Sint8  eventdat3, eventdat5, eventdat6;
	// Uint8  eventdat4;
	switch (eventRec[x].eventtype)
	{
		case  12: // NewEnemy_Ground4X4Custom
			eventRec[x].eventdat6 = j.value<Sint8> ("bg", 0);
			goto groundcustom_rejoin;

		case   6: // NewEnemy_Ground
		case   7: // NewEnemy_Top
		case  10: // NewEnemy_Ground2
		case  15: // NewEnemy_Sky
		case  17: // NewEnemy_GroundBottom
		case  18: // NewEnemy_SkyBottom
		case  23: // NewEnemy_Sky2Bottom
		case  32: // NewEnemy_Sky2Bottom190
		case  56: // NewEnemy_Ground2Bottom
			eventRec[x].eventdat6 = j.value<Sint8> ("fixedmovey", 0);
		groundcustom_rejoin:
			eventRec[x].eventdat  = j.value<Sint16>("type", 0);
			eventRec[x].eventdat2 = j.value<Sint16>("ex", 0);
			eventRec[x].eventdat3 = j.value<Sint8> ("eyc", 0);
			eventRec[x].eventdat5 = j.value<Sint8> ("ey", 0);
			break;

		case  16: // VoicedCallout
			if (j.at("output").is_number())
				eventRec[x].eventdat = j.at("output");
			else // assume string
			{
				std::string evStr = j["output"].template get<std::string>();
				if      (evStr == "Enemy approaching from behind.") eventRec[x].eventdat = 1;
				else if (evStr == "Large mass detected ahead!")     eventRec[x].eventdat = 2;
				else if (evStr == "Intercepting enemy aircraft.")   eventRec[x].eventdat = 3;
				else if (evStr == "Cleared enemy platforms.")       eventRec[x].eventdat = 4;
				else if (evStr == "Approaching enemy platforms...") eventRec[x].eventdat = 5;
				else if (evStr == "WARNING: Spikes ahead!")         eventRec[x].eventdat = 6;
				else if (evStr == "Afterburners activated!")        eventRec[x].eventdat = 7;
				else if (evStr == "** Danger! **")                  eventRec[x].eventdat = 8;
				else if (evStr == "Bonus Level")                    eventRec[x].eventdat = 9;
			}
			break;

		case  20: // EnemyGlobal_Accel
			eventRec[x].eventdat  = j.value<Sint16>("excc", 0);
			eventRec[x].eventdat2 = j.value<Sint16>("eycc", 0);
			eventRec[x].eventdat3 = j.value<Sint8> ("unknown", 0);
			eventRec[x].eventdat5 = j.value<Sint8> ("cycle", 0);
			eventRec[x].eventdat6 = j.value<Sint8> ("ani", 0);
			break;

		case  33: // EnemyFromOtherEnemy
			eventRec[x].eventdat  = j.value<Sint16>("type", 0);
			break;

		case 200: // APItemFromEnemy
			eventRec[x].eventdat  = j.value<Sint16>("ap_id", 0);
			break;

		default:
			break;
	}
}

void Patcher_ReadyPatch(Uint32 crc, Uint8 levelNum)
{
	loadedPatch = nullptr;
	patchEvents.clear();

	std::string crcStr = crc32str(crc);
	std::string levelStr = std::to_string(levelNum);

	if (!patchData["levelFiles"].contains(crcStr))
		return; // Episode not recognized

	auto episodePatchList = patchData["levelFiles"][crcStr];
	if (!episodePatchList.contains(levelStr))
		return; // No patch for this level

	std::string patchName = episodePatchList[levelStr].template get<std::string>();
	if (!patchData["patches"].contains(patchName))
	{
		std::cout << "warning: This level wants patch '" << patchName << "' but that patch doesn't exist" << std::endl;
		return;
	}

	std::cout << "Loading patch " << patchName << std::endl;
	loadedPatch = patchData["patches"][patchName];
	eventCount = 0;

	for (auto &elem : loadedPatch.items())
	{
		std::string action = elem.key();
		std::string::size_type restActionPoint;
		int line_num = std::stoi(action, &restActionPoint);
		std::string restAction = action.substr(restActionPoint);

		if (restAction == ":REPLACE" || restAction == ":PREPEND")
			patchEvents.push_back(line_num);
		else if (restAction == ":APPEND")
			patchEvents.push_back(line_num+1);
		else
			std::cout << "warning: Unrecognized action '" << action << "' in patch" << std::endl;
	}

	patchEvents.sort();
	patchEvents.unique();
	for(std::list<int>::iterator iter = patchEvents.begin(); iter != patchEvents.end(); iter++){
	   std::cout<<*iter<<std::endl;
	}
}

Uint16 Patcher_DoPatch(Uint16 *idx)
{
	if (loadedPatch == nullptr || patchEvents.empty())
		return 0;

	++eventCount; 
	if (eventCount < patchEvents.front())
		return 0;
	if (eventCount > patchEvents.front())
	{
		std::cout << "error: Patch events within an earlier replacement (noticed at " << eventCount << ")" << std::endl;
		throw new std::logic_error("Patch events within earlier replacement");
	}
	patchEvents.pop_front();

	// ------------------------------------------------------------------------
	// Handle insertion
	std::string actionStr = std::to_string(eventCount) + ":PREPEND";
	if (!loadedPatch.contains(actionStr))
		actionStr = std::to_string(eventCount-1) + ":APPEND";
	if (loadedPatch.contains(actionStr))
	{
		// Don't increment event count, we want it to stay as the original event number
		// for making creating patches easier
		std::cout << "Insert event at " << eventCount << std::endl;
		if (loadedPatch[actionStr].is_array())
		{
			for (auto &elem : loadedPatch[actionStr].items())
				jsonEventToGameEvent(elem.value(), (*idx)++);
			maxEvent += loadedPatch[actionStr].size();
		}
		else if (loadedPatch[actionStr].is_object())
		{
			jsonEventToGameEvent(loadedPatch[actionStr], (*idx)++);
			++maxEvent;
		}
	}

	// ------------------------------------------------------------------------
	// Handle replacement
	actionStr = std::to_string(eventCount) + ":REPLACE";
	if (loadedPatch.contains(actionStr))
	{
		// We need to increment event count if we're skipping more than one event,
		// to correspond to the number of events we're skipping from the file
		std::cout << "Replace event at " << eventCount << std::endl;
		if (loadedPatch[actionStr].is_array())
		{
			for (auto &elem : loadedPatch[actionStr].items())
				jsonEventToGameEvent(elem.value(), (*idx)++);
			eventCount += loadedPatch[actionStr].size() - 1;
			return loadedPatch[actionStr].size();
		}
		else if (loadedPatch[actionStr].is_object())
		{
			jsonEventToGameEvent(loadedPatch[actionStr], (*idx)++);
			return 1;
		}
	}
	return 0;
}
