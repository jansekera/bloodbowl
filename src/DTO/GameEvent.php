<?php
declare(strict_types=1);

namespace App\DTO;

final class GameEvent
{
    /**
     * @param array<string, mixed> $data
     */
    public function __construct(
        private readonly string $type,
        private readonly string $description,
        private readonly array $data = [],
    ) {
    }

    public static function playerMove(int $playerId, string $from, string $to): self
    {
        return new self('player_move', "Player moved from {$from} to {$to}", [
            'playerId' => $playerId,
            'from' => $from,
            'to' => $to,
        ]);
    }

    public static function dodgeAttempt(int $playerId, int $target, int $roll, bool $success): self
    {
        $result = $success ? 'succeeded' : 'failed';
        return new self('dodge', "Dodge {$result} (needed {$target}+, rolled {$roll})", [
            'playerId' => $playerId,
            'target' => $target,
            'roll' => $roll,
            'success' => $success,
        ]);
    }

    public static function gfiAttempt(int $playerId, int $roll, bool $success): self
    {
        $result = $success ? 'succeeded' : 'failed';
        return new self('gfi', "GFI {$result} (needed 2+, rolled {$roll})", [
            'playerId' => $playerId,
            'roll' => $roll,
            'success' => $success,
        ]);
    }

    public static function turnover(string $reason): self
    {
        return new self('turnover', "Turnover! {$reason}", [
            'reason' => $reason,
        ]);
    }

    public static function endTurn(string $teamName): self
    {
        return new self('end_turn', "{$teamName} ends their turn", []);
    }

    public static function playerFell(int $playerId): self
    {
        return new self('player_fell', "Player fell!", [
            'playerId' => $playerId,
        ]);
    }

    /**
     * @param list<string> $faces
     */
    public static function blockAttempt(
        int $attackerId,
        int $defenderId,
        int $diceCount,
        bool $attackerChooses,
        array $faces,
        string $chosenFace,
    ): self {
        $chooser = $attackerChooses ? 'Attacker' : 'Defender';
        return new self('block', "{$chooser} chose {$chosenFace} from [" . implode(', ', $faces) . "]", [
            'attackerId' => $attackerId,
            'defenderId' => $defenderId,
            'diceCount' => $diceCount,
            'attackerChooses' => $attackerChooses,
            'faces' => $faces,
            'chosen' => $chosenFace,
        ]);
    }

    public static function playerPushed(int $playerId, string $from, string $to): self
    {
        return new self('push', "Player pushed from {$from} to {$to}", [
            'playerId' => $playerId,
            'from' => $from,
            'to' => $to,
        ]);
    }

    public static function crowdSurf(int $playerId): self
    {
        return new self('crowd_surf', "Player surfed into the crowd!", [
            'playerId' => $playerId,
        ]);
    }

    public static function armourRoll(int $playerId, int $roll, int $modifier, int $armourValue, bool $broken): self
    {
        $result = $broken ? 'broken' : 'held';
        $modStr = $modifier > 0 ? " +{$modifier}" : '';
        return new self('armour_roll', "Armour roll: {$roll}{$modStr} vs AV{$armourValue} - {$result}", [
            'playerId' => $playerId,
            'roll' => $roll,
            'modifier' => $modifier,
            'armourValue' => $armourValue,
            'broken' => $broken,
        ]);
    }

    public static function injuryRoll(int $playerId, int $roll, int $modifier, string $result): self
    {
        $modStr = $modifier > 0 ? " +{$modifier}" : '';
        return new self('injury_roll', "Injury roll: {$roll}{$modStr} - {$result}", [
            'playerId' => $playerId,
            'roll' => $roll,
            'modifier' => $modifier,
            'result' => $result,
        ]);
    }

    public static function followUp(int $playerId, string $from, string $to): self
    {
        return new self('follow_up', "Player followed up from {$from} to {$to}", [
            'playerId' => $playerId,
            'from' => $from,
            'to' => $to,
        ]);
    }

    public static function ballPickup(int $playerId, int $target, int $roll, bool $success): self
    {
        $result = $success ? 'succeeded' : 'failed';
        return new self('pickup', "Pickup {$result} (needed {$target}+, rolled {$roll})", [
            'playerId' => $playerId,
            'target' => $target,
            'roll' => $roll,
            'success' => $success,
        ]);
    }

