
from dataclasses import dataclass
from Options import PerGameCommonOptions, Toggle, DefaultOnToggle, Choice, Range, NamedRange, DeathLink

# ===================
# === Goals, etc. ===
# ===================

class EnableTyrian2000Support(Toggle):
    """
    Use Tyrian 2000's data instead of Tyrian 2.1. Turning this on is mandatory if you want to do anything with
    Episode 5. All of Tyrian 2000's weapons and new items will also be added to the item pool.
    """
    display_name = "Enable Tyrian 2000 support"

class GoalEpisode1(Choice):
    """
    Add Episode 1 (Escape) levels to the pool.
    If "goal" is chosen, you'll need to complete "ASSASSIN" (in addition to other episode goals) to win.
    """
    display_name = "Episode 1"
    option_goal = 2
    option_on = 1
    option_off = 0
    default = 2

class GoalEpisode2(Choice):
    """
    Add Episode 2 (Treachery) levels to the pool.
    If "goal" is chosen, you'll need to complete "GRYPHON" (in addition to other episode goals) to win.
    """
    display_name = "Episode 2"
    option_goal = 2
    option_on = 1
    option_off = 0
    default = 2

class GoalEpisode3(Choice):
    """
    Add Episode 3 (Mission: Suicide) levels to the pool.
    If "goal" is chosen, you'll need to complete "FLEET" (in addition to other episode goals) to win.
    """
    display_name = "Episode 3"
    option_goal = 2
    option_on = 1
    option_off = 0
    default = 2

class GoalEpisode4(Choice):
    """
    Add Episode 4 (An End to Fate) levels to the pool.
    If "goal" is chosen, you'll need to complete "NOSE DRIP" (in addition to other episode goals) to win.
    """
    display_name = "Episode 4"
    option_goal = 2
    option_on = 1
    option_off = 0
    default = 2

class GoalEpisode5(Choice):
    """
    Add Episode 5 (Hazudra Fodder) levels to the pool. This requires you to enable Tyrian 2000 support.
    If "goal" is chosen, you'll need to complete "FRUIT" (in addition to other episode goals) to win.   
    """
    display_name = "Episode 5"
    option_goal = 2
    option_on = 1
    option_off = 0
    default = 0

# =======================
# === Shops and Money ===
# =======================

class ShopMode(Choice):
    """
    Determine if shops exist and how they behave.

    If 'none', shops will not exist; credits will only be used to upgrade weapons.
    If 'standard', each level will contain a shop that is accessible after clearing it, that contains from 1 to 5
    checks for the multiworld. Progression items will generally be more expensive than useful items, and so on.
    If 'hidden', shops will behave as above, but will not tell you what you're buying until after you spend credits.
    Prices will be random across the board.
    """
    display_name = "Shop Mode"
    option_none = 0
    option_standard = 1
    option_hidden = 2
    default = 1

class ShopItemCount(NamedRange):
    """
    The number of shop location checks that will be added.
    All levels are guaranteed to have at least one shop item if there's more shop location checks than levels.
    You may also specify 'always_one', 'always_two', 'always_three', 'always_four', or 'always_five' to
    guarantee that shops will have exactly that many items.
    """
    display_name = "Shop Item Count"
    range_start = 1
    range_end = 325
    special_range_names = {
        "always_one":   -1,
        "always_two":   -2,
        "always_three": -3,
        "always_four":  -4,
        "always_five":  -5,
    }
    default = 100

class PriceScale(Range):
    """Prices of all items (shop items and weapon upgrades) will be scaled by this amount, as a percentage."""
    display_name = "Price Scaling"
    range_start = 50
    range_end = 500
    default = 100

class MoneyPoolScale(Range):
    """
    Change the amount of money in the pool, as a percentage.

    At 100 (100%), the total amount of money in the pool will be equal to the cost of upgrading the most expensive
    weapon to the maximum level, plus the cost of purchasing all items from every shop.
    """
    display_name = "Money Pool Scaling"
    range_start = 50
    range_end = 500
    default = 150

class BaseWeaponCost(Choice):
    """
    Change the amount that weapons (and, in turn, weapon power upgrades) cost.

    If 'original', weapons will cost the same amount that they do in the original game.
    If 'balanced', prices will be changed around such that generally more powerful and typically used weapons
    (Laser, etc.) will cost more.
    If 'randomized', weapons will have random costs.
    If 'fixed', weapons will all have the same fixed cost set by 'fixed_weapon_cost'.
    """
    display_name = "Base Weapon Cost"
    option_original = 0
    option_balanced = 1
    option_randomized = 2
    option_fixed = 3
    default = 0

