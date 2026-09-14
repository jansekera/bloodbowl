<?php
declare(strict_types=1);

namespace App\AI;

use App\DTO\GameState;
use App\DTO\MatchPlayerDTO;
use App\Engine\RulesEngine;
use App\Enum\ActionType;
use App\Enum\PlayerState;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use App\ValueObject\Position;

final class LearningAICoach implements AICoachInterface
{
    /** ⭐ PHP33: obsazeny ROH klece. */
    private const CAGE_CORNER_BONUS = 1.5;
    /**
     * ⭐ PRITAZLIVOST KLECE: hrac, ktery na roh v tomhle kole nedosahne, ma
     * mit duvod se k nemu PRIBLIZIT. Bez toho ho tam netahne nic -- bonus
     * dostaval jen za pole, ktere rohem UZ JE, takze posadka klece stala.
     * Klesa se vzdalenosti a nikdy neprebije samotny roh.
     */
    private const CAGE_APPROACH_BONUS = 2.0;
        /**
     * ⛔ MERENI 12.09.: klec se sejde jen v 7 kolech z 317 (2,2 %), pritom
     * nosic sam existuje v 82 kolech. Duvod: hrac bez mice dostava
     * `advancement * 0.1` za postup vpred -- za sest poli tedy az 0,6, coz
     * prebije priblizeni ke kleci. Posadka misto skladani klece BEZI DOPREDU.
     * ⇒ Kdyz mic drzi NAS tym, obecny postup vpred se pro hrace bez mice
     * tlumi: jeho ukol je klec, ne zavod do koncove zony.
     */
    private const ADVANCE_DAMP_WITH_CAGE = 0.3;
    /**
     * ⭐⭐ Uzivatel 12.09.: "kolem rohu nesmi byt sousedi souperi" a "muze se
     * posunout i do boku, nejen ciste dopredu."
     * ⇒ NOSIC si nevybira jen pole pro sebe, ale MISTO PRO CELOU KLEC:
     * ocenuje se kazdy roh ciloveho pole, ktery je volny (nebo nas) a nema
     * vedle sebe stojiciho soupere. Ctyri ciste rohy tedy daji +1,0.
     */
    private const CARRIER_CLEAN_CORNER_BONUS = 0.6;
    /**
     * ⭐⭐ PRIVEST ASISTENTA (uzivatel 13.09.: "a privezt asistenta by mel uz
     * umet taky -- ale neprivadej rohy klece nebo nosice").
     * Hrac, ktery si stoupne k souperi, ktereho uz nekdo nas oznackoval,
     * pridava asistenci a tim posouva kostky ve prospech toho druheho.
     * ⛔ Nesmi to delat nosic (ten k souperi vubec nesmi) ani hrac drzici roh
     * klece (ten ma vlastni ukol).
     */
    private const ASSIST_BONUS = 1.2;   // musi prebit i plny sprint vpred (0,6 + 0,4)
    /**
     * ⭐⭐ BLOK SE VYBIRA PODLE KOSTEK, NE PAUSALEM (uzivatel 12.09.:
     * "na blitz mame mit lepsi kandidaty a asistenty").
     * Do ted mel kazdy blok `+0,05` bez ohledu na to, jestli se hazi tremi
     * kostkami ve svuj prospech, nebo jednou, nebo dokonce dvema PROTI.
     * Asistence pritom uz engine pocita -- `StrengthCalculator`.
     *
     * Poradi hodnot je poradi realne vyhodnosti:
     *   3 kostky pro mne  >  2 pro mne  >  1
     *
     * ⛔⛔ PHP38 (13.09.): ZAPORNA PULKA TABULKY ZMIZELA (`2-` = -1,5,
     *   `3-` = -3,0). Byl to zakaz zapsany VELIKOSTI CISLA a nedelal, co
     *   tvrdil: komentar u nej sliboval "bonus za nosice to muze prebit"
     *   (+0,5 blok / +0,8 blitz), jenze -3,0 neprebije +0,8 NIKDY.
     *   ⇒ Nevyhodnost bloku se ted nepise do skore, ale do
     *   `pTurnover` -- `CoachHeuristics::pTurnoverBloku()` da dvema kostkam
     *   proti 55,6 % a trem 70,4 %, coz je nad prahem NOUZE. Blok na
     *   silnejsiho nosice tim zustane hratelny jako nouzova moznost,
     *   presne jak si uzivatel 13.09. vyzadal.
     */
    private const BLOCK_DICE_VALUE = [
        // ⏰ Zaporne polozky DOCASNE ZPET (14.09.2026): PHP38 je mel nahradit
        //   turnoverovou branou, ale `oceneniBloku()` je cte dal a chybejici
        //   klic v PHP vraci `null` -- odtud 4 TypeError. Az bude brana
        //   zapojena, smazou se i s tim ctenim, ne driv.
        '3+' => 1.2, '2+' => 0.7, '1' => 0.1, '2-' => -1.5, '3-' => -3.0,
    ];
    /**
     * ⛔⛔ Uzivatel 12.09.: cista klec "je nutna -- jinak o mic prijdeme."
     * ⇒ Neni to preference, je to PODMINKA: spinavy roh znamena, ze souper
     * srazi rohoveho hrace a je u nosice. Proto je pokuta za roh se souperem
     * vedle tezka, ne kosmeticka.
     */
    private const CAGE_DIRTY_HARD_PENALTY = 2.0;
    /** ⭐ Uz stojim v rohu => DRZ POZICI. Musi prebit presun na jiny roh. */
    private const CAGE_HOLD_BONUS = 1.8;
    /** ⛔ Nosic NESMI utect vlastni kleci -- pokuta za kazde pole navic. */
    private const CAGE_OUTRUN_PENALTY = 0.9;
    /**
     * ⭐ Uzivatel 12.09.: "kdyz je jen jeden z peti MA 4 a ostatni MA 5, tak
     * je jeho pohyb POSLEDNI s GFI -- riziko na konec."
     * ⇒ Nejpomalejsi hrac klec NEZASTAVI: dozene ji pres GFI. Takove pole
     * navic tedy neni zakazane, jen RIZIKOVE -- mensi pokuta, a diky
     * `RISK_FREE_BONUS` se takovy tah zahraje az na konci kola.
     */
    private const CAGE_GFI_REACH = 2;
    private const CAGE_GFI_PENALTY = 0.25;
    /**
     * ⛔⛔ Uzivatel 12.09.: "a postavit nosice vedle soupere nesmime uz vubec."
     * Nosic, ktery skonci v zone zachyceni soupere, muze byt blokovan,
     * sražen a o mic pripraven -- a klec kolem nej uz nic nezachrani.
     * Pokuta je zamerne VETSI nez cokoli, co se za pohyb da ziskat, takze
     * takove pole prohraje se vsim krome touchdownu (ten ma vlastni vetev
     * a sem se nedostane).
     */
    private const CARRIER_NEXT_TO_ENEMY_PENALTY = 5.0;
    /**
     * ⭐⭐ Uzivatel 12.09.: "pokud je mic na zemi a nejsou kolem souperi --
     * musi runner nebo thrower k mici a zvednout = nejen v prvnim kole,
     * NEJ PRIORITA."
     * ⇒ Volny mic bez soupere v okoli je nejlepsi vec, ktera se da udelat:
     * nikdo o nej nesoupeu a tym z nej ma cely zbytek tahu. Bonus je proto
     * vyssi nez cokoli krome touchdownu.
     */
    private const PICKUP_UNCONTESTED_BONUS = 4.0;
    /** A jde pro nej hrac, ktery ho udrzi -- runner nebo thrower. */
    private const PICKUP_SPECIALIST_BONUS = 1.0;

    private readonly \App\Engine\BallResolver $ballResolver;
    private readonly \App\Engine\StrengthCalculator $strCalc;
    /**
     * ⭐⭐⭐ PHP38 (13.09.): JEDNA CENA TURNOVERU MISTO PETI VAH.
     *
     * ⛔ Do 13.09. se riziko ocenovalo na PETI mistech s vlastni vahou
     *   a vlastnim prahem: cesta (`RISK_WEIGHT` 1,5 / 5,0 + privazky 2,0/4,0),
     *   zvednuti (`PICKUP_FAIL_WEIGHT` 4,0), chyceni (`CATCH_FAIL_WEIGHT` 4,0),
     *   blok (zaporna pulka `BLOCK_DICE_VALUE`) a blitz (kopie prahu u cesty).
     *   Faul a multiblok neresily riziko VUBEC.
     * ⇒ Ted kazda akce vraci svou `pTurnover` a brana je jedna:
     *   `score -= pT * cenaTurnoveru()`.
     *
     * ⭐ POMER ZUSTAVA UZIVATELUV (12.09.): riziko se ocenuje podle toho, KDO
     *   ho podstupuje. Kdyz je ve hre MIC (nosic se hybe, zvedame, hazime,
     *   predavame), ztracime tah I MIC -- proto 5,0. Jinak jen tah -- 1,5.
     *   Cisla jsou puvodni `RISK_WEIGHT` / `RISK_WEIGHT_CARRIER`.
     */
    private const CENA_TURNOVERU = 1.5;
    private const CENA_TURNOVERU_MIC = 5.0;
    /**
     * ⏰ DOCASNE ZPET (14.09.2026): tvrde odmitnuti pod prahem.
     *   PHP38 ho ma nahradit VRSTVOU NOUZE, jenze vrstvy zatim NIKDO NEVOLA
     *   (`VRSTVA_*` jsou deklarovane a nepouzite). Az do jejich zapojeni
     *   plati puvodni privazky -- jinak by se odmitani ztratilo potichu
     *   a zmena chovani by se schovala do refaktoru.
     */
    /**
     * ⏰ DOCASNE ZPET (14.09.2026) ze stejneho duvodu jako privazky odmitnuti:
     *   PHP38 mel obe nahradit jednou cenou turnoveru (`pT * cenaTurnoveru()`),
     *   ale prevod volajicich mist se neudelal. Do te doby plati puvodni vahy.
     */
    private const PICKUP_FAIL_WEIGHT = 4.0;
    private const CATCH_FAIL_WEIGHT = 4.0;
    private const RISK_REFUSE_BELOW = 50;
    private const RISK_REFUSE_PENALTY = 2.0;
    private const RISK_REFUSE_PENALTY_CARRIER = 4.0;
    /**
     * ⛔ CENA VYHOZENI ZA FAUL. `FoulHandler.php:110`: "Foul is NEVER
     *   a turnover (even with ejection)" -- faul tedy do turnoverove brany
     *   NEPATRI, jeho riziko je ZTRATA HRACE do konce zapasu.
     *   Hodnota je zamerne v tomtez pasmu jako nejlepsi blok (`3+` = 1,2):
     *   prijit o hrace je horsi nez cokoli, co jeden faul prinese.
     */
    private const CENA_VYHOZENI = 1.2;
    /** Od kolika obsazenych rohu se to uz pocita za klec, kterou ma cenu drzet. */
    private const CAGE_MIN_CORNERS = 2;
    /**
     * ⭐ PORADI AKTIVACI: NEJDRIV BEZ RIZIKA (uzivatel 12.09.).
     * Bonus dostane tah, ktery nepotrebuje ANI JEDEN hod -- zadny dodge,
     * zadne GFI. Tim se bezpecne aktivace udelaji DRIV a rizikove zbydou
     * na konec kola, kdy uz turnover stoji min.
     */
    private const RISK_FREE_BONUS = 0.4;

