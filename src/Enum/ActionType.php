<?php
declare(strict_types=1);

namespace App\Enum;

enum ActionType: string
{
    case MOVE = 'move';
    case BLOCK = 'block';
    case BLITZ = 'blitz';
    case PASS = 'pass';
    case HAND_OFF = 'hand_off';
    case FOUL = 'foul';
    case THROW_TEAM_MATE = 'throw_team_mate';
    case BOMB_THROW = 'bomb_throw';
    case HYPNOTIC_GAZE = 'hypnotic_gaze';
    case BALL_AND_CHAIN = 'ball_and_chain';
    case MULTIPLE_BLOCK = 'multiple_block';
    case END_TURN = 'end_turn';
    case SETUP_PLAYER = 'setup_player';
    case END_SETUP = 'end_setup';

    public function requiresPlayer(): bool
    {
        return match ($this) {
            self::END_TURN, self::END_SETUP => false,
            default => true,
        };
    }

    public function isOncePerTurn(): bool
    {
        return match ($this) {
            self::BLITZ, self::PASS, self::FOUL, self::THROW_TEAM_MATE, self::BOMB_THROW, self::HYPNOTIC_GAZE => true,
            default => false,
        };
    }
}