    public static function ballBounce(string $from, string $to, int $direction): self
    {
        return new self('ball_bounce', "Ball bounced from {$from} to {$to}", [
            'from' => $from,
            'to' => $to,
            'direction' => $direction,
        ]);
    }

    public static function passAttempt(int $playerId, string $from, string $to, string $range, int $target, int $roll, string $result): self
    {
        return new self('pass', "Pass {$result} ({$range}, needed {$target}+, rolled {$roll})", [
            'playerId' => $playerId,
            'from' => $from,
            'to' => $to,
            'range' => $range,
            'target' => $target,
            'roll' => $roll,
            'result' => $result,
        ]);
    }

    public static function catchAttempt(int $playerId, int $target, int $roll, bool $success): self
    {
        $result = $success ? 'succeeded' : 'failed';
        return new self('catch', "Catch {$result} (needed {$target}+, rolled {$roll})", [
            'playerId' => $playerId,
            'target' => $target,
            'roll' => $roll,
            'success' => $success,
        ]);
    }

    public static function handOff(int $giverId, int $receiverId): self
    {
        return new self('hand_off', "Hand-off attempted", [
            'giverId' => $giverId,
            'receiverId' => $receiverId,
        ]);
    }

    public static function interception(int $playerId, int $target, int $roll, bool $success): self
    {
        $result = $success ? 'intercepted' : 'failed';
        return new self('interception', "Interception {$result} (needed {$target}+, rolled {$roll})", [
            'playerId' => $playerId,
            'target' => $target,
            'roll' => $roll,
            'success' => $success,
        ]);
    }

    public static function touchdown(int $playerId, string $teamName): self
    {
        return new self('touchdown', "TOUCHDOWN! {$teamName} scores!", [
            'playerId' => $playerId,
            'teamName' => $teamName,
        ]);
    }

    public static function kickoff(string $kickTo, string $landedAt): self
    {
        return new self('kickoff', "Kickoff aimed at {$kickTo}, landed at {$landedAt}", [
            'kickTo' => $kickTo,
            'landedAt' => $landedAt,
        ]);
    }

    public static function touchback(int $playerId, string $teamName): self
    {
        return new self('touchback', "Touchback! Ball given to {$teamName}", [
            'playerId' => $playerId,
            'teamName' => $teamName,
        ]);
    }

    public static function throwIn(string $from, string $to, int $direction, int $distance): self
    {
        return new self('throw_in', "Throw-in from {$from} to {$to}", [
            'from' => $from,
            'to' => $to,
            'direction' => $direction,
            'distance' => $distance,
        ]);
    }

    public static function halfTime(int $half): self
    {
        return new self('half_time', "Half {$half} ended", [
            'half' => $half,
        ]);
    }

    public static function gameOver(string $homeTeam, int $homeScore, string $awayTeam, int $awayScore): self
    {
        return new self('game_over', "Game Over! {$homeTeam} {$homeScore} - {$awayScore} {$awayTeam}", [
            'homeTeam' => $homeTeam,
            'homeScore' => $homeScore,
            'awayTeam' => $awayTeam,
            'awayScore' => $awayScore,
        ]);
    }

    public static function foulAttempt(
        int $attackerId,
        int $targetId,
        int $die1,
        int $die2,
        int $armourValue,
        bool $armourBroken,
    ): self {
        $total = $die1 + $die2 + 1;
        $result = $armourBroken ? 'broken' : 'held';
        return new self('foul', "Foul: {$total} (dice {$die1}+{$die2}+1) vs AV{$armourValue} - armour {$result}", [
            'attackerId' => $attackerId,
            'targetId' => $targetId,
            'die1' => $die1,
            'die2' => $die2,
            'total' => $total,
            'armourValue' => $armourValue,
            'armourBroken' => $armourBroken,
            'doubles' => $die1 === $die2,
        ]);
    }

    public static function playerEjected(int $playerId): self
    {
        return new self('ejection', "Player ejected by referee!", [
            'playerId' => $playerId,
        ]);
    }

    public static function frenzyBlock(int $attackerId, int $targetId): self
    {
        return new self('frenzy', 'Frenzy! Second block!', [
            'attackerId' => $attackerId,
            'targetId' => $targetId,
        ]);
    }