    /**
     * ⭐⭐⭐ PHP38 -- VRSTVY MISTO ZAPORNYCH KONSTANT (uzivatel 13.09.:
     *   "nebo akci provest jen v nouzi").
     *
     *   NORMAL  -- turnover pod 50 %, hraje se normalne.
     *   NOUZE   -- turnover 50 % a vic; sahne se po nem, AZ KDYZ je NORMAL
     *              prazdny. Nahrazuje privazky `RISK_REFUSE_PENALTY(_CARRIER)`
     *              i tvrde `continue` u zvedani mice.
     *   POSLEDNI-- zachranny `STAND_PAT`. Do 13.09. mel skore `baseScore - 0,5`,
     *              tedy zakaz zapsany velikosti cisla; ted je to proste
     *              nejnizsi vrstva, a END_TURN je az za ni.
     */
    private const VRSTVA_NORMAL = 0;
    private const VRSTVA_NOUZE = 1;
    private const VRSTVA_POSLEDNI = 2;

    private string $modelType = 'linear';
    /** @var list<float> */
    private array $weights = [];
    private float $epsilon;

    // Neural network parameters (only used when modelType === 'neural')
    /** @var list<list<float>> */
    private array $W1 = [];
    /** @var list<float> */
    private array $b1 = [];
    /** @var list<list<float>> */
    private array $W2 = [];
    /** @var list<float> */
    private array $b2 = [];

    /**
     * @param string|null $weightsFile Path to weights JSON file (null = zero weights)
     */
    public function __construct(?string $weightsFile = null, float $epsilon = 0.0)
    {
        $this->epsilon = $epsilon;
        // ⛔ Jen kvuli `getPickupTarget()` -- kostkou se tady nikdy nehazi.
        $this->strCalc = new \App\Engine\StrengthCalculator(new \App\Engine\TacklezoneCalculator());
        $this->ballResolver = new \App\Engine\BallResolver(
            new \App\Engine\RandomDiceRoller(),
            new \App\Engine\TacklezoneCalculator(),
            new \App\Engine\ScatterCalculator(),
        );

        if ($weightsFile !== null && file_exists($weightsFile)) {
            $json = file_get_contents($weightsFile);
            $data = $json !== false ? json_decode($json, true) : null;
            if (is_array($data) && isset($data['type']) && $data['type'] === 'neural') {
                $this->modelType = 'neural';
                $this->W1 = $data['W1'];
                $this->b1 = $data['b1'];
                $this->W2 = $data['W2'];
                $this->b2 = $data['b2'];
            } else {
                // ⛔⛔⛔ OPRAVA 11.09.2026: TADY SE NAČÍTAL CELÝ OBJEKT JSON.
                //   `array_map('floatval', $data)` jelo přes KLÍČE NEJVYŠŠÍ
                //   ÚROVNĚ (`type`, `value_weights`, `policy_weights`,
                //   `policy_bias`, `policy_temperature`), ne přes váhy.
                //   Výsledek: `floatval('alphazero_linear')`=0, `floatval(pole)`=1,
                //   `floatval(pole)`=1, bias, temperature
                //   ⇒ `$this->weights` byl **[0, 1, 1, ~0, 1]** -- PĚT čísel
                //   místo 73 natrénovaných vah.
                //
                // ⛔ A NEBYLO TO VIDĚT, protože `dotProduct` bere
                //   `min(count($a), count($b))` = min(5, 73) = 5 a zbytek
                //   TIŠE USEKNE -- bez chyby, bez hlášky. Stav se tedy
                //   hodnotil jako `f1 + f2 + f4` a celý trénink se zahazoval.
                //   Tatáž třída jako `try/catch` v `cli/simulate.php:157`:
                //   pojistka, která vadu schová před měřením.
                //
                // ⚠️ `array_map` nad plochým seznamem byl nejspíš správný pro
                //   STARŠÍ formát souboru (holé pole floatů). Formát se změnil
                //   na strukturovaný objekt a tohle místo se neaktualizovalo.
                //   Proto se podporují OBA tvary, ne jen ten nový.
                $raw = (isset($data['value_weights']) && is_array($data['value_weights']))
                    ? $data['value_weights']
                    : $data;
                $this->weights = self::normalizeWeights(array_values(array_map('floatval', $raw)));
            }
        } else {
            $this->weights = array_fill(0, FeatureExtractor::NUM_FEATURES, 0.0);
        }
    }

    /**
     * ⭐ KLEC (PHP33, zadani uzivatele 12.09.2026): klec je nosic + CTYRI
     *    DIAGONALNI ROHY. Ortogonalni soused je k nicemu -- souper na nosice
     *    dosahne stejne a jeste si to pole sam zabere.
     *
     * ⚠️ Uzivatel k tomu dodal: "jeste je varianta, kdy je nosic u kraje
     *    a jsou jen dva rohy -- ale to je nebezpecne." Dvourohá klec je tedy
     *    NOUZE, ne cil; tahle metoda o ni nic netvrdi, jen rika, jestli
     *    dane pole JE roh.
     */
    private static function isCageCorner(Position $carrierPos, Position $pos): bool
    {
        return abs($carrierPos->getX() - $pos->getX()) === 1
            && abs($carrierPos->getY() - $pos->getY()) === 1;
    }


    /**
     * ⭐ PHP33: hraci vlastniho tymu stojici v ROZICH kolem daneho pole.
     */
    private function cornerPlayers(GameState $state, TeamSide $side, Position $center, int $exceptId): array
    {
        $out = [];
        foreach ($state->getPlayersOnPitch($side) as $p) {
            if ($p->getId() === $exceptId) {
                continue;
            }
            $pos = $p->getPosition();
            if ($pos !== null && self::isCageCorner($center, $pos)) {
                $out[] = $p;
            }
        }

        return $out;
    }

    /**
     * Kolik poli ujde NEJPOMALEJSI z klece. Uzivatel 12.09.: "max pohyb
     * klece podle nejmensiho MA ze vsech peti."
     */
    private function slowestRemaining(array $players): int
    {
        $min = PHP_INT_MAX;
        foreach ($players as $p) {
            $min = min($min, $p->getMovementRemaining());
        }

        return $min === PHP_INT_MAX ? 0 : max(0, $min);
    }

    /**
     * Sance (0-1), ze TENHLE hrac zvedne mic na danem poli.
     *
     * ⭐ Prah se NEPOCITA znovu -- bere se `BallResolver::getPickupTarget()`,
     *    tedy tataz funkce, kterou pak pouzije engine. Hrac se pro vypocet
     *    postavi na pole s micem, aby sedely zony zachyceni.
     * ⭐ Sure Hands dava opakovani hodu, takze sance je 1-(1-p)^2.
     */
    private function pickupChance(GameState $state, MatchPlayerDTO $player, Position $ballPos): float
    {
        $naMici = $player->withPosition($ballPos);
        $prah = $this->ballResolver->getPickupTarget($state, $naMici);
        $p = max(0.0, min(1.0, (7 - $prah) / 6));
        if ($player->hasSkill(SkillName::SureHands)) {
            $p = 1 - (1 - $p) ** 2;
        }

        return $p;
    }

    /**
     * ⭐⭐ ROZPUSTIT KLEC A VYRAZIT SAM -- uzivatel 12.09.:
     *   "klec rozpustit ve chvili, kdy dopocitam, ze cela klec do TD zony
     *    nedojde, ale nosic sam by dosel ... vyrazit na cestu bez cele klece
     *    v kole PREDTIM, nez to bude pro nosice natesno s dobehnutim."
     *
     * ⇒ Pocita se, kolik kol nosic sam potrebuje (`vzdalenost / MA`, nahoru)
     *   a kolik kol zbyva. Vyrazi se, kdyz uz je na to **jen tak tak** --
     *   tedy o kolo DRIV, nez by mu zbyvalo presne tolik kol, kolik cesta trva.
     */
    private function musiVyrazitSam(GameState $state, TeamSide $side, MatchPlayerDTO $nosic): bool
    {
        $pos = $nosic->getPosition();
        if ($pos === null) {
            return false;
        }
        $endZoneX = CoachHeuristics::endZoneX($side);
        $vzdalenost = abs($pos->getX() - $endZoneX);
        $ma = max(1, $nosic->getStats()->getMovement());
        $kolNaCestu = (int) ceil($vzdalenost / $ma);
        $zbyvaKol = max(0, 9 - $state->getTeamState($side)->getTurnNumber());

        // ⭐ Ta "jedna kolo rezervy" je jadro uzivatelova zaveru: nevyrazi se
        //   az kdyz to presne vyjde, ale uz o kolo driv.
        return $zbyvaKol <= $kolNaCestu + 1;
    }

