from typing import Optional, Dict, Union

from .Logic import RequirementList

class LevelRegion:
	completion_reqs: Union[str, RequirementList, None]
	locations: Dict[str, Optional[RequirementList]]
	base_id: int # Should match ID in lvlmast.c

	def __init__(self, base_id: int, locations: Dict[str, Optional[RequirementList]], \
		completion_reqs: Optional[RequirementList] = None):
		self.base_id = base_id
		self.locations = locations
		self.completion_reqs = completion_reqs

	def items(self):
		return self.locations.items()

	def keys(self):
		return self.locations.keys()

class LevelLocationData:

	# Note: completion_reqs="Boss" is a shorthand for "Whatever (levelname) - Boss requires"
	level_regions: Dict[str, LevelRegion] = {

		# =============================================================================================
		# EPISODE 1 - ESCAPE
		# =============================================================================================

		"TYRIAN (Episode 1)": LevelRegion(base_id=0, locations={
			# The hard variant of Tyrian is similarly designed, just with more enemies, so it shares checks.
			"TYRIAN (Episode 1) - First U-Ship Secret": None,
			"TYRIAN (Episode 1) - Second Spinner Formation": None,
			"TYRIAN (Episode 1) - Lander near BUBBLES Rock": None,
			"TYRIAN (Episode 1) - BUBBLES Rock": None,
			"TYRIAN (Episode 1) - HOLES Orb": RequirementList(obscure=True),
			"TYRIAN (Episode 1) - First Line of Tanks": None,
			"TYRIAN (Episode 1) - SOH JIN Orb": None,
			"TYRIAN (Episode 1) - Boss": None,
		}, completion_reqs=None),

		"BUBBLES (Episode 1)": LevelRegion(base_id=10, locations={
			"BUBBLES (Episode 1) - Orbiting Bubbles 1": None,
			"BUBBLES (Episode 1) - Orbiting Bubbles 2": RequirementList(['SideHighDPS'], ['Power5']),
			"BUBBLES (Episode 1) - Orbiting Bubbles 3" : RequirementList(['SideHighDPS'], ['Power5']),
			"BUBBLES (Episode 1) - Coin Rain, First Bubble Line": RequirementList(['SideHighDPS'], ['Power5']),
			"BUBBLES (Episode 1) - Coin Rain, Fourth Bubble Line": RequirementList(['SideHighDPS'], ['Power5']),
			"BUBBLES (Episode 1) - Coin Rain, Sixth Bubble Line": RequirementList(['SideHighDPS'], ['Power5']),
		}, completion_reqs=None),

		"HOLES (Episode 1)": LevelRegion(base_id=20, locations={
			"HOLES (Episode 1) - First U-Ship Formation": RequirementList(['Power3']),
			"HOLES (Episode 1) - Fourth U-Ship Formation": RequirementList(['Power3']),
			"HOLES (Episode 1) - Lander After Spinners": RequirementList(['Power3', 'SideDefensive']),
			"HOLES (Episode 1) - U-Ships after Wandering Boss": RequirementList(['Power3', 'SideDefensive']),
		}, completion_reqs=RequirementList(['Power3', 'SideDefensive']) ),

		"SOH JIN (Episode 1)": LevelRegion(base_id=30, locations={
			"SOH JIN (Episode 1) - Starting Alcove": RequirementList(['Power3'], ['SideHighDPS']), # To get out
			"SOH JIN (Episode 1) - Guarded Orb Launcher": RequirementList(['Power3'], ['SideHighDPS']), # To get in and out
			"SOH JIN (Episode 1) - Triple Diagonal Launchers": None,
			"SOH JIN (Episode 1) - Turret Checkerboard": None,
			"SOH JIN (Episode 1) - Alcove Past Orb Launchers": None,
			"SOH JIN (Episode 1) - Double Orb Launcher Line": None,
			"SOH JIN (Episode 1) - Next to Double Point Items": None,
		}, completion_reqs=None),

		"ASTEROID1 (Episode 1)": LevelRegion(base_id=40, locations={
			"ASTEROID1 (Episode 1) - Railgunner 1": None,
			"ASTEROID1 (Episode 1) - Railgunner 2": None,
			"ASTEROID1 (Episode 1) - Railgunner 3": None,
			"ASTEROID1 (Episode 1) - ASTEROID? Face Rock": None,
			"ASTEROID1 (Episode 1) - Boss": None,
		}, completion_reqs="Boss"),

		"ASTEROID2 (Episode 1)": LevelRegion(base_id=50, locations={
			"ASTEROID2 (Episode 1) - Tank Turn-around Secret 1": None, # Not obscure, game hints tell you
			"ASTEROID2 (Episode 1) - First Tank Squadron": None,
			"ASTEROID2 (Episode 1) - Tank Turn-around Secret 2": RequirementList(obscure=True),
			"ASTEROID2 (Episode 1) - Second Tank Squadron": None,
			"ASTEROID2 (Episode 1) - Tank Bridge": None,
			"ASTEROID2 (Episode 1) - Tank Assault Right Tank Secret": RequirementList(obscure=True),
			"ASTEROID2 (Episode 1) - MINEMAZE Face Rock": None,
			"ASTEROID2 (Episode 1) - Boss": None, # Needs conditions
		}, completion_reqs="Boss"),

		"MINEMAZE (Episode 1)": LevelRegion(base_id=60, locations={
			"MINEMAZE (Episode 1) - Lone Orb": None,
			"MINEMAZE (Episode 1) - Right Path Gate": None,
			"MINEMAZE (Episode 1) - ASTEROID? Orb": RequirementList(['Power3']),
			"MINEMAZE (Episode 1) - Ships Behind Central Gate": RequirementList(['Power3'])
		}, completion_reqs=RequirementList(['Power3']) ),

		"ASTEROID? (Episode 1)": LevelRegion(base_id=70, locations={
			"ASTEROID? (Episode 1) - Welcoming Launchers 1": RequirementList(['Power4', 'Generator2']),
			"ASTEROID? (Episode 1) - Welcoming Launchers 2": RequirementList(['Power4', 'Generator2']),
			"ASTEROID? (Episode 1) - Boss Launcher": RequirementList(['Power4', 'Generator2']),
			"ASTEROID? (Episode 1) - WINDY Orb": RequirementList(['Power4', 'Generator2'], obscure=True),

			# These two checks require you to:
			# - Destroy the correct enemies to spawn the WINDY orb, and
			# - Destroy the specific ships after the WINDY orb within two seconds of them spawning
			"ASTEROID? (Episode 1) - Secret Ship Quick Shot 1": RequirementList(
				['FrontHighDPS', 'Power7', 'Generator3'], obscure=True),
			"ASTEROID? (Episode 1) - Secret Ship Quick Shot 2": RequirementList(
				['FrontHighDPS', 'Power7', 'Generator3'], obscure=True)
		}, completion_reqs=RequirementList(['Power4', 'Generator2']) ),

		"WINDY (Episode 1)": LevelRegion(base_id=80, locations={
			# No checks. Money grind level.
		}, completion_reqs=RequirementList(['Power5', 'Armor3']) ),

		"SAVARA (Episode 1)": LevelRegion(base_id=90, locations={
			# This is the variant of Savara on Easy or Medium.
			"SAVARA (Episode 1) - Second Oncoming Formation": None, # Needs conditions
			"SAVARA (Episode 1) - Green Plane Line": None, # Needs conditions
			"SAVARA (Episode 1) - Brown Plane Breaking Formation": None, # Needs conditions
			"SAVARA (Episode 1) - Speeding Large Plane": None, # Needs conditions
			"SAVARA (Episode 1) - Vulcan Planes": None, # Needs conditions
		}, completion_reqs=None),

		"SAVARA II (Episode 1)": LevelRegion(base_id=100, locations={
			# This is the variant of Savara on Hard or above.
			"SAVARA II (Episode 1) - First Set of Launched Planes": None, # Needs conditions
			"SAVARA II (Episode 1) - First Green Plane Sequence": None, # Needs conditions
			"SAVARA II (Episode 1) - Vulcan Planes Near Blimp": None, # Needs conditions
			"SAVARA II (Episode 1) - Last Set of Launched Planes": None, # Needs conditions
			"SAVARA II (Episode 1) - Second Green Plane Sequence": None, # Needs conditions
		}, completion_reqs=None),

		"BONUS (Episode 1)": LevelRegion(base_id=110, locations={
			# No checks.
		}, completion_reqs=None),

		"MINES (Episode 1)": LevelRegion(base_id=120, locations={
			"MINES (Episode 1) - Blue Mine": None
		}, completion_reqs=None),

		"DELIANI (Episode 1)": LevelRegion(base_id=130, locations={
			"DELIANI (Episode 1) - First Turret Line": None, # Needs conditions
			"DELIANI (Episode 1) - Second Turret Line": None, # Needs conditions
			"DELIANI (Episode 1) - Third Turret Line": None, # Needs conditions
			"DELIANI (Episode 1) - Fourth Turret Line": None, # Needs conditions
			"DELIANI (Episode 1) - Ambush": None, # Needs conditions
			"DELIANI (Episode 1) - Last Cross Formation": None, # Needs conditions
			"DELIANI (Episode 1) - Boss": None, # Needs conditions
		}, completion_reqs="Boss"),

		"SAVARA V (Episode 1)": LevelRegion(base_id=140, locations={
			"SAVARA V (Episode 1) - Mid-Boss": RequirementList(
				["Power7", "Generator3"],
				["FrontHighDPS", "Power5", "Generator3"]),
			"SAVARA V (Episode 1) - Boss": RequirementList(
				["Power7", "Generator3"],
				["FrontHighDPS", "Power5", "Generator3"],
				["SideHighDPS", "Power5", "Generator2"])
		}, completion_reqs="Boss"),

		"ASSASSIN (Episode 1)": LevelRegion(base_id=150, locations={
			"ASSASSIN (Episode 1) - Boss": RequirementList(["Power8"], ["FrontHighDPS", "Power5"],
				require_all=["Generator3"]),
		}, completion_reqs="Boss"),
		# Event: "Episode 1 (Escape) Complete"

		# =============================================================================================
		# EPISODE 2 - TREACHERY
		# =============================================================================================

		"TORM (Episode 2)": LevelRegion(base_id=160, locations={
			"TORM (Episode 2) - Ship Fleeing Dragon Secret": None,
			"TORM (Episode 2) - Boss": None,
		}, completion_reqs=RequirementList(['Armor4']) ),

		"GYGES (Episode 2)": LevelRegion(base_id=170, locations={
			"GYGES (Episode 2) - Boss": None,
		}, completion_reqs="Boss"),

		"BONUS 1 (Episode 2)": LevelRegion(base_id=180, locations={
			# No checks, just completion.
		}, completion_reqs=None),

		"ASTCITY (Episode 2)": LevelRegion(base_id=190, locations={
		}, completion_reqs=None),

		"BONUS 2 (Episode 2)": LevelRegion(base_id=200, locations={
			# No checks, just completion.
		}, completion_reqs=None),

		"GEM WAR (Episode 2)": LevelRegion(base_id=210, locations={
		}, completion_reqs=None),

		"MARKERS (Episode 2)": LevelRegion(base_id=220, locations={
		}, completion_reqs=None),

		"MISTAKES (Episode 2)": LevelRegion(base_id=230, locations={
		}, completion_reqs=None),

		"SOH JIN (Episode 2)": LevelRegion(base_id=240, locations={
		}, completion_reqs=None),

		"BOTANY A (Episode 2)": LevelRegion(base_id=250, locations={
			"BOTANY A (Episode 2) - Boss": None,
		}, completion_reqs="Boss"),

		"BOTANY B (Episode 2)": LevelRegion(base_id=260, locations={
			"BOTANY B (Episode 2) - Starting Platform Sensor": RequirementList(
				['Armor7'], ['SideDefensive'], require_all=['Power5', 'Generator2']),
			"BOTANY B (Episode 2) - Main Platform First Sensor": RequirementList(
				['Armor7'], ['SideDefensive'], require_all=['Power5', 'Generator2']),
			"BOTANY B (Episode 2) - Main Platform Second Sensor": RequirementList(
				['Armor7'], ['SideDefensive'], require_all=['Power5', 'Generator2']),
			"BOTANY B (Episode 2) - Main Platform Last Sensor": RequirementList(
				['Armor7'], ['SideDefensive'], require_all=['Power5', 'Generator2']),
			"BOTANY B (Episode 2) - Super-Turret on Bridge": RequirementList(
				['Armor7'], ['SideDefensive'], require_all=['Power5', 'Generator2']),
			"BOTANY B (Episode 2) - Boss": None,
		}, completion_reqs="Boss"),

		"GRYPHON (Episode 2)": LevelRegion(base_id=270, locations={
			"GRYPHON (Episode 2) - Boss": None,
		}, completion_reqs="Boss"),
		# Event: "Episode 2 (Treachery) Complete"

		# =============================================================================================
		# EPISODE 3 - MISSION: SUICIDE
		# =============================================================================================

		"GAUNTLET (Episode 3)": LevelRegion(base_id=280, locations={
		}, completion_reqs=None),

		"IXMUCANE (Episode 3)": LevelRegion(base_id=290, locations={
			"IXMUCANE (Episode 3) - Boss": None, # Needs conditions
		}, completion_reqs=None),

		"BONUS (Episode 3)": LevelRegion(base_id=300, locations={
			"BONUS (Episode 3) - First Lone Turret": RequirementList(['Power5']),
			"BONUS (Episode 3) - Behind First Onslaught": RequirementList(['Power9', 'Armor7', 'Generator3']),
			"BONUS (Episode 3) - Behind Second Onslaught": RequirementList(['Power9', 'Armor7', 'Generator3']),
			"BONUS (Episode 3) - Second Lone Turret": RequirementList(['Power9', 'Armor7', 'Generator3']),
			"BONUS (Episode 3) - Sonic Wave Hell Turret": RequirementList(
				['Repulsor', 'Power9', 'Armor7', 'Generator3'], # The only way this is remotely feasible
				['Armor14', 'Power11', 'Generator5'], # Basically require everything, absurdly difficult
			obscure=True), # Excessively difficult
		}, completion_reqs=RequirementList(
			['Repulsor', 'Power9', 'Armor7', 'Generator3'], # The only way this is remotely feasible
			['Armor14', 'Power11', 'Generator5'], obscure=True, # Excessively difficult
		) ),

		"STARGATE (Episode 3)": LevelRegion(base_id=310, locations={
			"STARGATE (Episode 3) - First Bubbleway": RequirementList(['Power3']),
			"STARGATE (Episode 3) - First Bubble Spawner": RequirementList(['Power3']),
			"STARGATE (Episode 3) - AST. CITY Orb 1": RequirementList(['Power3']),
			"STARGATE (Episode 3) - AST. CITY Orb 2": RequirementList(['Power3']),
			"STARGATE (Episode 3) - SAWBLADES Orb 1": RequirementList(['Power3']),
			"STARGATE (Episode 3) - SAWBLADES Orb 2": RequirementList(['Power3']),
			# There is a "boss" here, but it's really just a whole mass of bubbles
		}, completion_reqs=RequirementList(['Power3']) ),

		"AST. CITY (Episode 3)": LevelRegion(base_id=320, locations={
		}, completion_reqs=None),

		"SAWBLADES (Episode 3)": LevelRegion(base_id=330, locations={
		}, completion_reqs=None),

		"CAMANIS (Episode 3)": LevelRegion(base_id=340, locations={
			"CAMANIS (Episode 3) - Boss": None, # Needs conditions
		}, completion_reqs="Boss"),

		"MACES (Episode 3)": LevelRegion(base_id=350, locations={
			# Nothing in this level is destructible. It's entirely a test of dodging.
			# Therefore nothing is actually required to complete it or get any check.
			"MACES (Episode 3) - Third Mace": None,
			"MACES (Episode 3) - Sixth Mace": None,
			"MACES (Episode 3) - Mace Reprieve 1": None,
			"MACES (Episode 3) - Mace Reprieve 2": None,
			"MACES (Episode 3) - Mace Reprieve 3": None,
		}, completion_reqs=None),

		"FLEET (Episode 3)": LevelRegion(base_id=360, locations={
			"FLEET (Episode 3) - Boss": None, # Needs conditions
		}, completion_reqs="Boss"),
		# Event: "Episode 3 (Mission: Suicide) Complete"

		"TYRIAN X (Episode 3)": LevelRegion(base_id=370, locations={
			"TYRIAN X (Episode 3) - First U-Ship Secret": None, # Needs conditions
			"TYRIAN X (Episode 3) - Second Secret, Same as the First": None, # Needs conditions
			"TYRIAN X (Episode 3) - Side-flying Ship Near Landers": None, # Needs conditions
			"TYRIAN X (Episode 3) - Platform Spinner Sequence": None, # Needs conditions
			"TYRIAN X (Episode 3) - Tank Near Purple Structure": None, # Needs conditions
			"TYRIAN X (Episode 3) - Boss": None, # Needs conditions
		}, completion_reqs=None),

		"SAVARA Y (Episode 3)": LevelRegion(base_id=380, locations={
			"SAVARA Y (Episode 3) - Boss": None,
		}, completion_reqs=None),

		"NEW DELI (Episode 3)": LevelRegion(base_id=390, locations={
			"NEW DELI (Episode 3) - First Turret Line": None, # Needs conditions
			"NEW DELI (Episode 3) - Second Turret Line": None, # Needs conditions
			"NEW DELI (Episode 3) - Third Turret Line": None, # Needs conditions
			"NEW DELI (Episode 3) - Fourth Turret Line": None, # Needs conditions
			"NEW DELI (Episode 3) - Fifth Turret Line": None, # Needs conditions
			"NEW DELI (Episode 3) - Sixth Turret Line": None, # Needs conditions
			"NEW DELI (Episode 3) - Boss": None, # Needs conditions
		}, completion_reqs="Boss"),

		# =============================================================================================
		# EPISODE 4 - AN END TO FATE
		# =============================================================================================

		"SURFACE (Episode 4)": LevelRegion(base_id=400, locations={
			"SURFACE (Episode 4) - WINDY Orb": RequirementList(obscure=True), # Needs conditions
			"SURFACE (Episode 4) - Triple V Formation": None, # Needs conditions
		}, completion_reqs=None),

		"WINDY (Episode 4)": LevelRegion(base_id=410, locations={
		}, completion_reqs=None),

		"LAVA RUN (Episode 4)": LevelRegion(base_id=420, locations={
			"LAVA RUN (Episode 4) - Second Laser Shooter": None, # Needs conditions
			"LAVA RUN (Episode 4) - Left Side Missile Launcher": None, # Needs conditions
			"LAVA RUN (Episode 4) - Boss": RequirementList(['Power4', 'Generator2', 'SideDefensive']),
		}, completion_reqs=RequirementList(['Power4', 'Generator2', 'SideDefensive']) ),

		"CORE (Episode 4)": LevelRegion(base_id=430, locations={
			"CORE (Episode 4) - Boss": None, # Needs conditions
		}, completion_reqs=None),

		"LAVA EXIT (Episode 4)": LevelRegion(base_id=440, locations={
			"LAVA EXIT (Episode 4) - DESERTRUN Orb": None, # Needs conditions
			"LAVA EXIT (Episode 4) - Lava Bubble Assault": None, # Needs conditions
		}, completion_reqs=None),

		"DESERTRUN (Episode 4)": LevelRegion(base_id=450, locations={
			"DESERTRUN (Episode 4) - Afterburner Smooth Flying": None, # Needs conditions
			"DESERTRUN (Episode 4) - Ending Slalom 1": None, # Needs conditions
			"DESERTRUN (Episode 4) - Ending Slalom 2": None, # Needs conditions
			"DESERTRUN (Episode 4) - Ending Slalom 3": None, # Needs conditions
			"DESERTRUN (Episode 4) - Ending Slalom 4": None, # Needs conditions
			"DESERTRUN (Episode 4) - Ending Slalom 5": None, # Needs conditions
		}, completion_reqs=None),

		"SIDE EXIT (Episode 4)": LevelRegion(base_id=460, locations={
		}, completion_reqs=None),

		"?TUNNEL? (Episode 4)": LevelRegion(base_id=470, locations={
			"?TUNNEL? (Episode 4) - Test of Courage": None, # Needs conditions
			"?TUNNEL? (Episode 4) - Boss": None, # Needs conditions
		}, completion_reqs=None),

		"ICE EXIT (Episode 4)": LevelRegion(base_id=480, locations={
			"ICE EXIT (Episode 4) - ICESECRET Orb": None, # Needs conditions
		}, completion_reqs=None),

		"ICESECRET (Episode 4)": LevelRegion(base_id=490, locations={
			"ICESECRET (Episode 4) - Large U-Ship Mini-Boss": None, # Needs conditions
			"ICESECRET (Episode 4) - MegaLaser Dual Drop": None, # Needs conditions
			"ICESECRET (Episode 4) - Boss": None, # Needs conditions
		}, completion_reqs=None),

		"HARVEST (Episode 4)": LevelRegion(base_id=500, locations={
			"HARVEST (Episode 4) - High Speed V Formation": None, # Needs conditions
			"HARVEST (Episode 4) - Shooter with Gravity Orbs": None, # Needs conditions
			"HARVEST (Episode 4) - Shooter with Clone Bosses": None, # Needs conditions
			"HARVEST (Episode 4) - Grounded Shooter 1": None, # Needs conditions
			"HARVEST (Episode 4) - Grounded Shooter 2": None, # Needs conditions
			"HARVEST (Episode 4) - Ending V Formation": None, # Needs conditions
			"HARVEST (Episode 4) - Boss": None, # Needs conditions
		}, completion_reqs=None),

		"UNDERDELI (Episode 4)": LevelRegion(base_id=510, locations={
			"UNDERDELI (Episode 4) - Boss's Red Eye": None, # Needs conditions
			"UNDERDELI (Episode 4) - Boss": None, # Needs conditions

			# You can technically time this boss out, but it's really, really slow
		}, completion_reqs=None),

		"APPROACH (Episode 4)": LevelRegion(base_id=520, locations={
		}, completion_reqs=None),

		"SAVARA IV (Episode 4)": LevelRegion(base_id=530, locations={
			"SAVARA IV (Episode 4) - First Drunk Plane": RequirementList(['Power5', 'Generator2']),
			"SAVARA IV (Episode 4) - Second Drunk Plane": RequirementList(['Power5', 'Generator2']),
			"SAVARA IV (Episode 4) - Boss": RequirementList(
				['RearSideways', 'Armor7'], # Can move up to the top and hit from the side
				['SpecialPierces'], # SDF Main Gun, etc gets past
				['FrontPierces'], # Piercing front weapon gets past
				['SideRightOnly'], # Right-only Sidekicks can be fired out
				['Armor9', 'Power8'], # If you survive long enough, enemy planes will start dropping SuperBombs
				require_all=['Power5', 'Generator2']),

			# Can time out, but beating the boss is more likely than surviving that long
		}, completion_reqs=None),

		"DREAD-NOT (Episode 4)": LevelRegion(base_id=540, locations={
			"DREAD-NOT (Episode 4) - Boss": RequirementList(
				['FrontHighDPS', 'Power6', 'Armor6', 'Generator3'],
				['Power8', 'Armor6', 'Generator3'],
			),
		}, completion_reqs="Boss"),

		"EYESPY (Episode 4)": LevelRegion(base_id=550, locations={
			"EYESPY (Episode 4) - Billiard Break Secret": RequirementList(obscure=True),
			"EYESPY (Episode 4) - Boss": None,
		}, completion_reqs="Boss"),

		"BRAINIAC (Episode 4)": LevelRegion(base_id=560, locations={
			"BRAINIAC (Episode 4) - Turret-Guarded Pathway": None, # Needs conditions
			"BRAINIAC (Episode 4) - Mid-Boss 1": None, # Needs conditions
			"BRAINIAC (Episode 4) - Mid-Boss 2": RequirementList(
				# Stays at the bottom of the screen
				['RearSideways', 'Power9', 'Armor9', 'Generator5']),
			"BRAINIAC (Episode 4) - Boss": RequirementList(
				['FrontHighDPS'],
				['FrontPierces'],
				['SpecialPierces'],
				['SideRightOnly'],
				require_all=['Power9', 'Armor9', 'Generator5']),
		}, completion_reqs="Boss"),

		"NOSE DRIP (Episode 4)": LevelRegion(base_id=570, locations={
			"NOSE DRIP (Episode 4) - Boss": RequirementList(
				# Without a good loadout, this boss is extremely difficult, so...
				['FrontHighDPS'],
				['SideHighDPS', 'SideDefensive'],
				require_all=['Power9', 'Armor11', 'Generator5']),
		}, completion_reqs="Boss"),
		# Event: "Episode 4 (An End To Fate) Complete"

		# =============================================================================================
		# EPISODE 5 - HAZUDRA FODDER
		# =============================================================================================

		"ASTEROIDS (Episode 5)": LevelRegion(base_id=580, locations={
			"ASTEROIDS (Episode 5) - Boss": None,
		}, completion_reqs=None),

		"AST ROCK (Episode 5)": LevelRegion(base_id=590, locations={
		}, completion_reqs=None),

		"MINERS (Episode 5)": LevelRegion(base_id=600, locations={
			"MINERS (Episode 5) - Boss": None,
		}, completion_reqs=None),

		"SAVARA (Episode 5)": LevelRegion(base_id=610, locations={
		}, completion_reqs=None),

		"CORAL (Episode 5)": LevelRegion(base_id=620, locations={
			"CORAL (Episode 5) - Boss": None,
		}, completion_reqs=None),

		"STATION (Episode 5)": LevelRegion(base_id=630, locations={
			"STATION (Episode 5) - Boss": None,
		}, completion_reqs=None),

		"FRUIT (Episode 5)": LevelRegion(base_id=640, locations={
			"FRUIT (Episode 5) - Boss": None,
		}, completion_reqs=None),
		# Event: "Episode 5 (Hazudra Fodder) Complete" - Isn't this episode short???
	}

	shop_regions: Dict[str, int] = {name: i for (name, i) in zip(level_regions.keys(), range(1000, 2000, 10))}

	# Events for game completion
	events: Dict[str, str] = {
		"Episode 1 (Escape) Complete":           "ASSASSIN (Episode 1) - Boss",
		"Episode 2 (Treachery) Complete":        "GRYPHON (Episode 2) - Boss",
		"Episode 3 (Mission: Suicide) Complete": "FLEET (Episode 3) - Boss",
		"Episode 4 (An End to Fate) Complete":   "NOSE DRIP (Episode 4) - Boss",
		"Episode 5 (Hazudra Fodder) Complete":   "FRUIT (Episode 5) - Boss",
	}

	@classmethod
	def get_location_name_to_id(cls, base_id: int):
		all_locs = {}
		for (level, region) in cls.level_regions.items():
			all_locs.update({name: (base_id + region.base_id + i) for (name, i) in zip(region.keys(), range(99))})

		for (level, start_id) in cls.shop_regions.items():
			all_locs.update({f"Shop ({level}) - Item {i + 1}": (base_id + start_id + i) for i in range(0, 5)})

		return all_locs