    public static function ballStripped(int $playerId): self
    {
        return new self('strip_ball', 'Ball stripped from carrier!', [
            'playerId' => $playerId,
        ]);
    }

    public static function standUp(int $playerId, ?int $roll = null, bool $success = true): self
    {
        if ($roll !== null) {
            $result = $success ? 'succeeded' : 'failed';
            return new self('stand_up', "Stand up {$result} (needed 4+, rolled {$roll})", [
                'playerId' => $playerId,
                'roll' => $roll,
                'success' => $success,
            ]);
        }
        return new self('stand_up', "Player stood up", [
            'playerId' => $playerId,
            'success' => true,
        ]);
    }

    public static function rerollUsed(int $playerId, string $source): self
    {
        return new self('reroll', "Reroll used: {$source}", [
            'playerId' => $playerId,
            'source' => $source,
        ]);
    }

    public static function koRecovery(int $playerId, int $roll, bool $success): self
    {
        $result = $success ? 'recovered' : 'stayed KO';
        return new self('ko_recovery', "KO recovery: {$result} (rolled {$roll})", [
            'playerId' => $playerId,
            'roll' => $roll,
            'success' => $success,
        ]);
    }

    public static function apothecaryUsed(int $playerId, string $originalResult, string $newResult): self
    {
        return new self('apothecary', "Apothecary used: {$originalResult} → {$newResult}", [
            'playerId' => $playerId,
            'originalResult' => $originalResult,
            'newResult' => $newResult,
        ]);
    }

    public static function weatherChange(string $from, string $to): self
    {
        return new self('weather_change', "Weather changed from {$from} to {$to}", [
            'from' => $from,
            'to' => $to,
        ]);
    }

    public static function swelteringHeat(int $playerId, string $playerName, string $teamName): self
    {
        return new self('sweltering_heat', "{$playerName} ({$teamName}) collapses from the heat!", [
            'playerId' => $playerId,
            'playerName' => $playerName,
            'teamName' => $teamName,
        ]);
    }

    public static function kickoffTableEvent(int $roll, string $eventName, string $effect): self
    {
        return new self('kickoff_table', "Kickoff: {$eventName} (rolled {$roll}) - {$effect}", [
            'roll' => $roll,
            'event' => $eventName,
            'effect' => $effect,
        ]);
    }

    public static function boneHeadFail(int $playerId, int $roll): self
    {
        return new self('bone_head', "Bone-head! Player loses action (rolled {$roll})", [
            'playerId' => $playerId,
            'roll' => $roll,
            'success' => false,
        ]);
    }

    public static function boneHeadPass(int $playerId, int $roll): self
    {
        return new self('bone_head', "Bone-head check passed (rolled {$roll})", [
            'playerId' => $playerId,
            'roll' => $roll,
            'success' => true,
        ]);
    }

    public static function reallyStupidFail(int $playerId, int $roll, bool $hasAdjacentAlly): self
    {
        $needed = $hasAdjacentAlly ? '2+' : '4+';
        return new self('really_stupid', "Really Stupid! Failed (needed {$needed}, rolled {$roll})", [
            'playerId' => $playerId,
            'roll' => $roll,
            'success' => false,
            'hasAdjacentAlly' => $hasAdjacentAlly,
        ]);
    }

    public static function reallyStupidPass(int $playerId, int $roll, bool $hasAdjacentAlly): self
    {
        $needed = $hasAdjacentAlly ? '2+' : '4+';
        return new self('really_stupid', "Really Stupid check passed (needed {$needed}, rolled {$roll})", [
            'playerId' => $playerId,
            'roll' => $roll,
            'success' => true,
            'hasAdjacentAlly' => $hasAdjacentAlly,
        ]);
    }

    public static function wildAnimalFail(int $playerId, int $roll): self
    {
        return new self('wild_animal', "Wild Animal! Player loses action (rolled {$roll})", [
            'playerId' => $playerId,
            'roll' => $roll,
            'success' => false,
        ]);
    }

    public static function wildAnimalPass(int $playerId, int $roll): self
    {
        return new self('wild_animal', "Wild Animal check passed (rolled {$roll})", [
            'playerId' => $playerId,
            'roll' => $roll,
            'success' => true,
        ]);
    }