    /**
     * ⭐ Uzivatel 12.09.: "pokud jsme proti pomalemu souperi, muze nosic na
     *   nasi puli bezet sam rychleji kupredu bez klece -- DOKUD SE K NEMU
     *   SOUPER NEDOSTANE."
     *
     * ⇒ Dosah soupere = jeho MA + 2 pole na GFI. Kdyz na cilove pole
     *   nedosahne ani ten nejblizsi stojici, klec neni potreba a nosic muze
     *   bezet naplno. Pomaly souper (trpaslik MA 4) tim nechava vic prostoru
     *   nez rychly (elf MA 8) -- presne jak uzivatel rika.
     */
    private function souperNedosahne(GameState $state, TeamSide $side, Position $cil): bool
    {
        foreach ($state->getPlayersOnPitch($side->opponent()) as $souper) {
            $sp = $souper->getPosition();
            if ($sp === null) {
                continue;
            }

            // ⛔⛔ LEZICI SOUPER NENI MIMO HRU (uzivatel 12.09.: "u mereni
            //   vzdalenosti nezapomen na nejblizsi lezici a i na nejblizsi
            //   lezici s Jump Up"). Do ted se preskakoval kazdy, kdo nestal --
            //   a pritom:
            //     * PRONE se postavi za 3 pole pohybu a zbytkem dojde,
            //     * PRONE s JUMP UP se postavi ZADARMO, takze ma plny dosah,
            //     * STUNNED tenhle tah jednat nemuze, ten se opravdu nepocita.
            $ma = $souper->getStats()->getMovement();
            $dosah = match (true) {
                $souper->getState() === PlayerState::STANDING => $ma + 2,
                $souper->getState() === PlayerState::PRONE
                    && $souper->hasSkill(SkillName::JumpUp) => $ma + 2,
                $souper->getState() === PlayerState::PRONE => max(0, $ma - 3) + 2,
                default => -1,   // STUNNED a spol. -- letos nikam nejde
            };
            if ($dosah >= 0 && $sp->distanceTo($cil) <= $dosah) {
                return false;
            }
        }

        return true;
    }

    /**
     * ⭐⭐ OCENIT BLOK PRED AKCI -- stejne jako dodge (uzivatel 13.09.:
     *   "nas block by mel byt schopen ohodnoceni pred akci stejne jako jsme
     *    to resili u dodge a nebezpecne odmitnout -- stejne v akci block
     *    i blitz").
     *
     * Vraci ocenení podle poctu kostek VCETNE ASISTENCI: kladne, kdyz kostky
     * vybira utocnik, zaporne, kdyz je vybira souper. ⛔ Uz NEVRACI `null` --
     * to zahazovalo i bloky na silnejsiho NOSICE, kde se to presto vyplati.
     */
    private function oceneniBloku(GameState $state, MatchPlayerDTO $utocnik, MatchPlayerDTO $obrance): float
    {
        $up = $utocnik->getPosition();
        $op = $obrance->getPosition();
        if ($up === null || $op === null) {
            return 0.05;
        }

        // ⭐ Bezstavovy pomocnik patri do konstruktoru, ne do `static`
        //   promenne uvnitr metody (nalez /simplify).
        $kostky = $this->strCalc->getBlockDiceInfo(
            $this->strCalc->calculateEffectiveStrength($state, $utocnik, $op),
            $this->strCalc->calculateEffectiveStrength($state, $obrance, $up),
        );

        // ⛔ KOSTKY PROTI NAM: tezka pokuta, ale UZ NE `null`.
        //   ⭐ OPRAVA 13.09. (review): tvrde odmitnuti bezelo PRED bonusem za
        //   nosice, takze silnejsiho NOSICE neslo nikdy blokovat -- a proti
        //   silnejsimu tymu je to jedina cesta, jak se dostat k mici.
        //   Ted se pokuta jen zapocita a bonus za nosice (+0,5 blok / +0,8
        //   blitz) ji muze prebit, kdyz to stoji za to.
        if (!$kostky['attackerChooses']) {
            // ⛔⛔ OPRAVA 13.09. (simplify): SLIBENA VYJIMKA NEFUNGOVALA.
            //   Komentar tvrdil, ze pokutu "muze prebit bonus za nosice"
            //   (+0,5 blok / +0,8 blitz) -- jenze -3,0 neprebije +0,8 NIKDY,
            //   takze blok na silnejsiho nosice zustal fakticky zakazany,
            //   presne to, co se melo opravit.
            // ⇒ Ted se rozhoduje EXPLICITNE, ne velikosti cisla: kdyz je
            //   cilem NOSIC, pokuta se zmirni tak, aby blok zustal ve hre
            //   jako nouzova moznost; jinak plati plna.
            $cilJeNosic = $state->getBall()->isHeld()
                && $state->getBall()->getCarrierId() === $obrance->getId();

            return match (true) {
                $cilJeNosic => self::BLOCK_DICE_VALUE['1'] - 0.4,
                $kostky['count'] === 3 => self::BLOCK_DICE_VALUE['3-'],
                default => self::BLOCK_DICE_VALUE['2-'],
            };
        }

        return match ($kostky['count']) {
            3 => self::BLOCK_DICE_VALUE['3+'],
            2 => self::BLOCK_DICE_VALUE['2+'],
            default => self::BLOCK_DICE_VALUE['1'],
        };
    }

    /**
     * Sance (0-1), ze hrac DOJDE vedle daneho soupere -- tedy nejlepsi
     * `successChance` mezi policky, ze kterych se da blitzovat.
     * `null` = nedojde vubec.
     *
     * ⭐ Pouziva tutez cestu jako pohyb (`getValidMoveTargets`), takze se
     *   riziko oceni STEJNE jako u dodge a GFI -- viz `RISK_WEIGHT`.
     */
    private function sanceDojitK(GameState $state, RulesEngine $rules, int $playerId, MatchPlayerDTO $cil, ?array $cile = null): ?float
    {
        $cp = $cil->getPosition();
        $mujPos = $state->getPlayer($playerId)?->getPosition();
        if ($cp === null || $mujPos === null) {
            return null;
        }

        // Uz vedle nej stojim => zadna cesta, zadne riziko.
        if ($mujPos->distanceTo($cp) === 1) {
            return 1.0;
        }

        $nej = null;
        foreach ($cile ?? $rules->getValidMoveTargets($state, $playerId) as $t) {
            if (max(abs($t['x'] - $cp->getX()), abs($t['y'] - $cp->getY())) !== 1) {
                continue;
            }
            $sance = max(0, min(100, (int) ($t['successChance'] ?? 100))) / 100;
            $nej = $nej === null ? $sance : max($nej, $sance);
        }

        return $nej;
    }

    /**
     * Sance (0-1), ze TENHLE hrac chyti mic na svem poli.
     *
     * ⭐ Prah se nepocita znovu -- bere se `BallResolver::getCatchTarget()`,
     *   tedy tataz funkce, kterou pak pouzije engine.
     * @param int $modifikator +1 za presnou prihravku, 0 za predani z ruky
     */
    private function catchChance(GameState $state, MatchPlayerDTO $prijemce, int $modifikator = 0): float
    {
        $prah = $this->ballResolver->getCatchTarget($state, $prijemce, $modifikator);
        $p = max(0.0, min(1.0, (7 - $prah) / 6));
        if ($prijemce->hasSkill(SkillName::Catch)) {
            $p = 1 - (1 - $p) ** 2;   // Catch dava opakovani hodu
        }

        return $p;
    }

    /** Nosic vlastniho tymu, nebo `null`. */
    private function ownCarrierPosition(GameState $state, TeamSide $side): ?Position
    {
        $ball = $state->getBall();
        if (!$ball->isHeld() || $ball->getCarrierId() === null) {
            return null;
        }
        $carrier = $state->getPlayer($ball->getCarrierId());
        if ($carrier === null || $carrier->getTeamSide() !== $side) {
            return null;
        }

        return $carrier->getPosition();
    }

