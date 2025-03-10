
#include <cstdarg>
#include <iostream>
#include <sstream>
#include <list>

#define JSON_USE_IMPLICIT_CONVERSIONS 0
#include <nlohmann/json.hpp>

extern "C" {
	// Function definitions that go out C side need to not be mangled by C++
	#include "patcher.h"

	// things we need from C side
	extern struct {
		Uint16 eventtime;
		Uint8  eventtype;
		Sint16 eventdat, eventdat2;
		Sint8  eventdat3, eventdat5, eventdat6;
		Uint8  eventdat4;
	} eventRec[2500];
	extern Uint16 maxEvent;

	extern const char soundTitle[128][9]; // Sound effect event
}

using nlohmann::json;

std::string episodeToString(Uint8 episode)
{
	std::stringstream s;
	s << "Episode " << std::to_string(episode);
	return s.str();
}

std::string patchVersionToString(const patcher_version_t *version)
{
	std::stringstream s;
	if (!version)
		s << "(missing)";
	else
		s << version->major << "." << version->minor << "." << version->revision;
	return s.str();
}

// Returns -1 if patches are out of date, 1 if patches are newer, 0 if equal
int patchVersionCompare(const patcher_version_t *ours, const patcher_version_t *theirs)
{
	if (!ours || !theirs)
		return -1;

	if (theirs->major > ours->major)       return 1;
	if (theirs->major < ours->major)       return -1;
	if (theirs->minor > ours->minor)       return 1;
	if (theirs->minor < ours->minor)       return -1;
	if (theirs->revision > ours->revision) return 1;
	if (theirs->revision < ours->revision) return -1;
	return 0;
}
// ----------------------------------------------------------------------------

static const patcher_version_t requiredVersion = {0, 80, 2};
static json patchData;

static std::string errorString;

const char *Patcher_SystemInit(FILE *file)
{
	try
	{
		patchData = json::parse(file, nullptr, true, true);

		if (patchData.contains("_version"))
		{
			patcher_version_t givenVersion = {
				{patchData["_version"].at(0).template get<int>()},
				{patchData["_version"].at(1).template get<int>()},
				{patchData["_version"].at(2).template get<int>()}
			};

			int comparison = patchVersionCompare(&requiredVersion, &givenVersion);
			if (comparison > 0)
			{
				std::cout << "Patch file is for a newer version ("
					<< patchVersionToString(&givenVersion)
					<< "); continuing anyway." << std::endl;
			}
			else if (comparison < 0)
			{
				errorString = "Patch file is too old (need "
					+ patchVersionToString(&requiredVersion)
					+ ", got "
					+ patchVersionToString(&givenVersion)
					+ ")";
				return errorString.c_str();
			}
		}
		else
		{
			errorString = "Patch file is too old (missing version)";
			return errorString.c_str();
		}

		if (!patchData.contains("patchList") || !patchData.contains("patches"))
		{
			errorString = "Patch file is missing required data.";
			return errorString.c_str();
		}
	}
	catch (const json::parse_error& e)
	{
		errorString = e.what();
		return errorString.c_str();
	}

	return NULL;
}

// ----------------------------------------------------------------------------

// JSON blob of the currently loaded patch, nullptr if there isn't one.
static json loadedPatch = nullptr;

// List of all event numbers that the patch modifies.
// Used to speed up patch processing.
static std::list<int> patchEvents;

// 
static int eventCount = 0;

static void assignFromArrayDat2(json &j, Uint16 x)
{
	if (!j.is_array())
		return;
	try
	{
		eventRec[x].eventdat2 = j.at(0).template get<Sint16>();
		eventRec[x].eventdat3 = j.at(1).template get<Sint8>();
		eventRec[x].eventdat4 = j.at(2).template get<Uint8>();
		eventRec[x].eventdat5 = j.at(3).template get<Sint8>();
		eventRec[x].eventdat6 = j.at(4).template get<Sint8>();
	}
	catch (json::out_of_range&) {} // expected, do nothing more
}

