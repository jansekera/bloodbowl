<?php

declare(strict_types=1);

namespace App\Enum;

enum SkillName: string
{
    case Block = 'Block';
    case Catch = 'Catch';
    case Dodge = 'Dodge';
    case Frenzy = 'Frenzy';
    case Guard = 'Guard';
    case MightyBlow = 'Mighty Blow';
    case Pass = 'Pass';
    case SideStep = 'Side Step';
    case StandFirm = 'Stand Firm';
    case StripBall = 'Strip Ball';
    case SureHands = 'Sure Hands';
    case Tackle = 'Tackle';
    // Phase 8 new skills:
    case SureFeet = 'Sure Feet';
    case NervesOfSteel = 'Nerves of Steel';
    case Pro = 'Pro';
    case Regeneration = 'Regeneration';
    case ThickSkull = 'Thick Skull';
    case Horns = 'Horns';
    case Dauntless = 'Dauntless';
    case BigHand = 'Big Hand';

    // Phase 10 new skills:
    case Loner = 'Loner';
    case BoneHead = 'Bone-head';
    case ReallyStupid = 'Really Stupid';
    case WildAnimal = 'Wild Animal';
    case ThrowTeamMate = 'Throw Team-Mate';
    case RightStuff = 'Right Stuff';
    case Stunty = 'Stunty';
    case PrehensileTail = 'Prehensile Tail';
    case TakeRoot = 'Take Root';
    case JumpUp = 'Jump Up';
    case Sprint = 'Sprint';
    case BreakTackle = 'Break Tackle';
    case DirtyPlayer = 'Dirty Player';
    case Juggernaut = 'Juggernaut';
    case NoHands = 'No Hands';
    case SecretWeapon = 'Secret Weapon';

    // Phase 11 new skills:
    case Wrestle = 'Wrestle';
    case Claw = 'Claw';
    case Grab = 'Grab';
    case Tentacles = 'Tentacles';
    case DisturbingPresence = 'Disturbing Presence';
    case DivingTackle = 'Diving Tackle';
    case Leap = 'Leap';

    // Phase 12 new skills:
    case Accurate = 'Accurate';
    case StrongArm = 'Strong Arm';
    case SafeThrow = 'Safe Throw';
    case TwoHeads = 'Two Heads';
    case ExtraArms = 'Extra Arms';
    case SneakyGit = 'Sneaky Git';
    case Fend = 'Fend';
    case PilingOn = 'Piling On';
    case Kick = 'Kick';
    case KickOffReturn = 'Kick-Off Return';
    case Leader = 'Leader';
    case HailMaryPass = 'Hail Mary Pass';
    case DumpOff = 'Dump-Off';
    case DivingCatch = 'Diving Catch';
    case Shadowing = 'Shadowing';
    case Stab = 'Stab';

    // Phase 19 new skills:
    case Bombardier = 'Bombardier';
    case Bloodlust = 'Bloodlust';
    case HypnoticGaze = 'Hypnotic Gaze';
    case BallAndChain = 'Ball & Chain';

    // Phase 20 new skills:
    case Decay = 'Decay';

    // Phase 21 new skills:
    case Chainsaw = 'Chainsaw';
    case FoulAppearance = 'Foul Appearance';
    case AlwaysHungry = 'Always Hungry';
    case VeryLongLegs = 'Very Long Legs';

    // Phase 22 new skills:
    case Animosity = 'Animosity';
    case PassBlock = 'Pass Block';

    // Phase 23 new skills:
    case NurglesRot = "Nurgle's Rot";
    case Titchy = 'Titchy';
    case Stakes = 'Stakes';

    // Phase 24 new skills:
    case MultipleBlock = 'Multiple Block';
}