    public function decideAction(GameState $state, RulesEngine $rules): array
    {
        $side = $state->getActiveTeam();
        $actions = $rules->getAvailableActions($state);

        // Cache base state evaluation (used by many scoring functions)
        $baseScore = $this->evaluateState($state, $side);

        // Deduplicate action types per player to avoid redundant work
        $seenActions = [];

        // Build scored candidate actions
        /** @var list<array{action: ActionType, params: array<string, mixed>, score: float}> */
        $candidates = [];
        foreach ($actions as $action) {
            $type = ActionType::from($action['type']);
            $playerId = $action['playerId'] ?? null;

            if ($type === ActionType::SETUP_PLAYER || $type === ActionType::END_SETUP) {
                continue;
            }
            // ⏸ PHP25: schopnost "nic nedelat" uz v enginu JE, ale kouc ji
            //   zatim NEPOUZIVA -- kdy ji ma volit, je vlastni rozhodnuti
            //   (jinak by se tise vratila vada, kterou PHP13/PHP24 zaviraly).
            if ($type === ActionType::STAND_PAT) {
                // ⭐ PHP25 ODBLOKOVAN 12.09. (uzivatel: "odblokuj PHP25") --
                //   a prvni pouziti je prave klec: hrac, ktery UZ STOJI
                //   v rohu klece, nema kam chodit. Do ted se musel hnout,
                //   protoze END_TURN smi az kdyz nabidka nic nema (PHP13/24),
                //   a tim se klec kazde kolo rozsypala.
                // ⛔ Zadna jina situace sem zatim nepatri; "kdy jeste nechat
                //   hrace stat" je porad otevrena otazka (PHP20/PHP25).
                $carrierPos = $this->ownCarrierPosition($state, $side);
                $mujPos = $playerId !== null ? ($state->getPlayer((int) $playerId)?->getPosition()) : null;
                if ($carrierPos !== null && $mujPos !== null
                    && self::isCageCorner($carrierPos, $mujPos)) {
                    $candidates[] = [
                        'action' => ActionType::STAND_PAT,
                        'params' => ['playerId' => (int) $playerId],
                        // Zustat stat nestoji ani jeden hod => patri mezi bezrizikove.
                        'score' => $baseScore + self::CAGE_HOLD_BONUS + self::RISK_FREE_BONUS,
                    ];
                } else {
                    // ⛔⛔ ZACHRANA PROTI NAVRATU VADY PHP13/PHP24 (13.09.).
                    //   Od te doby, co kouc nektere akce ODMITA (nosic nebojuje,
                    //   blok proti presile, hod pod 50 %), muze se stat, ze se
                    //   hraci nepostavi ZADNY kandidat -- a kolo pak skoncilo
                    //   END_TURN, ackoli bylo co hrat. Merenim 13.09.: 70 kol
                    //   z 291 (24,1 %), pritom rano to byla 3 kola.
                    //   ⇒ Takovy hrac ma ZUSTAT STAT, ne ukoncit kolo celemu
                    //   tymu. Skore je zamerne pod nulou, takze vyhraje jedine
                    //   tehdy, kdyz opravdu nic jineho neni.
                    $candidates[] = [
                        'action' => ActionType::STAND_PAT,
                        'params' => ['playerId' => (int) $playerId],
                        'score' => $baseScore - 0.5,
                    ];
                }
                continue;
            }


            // ⛔⛔⛔ OPRAVA 11.09.2026 -- polozka PHP24 fronty, MEKCI TVAR
            //   tehoz, co `PHP13` opravil u `GreedyAICoach` (`f44270f5`).
            //   END_TURN tu byl SKOROVANY KANDIDAT se skore `baseScore - 0.01`.
            //   Nemuselo tedy selhat nic: stacilo, aby vsichni POSTAVENI
            //   kandidati spadli pod tu hranici, a END_TURN VYHRAL SKOREM,
            //   i kdyz bylo co hrat. `buildMoveAction` pritom zaporne skore
            //   vraci snadno -- odecita za GFI (`gfis * 0.08`), za dodge
            //   a za postranni caru.
            //
            // ⭐ ZMERENO PRED OPRAVOU (`cli/diag_ai_endturn_20260911.php`):
            //   23 ze 7 666 rozhodnuti = **1,2 % KOL**, prumer **8,09 hrace**
            //   propadlo. ⚠️ A je to kouc, proti kteremu hraje CLOVEK
            //   (`ServiceProvider.php:121-127`).
            //
            // ⭐ PRAVIDLOVA KOTVA (`rules_bb2016.txt` r. 363-367 + uzavreny
            //   katalog turnoveru r. 368-384): "kandidati meli nizke skore"
            //   v tom katalogu NENI.
            //
            // ⇒ Oprava je TATAZ jako v C++ (`0630f854`), v `RandomAICoach`
            //   (`b0d01ccb`) a v `GreedyAICoach` (`f44270f5`): END_TURN az
            //   tehdy, kdyz nabidka nic jineho nema. Zadne nove skore se
            //   nevymysli -- END_TURN se jen prestane ucastnit souteze.
            //
            // ⚠️ CO SE TIM ZTRACI: kouc uz nemuze kolo ukoncit DOBROVOLNE
            //   (drive to umel prave pres tenhle kandidat). Zadny z ostatnich
            //   tri koucu to neumi taky -- parita je tim uplna, ale je to
            //   zmena chovani, ne jen odstraneni vady.
            if ($type === ActionType::END_TURN) {
                continue;
            }

            if ($playerId === null) {
                continue;
            }

            // Deduplicate: only evaluate one action per type+player
            $key = $type->value . '_' . $playerId;
            if (isset($seenActions[$key])) {
                continue;
            }
            $seenActions[$key] = true;

            $built = $this->buildScoredAction($state, $rules, $type, (int) $playerId, $side, $baseScore);
            if ($built !== null) {
                $candidates[] = $built;
            }
        }

        // ⭐ END_TURN az tady: kdyz nabidka opravdu nic hratelneho nemela.
        if ($candidates === []) {
            return ['action' => ActionType::END_TURN, 'params' => []];
        }

        // Epsilon-greedy: with probability epsilon, pick random
        if ($this->epsilon > 0 && (mt_rand() / mt_getrandmax()) < $this->epsilon) {
            $pick = $candidates[array_rand($candidates)];
            return ['action' => $pick['action'], 'params' => $pick['params']];
        }

        // Pick the highest-scored candidate
        $bestScore = -PHP_FLOAT_MAX;
        $bestAction = $candidates[0];
        foreach ($candidates as $candidate) {
            if ($candidate['score'] > $bestScore) {
                $bestScore = $candidate['score'];
                $bestAction = $candidate;
            }
        }

        return ['action' => $bestAction['action'], 'params' => $bestAction['params']];
    }

    public function setupFormation(GameState $state, TeamSide $side): GameState
    {
        $offPitchPlayers = [];
        foreach ($state->getTeamPlayers($side) as $player) {
            if ($player->getState() === PlayerState::OFF_PITCH) {
                $offPitchPlayers[] = $player;
            }
        }

        if ($side === TeamSide::HOME) {
            $positions = [
                new Position(12, 6), new Position(12, 7), new Position(12, 8),
                new Position(8, 4), new Position(8, 6), new Position(8, 8), new Position(8, 10),
                // ⭐ ČTYŘI VZADU JSOU POSÁDKA KLECE (uživatel 12.09.: "na to máš
                //   mít při rozestavení 4 další hráče vzadu"). Dřív stáli
                //   rozházení po šířce (y 3, 5, 9, 11) a k nosiči uprostřed
                //   se sbíhali půl kola. Teď stojí u středu, aby klec vznikla
                //   hned, jakmile nosič zvedne míč a vyrazí.
                new Position(4, 5), new Position(4, 6), new Position(4, 8), new Position(4, 9),
            ];
        } else {
            $positions = [
                new Position(13, 6), new Position(13, 7), new Position(13, 8),
                new Position(17, 4), new Position(17, 6), new Position(17, 8), new Position(17, 10),
                // ⭐ Totéž pro AWAY -- čtyři vzadu u středu jako posádka klece.
                new Position(21, 5), new Position(21, 6), new Position(21, 8), new Position(21, 9),
            ];
        }

        $count = min(count($offPitchPlayers), count($positions));
        for ($i = 0; $i < $count; $i++) {
            $state = $state->withPlayer(
                $offPitchPlayers[$i]
                    ->withPosition($positions[$i])
                    ->withState(PlayerState::STANDING),
            );
        }

        return $state;
    }

    /**
     * Score a state using the loaded model (linear dot product or neural network).
     */
    public function evaluateState(GameState $state, TeamSide $perspective): float
    {
        $features = FeatureExtractor::extract($state, $perspective);
        if ($this->modelType === 'neural') {
            return $this->evaluateNeural($features);
        }
        return self::dotProduct($this->weights, $features);
    }

    /**
     * Neural network inference: h = ReLU(features @ W1 + b1), output = tanh(h @ W2 + b2).
     *
     * @param list<float> $features
     */
    public function evaluateNeural(array $features): float
    {
        $nFeatures = count($this->W1);
        $hiddenSize = count($this->b1);

        // h = features @ W1 + b1 (then ReLU)
        $h = [];
        for ($j = 0; $j < $hiddenSize; $j++) {
            $sum = $this->b1[$j];
            $len = min(count($features), $nFeatures);
            for ($i = 0; $i < $len; $i++) {
                $sum += $features[$i] * $this->W1[$i][$j];
            }
            // ReLU
            $h[$j] = max(0.0, $sum);
        }

        // output = tanh(h @ W2 + b2)
        $out = $this->b2[0];
        for ($j = 0; $j < $hiddenSize; $j++) {
            $out += $h[$j] * $this->W2[$j][0];
        }

        return tanh($out);
    }

    public function getModelType(): string
    {
        return $this->modelType;
    }

    /**
     * @return list<float>
     */
    public function getWeights(): array
    {
        return $this->weights;
    }

    /**
     * @param list<float> $weights
     */
    public function setWeights(array $weights): void
    {
        // ⭐ Druha cesta, kterou se vahy dostanou dovnitr -- musi projit tymz
        //   srovnanim delky jako nacteni ze souboru, jinak by `dotProduct`
        //   zase tise usekaval (viz `1b26717a`).
        $weights = self::normalizeWeights(array_values(array_map('floatval', $weights)));
        $this->modelType = 'linear';
        $this->weights = $weights;
    }

    /**
     * Set neural network weights directly.
     *
     * @param list<list<float>> $W1
     * @param list<float> $b1
     * @param list<list<float>> $W2
     * @param list<float> $b2
     */
    public function setNeuralWeights(array $W1, array $b1, array $W2, array $b2): void
    {
        $this->modelType = 'neural';
        $this->W1 = $W1;
        $this->b1 = $b1;
        $this->W2 = $W2;
        $this->b2 = $b2;
    }

    /**
     * Build a scored action: picks the best target and evaluates resulting hypothetical state.
     *
     * @return array{action: ActionType, params: array<string, mixed>, score: float}|null
     */
    private function buildScoredAction(
        GameState $state,
        RulesEngine $rules,
        ActionType $type,
        int $playerId,
        TeamSide $side,
        float $baseScore,
    ): ?array {
        // ⛔⛔ NOSIC NEBLOKUJE A NEBLITZUJE (uzivatel 12.09.: "nosic nesmi
        //   blitzovat ani jit vedle soupere -- na blitz mame mit lepsi
        //   kandidaty a asistenty"). Blok i blitz ho postavi k souperi,
        //   riskuji jeho srazeni s micem a jeste utrati tymovy blitz,
        //   ktery ma udelat nekdo s asistenci.
        //   Naměřeno v `CagePlaybookTest`: nosic blitzoval uprostred
        //   sestavovani klece.
        $ball = $state->getBall();
        $jeNosic = $ball->isHeld() && $ball->getCarrierId() === $playerId;
        if ($jeNosic && in_array($type, [
            ActionType::BLOCK, ActionType::BLITZ, ActionType::MULTIPLE_BLOCK,
            ActionType::FOUL,
        ], true)) {
            return null;
        }

        // ⛔⛔ A UVNITR STOJICI KLECE SE MIC NEPREDAVA. `CagePlaybookTest`
        //   ukazal, ze kouc klec poctive postavil a pak ji jednim `hand_off`
        //   rozbil: nosicem se stal rohovy hrac a rohy jsou kolem nej jinde.
        //   Mimo klec prihravka smysl ma, proto se zakazuje jen tehdy,
        //   kdyz klec opravdu stoji.
        if ($jeNosic && in_array($type, [
            ActionType::HAND_OFF, ActionType::PASS, ActionType::THROW_TEAM_MATE,
        ], true)) {
            $mojePos = $state->getPlayer($playerId)?->getPosition();
            if ($mojePos !== null
                && count($this->cornerPlayers($state, $side, $mojePos, $playerId)) >= self::CAGE_MIN_CORNERS) {
                return null;
            }
        }

        return match ($type) {
            ActionType::MOVE => $this->buildMoveAction($state, $rules, $playerId, $side),
            ActionType::BLOCK => $this->buildBlockAction($state, $rules, $playerId, $side, $baseScore),
            ActionType::BLITZ => $this->buildBlitzAction($state, $rules, $playerId, $side, $baseScore),
            ActionType::PASS => $this->buildPassAction($state, $rules, $playerId, $side, $baseScore),
            ActionType::HAND_OFF => $this->buildHandOffAction($state, $rules, $playerId, $side, $baseScore),
            ActionType::FOUL => $this->buildFoulAction($state, $rules, $playerId, $side, $baseScore),
            ActionType::BALL_AND_CHAIN => $this->buildBallAndChainAction($baseScore, $playerId),
            ActionType::HYPNOTIC_GAZE => $this->buildHypnoticGazeAction($state, $rules, $playerId, $side, $baseScore),
            ActionType::BOMB_THROW => $this->buildBombThrowAction($state, $rules, $playerId, $side, $baseScore),
            ActionType::MULTIPLE_BLOCK => $this->buildMultipleBlockAction($state, $rules, $playerId, $side, $baseScore),
            default => null,
        };
    }

