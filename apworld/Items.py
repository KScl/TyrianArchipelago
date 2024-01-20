from typing import List, Dict
from enum import IntEnum

from BaseClasses import ItemClassification as IC

class Episode(IntEnum):
	Escape = 1
	Treachery = 2
	MissionSuicide = 3
	AnEndToFate = 4
	HazudraFodder = 5

class LocalItem:
	local_id: int
	count: int
	item_class: IC

	def __init__(self, local_id: int, count: int = 0, item_class: IC = IC.filler):
		self.local_id = local_id
		self.count = count
		self.item_class = item_class

class LocalLevel(LocalItem):
	episode: Episode

	def __init__(self, local_id: int, episode: Episode):
		self.local_id = local_id
		self.count = 1
		self.item_class = IC.progression
		self.episode = episode

class LocalWeapon(LocalItem):
	tags: List[str]

	def __init__(self, local_id: int, tags: List[str] = [], count: int = 1, item_class: IC = IC.filler):
		self.local_id = local_id
		self.count = count
		self.item_class = item_class
		self.tags = tags

# --------------------------------------------------------------------------------------------------------------------

class LocalItemData:
	levels: Dict[str, LocalLevel] = {
		"TYRIAN (Episode 1)":    LocalLevel( 1, Episode.Escape), # Starts unlocked
		"BUBBLES (Episode 1)":   LocalLevel( 2, Episode.Escape),
		"HOLES (Episode 1)":     LocalLevel( 3, Episode.Escape),
		"SOH JIN (Episode 1)":   LocalLevel( 4, Episode.Escape),
		"ASTEROID1 (Episode 1)": LocalLevel( 5, Episode.Escape),
		"ASTEROID2 (Episode 1)": LocalLevel( 6, Episode.Escape),
		"ASTEROID? (Episode 1)": LocalLevel( 7, Episode.Escape),
		"MINEMAZE (Episode 1)":  LocalLevel( 8, Episode.Escape),
		"WINDY (Episode 1)":     LocalLevel( 9, Episode.Escape),
		"SAVARA (Episode 1)":    LocalLevel(10, Episode.Escape),
		"SAVARA II (Episode 1)": LocalLevel(11, Episode.Escape), # Savara Hard
		"BONUS (Episode 1)":     LocalLevel(12, Episode.Escape),
		"MINES (Episode 1)":     LocalLevel(13, Episode.Escape),
		"DELIANI (Episode 1)":   LocalLevel(14, Episode.Escape),
		"SAVARA V (Episode 1)":  LocalLevel(15, Episode.Escape),
		"ASSASSIN (Episode 1)":  LocalLevel(16, Episode.Escape), # Goal

		"TORM (Episode 2)":      LocalLevel(17, Episode.Treachery),
		"GYGES (Episode 2)":     LocalLevel(18, Episode.Treachery),
		"BONUS 1 (Episode 2)":   LocalLevel(19, Episode.Treachery),
		"ASTCITY (Episode 2)":   LocalLevel(20, Episode.Treachery),
		"BONUS 2 (Episode 2)":   LocalLevel(21, Episode.Treachery),
		"GEM WAR (Episode 2)":   LocalLevel(22, Episode.Treachery),
		"MARKERS (Episode 2)":   LocalLevel(23, Episode.Treachery),
		"MISTAKES (Episode 2)":  LocalLevel(24, Episode.Treachery),
		"SOH JIN (Episode 2)":   LocalLevel(25, Episode.Treachery),
		"BOTANY A (Episode 2)":  LocalLevel(26, Episode.Treachery),
		"BOTANY B (Episode 2)":  LocalLevel(27, Episode.Treachery),
		"GRYPHON (Episode 2)":   LocalLevel(28, Episode.Treachery), # Goal

		"GAUNTLET (Episode 3)":  LocalLevel(29, Episode.MissionSuicide),
		"IXMUCANE (Episode 3)":  LocalLevel(30, Episode.MissionSuicide),
		"BONUS (Episode 3)":     LocalLevel(31, Episode.MissionSuicide),
		"STARGATE (Episode 3)":  LocalLevel(32, Episode.MissionSuicide),
		"AST. CITY (Episode 3)": LocalLevel(33, Episode.MissionSuicide),
		"SAWBLADES (Episode 3)": LocalLevel(34, Episode.MissionSuicide),
		"CAMANIS (Episode 3)":   LocalLevel(35, Episode.MissionSuicide),
		"MACES (Episode 3)":     LocalLevel(36, Episode.MissionSuicide),
		"FLEET (Episode 3)":     LocalLevel(37, Episode.MissionSuicide), # Goal
		"TYRIAN X (Episode 3)":  LocalLevel(38, Episode.MissionSuicide),
		"SAVARA Y (Episode 3)":  LocalLevel(39, Episode.MissionSuicide),
		"NEW DELI (Episode 3)":  LocalLevel(40, Episode.MissionSuicide),

		"SURFACE (Episode 4)":   LocalLevel(41, Episode.AnEndToFate),
		"WINDY (Episode 4)":     LocalLevel(42, Episode.AnEndToFate),
		"LAVA RUN (Episode 4)":  LocalLevel(43, Episode.AnEndToFate),
		"CORE (Episode 4)":      LocalLevel(44, Episode.AnEndToFate),
		"LAVA EXIT (Episode 4)": LocalLevel(45, Episode.AnEndToFate),
		"DESERTRUN (Episode 4)": LocalLevel(46, Episode.AnEndToFate),
		"SIDE EXIT (Episode 4)": LocalLevel(47, Episode.AnEndToFate),
		"?TUNNEL? (Episode 4)":  LocalLevel(48, Episode.AnEndToFate),
		"ICE EXIT (Episode 4)":  LocalLevel(49, Episode.AnEndToFate),
		"ICESECRET (Episode 4)": LocalLevel(50, Episode.AnEndToFate),
		"HARVEST (Episode 4)":   LocalLevel(51, Episode.AnEndToFate),
		"UNDERDELI (Episode 4)": LocalLevel(52, Episode.AnEndToFate),
		"APPROACH (Episode 4)":  LocalLevel(53, Episode.AnEndToFate),
		"SAVARA IV (Episode 4)": LocalLevel(54, Episode.AnEndToFate),
		"DREAD-NOT (Episode 4)": LocalLevel(55, Episode.AnEndToFate),
		"EYESPY (Episode 4)":    LocalLevel(56, Episode.AnEndToFate),
		"BRAINIAC (Episode 4)":  LocalLevel(57, Episode.AnEndToFate),
		"NOSE DRIP (Episode 4)": LocalLevel(58, Episode.AnEndToFate), # Goal

		"ASTEROIDS (Episode 5)": LocalLevel(59, Episode.HazudraFodder),
		"AST ROCK (Episode 5)":  LocalLevel(60, Episode.HazudraFodder),
		"MINERS (Episode 5)":    LocalLevel(61, Episode.HazudraFodder),
		"SAVARA (Episode 5)":    LocalLevel(62, Episode.HazudraFodder),
		"CORAL (Episode 5)":     LocalLevel(63, Episode.HazudraFodder),
		"STATION (Episode 5)":   LocalLevel(64, Episode.HazudraFodder),
		"FRUIT (Episode 5)":     LocalLevel(65, Episode.HazudraFodder), # Goal
	}

	# ----------------------------------------------------------------------------------------------------------------

	# Tags are as follows
	# 'Pierces': Goes through enemies to hit others, also hits enemies that can't normally be hit from the front
	# 'HighDPS': Self-explanatory (Concentrated fire harmful to all enemy life)
	# 'Sideways': Can reliably hit things horizontally adjacent to you (required for some bosses)
	# 'HasAmmo': Sidekicks with limited ammo (they replenish slowly, but still limits firing)
	# 'RightOnly': Distinguishes sidekicks that can only be on the right (so only one in pool)
	# 'Defensive': Sidekick that provides some level of all-around defense.
	front_ports: Dict[str, LocalWeapon] = {
		"Pulse-Cannon":           LocalWeapon(100), # Default start
		"Multi-Cannon":           LocalWeapon(101),
		"Mega Cannon":            LocalWeapon(102, tags=['Pierces']),
		"Laser":                  LocalWeapon(103, tags=['HighDPS'], item_class=IC.useful),
		"Zica Laser":             LocalWeapon(104, tags=['HighDPS'], item_class=IC.useful),
		"Protron Z":              LocalWeapon(105, tags=['HighDPS']),
		"Vulcan Cannon":          LocalWeapon(106),
		"Lightning Cannon":       LocalWeapon(107, tags=['HighDPS']),
		"Protron":                LocalWeapon(108),
		"Missile Launcher":       LocalWeapon(109),
		"Mega Pulse":             LocalWeapon(110, tags=['HighDPS']),
		"Heavy Missile Launcher": LocalWeapon(111),
		"Banana Blast":           LocalWeapon(112),
		"HotDog":                 LocalWeapon(113),
		"Hyper Pulse":            LocalWeapon(114),
		"Guided Bombs":           LocalWeapon(115),
		"Shuruiken Field":        LocalWeapon(116, tags=['HighDPS']),
		"Poison Bomb":            LocalWeapon(117, tags=['HighDPS'], item_class=IC.useful),
		"Protron Wave":           LocalWeapon(118),
		"The Orange Juicer":      LocalWeapon(119), # Requires suicidal flying for max DPS
		"NortShip Super Pulse":   LocalWeapon(120, tags=['HighDPS']),
		"Atomic RailGun":         LocalWeapon(121, tags=['HighDPS'], item_class=IC.useful),
		"Widget Beam":            LocalWeapon(122),
		"Sonic Impulse":          LocalWeapon(123, tags=['Pierces']),
		"RetroBall":              LocalWeapon(124),
		# ---------- TYRIAN 2000 LINE ----------
	}

	rear_ports: Dict[str, LocalWeapon] = {
		"Starburst":                     LocalWeapon(200, tags=['Sideways']),
		"Multi-Cannon (Rear)":           LocalWeapon(201, tags=['Sideways']),
		"Sonic Wave":                    LocalWeapon(202, tags=['Sideways']),
		"Protron (Rear)":                LocalWeapon(203, tags=['Sideways']),
		"Wild Ball":                     LocalWeapon(204),
		"Vulcan Cannon (Rear)":          LocalWeapon(205),
		"Fireball":                      LocalWeapon(206),
		"Heavy Missile Launcher (Rear)": LocalWeapon(207),
		"Mega Pulse (Rear)":             LocalWeapon(208, tags=['Sideways']),
		"Banana Blast (Rear)":           LocalWeapon(209),
		"HotDog (Rear)":                 LocalWeapon(210),
		"Guided Micro Bombs":            LocalWeapon(211),
		"Heavy Guided Bombs":            LocalWeapon(212),
		"Scatter Wave":                  LocalWeapon(213, tags=['Sideways']),
		# TODO Should these be front or rear?
		"NortShip Spreader":             LocalWeapon(214), # Sideways at some levels?
		"NortShip Spreader B":           LocalWeapon(215, tags=['Pierces']), # Pierces but awkward to use
		# ---------- TYRIAN 2000 LINE ----------
	}

	special_weapons: Dict[str, LocalWeapon] = {
		"Repulsor":          LocalWeapon(300, item_class=IC.progression), # Specifically required for some checks
		"Pearl Wind":        LocalWeapon(301),
		"Soul of Zinglon":   LocalWeapon(302), # Pierces, but doesn't normally do damage
		"Attractor":         LocalWeapon(303),
		"Ice Beam":          LocalWeapon(304),
		"Flare":             LocalWeapon(305, tags=['FullScreen']),
		"Blade Field":       LocalWeapon(306),
		"SandStorm":         LocalWeapon(307, tags=['FullScreen']),
		"MineField":         LocalWeapon(308, tags=['FullScreen']),
		"Dual Vulcan":       LocalWeapon(309),
		"Banana Bomb":       LocalWeapon(310, tags=['HighDPS']),
		"Protron Dispersal": LocalWeapon(311),
		"Astral Zone":       LocalWeapon(312, tags=['FullScreen']),
		"Xega Ball":         LocalWeapon(313),
		"MegaLaser Dual":    LocalWeapon(314, tags=['HighDPS']),
		"Orange Shield":     LocalWeapon(315),
		"Pulse Blast":       LocalWeapon(316),
		"MegaLaser":         LocalWeapon(317, tags=['Pierces']),
		"Missile Pod":       LocalWeapon(318),
		"Invulnerability":   LocalWeapon(319, tags=['Pierces', 'HighDPS'], item_class=IC.useful),
		"Lightning Zone":    LocalWeapon(320),
		"SDF Main Gun":      LocalWeapon(321, tags=['Pierces', 'HighDPS'], item_class=IC.useful),
		# ---------- TYRIAN 2000 LINE ----------
	}

	sidekicks: Dict[str, LocalWeapon] = {
		"Single Shot Option":         LocalWeapon(400, count=2),
		"Dual Shot Option":           LocalWeapon(401, count=2),
		"Charge Cannon":              LocalWeapon(402, count=2),
		"Vulcan Shot Option":         LocalWeapon(403, count=2),
		"Wobbley":                    LocalWeapon(404, count=2),
		"MegaMissile":                LocalWeapon(405, count=2, tags=['HasAmmo', 'HighDPS'], \
			item_class=IC.useful),
		"Atom Bombs":                 LocalWeapon(406, count=2, tags=['HasAmmo', 'HighDPS'], \
			item_class=IC.useful),
		"Phoenix Device":             LocalWeapon(407, count=2, tags=['HasAmmo', 'HighDPS', 'Defensive'], \
			item_class=IC.useful),
		"Plasma Storm":               LocalWeapon(408, count=1, tags=['HasAmmo', 'HighDPS'], \
			item_class=IC.useful), # Too OP for two sidekicks, reduced to just one (still pretty good)
		"Mini-Missile":               LocalWeapon(409, count=2, tags=['HasAmmo']),
		"Buster Rocket":              LocalWeapon(410, count=2, tags=['HasAmmo', 'HighDPS']),
		"Zica Supercharger":          LocalWeapon(411, count=2),
		"MicroBomb":                  LocalWeapon(412, count=2, tags=['HasAmmo']),
		"8-Way MicroBomb":            LocalWeapon(413, count=2, tags=['HasAmmo', 'Defensive']),
		"Post-It Mine":               LocalWeapon(414, count=2, tags=['HasAmmo']),
		"Mint-O-Ship":                LocalWeapon(415, count=2),
		"Zica Flamethrower":          LocalWeapon(416, count=2),
		"Side Ship":                  LocalWeapon(417, count=2, tags=['HasAmmo']),
		"Companion Ship Warfly":      LocalWeapon(418, count=2),
		"MicroSol FrontBlaster":      LocalWeapon(419, count=1, tags=['RightOnly']),
		"Companion Ship Gerund":      LocalWeapon(420, count=2),
		"BattleShip-Class Firebomb":  LocalWeapon(421, count=1, tags=['RightOnly', 'HighDPS', 'Defensive']),
		"Protron Cannon Indigo":      LocalWeapon(422, count=1, tags=['RightOnly']),
		"Companion Ship Quicksilver": LocalWeapon(423, count=2),
		"Protron Cannon Tangerine":   LocalWeapon(424, count=1, tags=['RightOnly', 'Defensive']),
		"MicroSol FrontBlaster II":   LocalWeapon(425, count=1, tags=['RightOnly']),
		"Beno Wallop Beam":           LocalWeapon(426, count=1, tags=['RightOnly', 'HighDPS']),
		"Beno Protron System -B-":    LocalWeapon(427, count=1, tags=['RightOnly', 'Defensive']),
		"Tropical Cherry Companion":  LocalWeapon(428, count=2),
		"Satellite Marlo":            LocalWeapon(429, count=2),
		# ---------- TYRIAN 2000 LINE ----------
	}

	# ----------------------------------------------------------------------------------------------------------------

	upgrades: Dict[str, LocalItem] = {
		"Armor Up":              LocalItem(500, count=9,  item_class=IC.progression), # Starts at 5, caps at 14
		"Maximum Power Up":      LocalItem(501, count=10, item_class=IC.progression), # Starts at 1, caps at 11
		"Shield Up":             LocalItem(503, count=9,  item_class=IC.useful), # Starts at 5, caps at 14

		"Progressive Generator": LocalItem(502, count=5, item_class=IC.progression),
		"Solar Shields":         LocalItem(504, count=1, item_class=IC.useful),
	}

	junk: Dict[str, LocalItem] = {
		# All Credits items have their count set dynamically.
		"SuperBomb":      LocalItem(600, count=8),
		"10 Credits":     LocalItem(601),
		"25 Credits":     LocalItem(602),
		"50 Credits":     LocalItem(603),
		"75 Credits":     LocalItem(604),
		"100 Credits":    LocalItem(605),
		"150 Credits":    LocalItem(606),
		"200 Credits":    LocalItem(607),
		"300 Credits":    LocalItem(608),
		"375 Credits":    LocalItem(609),
		"500 Credits":    LocalItem(610),
		"750 Credits":    LocalItem(611),
		"800 Credits":    LocalItem(612),
		"1000 Credits":   LocalItem(613),
		"2000 Credits":   LocalItem(614),
		"5000 Credits":   LocalItem(615),
		"7500 Credits":   LocalItem(616),
		"10000 Credits":  LocalItem(617),
		"20000 Credits":  LocalItem(618),
		"40000 Credits":  LocalItem(619),
		"75000 Credits":  LocalItem(620),
		"100000 Credits": LocalItem(621),
		"Dynamic Junk":   LocalItem(699), # Does nothing, placeholder for proper junk fill 
	}

	@classmethod
	def front_ports_by_tag(cls, tag: str):
		return [name for (name, contents) in cls.front_ports.items() if tag in contents.tags]

	@classmethod
	def rear_ports_by_tag(cls, tag: str):
		return [name for (name, contents) in cls.rear_ports.items() if tag in contents.tags]

	@classmethod
	def specials_by_tag(cls, tag: str):
		return [name for (name, contents) in cls.special_weapons.items() if tag in contents.tags]

	@classmethod
	def sidekicks_by_tag(cls, tag: str):
		return [name for (name, contents) in cls.sidekicks.items() if tag in contents.tags]

	@classmethod
	def get_item_name_to_id(cls, base_id: int):
		all_items = {}
		all_items.update({name: (base_id + item.local_id) for (name, item) in cls.levels.items()})
		all_items.update({name: (base_id + item.local_id) for (name, item) in cls.front_ports.items()})
		all_items.update({name: (base_id + item.local_id) for (name, item) in cls.rear_ports.items()})
		all_items.update({name: (base_id + item.local_id) for (name, item) in cls.special_weapons.items()})
		all_items.update({name: (base_id + item.local_id) for (name, item) in cls.sidekicks.items()})
		all_items.update({name: (base_id + item.local_id) for (name, item) in cls.upgrades.items()})
		all_items.update({name: (base_id + item.local_id) for (name, item) in cls.junk.items()})
		return all_items

	@classmethod
	def get(cls, name: str) -> LocalItem:
		if name in cls.levels:          return cls.levels[name]
		if name in cls.front_ports:     return cls.front_ports[name]
		if name in cls.rear_ports:      return cls.rear_ports[name]
		if name in cls.special_weapons: return cls.special_weapons[name]
		if name in cls.sidekicks:       return cls.sidekicks[name]
		if name in cls.upgrades:        return cls.upgrades[name]
		if name in cls.junk:            return cls.junk[name]
		raise KeyError(f"Item {name} not found")