    public static function lonerCheck(int $playerId, int $roll, bool $success): self
    {
        $result = $success ? 'passed' : 'failed';
        return new self('loner', "Loner check {$result} (needed 4+, rolled {$roll})", [
            'playerId' => $playerId,
            'roll' => $roll,
            'success' => $success,
        ]);
    }

    public static function regeneration(int $playerId, int $roll, bool $success): self
    {
        $result = $success ? 'Regenerated!' : 'Failed to regenerate';
        return new self('regeneration', "{$result} (rolled {$roll})", [
            'playerId' => $playerId,
            'roll' => $roll,
            'success' => $success,
        ]);
    }

    public static function proReroll(int $playerId, int $proRoll, bool $proSuccess, ?int $reroll): self
    {
        if ($proSuccess) {
            return new self('pro', "Pro activated (rolled {$proRoll}), reroll: {$reroll}", [
                'playerId' => $playerId,
                'proRoll' => $proRoll,
                'proSuccess' => true,
                'reroll' => $reroll,
            ]);
        }
        return new self('pro', "Pro failed (rolled {$proRoll})", [
            'playerId' => $playerId,
            'proRoll' => $proRoll,
            'proSuccess' => false,
            'reroll' => null,
        ]);
    }

    public static function wrestle(int $attackerId, int $defenderId): self
    {
        return new self('wrestle', 'Wrestle! Both players go prone without armor rolls', [
            'attackerId' => $attackerId,
            'defenderId' => $defenderId,
        ]);
    }

    public static function tentacles(int $moverId, int $tentaclesPlayerId, int $moverRoll, int $tentRoll, bool $escaped): self
    {
        $result = $escaped ? 'escaped' : 'caught';
        return new self('tentacles', "Tentacles: {$result} (mover {$moverRoll}, tentacles {$tentRoll})", [
            'moverId' => $moverId,
            'tentaclesPlayerId' => $tentaclesPlayerId,
            'moverRoll' => $moverRoll,
            'tentRoll' => $tentRoll,
            'escaped' => $escaped,
        ]);
    }

    public static function juggernaut(int $playerId): self
    {
        return new self('juggernaut', 'Juggernaut! Both Down converted to push', [
            'playerId' => $playerId,
        ]);
    }

    public static function divingTackle(int $playerId): self
    {
        return new self('diving_tackle', 'Diving Tackle! Player goes prone', [
            'playerId' => $playerId,
        ]);
    }

    public static function leap(int $playerId, int $target, int $roll, bool $success): self
    {
        $result = $success ? 'succeeded' : 'failed';
        return new self('leap', "Leap {$result} (needed {$target}+, rolled {$roll})", [
            'playerId' => $playerId,
            'target' => $target,
            'roll' => $roll,
            'success' => $success,
        ]);
    }

    public static function throwTeamMate(int $throwerId, int $targetId, int $roll, string $result): self
    {
        return new self('throw_team_mate', "Throw Team-Mate: {$result} (rolled {$roll})", [
            'throwerId' => $throwerId,
            'targetId' => $targetId,
            'roll' => $roll,
            'result' => $result,
        ]);
    }

    public static function ttmLanding(int $playerId, int $roll, bool $success): self
    {
        $result = $success ? 'landed safely' : 'crashed';
        return new self('ttm_landing', "Landing {$result} (rolled {$roll})", [
            'playerId' => $playerId,
            'roll' => $roll,
            'success' => $success,
        ]);
    }

    public static function safeThrow(int $playerId, int $roll, bool $saved): self
    {
        $result = $saved ? 'saved' : 'failed';
        return new self('safe_throw', "Safe Throw {$result} (rolled {$roll})", [
            'playerId' => $playerId,
            'roll' => $roll,
            'saved' => $saved,
        ]);
    }

    public static function noHands(int $playerId): self
    {
        return new self('no_hands', "No Hands! Player cannot handle the ball", [
            'playerId' => $playerId,
        ]);
    }

    public static function sneakyGit(int $playerId): self
    {
        return new self('sneaky_git', "Sneaky Git! Avoided ejection on doubles", [
            'playerId' => $playerId,
        ]);
    }