    /**
     * @return array{action: ActionType, params: array<string, mixed>, score: float}|null
     */
    private function buildMoveAction(GameState $state, RulesEngine $rules, int $playerId, TeamSide $side): ?array
    {
        $targets = $rules->getValidMoveTargets($state, $playerId);
        if ($targets === []) {
            return null;
        }

        $player = $state->getPlayer($playerId);
        if ($player === null) {
            return null;
        }

        // Quick heuristic scoring without full feature extraction per target
        // Use the endzone direction and ball-related logic
        $ball = $state->getBall();
        $isCarrier = CoachHeuristics::jeNosic($state, $playerId);
        $endZoneX = CoachHeuristics::endZoneX($side);
        $currentPos = $player->getPosition();

        $bestScore = -PHP_FLOAT_MAX;
        $bestTarget = $targets[0];

        foreach ($targets as $target) {
            $score = 0.0;
            // ⛔ Riziko podle SKUTECNE sance, ne podle poctu hodu -- viz
            //   konstanty vys. Bez `successChance` se spadne na 100 %,
            //   tedy na "bez rizika", coz je bezpecny vychozi stav.
            $sance = max(0, min(100, (int) ($target['successChance'] ?? 100)));
            $riskPenalty = (1 - $sance / 100)
                * ($isCarrier ? self::CENA_TURNOVERU_MIC : self::CENA_TURNOVERU);
            if ($sance < self::RISK_REFUSE_BELOW) {
                // Pod hranici uz to neni vahani, ale zakaz.
                $riskPenalty += $isCarrier
                    ? self::RISK_REFUSE_PENALTY_CARRIER
                    : self::RISK_REFUSE_PENALTY;
            }

            // Touchdown: carrier reaching endzone
            if ($isCarrier) {
                $pos = new Position($target['x'], $target['y']);
                if ($pos->isInEndZone($side !== TeamSide::HOME)) {
                    $tdScore = $this->shouldStall($state, $side) ? 1.0 : 10.0;
                    $score = $tdScore - $riskPenalty;
                    if ($score > $bestScore) {
                        $bestScore = $score;
                        $bestTarget = $target;
                    }
                    continue;
                }
            }

            // Pick up ball
            if (!$ball->isHeld() && $ball->isOnPitch()) {
                $ballPos = $ball->getPosition();
                if ($ballPos !== null && $target['x'] === $ballPos->getX() && $target['y'] === $ballPos->getY()) {
                    $score = 5.0 - $riskPenalty;

                    // ⭐⭐ NEJVYSSI PRIORITA: mic lezi volne a NIKDO SOUPERUV
                    //   u nej nestoji. Pak se pro nej jde vzdycky, ne jen
                    //   v prvnim kole -- nikdo o nej nesouperi a tym z nej ma
                    //   cely zbytek tahu.
                    $uMiceStojiSouper = false;
                    foreach ($state->getPlayersOnPitch($side->opponent()) as $nepritel) {
                        if ($nepritel->getState() !== PlayerState::STANDING) {
                            continue;
                        }
                        $np = $nepritel->getPosition();
                        if ($np !== null && $np->distanceTo($ballPos) <= 1) {
                            $uMiceStojiSouper = true;
                            break;
                        }
                    }
                    // ⛔⛔ OPRAVA 13.09. (simplify): TADY BYLY DVA ROZPORY.
                    //   (a) "prah 50 %" u zvedani NEDELAL NIC: zaklad 5,0
                    //       + bonus 4,0 za nekryty mic proti pokute nejvyse
                    //       4,0 znamena, ze i pri sanci 10 % vyslo skore
                    //       kladne -- zvednuti neslo odmitnout NIKDY.
                    //   (b) RIZIKA SE NEKOMBINOVALA: cesta k mici a hod na
                    //       zvednuti se odecitaly jako dve nezavisle pokuty,
                    //       prestoze turnover nastane, kdyz selze KTERAKOLI
                    //       z nich: 60 % x 60 % = 36 %, tedy 64 % turnover.
                    $sanceZvednuti = $this->pickupChance($state, $player, $ballPos);
                    $sanceCestyKMici = max(0, min(100, (int) ($target['successChance'] ?? 100))) / 100;
                    $sanceCelkem = $sanceCestyKMici * $sanceZvednuti;

                    // ⛔ Tvrde odmitnuti pod prahem -- stejne jako u dodge.
                    //   `riskPenalty` uz je v `$score` zapocitany za cestu,
                    //   proto se tu odecita jen zbytek za samotny hod.
                    if ($sanceCelkem < self::RISK_REFUSE_BELOW / 100) {
                        continue;
                    }
                    $score -= (1 - $sanceZvednuti) * self::PICKUP_FAIL_WEIGHT;

                    if (!$uMiceStojiSouper) {
                        $score += self::PICKUP_UNCONTESTED_BONUS;

                        // ⭐ A jde pro nej ten, kdo ho udrzi: runner nebo
                        //   thrower (nebo kdokoli se Sure Hands).
                        $pojmenovani = strtolower($player->getPositionalName());
                        if ($player->hasSkill(SkillName::SureHands)
                            || str_contains($pojmenovani, 'runner')
                            || str_contains($pojmenovani, 'thrower')) {
                            $score += self::PICKUP_SPECIALIST_BONUS;
                        }
                    }
                    if ($score > $bestScore) {
                        $bestScore = $score;
                        $bestTarget = $target;
                    }
                    continue;
                }
            }

            // Forward advancement
            $distToEndZone = abs($target['x'] - $endZoneX);
            if ($currentPos !== null) {
                $currentDist = abs($currentPos->getX() - $endZoneX);
                $advancement = $currentDist - $distToEndZone;
                $postup = $advancement * 0.1;
                // ⛔ Hrac bez mice, kdyz mic drzi NAS tym: tlumit zavod dopredu,
                //   jinak prebije skladani klece (mereni 12.09., viz konstanta).
                if (!$isCarrier && $ball->isHeld() && $ball->getCarrierId() !== null) {
                    $nosic = $state->getPlayer($ball->getCarrierId());
                    if ($nosic !== null && $nosic->getTeamSide() === $side) {
                        $postup *= self::ADVANCE_DAMP_WITH_CAGE;
                    }
                }
                $score = $postup - $riskPenalty;
            }

            // Defensive positioning: penalize moving to sideline
            $targetY = $target['y'];
            if ($targetY === 0 || $targetY === 14) {
                $score -= 0.2;
            } elseif ($targetY === 1 || $targetY === 13) {
                $score -= 0.05;
            }

            // Carrier movement strategy
            if ($isCarrier) {
                if ($this->shouldStall($state, $side)) {
                    // ⭐⭐ UZIVATEL 12.09.: "kdyz je klec s nosicem vepredu tak,
                    //   ze PRVNI DVA ROHY JSOU V TD ZONE, tak se pocka na
                    //   posledni kolo a v tom nosic dojde dat TD."
                    //   Predni rohy jsou o jedno pole blize zone nez nosic,
                    //   takze ta pozice je `distToEndZone === 1`. Tam se ceka.
                    if ($distToEndZone === 1) {
                        $score += 2.5;
                    } elseif ($distToEndZone >= 2 && $distToEndZone <= 5) {
                        $score += 1.5;
                    }
                    // Centrality bonus (Y=7 is center of 0-14 pitch)
                    $centralityScore = 1.0 - abs($target['y'] - 7) / 7.0;
                    $score += $centralityScore * 0.3;
                } else {
                    // Normal: move closer to endzone
                    $score += (26 - $distToEndZone) * 0.05;
                }

                // ⛔⛔ PHP33 (B) -- POHYB CELE KLECE, NE UTEK NOSICE.
                //   Uzivatel 12.09.: "pohyb cele klece je jako mala cast pred
                //   celotahem." Kouc rozhoduje HRAC PO HRACI a nema kde drzet
                //   zamer na cele kolo, takze plan formace se sem napsat neda.
                //   Da se ale zaridit to podstatne: NOSIC NESMI KLECI UTECT.
                //
                //   Rohy klece jsou vzdy diagonala od nosice, takze kdyz se
                //   nosic posune o JEDNO pole, kazdy roh ho dozene taky jednim
                //   krokem a tvar zustane. Pri dvou a vice polich uz ne --
                //   klec zustane vzadu a nosic stoji sam.
                //
                //   ⇒ Pokuta roste se vzdalenosti, ale POUZE kdyz klec vubec
                //   existuje (aspon dva rohy). Bez klece se nosic pohybuje
                //   jako driv.
                // ⭐⭐ MISTO PRO CELOU KLEC, ne jen pro nosice: kolik rohu
                //   ciloveho pole by bylo CISTYCH (volne nebo nase pole bez
                //   stojiciho soupere v sousedstvi)?
                $cilKlec = new Position($target['x'], $target['y']);
                foreach ([[-1, -1], [1, -1], [-1, 1], [1, 1]] as [$dx, $dy]) {
                    $roh = new Position($cilKlec->getX() + $dx, $cilKlec->getY() + $dy);
                    if (!$roh->isOnPitch()) {
                        continue;
                    }
                    $volny = true;
                    foreach ($state->getPlayersOnPitch($side->opponent()) as $nepr) {
                        $npp = $nepr->getPosition();
                        if ($npp === null) {
                            continue;
                        }
                        // roh obsazeny souperem, nebo souper stojici vedle nej
                        if ($npp->equals($roh)
                            || ($nepr->getState() === PlayerState::STANDING
                                && $npp->distanceTo($roh) === 1)) {
                            $volny = false;
                            break;
                        }
                    }
                    if ($volny) {
                        $score += self::CARRIER_CLEAN_CORNER_BONUS;
                    }
                }

                // ⛔⛔ NOSIC NESMI SKONCIT VEDLE SOUPERE (uzivatel 12.09.).
                //   Pocitaji se jen STOJICI souperi -- lezici zonu zachyceni
                //   nemaji, takze vedle nich je to bezpecne.
                $cil = new Position($target['x'], $target['y']);
                foreach ($state->getPlayersOnPitch($side->opponent()) as $nepritel) {
                    if ($nepritel->getState() !== PlayerState::STANDING) {
                        continue;
                    }
                    $np = $nepritel->getPosition();
                    if ($np !== null && $np->distanceTo($cil) === 1) {
                        $score -= self::CARRIER_NEXT_TO_ENEMY_PENALTY;
                        break;
                    }
                }

                // ⭐⭐ Kdyz uz je na dobehnuti natesno, klec se ROZPOUSTI:
                //   zadny strop, nosic bezi sam. Jinak by cela klec dojela
                //   pred zonu az ve chvili, kdy uz nezbyva kolo na TD.
                $vyrazitSam = $this->musiVyrazitSam($state, $side, $player);

                // ⭐⭐ UZIVATEL 12.09.: "kdyz jsou souperi daleko, ma prednost
                //   beh s micem kupredu co nejrychleji -- cil je dosahnout
                //   s micem TD ... cokoliv nam pomuze se posunout BEZ OHROZENI,
                //   se hodi." ⇒ Kdyz na cilove pole nedosahne ani nejblizsi
                //   stojici souper (jeho MA + 2 GFI), neni pred cim klec drzet.
                if ($currentPos !== null && !$vyrazitSam
                    && !$this->souperNedosahne($state, $side, new Position($target['x'], $target['y']))) {
                    $rohovi = $this->cornerPlayers($state, $side, $currentPos, $playerId);
                    // ⛔⛔ NAPRED SESTAVIT, PAK HNOUT (uzivatel 12.09.).
                    //   Kdyz klec JESTE nestoji, nosic nema kam spechat:
                    //   `CagePlaybookTest` ukazal, ze vyrazil o SEST poli
                    //   a posadka pak skladala klec kolem mista, kam dobehl.
                    //   Bez klece je tedy strop JEDNO pole, s kleci strop
                    //   nejpomalejsiho z ni.
                    if (count($rohovi) < self::CAGE_MIN_CORNERS) {
                        // ⛔ Cekat ma smysl jen tehdy, kdyz MA KDO prijit.
                        //   Nosic bez spoluhracu na hristi neni klec, kterou
                        //   by bylo mozne sestavit -- ten at bezi.
                        $posadka = 0;
                        foreach ($state->getPlayersOnPitch($side) as $spolu) {
                            if ($spolu->getId() !== $playerId) {
                                $posadka++;
                            }
                        }
                        if ($posadka >= self::CAGE_MIN_CORNERS) {
                            $krok = max(abs($target['x'] - $currentPos->getX()),
                                        abs($target['y'] - $currentPos->getY()));
                            if ($krok > 1) {
                                $score -= ($krok - 1) * self::CAGE_OUTRUN_PENALTY;
                            }
                        }
                    }
                    if (count($rohovi) >= self::CAGE_MIN_CORNERS) {
                        // ⭐ UPRESNENO UZIVATELEM 12.09.: "max pohyb klece podle
                        //   NEJMENSIHO MA ze vsech peti." Klec se posouva cela,
                        //   takze dal, nez ujde nejpomalejsi z ni, jit nemuze --
                        //   jinak nekdo zustane vzadu a roh zustane prazdny.
                        //   Do ted tu byl natvrdo jeden krok, coz nejpomalejsiho
                        //   hrace ignorovalo a klec zbytecne brzdilo.
                        $limit = $this->slowestRemaining($rohovi);
                        $krok = max(abs($target['x'] - $currentPos->getX()),
                                    abs($target['y'] - $currentPos->getY()));
                        if ($krok > $limit + self::CAGE_GFI_REACH) {
                            // Tam uz nejpomalejsi nedojde ani na GFI.
                            $score -= ($krok - $limit) * self::CAGE_OUTRUN_PENALTY;
                        } elseif ($krok > $limit) {
                            // Dojde, ale pres GFI => riziko, ne zakaz.
                            $score -= ($krok - $limit) * self::CAGE_GFI_PENALTY;
                        }
                    }
                }
            }

            // ⭐⭐ PRIVEST ASISTENCI: stoupnout si k souperi, ktereho uz nekdo
            //   nas ma vedle sebe. Tim se druhemu hraci zlepsi kostky.
            //   ⛔ Nosic sem nesmi (resi se vys) a hrac, ktery UZ DRZI roh
            //   klece, se taky neodvolava -- ma vlastni ukol.
            if (!$isCarrier) {
                $cilA = new Position($target['x'], $target['y']);
                $drziRoh = false;
                $carrierPosA = $this->ownCarrierPosition($state, $side);
                if ($carrierPosA !== null && $currentPos !== null) {
                    $drziRoh = self::isCageCorner($carrierPosA, $currentPos);
                }
                if (!$drziRoh) {
                    foreach ($state->getPlayersOnPitch($side->opponent()) as $souperA) {
                        if ($souperA->getState() !== PlayerState::STANDING) {
                            continue;
                        }
                        $spA = $souperA->getPosition();
                        if ($spA === null || $spA->distanceTo($cilA) !== 1) {
                            continue;
                        }
                        // Ma uz nekdo nas toho soupere vedle sebe?
                        foreach ($state->getPlayersOnPitch($side) as $spolu) {
                            if ($spolu->getId() === $playerId
                                || $spolu->getState() !== PlayerState::STANDING) {
                                continue;
                            }
                            $pp = $spolu->getPosition();
                            if ($pp !== null && $pp->distanceTo($spA) === 1) {
                                // ⛔ OPRAVA 13.09. (review): asistence se
                                //   NEZAPOCITA, kdyz asistujici stoji v zone
                                //   JINEHO soupere (bez Guard). Bonus se tedy
                                //   platil i za vtazeni do dvojiteho oznaceni
                                //   za nula kostek navic.
                                $jinySouperVedle = false;
                                foreach ($state->getPlayersOnPitch($side->opponent()) as $jiny) {
                                    if ($jiny->getId() === $souperA->getId()
                                        || $jiny->getState() !== PlayerState::STANDING) {
                                        continue;
                                    }
                                    $jp = $jiny->getPosition();
                                    if ($jp !== null && $jp->distanceTo($cilA) === 1) {
                                        $jinySouperVedle = true;
                                        break;
                                    }
                                }
                                if (!$jinySouperVedle) {
                                    $score += self::ASSIST_BONUS;
                                    break 2;
                                }
                            }
                        }
                    }
                }
            }

            // ⭐ KLEC (PHP33) -- ROH ANO, HRANA SKORO NE.
            //   Do 12.09.2026 tu stalo `distanceTo($pos) === 1`, tedy +1.0 za
            //   JAKEKOLI sousedni pole. Z toho vznika HVEZDA kolem nosice,
            //   ne klec: ortogonalni soused nosice nechrani -- souper na nej
            //   dosahne stejne, a jeste tim pole zabere sam sobe.
            //   Uzivatel 12.09.: klec = nosic + ctyri DIAGONALNI rohy.
            if (!$isCarrier && $ball->isHeld() && $ball->getCarrierId() !== null) {
                $carrier = $state->getPlayer($ball->getCarrierId());
                if ($carrier !== null && $carrier->getTeamSide() === $side && $carrier->getPosition() !== null) {
                    $carrierPos = $carrier->getPosition();
                    $pos = new Position($target['x'], $target['y']);
                    if (self::isCageCorner($carrierPos, $pos)) {
                        $score += self::CAGE_CORNER_BONUS;
                        // ⚠️ Roh se souperem vedle je spinavy -- drzi se hur
                        //   a souper na nej dosahne bez blitzu.
                        foreach ($state->getPlayersOnPitch($side->opponent()) as $nepritel) {
                            if ($nepritel->getState() !== PlayerState::STANDING) {
                                continue;
                            }
                            $np = $nepritel->getPosition();
                            if ($np !== null && $np->distanceTo($pos) === 1) {
                                $score -= self::CAGE_DIRTY_HARD_PENALTY;
                                break;
                            }
                        }
                    } else {
                        // ⭐ Cim bliz k nejblizsimu rohu, tim lip -- aby se
                        //   posadka klece scházela i pres vic kol. Na roh
                        //   (vzdalenost 0) se sem uz nedojde, ten ma vetev vys.
                        $doRohu = PHP_INT_MAX;
                        foreach ([[-1, -1], [1, -1], [-1, 1], [1, 1]] as [$dx, $dy]) {
                            $roh = new Position($carrierPos->getX() + $dx, $carrierPos->getY() + $dy);
                            if (!$roh->isOnPitch()) {
                                continue;
                            }
                            $doRohu = min($doRohu, $roh->distanceTo($pos));
                        }
                        if ($doRohu !== PHP_INT_MAX) {
                            $score += self::CAGE_APPROACH_BONUS / (1 + $doRohu);
                        }
                    }
                }
            }

            // ⭐ PORADI AKTIVACI -- NEJDRIV BEZ RIZIKA (uzivatel 12.09.).
            //   Turnover na konci kola stoji min nez na zacatku: co uz je
            //   odehrane, to se neztrati. Bezpecne tahy tedy patri DOPREDU.
            //   Pocita se cil BEZ jedineho hodu -- ani dodge, ani GFI.
            if ($target['dodges'] === 0 && $target['gfis'] === 0) {
                $score += self::RISK_FREE_BONUS;
            }

            if ($score > $bestScore) {
                $bestScore = $score;
                $bestTarget = $target;
            }
        }

        // Add base state evaluation to the score
        $baseScore = $this->evaluateState($state, $side);

        return [
            'action' => ActionType::MOVE,
            'params' => ['playerId' => $playerId, 'x' => $bestTarget['x'], 'y' => $bestTarget['y']],
            'score' => $baseScore + $bestScore,
        ];
    }

