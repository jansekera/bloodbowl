<?php

declare(strict_types=1);

namespace App\Engine;

use App\Enum\TeamSide;
use App\ValueObject\Position;

final class ScatterCalculator
{
    /** @var array<int, array{int, int}> D8 direction offsets: 1=N, 2=NE, 3=E, 4=SE, 5=S, 6=SW, 7=W, 8=NW */
    private const DIRECTION_OFFSETS = [
        1 => [0, -1],
        2 => [1, -1],
        3 => [1, 0],
        4 => [1, 1],
        5 => [0, 1],
        6 => [-1, 1],
        7 => [-1, 0],
        8 => [-1, -1],
    ];

    /**
     * Scatter one square in a D8 direction (for bounce).
     */
    public function scatterOnce(Position $from, int $d8Direction): Position
    {
        [$dx, $dy] = self::DIRECTION_OFFSETS[$d8Direction];
        return new Position($from->getX() + $dx, $from->getY() + $dy);
    }

    /**
     * Scatter D8 direction × D6 distance -- jen vykop a throw-in (r. 271-273); nepresna prihravka se rozptyluje 3× po jednom poli.
     */
    public function scatterWithDistance(Position $from, int $d8Direction, int $d6Distance): Position
    {
        [$dx, $dy] = self::DIRECTION_OFFSETS[$d8Direction];
        return new Position(
            $from->getX() + $dx * $d6Distance,
            $from->getY() + $dy * $d6Distance,
        );
    }

    /**
     * Sablona Throw-in (rules_bb2016 r. 868-871; tvar sablony LRB6, shodne s C++ `ball_handler.cpp:129`).
     * U strany hod D6: 1-2 diagonala, 3-4 kolmo zpet do hriste, 5-6 druha diagonala.
     * V rohu D3 (= (D6+1)/2): podel jedne hrany, diagonala do hriste, podel druhe hrany.
     * Strana se urci z pole, kam mic vyletel; kdyz je to pole na hristi (volajici ho nezna),
     * z hrany, u ktere lezi posledni pole na hristi.
     *
     * @return array{int, int} [dx, dy]
     */
    public function throwInOffset(Position $lastOnPitch, Position $offPitchExit, int $d6): array
    {
        $ref = $offPitchExit->isOnPitch() ? $lastOnPitch : $offPitchExit;
        $maxX = Position::PITCH_WIDTH - 1;
        $maxY = Position::PITCH_HEIGHT - 1;
        $left = $offPitchExit->isOnPitch() ? $ref->getX() <= 0 : $ref->getX() < 0;
        $right = $offPitchExit->isOnPitch() ? $ref->getX() >= $maxX : $ref->getX() > $maxX;
        $top = $offPitchExit->isOnPitch() ? $ref->getY() <= 0 : $ref->getY() < 0;
        $bottom = $offPitchExit->isOnPitch() ? $ref->getY() >= $maxY : $ref->getY() > $maxY;

        // do hriste: +1 od leve/horni hrany, -1 od prave/dolni
        $inX = $left ? 1 : ($right ? -1 : 0);
        $inY = $top ? 1 : ($bottom ? -1 : 0);

        if ($inX !== 0 && $inY !== 0) {
            $d3 = intdiv($d6 + 1, 2);
            return match ($d3) {
                1 => [$inX, 0],
                2 => [$inX, $inY],
                default => [0, $inY],
            };
        }

        if ($inY !== 0) {
            // horni / dolni hrana: diagonaly vlevo a vpravo
            return $d6 <= 2 ? [-1, $inY] : ($d6 <= 4 ? [0, $inY] : [1, $inY]);
        }

        // leva / prava hrana (a nouzove, kdyz hrana nejde urcit, kolmo od leve)
        $inX = $inX === 0 ? 1 : $inX;
        return $d6 <= 2 ? [$inX, -1] : ($d6 <= 4 ? [$inX, 0] : [$inX, 1]);
    }

    /**
     * Sablona Throw-in natocena smerem `$smer` ([dx, dy], jeden ze ctyr: podel hriste
     * nebo k lajne). D6: 1-2 jedna diagonala, 3-4 rovne, 5-6 druha diagonala --
     * stejny tvar jako u vhazovani. Pouziva Ball & Chain (`rules_bb2016.txt` r. 7829-7833).
     *
     * @param array{int, int} $smer
     * @return array{int, int} [dx, dy]
     */
    public function templateOffset(array $smer, int $d6): array
    {
        [$dx, $dy] = $smer;
        $bok = $d6 <= 2 ? -1 : ($d6 <= 4 ? 0 : 1);

        return $dx !== 0 ? [$dx, $bok] : [$bok, $dy];
    }

    /**
     * @return array{int, int} [dx, dy]
     */
    public function getDirectionOffset(int $d8Direction): array
    {
        return self::DIRECTION_OFFSETS[$d8Direction];
    }

    /**
     * Check if position is in the receiving team's half.
     * Home receives: x 0..12. Away receives: x 13..25.
     */
    public function isInReceivingHalf(Position $pos, TeamSide $receivingTeam): bool
    {
        if (!$pos->isOnPitch()) {
            return false;
        }

        return $receivingTeam === TeamSide::HOME
            ? $pos->getX() <= 12
            : $pos->getX() >= 13;
    }
}