    public static function fend(int $playerId): self
    {
        return new self('fend', "Fend! No follow-up allowed", [
            'playerId' => $playerId,
        ]);
    }

    public static function pilingOn(int $attackerId, string $rerollType): self
    {
        return new self('piling_on', "Piling On! Rerolled {$rerollType}, attacker goes prone", [
            'attackerId' => $attackerId,
            'rerollType' => $rerollType,
        ]);
    }

    public static function kickSkill(int $playerId, int $originalDistance, int $reducedDistance): self
    {
        return new self('kick_skill', "Kick! Scatter reduced from {$originalDistance} to {$reducedDistance}", [
            'playerId' => $playerId,
            'originalDistance' => $originalDistance,
            'reducedDistance' => $reducedDistance,
        ]);
    }

    public static function leaderBonus(string $teamName): self
    {
        return new self('leader', "Leader bonus! {$teamName} gets +1 reroll", [
            'teamName' => $teamName,
        ]);
    }

    public static function secretWeaponEjection(int $playerId): self
    {
        return new self('secret_weapon', "Secret Weapon! Player ejected at drive end", [
            'playerId' => $playerId,
        ]);
    }

    public static function takeRoot(int $playerId, int $roll, bool $rooted): self
    {
        $result = $rooted ? 'rooted' : 'free';
        return new self('take_root', "Take Root: {$result} (rolled {$roll})", [
            'playerId' => $playerId,
            'roll' => $roll,
            'rooted' => $rooted,
        ]);
    }

    public static function hailMaryPass(int $playerId, string $from, string $to): self
    {
        return new self('hail_mary_pass', "Hail Mary Pass from {$from} to {$to}!", [
            'playerId' => $playerId,
            'from' => $from,
            'to' => $to,
        ]);
    }

    public static function dumpOff(int $playerId): self
    {
        return new self('dump_off', "Dump-Off! Quick pass before block", [
            'playerId' => $playerId,
        ]);
    }

    public static function divingCatch(int $playerId): self
    {
        return new self('diving_catch', "Diving Catch! Player dives for the ball", [
            'playerId' => $playerId,
        ]);
    }

    public static function stab(int $attackerId, int $defenderId): self
    {
        return new self('stab', "Stab! Bypassing block dice", [
            'attackerId' => $attackerId,
            'defenderId' => $defenderId,
        ]);
    }

    public static function shadowing(int $shadowerId, int $moverId, int $roll, bool $followed): self
    {
        $result = $followed ? 'followed' : 'failed to follow';
        return new self('shadowing', "Shadowing: {$result} (rolled {$roll})", [
            'shadowerId' => $shadowerId,
            'moverId' => $moverId,
            'roll' => $roll,
            'followed' => $followed,
        ]);
    }

    public static function chainPush(int $playerId, string $from, string $to): self
    {
        return new self('chain_push', "Chain push! Player pushed from {$from} to {$to}", [
            'playerId' => $playerId,
            'from' => $from,
            'to' => $to,
        ]);
    }

    public static function bombLanding(string $position): self
    {
        return new self('bomb_landing', "Bomb landed at {$position}!", [
            'position' => $position,
        ]);
    }

    public static function bombExplosion(int $playerId): self
    {
        return new self('bomb_explosion', "Player caught in bomb explosion!", [
            'playerId' => $playerId,
        ]);
    }

    public static function bombThrow(int $playerId, int $roll, string $result): self
    {
        return new self('bomb_throw', "Bomb throw: {$result} (rolled {$roll})", [
            'playerId' => $playerId,
            'roll' => $roll,
            'result' => $result,
        ]);
    }

    public static function bloodlustBite(int $vampireId, int $thrallId, int $roll): self
    {
        return new self('bloodlust_bite', "Bloodlust! Vampire bites a Thrall (rolled {$roll})", [
            'vampireId' => $vampireId,
            'thrallId' => $thrallId,
            'roll' => $roll,
        ]);
    }

    public static function bloodlustFail(int $vampireId, int $roll): self
    {
        return new self('bloodlust_fail', "Bloodlust! No Thrall to bite, vampire sent to reserves (rolled {$roll})", [
            'vampireId' => $vampireId,
            'roll' => $roll,
        ]);
    }