    /**
     * @return array{action: ActionType, params: array<string, mixed>, score: float}|null
     */
    private function buildBlockAction(GameState $state, RulesEngine $rules, int $playerId, TeamSide $side, float $baseScore): ?array
    {
        $player = $state->getPlayer($playerId);
        if ($player === null) {
            return null;
        }

        $targets = $rules->getBlockTargets($state, $player);
        if ($targets === []) {
            return null;
        }

        $bestScore = -PHP_FLOAT_MAX;
        $bestTarget = $targets[0];
        $ball = $state->getBall();

        foreach ($targets as $target) {
            $score = $baseScore + $this->oceneniBloku($state, $player, $target);

            if ($ball->isHeld() && $ball->getCarrierId() === $target->getId()) {
                $score += 0.5;
            }

            // Sideline surfing bonus
            $targetPos = $target->getPosition();
            if ($targetPos !== null) {
                $ty = $targetPos->getY();
                if ($ty === 0 || $ty === 14) {
                    $score += 0.3;
                } elseif ($ty === 1 || $ty === 13) {
                    $score += 0.1;
                }
            }

            if ($score > $bestScore) {
                $bestScore = $score;
                $bestTarget = $target;
            }
        }

        // ⭐ Pojistka z rana uz tu NENI potreba: `oceneniBloku` od opravy
        //   "blok na nosice" vraci vzdycky `float` (nebezpecny blok dostane
        //   zapornou hodnotu misto `null`), takze smycka skore vzdy nastavi.
        //   V `buildBlitzAction` tataz pojistka ZUSTAVA -- tam se `continue`
        //   dela pro lezici cile a pro nedosazitelne.
        return [
            'action' => ActionType::BLOCK,
            'params' => ['playerId' => $playerId, 'targetId' => $bestTarget->getId()],
            'score' => $bestScore,
        ];
    }