static void assignFromArray(json &j, Uint16 x)
{
	if (!j.is_array())
		return;
	try
	{
		eventRec[x].eventdat  = j.at(0).template get<Sint16>();
		eventRec[x].eventdat2 = j.at(1).template get<Sint16>();
		eventRec[x].eventdat3 = j.at(2).template get<Sint8>();
		eventRec[x].eventdat4 = j.at(3).template get<Uint8>();
		eventRec[x].eventdat5 = j.at(4).template get<Sint8>();
		eventRec[x].eventdat6 = j.at(5).template get<Sint8>();
	}
	catch (json::out_of_range&) {} // expected, do nothing more
}

static void jsonEventToGameEvent(json &j, Uint16 x)
{
	eventRec[x].eventtime = j.at("time").template get<Uint16>();
	eventRec[x].eventtype = j.at("event").template get<Uint8>();
	eventRec[x].eventdat  = 0; // Sint16
	eventRec[x].eventdat2 = 0; // Sint16
	eventRec[x].eventdat3 = 0; // Sint8
	eventRec[x].eventdat4 = 0; // Uint8
	eventRec[x].eventdat5 = 0; // Sint8
	eventRec[x].eventdat6 = 0; // Sint8

	// By default, since so many events use it
	if (eventRec[x].eventtype != 39 && eventRec[x].eventtype != 79)
		eventRec[x].eventdat4 = j.value<Uint8>("linknum", 0);

	switch (eventRec[x].eventtype)
	{
		case   1: // StarfieldSpeed
			eventRec[x].eventdat = j.value<Sint16>("speed", 0);
			break;

		case   2: // MapBackMove
		case   4: // MapStop
		case  83: // T2K_MapStop
			if (j.contains("mode") && j["mode"].is_array())
				assignFromArray(j["mode"], x);
			break;

		case   5: // LoadEnemyShapeBanks
			if (j.contains("banks") && j["banks"].is_array())
				assignFromArray(j["banks"], x);
			break;

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

		case  11: // EndLevel
			eventRec[x].eventdat = j.value<bool>("force", false) ? 1 : 0;
			break;

		case  12: // NewEnemy_Ground4X4Custom
			eventRec[x].eventdat6 = j.value<Sint8>("bg", 0);
			goto groundcustom_rejoin;

		case  16: // VoicedCallout
		conditionalcallout_rejoin:
			if (!j.contains("output"))
				break;
			if (j["output"].is_number())
				eventRec[x].eventdat = j.value<Sint16>("output", 0);
			else if (j["output"].is_string())
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

		case  19: // EnemyGlobal_Move
			if (j.contains("section"))
			{
				if (j["section"].is_number())
					eventRec[x].eventdat3 = j.value<Sint8>("section", 0);
				else if (j["section"].is_string())
				{
					std::string evStr = j["section"].template get<std::string>();
					if      (evStr == "ground") eventRec[x].eventdat3 = 1;
					else if (evStr == "sky")    eventRec[x].eventdat3 = 2;
					else if (evStr == "sky2")   eventRec[x].eventdat3 = 3;
					else if (evStr == "all")    eventRec[x].eventdat3 = 99;
				}
			}
			eventRec[x].eventdat  = j.value<Sint16>("exc", 0);
			eventRec[x].eventdat2 = j.value<Sint16>("eyc", 0);
			eventRec[x].eventdat5 = j.value<Sint8> ("cycle", 0);
			eventRec[x].eventdat6 = j.value<Sint8> ("fixedmovey", 0);
			break;

		case  20: // EnemyGlobal_Accel
			eventRec[x].eventdat  = j.value<Sint16>("excc", 0);
			eventRec[x].eventdat2 = j.value<Sint16>("eycc", 0);
			eventRec[x].eventdat3 = j.value<Sint8> ("unknown", 0);
			eventRec[x].eventdat5 = j.value<Sint8> ("cycle", 0);
			eventRec[x].eventdat6 = j.value<Sint8> ("ani", 0);
			break;

		case  24: // EnemyGlobal_Animate
			eventRec[x].eventdat  = j.value<Sint16>("ani", 0);
			eventRec[x].eventdat2 = j.value<Sint16>("cycle", 0);
			eventRec[x].eventdat3 = j.value<Sint8> ("mode", 0);
			break;

		case  25: // EnemyGlobal_SetHealth
			eventRec[x].eventdat  = j.value<Sint16>("health", 0);
			eventRec[x].eventdat6 = j.value<bool>("difficulty_scaled", false) ? 99 : 0;
			break;

		case  26: // SmallEnemyAdjust
		case  99: // RandomExplosions
			eventRec[x].eventdat = j.value<bool>("enable", false) ? 1 : 0;
			break;

		case  27: // EnemyGlobal_AccelRev
			eventRec[x].eventdat  = j.value<Sint16>("exrev", 0);
			eventRec[x].eventdat2 = j.value<Sint16>("eyrev", 0);
			eventRec[x].eventdat3 = j.value<Sint8> ("filter", 0);
			break;

		case  31: // EnemyGlobal_FireOverride
			if (j.contains("shot_freq") && j["shot_freq"].is_array())
				assignFromArray(j["shot_freq"], x);
			eventRec[x].eventdat5 = j.value<Sint8> ("launch_freq", 0);
			break;

		case  33: // EnemyFromOtherEnemy
		case  45: // Arcade_EnemyFromOtherEnemy
		case  85: // T2K_TimeBattle_EnemyFromOtherEnemy
			eventRec[x].eventdat = j.value<Sint16>("type", 0);
			break;

		case  35: // MusicPlay
			eventRec[x].eventdat = j.value<Sint16>("song", 0);
			break;

		case  37: // RandomEnemyFrequency
			eventRec[x].eventdat = 100 - j.value<Sint16>("percentage", 0);
			break;

		case  39: // EnemyGlobal_LinkNum
			eventRec[x].eventdat  = j.value<Sint16>("linknum", 0);
			eventRec[x].eventdat2 = j.value<Sint16>("new_linknum", 0);
			break;

		case  41: // RemoveEnemies
			eventRec[x].eventdat = j.value<bool>("all", true) ? 0 : 1;
			break;

		case  38: // Jump_NoReturn
		case  54: // Jump
		case  57: // Jump_OnLinkNum254Kill
		commonjump_rejoin:
			eventRec[x].eventdat = j.value<Sint16>("jump_time", 0);
			break;

		case  53: // ForceEvents
			eventRec[x].eventdat = j.value<bool>("enable", false) ? 0 : 99;
			break;

		case  58: // EnemyGlobal_SetLaunchType (Tyrian 2000)
			eventRec[x].eventdat = j.value<Sint16>("launch_type", 0);
			break;

		case  59: // EnemyGlobal_ReplaceEnemy (Tyrian 2000)
		case  68: // EnemyGlobal_ReplaceEnemy (Tyrian 2000)
			eventRec[x].eventdat = j.value<Sint16>("new_type", 0);
			break;

		case  61: // Skip_IfFlagEquals
		case 100: // Skip_IfFlagNotEquals (APTyrian)
		case 101: // Skip_IfFlagLessThan (APTyrian)
		case 102: // Skip_IfFlagGreaterThan (APTyrian)
			eventRec[x].eventdat3 = j.value<Sint8>("skip_events", 0);
			// fall through
		case  60: // EnemyGlobal_SetFlag
		case 110: // EnemyGlobal_IncrementFlag (APTyrian)
		case 111: // LastEnemy_IncrementFlag (APTyrian)
			eventRec[x].eventdat  = j.value<Sint16>("flag", 0);
			eventRec[x].eventdat2 = j.value<Sint16>("value", 1);
			break;

		case  62: // PlaySound
			if (!j.contains("sample"))
				break;
			if (j["sample"].is_number())
				eventRec[x].eventdat = j.value<Sint16>("sample", 0);
			else if (j["sample"].is_string())
			{
				std::string evStr = j["sample"].template get<std::string>();
				for (int i = 0; soundTitle[i][0]; ++i)
				{
					if (evStr == soundTitle[i])
					{
						eventRec[x].eventdat = i + 1;
						break;
					}
				}
			}
			break;

		case  67: // SetLevelTimer
			eventRec[x].eventdat  = j.value<bool>("enable", false) ? 1 : 0;
			eventRec[x].eventdat2 = j.value<Sint16>("jump_time", 0);
			eventRec[x].eventdat3 = j.value<Sint8> ("value", 0);
			break;

		case  70: // Jump_SearchFor
			if (j.contains("types") && j["types"].is_array())
				assignFromArrayDat2(j["types"], x);
			goto commonjump_rejoin;

		case  71: // Jump_IfMapPositionUnder
			eventRec[x].eventdat2 = j.value<Sint16>("position", 0);
			goto commonjump_rejoin;

		case  79: // SetBossBar
			if (!j.contains("linknum"))
				break;
			if (j["linknum"].is_number())
				eventRec[x].eventdat = j.value<Sint16>("linknum", 0);
			else if (j["linknum"].is_array())
				assignFromArray(j["linknum"], x);
			break;

		case 120: // EnemyGlobal_SetShotType (APTyrian)
			if (j.contains("shot_type") && j["shot_type"].is_array())
				assignFromArray(j["shot_type"], x);
			break;

		case 200: // AP_CheckFreestanding
			eventRec[x].eventdat2 = j.value<Sint16>("ex", 0);
			eventRec[x].eventdat5 = j.value<Sint8> ("ey", 0);
			eventRec[x].eventdat6 = (Sint8)(j.value<int>("backup", 450) - 450);
			goto apfreestanding_rejoin;

		case 201: // AP_CheckFromEnemy
		aplast_rejoin:
			eventRec[x].eventdat2 = j.value<Sint16>("backup", 0);
		apfreestanding_rejoin:
			eventRec[x].eventdat  = j.value<Sint16>("ap_id", 0);

			if (j.contains("movement") && j["movement"].is_string())
			{
				std::string evStr = j["movement"].template get<std::string>();
				if      (evStr == "static")       eventRec[x].eventdat3 = 0;
				else if (evStr == "falling")      eventRec[x].eventdat3 = 1;
				else if (evStr == "swaying")      eventRec[x].eventdat3 = 2;
				else if (evStr == "sky_static")   eventRec[x].eventdat3 = 3;
				else if (evStr == "sky_falling")  eventRec[x].eventdat3 = 4;
				else if (evStr == "gnd2_static")  eventRec[x].eventdat3 = 5;
				else if (evStr == "gnd2_falling") eventRec[x].eventdat3 = 6;
				else if (evStr == "top_static")   eventRec[x].eventdat3 = 7;
				else if (evStr == "top_falling")  eventRec[x].eventdat3 = 8;
				else if (evStr == "top_swaying")  eventRec[x].eventdat3 = 9;
			}
			break;

		case 202: // AP_CheckFromLastNewEnemy
			eventRec[x].eventdat5 = j.value<Sint8>("align_x", 0);
			eventRec[x].eventdat6 = j.value<Sint8>("align_y", 0);
			goto aplast_rejoin;

		case 216: // ConditionalVoicedCallout
			if (j.contains("hard_contact"))
				eventRec[x].eventdat6 = (j["hard_contact"].template get<bool>() ? 2 : 1);
			goto conditionalcallout_rejoin;

		default:
			break;
	}
}

// ----------------------------------------------------------------------------

void Patcher_ReadyPatch(const char *gameID, Uint8 episode, Uint8 levelNum)
{
	loadedPatch = nullptr;
	patchEvents.clear();

	std::string episodeStr = episodeToString(episode);
	std::string levelStr = std::to_string(levelNum);

	if (!patchData["patchList"].contains(gameID) || !patchData["patchList"][gameID].contains(episodeStr))
		return; // Game (???) or episode not recognized

	auto episodePatchList = patchData["patchList"][gameID][episodeStr];
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
		exit(1);
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