    public static function hypnoticGaze(int $gazerId, int $targetId, int $roll, bool $success): self
    {
        $result = $success ? 'succeeded' : 'failed';
        return new self('hypnotic_gaze', "Hypnotic Gaze {$result} (rolled {$roll})", [
            'gazerId' => $gazerId,
            'targetId' => $targetId,
            'roll' => $roll,
            'success' => $success,
        ]);
    }

    public static function ballAndChainMove(int $playerId, string $from, string $to, int $direction): self
    {
        return new self('ball_and_chain_move', "Ball & Chain moved from {$from} to {$to}", [
            'playerId' => $playerId,
            'from' => $from,
            'to' => $to,
            'direction' => $direction,
        ]);
    }

    public static function ballAndChainBlock(int $playerId, int $targetId): self
    {
        return new self('ball_and_chain_block', "Ball & Chain auto-block!", [
            'playerId' => $playerId,
            'targetId' => $targetId,
        ]);
    }

    public static function chainsaw(int $attackerId, int $defenderId): self
    {
        return new self('chainsaw', "Chainsaw! Bypassing block dice", [
            'attackerId' => $attackerId,
            'defenderId' => $defenderId,
        ]);
    }

    public static function chainsawKickback(int $playerId): self
    {
        return new self('chainsaw_kickback', "Chainsaw kickback! Double 1 — attacker takes armor roll", [
            'playerId' => $playerId,
        ]);
    }

    public static function foulAppearance(int $defenderId, int $attackerId, int $roll, bool $success): self
    {
        $result = $success ? 'overcame' : 'repulsed by';
        return new self('foul_appearance', "Foul Appearance: {$result} (rolled {$roll})", [
            'defenderId' => $defenderId,
            'attackerId' => $attackerId,
            'roll' => $roll,
            'success' => $success,
        ]);
    }

    public static function alwaysHungry(int $throwerId, int $targetId, int $roll, bool $eaten): self
    {
        $result = $eaten ? 'eaten' : 'safe';
        return new self('always_hungry', "Always Hungry: {$result} (rolled {$roll})", [
            'throwerId' => $throwerId,
            'targetId' => $targetId,
            'roll' => $roll,
            'eaten' => $eaten,
        ]);
    }

    public static function animosity(int $passerId, int $receiverId, int $roll, bool $success): self
    {
        $result = $success ? 'passed' : 'refused';
        return new self('animosity', "Animosity: {$result} (rolled {$roll})", [
            'passerId' => $passerId,
            'receiverId' => $receiverId,
            'roll' => $roll,
            'success' => $success,
        ]);
    }

    public static function kickOffReturn(int $playerId, string $from, string $to): self
    {
        return new self('kick_off_return', "Kick-Off Return: moved from {$from} to {$to}", [
            'playerId' => $playerId,
            'from' => $from,
            'to' => $to,
        ]);
    }

    public static function passBlock(int $playerId, string $from, string $to): self
    {
        return new self('pass_block', "Pass Block: moved from {$from} to {$to}", [
            'playerId' => $playerId,
            'from' => $from,
            'to' => $to,
        ]);
    }

    public static function nurglesRot(int $attackerId, int $victimId): self
    {
        return new self('nurgles_rot', "Nurgle's Rot! Victim infected", [
            'attackerId' => $attackerId,
            'victimId' => $victimId,
        ]);
    }

    public static function multipleBlock(int $attackerId, int $targetId1, int $targetId2): self
    {
        return new self('multiple_block', "Multiple Block! Blocking two opponents", [
            'attackerId' => $attackerId,
            'targetId1' => $targetId1,
            'targetId2' => $targetId2,
        ]);
    }

    public static function stakesBlockRegen(int $attackerId, int $victimId): self
    {
        return new self('stakes_block_regen', "Stakes! Regeneration blocked", [
            'attackerId' => $attackerId,
            'victimId' => $victimId,
        ]);
    }

    public function getType(): string { return $this->type; }
    public function getDescription(): string { return $this->description; }
    /** @return array<string, mixed> */
    public function getData(): array { return $this->data; }

    /**
     * @return array<string, mixed>
     */
    public function toArray(): array
    {
        return [
            'type' => $this->type,
            'description' => $this->description,
            'data' => $this->data,
        ];
    }
}