    /**
     * @return array{action: ActionType, params: array<string, mixed>, score: float}|null
     */
    private function buildBlitzAction(GameState $state, RulesEngine $rules, int $playerId, TeamSide $side, float $baseScore): ?array
    {
        $player = $state->getPlayer($playerId);
        if ($player === null) {
            return null;
        }

        $enemies = $state->getPlayersOnPitch($side->opponent());
        if ($enemies === []) {
            return null;
        }

        $ball = $state->getBall();
        $bestScore = -PHP_FLOAT_MAX;
        $bestTarget = $enemies[0];
        // ⭐ VYKON (review): pathfinder je pro daneho hrace a stav TOTOZNY pro
        //   vsechny cile -- volal se ale pro kazdeho soupere zvlast (zmereno
        //   ~10x prepocet, 122 ms proti 12,8 ms). Spocita se JEDNOU.
        $moznaPole = $rules->getValidMoveTargets($state, $playerId);

        foreach ($enemies as $enemy) {
            // ⛔ OPRAVA 13.09. (review): lezici a omracene cile se preskakuji.
            //   `oceneniBloku` je ohodnotilo jako "1 kostka, vybiram ja",
            //   takze blizky omraceny souper byl atraktivnejsi nez vzdaleny
            //   stojici -- a jednorazovy blitz se utratil za NELEGALNI blok.
            if ($enemy->getState() !== PlayerState::STANDING) {
                continue;
            }

            // ⛔ Blitz je TYZ blok, jen s rozbehem -- oceni se stejne,
            //   a nevyhodny dostane zapornou hodnotu. Navic je blitz JEDEN ZA KOLO,
            //   takze ho utratit za spatne kostky je drazsi nez u bloku.
            $ocena = $this->oceneniBloku($state, $player, $enemy);

            // ⛔⛔ RIZIKO CESTY K CILI (13.09.2026). Blitz je blok S ROZBEHEM,
            //   jenze `buildBlitzAction` vybira SOUPERE, ne policko -- o ceste
            //   k nemu tedy nevedel nic. Kouc si vybral krasny trikostkovy
            //   blitz pres tri zony zachyceni a cestou spadl.
            // ⭐ ZMERENO: `blitz` mel 32,0 % turnoveru na akci, kdezto `block`
            //   jen 8,8 % -- ctyrikrat vic, a rozdil je prave ta cesta.
            $sanceCesty = $this->sanceDojitK($state, $rules, $playerId, $enemy, $moznaPole);
            if ($sanceCesty === null) {
                continue;   // nedojde vubec
            }
            $riziko = (1 - $sanceCesty) * self::CENA_TURNOVERU;
            if ($sanceCesty < 0.5) {
                $riziko += self::RISK_REFUSE_PENALTY;
            }

            $score = $baseScore + $ocena - $riziko;
            if ($ball->isHeld() && $ball->getCarrierId() === $enemy->getId()) {
                $score += 0.8;
            }

            // Sideline surfing bonus (higher for blitz — it's a scarce resource)
            $enemyPos = $enemy->getPosition();
            if ($enemyPos !== null) {
                $ey = $enemyPos->getY();
                if ($ey === 0 || $ey === 14) {
                    $score += 0.4;
                } elseif ($ey === 1 || $ey === 13) {
                    $score += 0.15;
                }
                // End zone edges
                $ex = $enemyPos->getX();
                if ($ex === 0 || $ex === 25) {
                    $score += 0.2;
                }
            }

            if ($score > $bestScore) {
                $bestScore = $score;
                $bestTarget = $enemy;
            }
        }

        // ⛔⛔ OPRAVA 13.09. (review): totez co u bloku -- bez ohodnoceni
        //   se vracel `$enemies[0]`, tedy klidne nedosazitelny souper.
        //   `BlitzHandler` uz na nedosazitelnou deklaraci nehazi vyjimku
        //   (oprava PHP3), takze se jednorazovy blitz utratil za prochazku.
        if ($bestScore === -PHP_FLOAT_MAX) {
            return null;
        }

        return [
            'action' => ActionType::BLITZ,
            'params' => ['playerId' => $playerId, 'targetId' => $bestTarget->getId()],
            'score' => $bestScore,
        ];
    }

    /**
     * @return array{action: ActionType, params: array<string, mixed>, score: float}|null
     */
    private function buildPassAction(GameState $state, RulesEngine $rules, int $playerId, TeamSide $side, float $baseScore): ?array
    {
        $player = $state->getPlayer($playerId);
        if ($player === null) {
            return null;
        }

        $targets = $rules->getPassTargets($state, $player);
        if ($targets === []) {
            return null;
        }

        $bestScore = -PHP_FLOAT_MAX;
        $bestTarget = $targets[0];

        // ⛔⛔⛔ OPRAVA 13.09.2026: TADY SE HAZELO DO PRAZDNA.
        //   `getPassTargets` vraci VSECHNA pole v dosahu -- v beznem stavu
        //   kolem 359 poli -- a kouc z nich vybiral JEN podle vzdalenosti.
        //   Vetsina z nich je prazdna, takze prihravka nikoho nenasla
        //   a nedochycena prihravka je TURNOVER.
        // ⭐ ZMERENO PRED OPRAVOU: `pass` = 21 zahrani, 21 turnoveru, tedy
        //   100,0 % (`evidence/endturn_turnover_na_akci_20260913.txt`).
        // ⚠️ A druha vada v teze metode: klice vzdalenosti byly 'short'/'long',
        //   zatimco engine vraci 'short_pass'/'long_pass' -- `match` tedy
        //   NIKDY nesedl a vsechny cile mely tutez pokutu.
        $endZoneX = CoachHeuristics::endZoneX($side);
        $naslo = false;

        foreach ($targets as $target) {
            // Stoji na tom poli NAS hrac, ktery muze chytat?
            $prijemce = null;
            foreach ($state->getPlayersOnPitch($side) as $spolu) {
                $pp = $spolu->getPosition();
                if ($pp !== null && $pp->getX() === $target['x'] && $pp->getY() === $target['y']
                    && $spolu->getId() !== $playerId
                    && $spolu->getState() === PlayerState::STANDING) {
                    $prijemce = $spolu;
                    break;
                }
            }
            if ($prijemce === null) {
                continue;   // ⛔ do prazdna se nehazi
            }
            $naslo = true;

            $rangePenalty = match ($target['range']) {
                'quick_pass' => 0.0,
                'short_pass' => 0.2,
                'long_pass'  => 0.6,
                default      => 1.0,   // long bomb a cokoli neznameho
            };

            // Prihravka ma smysl, kdyz prijemce je BLIZ koncove zone nez hazec.
            $ziskPole = abs(($player->getPosition()?->getX() ?? $target['x']) - $endZoneX)
                - abs($target['x'] - $endZoneX);

            // ⛔ Sance, ze to prijemce CHYTI -- vcetne zon zachyceni kolem nej.
            //   +1 je modifikator za PRESNOU prihravku.
            $sanceChyceni = $this->catchChance($state, $prijemce, 1);
            $score = $baseScore - $rangePenalty + $ziskPole * 0.1
                - (1 - $sanceChyceni) * self::CATCH_FAIL_WEIGHT;

            if ($score > $bestScore) {
                $bestScore = $score;
                $bestTarget = $target;
            }
        }

        if (!$naslo) {
            return null;   // nikdo k nahrani => prihravka se nenabizi
        }

        return [
            'action' => ActionType::PASS,
            'params' => ['playerId' => $playerId, 'targetX' => $bestTarget['x'], 'targetY' => $bestTarget['y']],
            'score' => $bestScore,
        ];
    }

