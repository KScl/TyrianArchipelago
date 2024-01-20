from typing import TextIO, Optional, List, Dict

from BaseClasses import Item, Location, Region
from BaseClasses import ItemClassification as IC
from BaseClasses import LocationProgressType as LP

from .Items import LocalItemData, LocalItem
from .Locations import LevelLocationData
from .Logic import RequirementList, TyrianLogic
from .Options import TyrianOptions

from ..AutoWorld import World, WebWorld

class TyrianItem(Item):
	game = "Tyrian"

class TyrianLocation(Location):
	game = "Tyrian"
	requirement_list: Optional[RequirementList]

class TyrianWorld(World):
	"""
	Follow pilot Trent Hawkins in the year 20,031 as he flies through the galaxy to defend it from the evil 
	corporation, Microsol.
	"""
	game = "Tyrian"
	options_dataclass = TyrianOptions
	options: TyrianOptions

	base_id = 20031000

	item_name_to_id = LocalItemData.get_item_name_to_id(base_id)
	location_name_to_id = LevelLocationData.get_location_name_to_id(base_id)

	starting_level: str = "" # Level we start on, gets precollected automatically

	episodes_needed: List[int] = [] # Require these episodes for goal (1, 2, 3, 4, 5)
	episodes_wanted: List[int] = [] # Add levels from these episodes (1, 2, 3, 4, 5)

	all_levels: List[str] = [] # List of all levels available in seed
	local_itempool: List[str] = [] # Item pool for just us. Forced progression items start with !
	local_locationpool: List[str] = [] # Just locations in levels (ignoring shops)

	# ================================================================================================================

	def create_item(self, name: str) -> TyrianItem:
		force_progression = name.startswith("!")
		name = name[1:] if force_progression else name

		pool_data = LocalItemData.get(name)
		item_class = IC.progression if force_progression else pool_data.item_class

		return TyrianItem(name, item_class, self.item_name_to_id[name], self.player)

	def create_location(self, name: str, region: Region, reqs: Optional[RequirementList] = None) -> TyrianLocation:
		loc = TyrianLocation(self.player, name, self.location_name_to_id[name], region)
		loc.requirement_list = reqs
		if self.options.exclude_obscure_checks:
			loc.progress_type = LP.EXCLUDED if isinstance(reqs, RequirementList) and reqs.obscure else LP.DEFAULT
		# Otherwise, always set to LP.DEFAULT by default
		return loc

	def create_event(self, name: str) -> TyrianItem:
		return TyrianItem(name, IC.progression, None, self.player)

	# ================================================================================================================

	def get_dict_contents_as_items(self, target_dict: Dict[str, LocalItem]) -> List[str]:
		itemList = []

		for (name, item) in target_dict.items():
			if item.count > 0:
				itemList.extend([name] * item.count)

		return itemList

	def get_junk_items(self, total_checks: int, total_money: int) -> List[str]:
		total_money = int(total_money * (self.options.money_pool_scale / 100))

		valid_money_amounts = [int(name.rstrip(" Credits")) for name in LocalItemData.junk \
			if name.endswith(" Credits")]

		junk_list = []

		while total_checks > 1:
			# If we've overshot the target, fill all remaining slots with SuperBombs and leave.
			if total_money <= 0:
				junk_list.extend(["SuperBomb"] * total_checks)
				return junk_list

			# We want to allow a wide range of money values early, but then tighten up our focus as we start running
			# low on items remaining. This way we get a good variety of credit values while not straying too far
			# from the target value.
			average = total_money / total_checks
			avg_divisor = total_checks / 1.5 if total_checks > 3 else 2
			possible_choices = [i for i in valid_money_amounts if i > average / avg_divisor and i < average * 5]

			# In case our focus is a little _too_ tight and we don't actually have any money values close enough to
			# the average, just pick the next highest one above the average. That'll help ensure we're always over
			# the target value, and never under it.
			if len(possible_choices) == 0:
				item_choice = [i for i in valid_money_amounts if i >= total_money][0]
			else:
				item_choice = self.random.choice(possible_choices)

			total_money -= item_choice
			total_checks -= 1
			junk_list.append(f"{item_choice} Credits")

		# No point being random here. Just pick the first credit value that puts us over the target value.
		if total_checks == 1:
			item_choice = [i for i in valid_money_amounts if i >= total_money][0]
			junk_list.append(f"{item_choice} Credits")

		return junk_list;

	# ================================================================================================================

	def generate_early(self):
		if not self.options.enable_tyrian_2000_support:
			self.options.episode_5.value = 0

		self.episodes_needed.append(1) if self.options.episode_1 == 2 else None
		self.episodes_needed.append(2) if self.options.episode_2 == 2 else None
		self.episodes_needed.append(3) if self.options.episode_3 == 2 else None
		self.episodes_needed.append(4) if self.options.episode_4 == 2 else None
		self.episodes_needed.append(5) if self.options.episode_5 == 2 else None

		self.episodes_wanted.append(1) if self.options.episode_1 != 0 else None
		self.episodes_wanted.append(2) if self.options.episode_2 != 0 else None
		self.episodes_wanted.append(3) if self.options.episode_3 != 0 else None
		self.episodes_wanted.append(4) if self.options.episode_4 != 0 else None
		self.episodes_wanted.append(5) if self.options.episode_5 != 0 else None

		if len(self.episodes_needed) == 0:
			raise RuntimeError("Can't generate a Tyrian game with no episodes set as goal.")

		if 1 in self.episodes_wanted:   self.starting_level = "TYRIAN (Episode 1)"
		elif 2 in self.episodes_wanted: self.starting_level = "TORM (Episode 2)"
		elif 3 in self.episodes_wanted: self.starting_level = "GAUNTLET (Episode 3)"
		elif 4 in self.episodes_wanted: self.starting_level = "SURFACE (Episode 4)"
		else:                           self.starting_level = "ASTEROIDS (Episode 5)"

	def create_regions(self):
		menu_region = Region("Menu", self.player, self.multiworld)
		self.multiworld.regions.append(menu_region)

		play_level_region = Region("Play Next Level", self.player, self.multiworld)
		self.multiworld.regions.append(play_level_region)
		menu_region.connect(play_level_region)

		# ==============
		# === Levels ===
		# ==============

		for (name, location_list) in LevelLocationData.level_regions.items():
			region_item_data = LocalItemData.get(name) # Unlocking items have the same name as the region
			if region_item_data.episode not in self.episodes_wanted:
				continue

			self.all_levels.append(name)

			if self.starting_level == name:
				# World needs a starting point, give the item for the starting level immediately
				self.multiworld.push_precollected(self.create_item(name))
			else:
				self.local_itempool.append(name)

			new_region = Region(name, self.player, self.multiworld)
			play_level_region.connect(new_region, f"Open {name}")

			for (location_name, location_reqs) in location_list.items():
				self.local_locationpool.append(location_name)
				new_region.locations.append(self.create_location(location_name, new_region, location_reqs))

			self.multiworld.regions.append(new_region)

		# =============
		# === Shops ===
		# =============

		if self.options.shop_mode != "none":
			shop_region = Region("Shop", self.player, self.multiworld)
			self.multiworld.regions.append(shop_region)
			menu_region.connect(shop_region)

			# One of the "always_x" choices, add each level shop exactly x times
			if self.options.shop_item_count <= -1:
				times_to_add = abs(self.options.shop_item_count)			
				items_per_shop = {name: times_to_add for name in self.all_levels}

			# Not enough items for one in every shop
			elif self.options.shop_item_count < len(self.all_levels):
				items_per_shop = {name: 0 for name in self.all_levels}
				for level in self.random.sample(self.all_levels, self.options.shop_item_count):
					items_per_shop[level] = 1

			# More than enough items to go around
			else:
				# Silently correct too many items to just cap at the max
				total_item_count = min(self.options.shop_item_count, len(self.all_levels) * 5)

				# First guarantee every shop has at least one
				items_per_shop = {name: 1 for name in self.all_levels}
				total_item_count -= len(self.all_levels)

				# Then get a random sample of a list where every level is present four times
				# This gives us between 1 to 5 items in every level
				for level in self.random.sample(self.all_levels * 4, total_item_count):
					items_per_shop[level] += 1

			# ------------------------------------------------------------------------------------

			# Always add all shop regions, even if some may have no items
			for level in self.all_levels:
				new_region = Region(f"Shop ({level})", self.player, self.multiworld)
				new_entrance = shop_region.connect(new_region, f"Can shop at {level}")

				# Not used when setting up rules, but will mark all locations excluded if completion is obscure
				region_reqs = LevelLocationData.level_regions[level].completion_reqs
				for i in range(items_per_shop[level]):
					shop_loc_name = f"Shop ({level}) - Item {i + 1}"
					new_region.locations.append(self.create_location(shop_loc_name, new_region, region_reqs))

				self.multiworld.regions.append(new_region)

				# The shop entrance isn't a transition, it's unlocked by doing something in the level (completing it)
				# This is required, otherwise you get spurious generation failures
				main_level_region = self.multiworld.get_region(level, self.player)
				self.multiworld.register_indirect_condition(main_level_region, new_entrance)

		# ==============
		# === Events ===
		# ==============

		all_events = []
		episode_num = 0
		for event_name in LevelLocationData.events.keys():
			episode_num += 1
			if episode_num not in self.episodes_needed:
				continue

			event_location = TyrianLocation(self.player, event_name[0:9], None, menu_region)
			event_item = self.create_event(event_name)
			event_location.place_locked_item(event_item)
			all_events.append(event_name)
			menu_region.locations.append(event_location)

		# Victory condition
		self.multiworld.completion_condition[self.player] = lambda state: state.has_all(all_events, self.player)

	def create_items(self):
		# Level items are added into the pool in create_regions.
		self.local_itempool.extend(self.get_dict_contents_as_items(LocalItemData.front_ports))
		self.local_itempool.extend(self.get_dict_contents_as_items(LocalItemData.rear_ports))
		self.local_itempool.extend(self.get_dict_contents_as_items(LocalItemData.sidekicks))
		self.local_itempool.extend(self.get_dict_contents_as_items(LocalItemData.upgrades))
		self.local_itempool.extend(self.get_dict_contents_as_items(LocalItemData.junk))

		if self.options.specials == 2: # Specials as MultiWorld items
			self.local_itempool.extend(self.get_dict_contents_as_items(LocalItemData.special_weapons))

		def pop_from_pool(item_name) -> Optional[str]:
			if item_name in self.local_itempool: # Regular item
				self.local_itempool.remove(item_name)
				return item_name
			item_name = f"!{item_name}"
			if item_name in self.local_itempool: # Item promoted to progression
				self.local_itempool.remove(item_name)
				return item_name
			return None

		def pool_item_to_progression(item_name) -> bool:
			if item_name in self.local_itempool:
				self.local_itempool.remove(item_name)
				self.local_itempool.append(f"!{item_name}")
				return True
			return False

		# ----------------------------------------------------------------------------------------

		# Remove precollected (starting inventory) items from the pool.
		for precollect in self.multiworld.precollected_items[self.player]:
			pop_from_pool(precollect.name)

		# Pull the weapon we start with out of the pool.
		starting_weapon = pop_from_pool("Pulse-Cannon")
		self.multiworld.push_precollected(self.create_item(starting_weapon))

		if self.options.specials == 1: # Get a random special, no others
			random_special = self.random.choice(sorted(LocalItemData.special_weapons))

			# Force it to be considered progression, just in case we get something that can be used as such.
			self.multiworld.push_precollected(self.create_item(f"!{random_special}"))

		# ----------------------------------------------------------------------------------------

		# Promote some weapons to progression based on tags.
		for weapon in self.random.sample(LocalItemData.front_ports_by_tag("HighDPS"), 2):
			pool_item_to_progression(weapon)
		for weapon in self.random.sample(LocalItemData.front_ports_by_tag("Pierces"), 1):
			pool_item_to_progression(weapon)

		for weapon in self.random.sample(LocalItemData.rear_ports_by_tag("Sideways"), 1):
			pool_item_to_progression(weapon)

		for weapon in self.random.sample(LocalItemData.sidekicks_by_tag("HighDPS"), 1):
			pool_item_to_progression(weapon)
		for weapon in self.random.sample(LocalItemData.sidekicks_by_tag("Defensive"), 1):
			pool_item_to_progression(weapon)

		if self.options.specials == 2: # Specials as MultiWorld items
			# Promote some specials to progression, as well.
			for weapon in self.random.sample(LocalItemData.specials_by_tag("Pierces"), 1):
				pool_item_to_progression(weapon)

		if self.options.contact_bypasses_shields:
			# This can hit the same sidekick that was previously promoted to progressive, and that's fine.
			defensive_sidekick = self.random.choice(LocalItemData.sidekicks_by_tag("Defensive"))
			pool_item_to_progression(defensive_sidekick)
			self.multiworld.early_items[self.player][defensive_sidekick] = 1

		# ----------------------------------------------------------------------------------------

		# We need to know the total amount of money available for junk item placement, but we can't know that
		# until we know what's in every shop, and shop prices are calculated. So we punt until later (post_fill)
		rest_item_count = len(self.multiworld.get_unfilled_locations(self.player)) - len(self.local_itempool)
		if rest_item_count > 0:
			self.local_itempool.extend(["Dynamic Junk"] * rest_item_count)

		# We're finally done, dump everything we've got into the itempool
		self.multiworld.itempool.extend(self.create_item(item) for item in self.local_itempool)

	def set_rules(self):
		def create_level_unlock_rule(level_name: str):
			entrance = self.multiworld.get_entrance(f"Open {level_name}", self.player)
			entrance.access_rule = lambda state: state.has(level_name, self.player)

		def create_shop_rule(level_name: str):
			entrance = self.multiworld.get_entrance(f"Can shop at {level_name}", self.player)
			region_access = LevelLocationData.level_regions[level].completion_reqs
			if region_access == "Boss":
				entrance.access_rule = lambda state: \
					state.has(level_name, self.player) \
					and state.can_reach(f"{level_name} - Boss", "Location", self.player)
			else:
				entrance.access_rule = lambda state: \
					state.has(level_name, self.player) \
					and state._tyrian_requirement_satisfied(self.player, region_access)

		for level in self.all_levels:
			create_level_unlock_rule(level)
			if self.options.shop_mode != "none":
				create_shop_rule(level)

		# ----------------------------------------------------------------------------------------

		def create_location_rule(location_name: str):
			location = self.multiworld.get_location(location_name, self.player)
			location.access_rule = lambda state: \
				state._tyrian_requirement_satisfied(self.player, location.requirement_list)

		for check in self.local_locationpool:
			create_location_rule(check)

		# ----------------------------------------------------------------------------------------

		def create_episode_complete_rule(event_name, location_name):
			event = self.multiworld.find_item(event_name, self.player)
			event.access_rule = lambda state: state.can_reach(location_name, "Location", self.player)

		episode_num = 0
		for (event_name, location_needed) in LevelLocationData.events.items():
			episode_num += 1
			if episode_num not in self.episodes_needed:
				continue

			create_episode_complete_rule(event_name, location_needed)

	def post_fill(self):
		# Remember when we punted earlier during junk item filling?
		# Go through the entire multiworld and pull out our dynamic junk items, we're going to replace them
		placeholder_locations = self.multiworld.find_item_locations("Dynamic Junk", self.player)
		dynamic_junk = self.get_junk_items(len(placeholder_locations), 440000)
		self.random.shuffle(dynamic_junk)

		for (location, junk_item) in zip(placeholder_locations, dynamic_junk):
			location.item = self.create_item(junk_item)

	def write_spoiler(self, spoiler_handle: TextIO):
		spoiler_handle.write(f"\n\nLevel locations ({self.multiworld.player_name[self.player]}):\n\n")

		for level in self.all_levels:
			if self.starting_level == level:
				spoiler_handle.write(f"{level}: Start\n")
			else:
				level_item_location = self.multiworld.find_item(level, self.player)
				spoiler_handle.write(f"{level}: {level_item_location.name}\n")

		spoiler_handle.write(f"\n\nSpecial Weapon ({self.multiworld.player_name[self.player]}):\n")
		spoiler_handle.write("None\n")

		spoiler_handle.write(f"\n\nTwiddles ({self.multiworld.player_name[self.player]}):\n")
		spoiler_handle.write("None\n")

	def fill_slot_data(self) -> dict:
		return {
			"Settings": {
				"Tyrian2000": bool(self.options.enable_tyrian_2000_support),
				"Goal": sum(1 << i for i in self.episodes_needed),
				"Shops": (self.options.shop_mode != "none"),
				"Difficulty": int(self.options.difficulty),
				"HardContact": bool(self.options.contact_bypasses_shields),
				"ExcessArmor": bool(self.options.allow_excess_armor),
				"TwiddleCmds": bool(self.options.show_twiddle_inputs),
				"APRadar": bool(self.options.archipelago_radar),
				"Christmas": bool(self.options.christmas_mode)
			},
			"Twiddles": {
			},
			"Shops": {
			}
		}
