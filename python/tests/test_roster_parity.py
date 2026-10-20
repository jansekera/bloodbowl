"""Parita soupisek TV1500 mezi enginy (20.10.2026).

PHP `cli/tv1500_rosters.php` zůstalo od 07.10. pozadu za C++ `engine/src/roster.cpp` — změny elfů (16.10.:
Nerves of Steel místo Diving Catch u jednoho Catchera, Lineman bez Wrestle) a skavenů (18.10.: Strip Ball
lovci a hlavnímu Blitzerovi) se do PHP nedostaly a žádný test to nehlídal. Hlavička PHP souboru přitom říká
„kdo změní jednu stranu, změní i druhou“. Tady se obě jedenáctky porovnají hráč po hráči
(MA/ST/AG/AV + úplné skilly).
"""
import os
import subprocess
import sys

import pytest

KOREN = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
sys.path.insert(0, KOREN)
import bb_kontrola_tahu as kt  # noqa: E402

RASY = [("dwarf", "Dwarf"), ("woodelf", "Wood Elf"), ("skaven", "Skaven"), ("orc", "Orc"), ("human", "Human")]

PHP_DUMP = r"""
require '%(koren)s/vendor/autoload.php';
require '%(koren)s/cli/tv1500_rosters.php';
foreach (getTv1500RaceRoster(App\Enum\TeamSide::HOME, '%(rasa)s') as $p) {
    $s = $p->getStats();
    $sk = array_map(fn($x) => $x->name, $p->getSkills());
    sort($sk);
    printf("%%d/%%d/%%d/%%d %%s\n", $s->getMovement(), $s->getStrength(), $s->getAgility(), $s->getArmour(), implode('+', $sk));
}
"""


def _cpp(rasa):
    bb = kt.bb
    s = bb.GameState()
    bb.setup_half(s, bb.get_developed_roster(rasa, 1500), bb.get_developed_roster("orc", 1500), bb.TeamSide.AWAY)
    return sorted("%d/%d/%d/%d " % (p.stats.movement, p.stats.strength, p.stats.agility, p.stats.armour)
                  + "+".join(sorted(n for n in kt.SKILL_NAMES if kt.ma_skill(p, n)))
                  for p in kt.hraci(s) if p.team_side == bb.TeamSide.HOME)


def _php(rasa):
    out = subprocess.run(["php", "-d", "display_errors=stderr", "-r", PHP_DUMP % {"koren": KOREN, "rasa": rasa}],
                         capture_output=True, text=True, check=True).stdout
    return sorted(line for line in out.splitlines() if line.strip())


@pytest.mark.parametrize("cpp_rasa,php_rasa", RASY)
def test_tv1500_eleven_is_the_same_in_both_engines(cpp_rasa, php_rasa):
    cpp, php = _cpp(cpp_rasa), _php(php_rasa)
    assert len(cpp) == 11 and len(php) == 11
    assert cpp == php, f"{php_rasa}: C++ {sorted(set(cpp) - set(php))} × PHP {sorted(set(php) - set(cpp))}"