    /**
     * @return array{action: ActionType, params: array<string, mixed>, score: float}|null
     */
    private function buildHandOffAction(GameState $state, RulesEngine $rules, int $playerId, TeamSide $side, float $baseScore): ?array
    {
        $player = $state->getPlayer($playerId);
        if ($player === null) {
            return null;
        }

        $targets = $rules->getHandOffTargets($state, $player);
        if ($targets === []) {
            return null;
        }

        // ⛔⛔ OPRAVA 13.09.2026: TADY SE NEVYBIRALO VUBEC -- bral se
        //   `$targets[0]`, tedy prvni v seznamu. Uzivatel: "hand off zalezi
        //   na AG prijemce -- nepredavat min agilnim, dokud neni nouze."
        //   Chycene predani je hod na obratnost prijemce; predat mic hraci
        //   s AG 2 znamena ~33% sanci, ze mic spadne -- a to je TURNOVER.
        //   ⭐ ZMERENO PRED OPRAVOU: `hand_off` 4 zahrani, 2 turnovery (50 %).
        $mojeAG = $player->getStats()->getAgility();
        $endZoneX = CoachHeuristics::endZoneX($side);
        $mojeVzdalenost = $player->getPosition() !== null
            ? abs($player->getPosition()->getX() - $endZoneX)
            : 0;

        $best = null;
        $bestScore = -PHP_FLOAT_MAX;
        foreach ($targets as $kandidat) {
            $ag = $kandidat->getStats()->getAgility();
            $pos = $kandidat->getPosition();
            if ($pos === null) {
                continue;
            }

            // ⛔ Min agilnimu se nepredava, dokud neni nouze.
            if ($ag < $mojeAG) {
                continue;
            }

            // ⭐ OPRAVENO 13.09.: puvodni vzorec pocital JEN z agility
            //   a ignoroval ZONY ZACHYCENI kolem prijemce -- pritom predat mic
            //   hraci, ktery ma vedle sebe dva soupere, je uplne jina sance.
            //   `getCatchTarget()` zna oboji, plus Extra Arms a spol.
            // ⛔ OPRAVA 13.09. (review): `HandOffHandler:127` chyta
            //   s modifikatorem +1, kouc pocital s 0 -- u volneho AG 3 tedy
            //   50 % misto skutecnych 67 %, coz pri vaze 4.0 delalo falesnou
            //   pokutu -0,67 a dobra predani se zamitala. Odtud "hand_off:
            //   0 zahrani" v mereni 13.09.
            $sance = $this->catchChance($state, $kandidat, 1);
            $ziskPole = $mojeVzdalenost - abs($pos->getX() - $endZoneX);

            $score = $baseScore + $ziskPole * 0.1
                - (1 - $sance) * self::CATCH_FAIL_WEIGHT;
            if ($score > $bestScore) {
                $bestScore = $score;
                $best = $kandidat;
            }
        }

        if ($best === null) {
            return null;   // vsichni sousedi jsou min agilni => nepredavat
        }

        return [
            'action' => ActionType::HAND_OFF,
            'params' => ['playerId' => $playerId, 'targetId' => $best->getId()],
            'score' => $bestScore,
        ];
    }

    /**
     * @return array{action: ActionType, params: array<string, mixed>, score: float}|null
     */
    private function buildFoulAction(GameState $state, RulesEngine $rules, int $playerId, TeamSide $side, float $baseScore): ?array
    {
        $player = $state->getPlayer($playerId);
        if ($player === null) {
            return null;
        }

        $targets = $rules->getFoulTargets($state, $player);
        if ($targets === []) {
            return null;
        }

        $bestTarget = $targets[0];
        $lowestArmour = $bestTarget->getStats()->getArmour();
        foreach ($targets as $target) {
            if ($target->getStats()->getArmour() < $lowestArmour) {
                $lowestArmour = $target->getStats()->getArmour();
                $bestTarget = $target;
            }
        }

        return [
            'action' => ActionType::FOUL,
            'params' => ['playerId' => $playerId, 'targetId' => $bestTarget->getId()],
            'score' => $baseScore - 0.02,
        ];
    }

    /**
     * Ball & Chain is the only action B&C players can take — must always be used.
     *
     * @return array{action: ActionType, params: array<string, mixed>, score: float}
     */
    private function buildBallAndChainAction(float $baseScore, int $playerId): array
    {
        // ⛔ OPRAVA 11.09.2026 -- viz `GreedyAICoach::scoreBallAndChain`.
        //   Chybejici `playerId` => `getPlayer(0)` => 'Player not found'.
        return [
            'action' => ActionType::BALL_AND_CHAIN,
            'params' => ['playerId' => $playerId],
            'score' => $baseScore + 0.1,
        ];
    }

    /**
     * @return array{action: ActionType, params: array<string, mixed>, score: float}|null
     */
    private function buildHypnoticGazeAction(GameState $state, RulesEngine $rules, int $playerId, TeamSide $side, float $baseScore): ?array
    {
        $player = $state->getPlayer($playerId);
        if ($player === null) {
            return null;
        }

        $targets = $rules->getHypnoticGazeTargets($state, $player);
        if ($targets === []) {
            return null;
        }

        $ball = $state->getBall();
        $bestScore = -PHP_FLOAT_MAX;
        $bestTarget = $targets[0];

        foreach ($targets as $target) {
            $score = $baseScore + 0.05;

            // High priority: gaze the ball carrier
            if ($ball->isHeld() && $ball->getCarrierId() === $target->getId()) {
                $score += 0.4;
            }

            // Bonus for gazing strong players
            if ($target->getStats()->getStrength() >= 4) {
                $score += 0.1;
            }

            if ($score > $bestScore) {
                $bestScore = $score;
                $bestTarget = $target;
            }
        }

        return [
            'action' => ActionType::HYPNOTIC_GAZE,
            'params' => ['playerId' => $playerId, 'targetId' => $bestTarget->getId()],
            'score' => $bestScore,
        ];
    }

    /**
     * @return array{action: ActionType, params: array<string, mixed>, score: float}|null
     */
    private function buildBombThrowAction(GameState $state, RulesEngine $rules, int $playerId, TeamSide $side, float $baseScore): ?array
    {
        $player = $state->getPlayer($playerId);
        if ($player === null || !$player->hasSkill(SkillName::Bombardier)) {
            return null;
        }

        $targets = $rules->getPassTargets($state, $player);
        if ($targets === []) {
            return null;
        }

        $enemies = $state->getPlayersOnPitch($side->opponent());
        $myPlayers = $state->getPlayersOnPitch($side);

        $bestScore = -PHP_FLOAT_MAX;
        $bestTarget = null;

        foreach ($targets as $target) {
            $tx = $target['x'];
            $ty = $target['y'];

            $enemyCount = 0;
            $allyCount = 0;

            foreach ($enemies as $enemy) {
                $ePos = $enemy->getPosition();
                if ($ePos !== null && abs($ePos->getX() - $tx) <= 1 && abs($ePos->getY() - $ty) <= 1) {
                    $enemyCount++;
                }
            }

            foreach ($myPlayers as $ally) {
                $aPos = $ally->getPosition();
                if ($aPos !== null && abs($aPos->getX() - $tx) <= 1 && abs($aPos->getY() - $ty) <= 1) {
                    $allyCount++;
                }
            }

            if ($enemyCount === 0) {
                continue;
            }

            $score = $baseScore + $enemyCount * 0.08 - $allyCount * 0.06;

            if ($score > $bestScore) {
                $bestScore = $score;
                $bestTarget = $target;
            }
        }

        if ($bestTarget === null) {
            return null;
        }

        return [
            'action' => ActionType::BOMB_THROW,
            'params' => ['playerId' => $playerId, 'targetX' => $bestTarget['x'], 'targetY' => $bestTarget['y']],
            'score' => $bestScore,
        ];
    }

    /**
     * @return array{action: ActionType, params: array<string, mixed>, score: float}|null
     */
    private function buildMultipleBlockAction(GameState $state, RulesEngine $rules, int $playerId, TeamSide $side, float $baseScore): ?array
    {
        $player = $state->getPlayer($playerId);
        if ($player === null) {
            return null;
        }

        $targets = $rules->getBlockTargets($state, $player);
        if (count($targets) < 2) {
            return null;
        }

        $ball = $state->getBall();
        $bestPairScore = -PHP_FLOAT_MAX;
        $bestT1 = $targets[0];
        $bestT2 = $targets[1];

        // Evaluate all pairs (small count so O(n^2) is fine)
        for ($i = 0; $i < count($targets); $i++) {
            for ($j = $i + 1; $j < count($targets); $j++) {
                $score = $baseScore + 0.03;

                // Ball carrier bonus
                if ($ball->isHeld()) {
                    if ($ball->getCarrierId() === $targets[$i]->getId()) {
                        $score += 0.4;
                    }
                    if ($ball->getCarrierId() === $targets[$j]->getId()) {
                        $score += 0.4;
                    }
                }

                if ($score > $bestPairScore) {
                    $bestPairScore = $score;
                    $bestT1 = $targets[$i];
                    $bestT2 = $targets[$j];
                }
            }
        }

        return [
            'action' => ActionType::MULTIPLE_BLOCK,
            'params' => [
                'playerId' => $playerId,
                'targetId' => $bestT1->getId(),
                'targetId2' => $bestT2->getId(),
            ],
            'score' => $bestPairScore,
        ];
    }

    /**
     * Should the AI stall (hold the ball) rather than score immediately?
     */
    private function shouldStall(GameState $state, TeamSide $side): bool
    {
        $myTeam = $state->getTeamState($side);
        $oppTeam = $state->getTeamState($side->opponent());
        $scoreDiff = $myTeam->getScore() - $oppTeam->getScore();
        $turnsLeft = max(0, 9 - $myTeam->getTurnNumber());

        // Behind: never stall
        if ($scoreDiff < 0) {
            return false;
        }

        // Last 2 turns: always score
        if ($turnsLeft <= 2) {
            return false;
        }

        // Tied with 3 or fewer turns: score
        if ($scoreDiff === 0 && $turnsLeft <= 3) {
            return false;
        }

        // Ahead or tied with plenty of time: stall
        return true;
    }

    /**
     * @param list<float> $a
     * @param list<float> $b
     */
    /**
     * Srovná vektor vah na `NUM_FEATURES`.
     *
     * ⭐ PROČ JE DOPLNĚNÍ NULAMI BEZPEČNÉ: tři příznaky, o které jde
     *   (`70-72`, loose-ball field position), byly PŘIDÁNY NA KONEC
     *   (`30539d65`, `NUM_FEATURES` 70 -> 73), ne vloženy doprostřed.
     *   Indexy 0-69 tedy pořád znamenají totéž, co když se váhy trénovaly.
     *   Doplněná nula = "tenhle příznak zatím nemá váhu", ne posun.
     *   ⛔ Kdyby se někdy příznak vložil DOPROSTŘED, tohle by přestalo platit
     *   a váhy by se musely přetrénovat -- doplnit nulami by je rozházelo.
     *
     * ⛔ Dřív to dělal `min()` v `dotProduct` potichu. Teď se to děje na
     *   jednom místě a je to otestované.
     *
     * @param list<float> $w
     * @return list<float>
     */
    private static function normalizeWeights(array $w): array
    {
        // Jeden vyraz pokryva kratsi, delsi i presnou delku.
        return array_slice(
            array_pad($w, FeatureExtractor::NUM_FEATURES, 0.0),
            0,
            FeatureExtractor::NUM_FEATURES,
        );
    }

    private static function dotProduct(array $a, array $b): float
    {
        $sum = 0.0;
        $len = min(count($a), count($b));
        for ($i = 0; $i < $len; $i++) {
            $sum += $a[$i] * $b[$i];
        }
        return $sum;
    }
}