class FixedWeaponCost(Range):
    """Set the amount of money all weapons will cost, with 'base_weapon_cost: fixed'."""
    display_name = "Fixed Weapon Cost"
    range_start = 1
    range_end = 3000
    default = 1000

class Specials(Choice):
    """
    Enable or disable specials (extra behaviors when starting to fire).

    If 'on', your ship will have a random special from the start.
    If 'as_items', specials will be added to the item pool, and can be freely chosen once acquired.
    If 'off', specials won't be available at all.
    """
    display_name = "Specials"
    option_on = 1
    option_as_items = 2
    option_off = 0
    default = 2

class Twiddles(Choice):
    """
    Enable or disable twiddles (Street Fighter-esque button combinations).

    If 'on', your ship will have three random twiddles. Their button combinations will be the same as in the original
    game; as will their use costs.
    If 'chaos', your ship will have three random twiddles with new inputs. They may have new, unique behaviors; and
    they may have different use costs.
    If 'off', no twiddles will be available.
    """
    display_name = "Twiddles"
    option_on = 1
    option_chaos = 2
    option_off = 0
    default = 1

# ==================
# === Difficulty ===
# ==================

class Difficulty(Choice):
    """
    Select the base difficulty of the game. Anything beyond Impossible is VERY STRONGLY not recommended unless you
    know what you're doing.
    """
    display_name = "Difficulty"
    # For future reference... In addition to other changes difficulty makes, it scales health by this amount:
    # Easy: 75%
    # Normal: 100%
    # Hard: 120%
    # Impossible: 150%
    # Suicide: 200%
    # Lord of Game: 400%
    option_easy = 1
    option_normal = 2
    option_hard = 3
    option_impossible = 4
    option_suicide = 6
    option_lord_of_game = 8
    alias_lord_of_the_game = option_lord_of_game
    alias_zinglon = option_lord_of_game
    default = 2

class HardContact(Toggle):
    """
    Direct contact with an enemy or anything else will completely power down your shields and deal armor damage.

    Note that this makes the game significantly harder. Additional "Enemy approaching from behind" callouts will be
    given if this is enabled, and some defensive sidekicks will be considered progression and placed early in the seed.
    """
    display_name = "Contact Bypasses Shields"

class ExcessArmor(DefaultOnToggle):
    """
    Twiddles, pickups, etc. can cause your ship to have more armor than its maximum armor rating."""
    display_name = "Allow Excess Armor"

class ObscurityExclusions(DefaultOnToggle):
    """
    Automatically exclude some checks that are deemed to require obscure game knowledge or are otherwise difficult
    to obtain in a casual setting.
    """
    display_name = "Exclude Obscure Checks"

# =====================
# === Visual Tweaks ===
# =====================

class ShowTwiddleInputs(DefaultOnToggle):
    """If twiddles are enabled, show their inputs in "Ship Info" next to the name of each twiddle."""
    display_name = "Show Twiddle Inputs"

class ArchipelagoRadar(DefaultOnToggle):
    """Shows a bright outline around any enemy that contains an Archipelago Item. Recommended for beginners."""
    display_name = "Archipelago Radar"

class Christmas(Toggle):
    """Use the Christmas set of graphics and sound effects. """
    display_name = "Christmas Mode"

# =============================================================================

@dataclass
class TyrianOptions(PerGameCommonOptions):
    enable_tyrian_2000_support: EnableTyrian2000Support
    episode_1: GoalEpisode1
    episode_2: GoalEpisode2
    episode_3: GoalEpisode3
    episode_4: GoalEpisode4
    episode_5: GoalEpisode5

    shop_mode: ShopMode
    shop_item_count: ShopItemCount
    price_scale: PriceScale
    money_pool_scale: MoneyPoolScale
    base_weapon_cost: BaseWeaponCost
    fixed_weapon_cost: FixedWeaponCost
    specials: Specials
    twiddles: Twiddles

    difficulty: Difficulty
    contact_bypasses_shields: HardContact
    allow_excess_armor: ExcessArmor
    exclude_obscure_checks: ObscurityExclusions

    show_twiddle_inputs: ShowTwiddleInputs
    archipelago_radar: ArchipelagoRadar
    christmas_mode: Christmas
    deathlink: DeathLink
