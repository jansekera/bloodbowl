# Web App Code Review — Blood Bowl (PHP backend + TS frontend)

**Reviewer:** Team-1-style full-code review (Opus)
**Date:** 2026-06-12
**Scope:** `src/` (PHP), `public/`, `templates/`, `migrations/`, `config.php`, `frontend/`. C++ AI engine excluded.

## Verdict

The **game-rule engine is solid**: every action handler and `RulesEngine::validate*` consistently checks that the acting player belongs to `getActiveTeam()`, money is integer-based and guarded, and **the entire SQL layer is parameterized** (named placeholders everywhere; the one dynamic `IN (...)` uses positional `?`). Twig autoescaping is on (no `|raw`), so no stored/reflected XSS found.

**However, the authorization layer is broken.** The server authenticates *that you are logged in* but almost never checks *that you own the thing you are acting on*. The engine equates "active team's side" with "whoever sent the HTTP request" — the requester's coach identity is never compared to the side they control. This makes several CRITICAL/HIGH server-trust violations. There is also **no CSRF protection anywhere**. These dominate the findings; correctness of the rules themselves is good.

Phase 1-6 web layer is otherwise complete and coherent; no large Phase-7 stubs in the web layer (Phase 7 is AI/training, out of scope).

---

## CRITICAL

### C1. No turn/match ownership check on `submitAction` — any logged-in coach can play another coach's match (and another coach's turn)
**File:** `src/Controller/MatchApiController.php:68-100`, `src/Service/MatchService.php:222-259`
The only check is `requireAuth()`. The authenticated coach's id is **never passed to** `submitAction` and never compared to `matches.home_coach_id` / `away_coach_id` nor to the active side. The engine only verifies the *game state's* active team, not the requester.
**Failure scenario:** In a human-vs-human match, when it is coach B's turn, coach A `POST /api/v1/matches/{id}/actions` and plays B's entire turn (move/block/score with B's players). Or any logged-in stranger drives any match to completion, corrupting results and SPP awards.
**Fix:** Plumb `$coach->getId()` into `submitAction`; load the match, map the coach to HOME/AWAY via `home_coach_id`/`away_coach_id`, and reject if `mappedSide !== $state->getActiveTeam()` (and reject non-participants outright). Enforce in the service, not the controller.

### C2. `createMatch` does not verify the coach owns the teams being matched
**File:** `src/Controller/MatchApiController.php:20-48`, `src/Controller/MatchPageController.php:35-55`, `src/Service/MatchService.php:60-86`
`createMatch(homeTeamId, awayTeamId, coachId)` looks up the teams but never checks `team.coachId === coachId`. `away_coach_id` is left `null`.
**Failure scenario:** A coach `POST /api/v1/matches` with `home_team_id`/`away_team_id` pointing at *other coaches'* teams, starting matches with rosters they do not own (and, combined with C1, then plays both sides / both teams).
**Fix:** Require the creator to own at least the home team; resolve `away_coach_id` from the away team's owner; reject teams the caller may not use.

### C3. IDOR on player advancement — advance/inspect any player by id
**File:** `src/Controller/TeamApiController.php:149-187`, `src/Service/TeamService.php:215-318`
`getAvailableSkills` and `advancePlayer` only call `requireAuth()`. Neither the controller nor `TeamService::advancePlayer` checks that the player's team belongs to the requesting coach (unlike the team-scoped endpoints, which do check ownership).
**Failure scenario:** Logged-in coach A calls `POST /api/v1/players/{B_playerId}/advance {skill_id}` and assigns skills to coach B's players (griefing / sabotage), or enumerates `available-skills` for any player.
**Fix:** Load the player → its team → assert `team.coachId === coach.getId()` before reading or mutating.

---

## HIGH

### H1. All match *read* endpoints are completely unauthenticated
**File:** `src/Controller/MatchApiController.php:50-66,102-215` (`getState`, `getValidMoves`, `getAvailableActions`, `getBlockTargets`, `getPassTargets`, `getHandOffTargets`, `getFoulTargets`, `getEvents`) and `public/index.php:116-151`
None of these call `requireAuth()` or check participation.
**Failure scenario:** Any anonymous client reads the full game state (positions, ball, hidden setup, score) and complete event log of *any* match by incrementing the id (`GET /api/v1/matches/1/state`, `/2/state`, ...). Trivial scraping / opponent-intel leak.
**Fix:** Require auth and participant check on all match read endpoints (or an explicit "spectator" allowlist if intended).

### H2. No CSRF protection on any state-changing endpoint
**File:** all POST/DELETE routes in `public/index.php`; page forms in `src/Controller/TeamPageController.php`, `PageController.php`, `MatchPageController.php`
Auth is session-cookie based (`AuthService` `$_SESSION`), cookies are auto-sent same-origin, and there are **no CSRF tokens** and **no `SameSite` cookie attribute** (`session_start()` called with no params, `src/Service/AuthService.php:17`).
**Failure scenario:** An attacker page auto-submits `<form method=POST action="https://victim/teams/{id}/retire">` (or buy-reroll, hire, fire); the victim's browser sends the session cookie → action executes. The HTML page-routes use `$_POST` form data and are fully forgeable cross-site. (The JSON API routes require `Content-Type: application/json`, which gives partial protection via preflight, but the page routes do not.)
**Fix:** Add per-session CSRF tokens to all forms/state-changing requests and verify them; set the session cookie `SameSite=Lax` (or `Strict`) + `HttpOnly` + `Secure` via `session_set_cookie_params()`.

### H3. TOCTOU / last-write-wins on concurrent game actions and treasury spends (no transactions / locking)
**File:** `src/Service/MatchService.php:222-240` (read `game_state` → resolve → write back, no row lock), `src/Service/TeamService.php:108-132,147-170,173-198` (read treasury → write player + write treasury as separate statements)
No `BEGIN/COMMIT`, no `SELECT ... FOR UPDATE`, no optimistic-version column.
**Failure scenario (game):** Two concurrent `submitAction` requests both load the same `game_state`, both resolve against it, and the second write clobbers the first → duplicated/lost actions, corrupted state. **Failure scenario (money):** Two concurrent hire/buy requests both read the same treasury (e.g. 50k), both pass the funds check, both deduct → the team spends more gold than it has / ends with an inconsistent player+treasury pair if one write fails mid-way (`hirePlayer` inserts the player and *then* deducts treasury in a separate statement — a crash between them leaves a free player).
**Fix:** Wrap each money mutation (insert player + deduct treasury) in a single DB transaction; for match actions use a transaction with `SELECT game_state ... FOR UPDATE` (or an optimistic `version` column the client/state must match) so concurrent submits serialize.

---

## MEDIUM

### M1. `requireAuth()` throws an uncaught `\RuntimeException` → 500 instead of 401
**File:** `src/Service/AuthService.php:66-74`; callers `MatchApiController::submitAction:70`, `TeamApiController` methods.
`submitAction` catches only `ValidationException`/`NotFoundException`/`InvalidArgumentException`. An unauthenticated POST yields an uncaught exception (HTTP 500, possibly a stack trace if `display_errors` is on) rather than a clean 401.
**Fix:** Introduce an `UnauthorizedException` mapped to 401 in a central handler, or have `requireAuth` set 401 and exit.

### M2. No password / email validation on registration
**File:** `src/Service/AuthService.php:21-33`, `src/Controller/PageController.php:69-84`
Empty or 1-char passwords are accepted (`password_hash('')` succeeds); no email format check; no max length (DoS via huge bcrypt input is bounded by bcrypt's 72-byte truncation but the request body isn't size-checked).
**Fix:** Enforce min length (e.g. ≥8), validate email with `filter_var(..., FILTER_VALIDATE_EMAIL)`, bound input length.

### M3. CORS `Access-Control-Allow-Origin: *` on the API
**File:** `public/index.php:29-38`
Wildcard origin on every `/api/v1/*` response. Because the frontend uses session cookies and does **not** send `credentials: 'include'`, browsers won't expose credentialed responses cross-origin, so this is not directly a session-theft vector today — but it invites a future `credentials`/`Allow-Credentials` change that would be exploitable, and it lets any site read the (currently unauthenticated, see H1) match data.
**Fix:** Drop the wildcard; reflect a configured allowlist of trusted origins, and never combine `*` with credentials.

### M4. Username/team/player names: no XSS in Twig (good) but verify any client-side `innerHTML`
**File:** `frontend/src/ui/*` (e.g. GameLog, PlayerCard, Toast)
Server output is Twig-escaped, but match player names / event descriptions are returned via JSON and rendered by the canvas/UI layer. Confirm the TS UI uses `textContent` (not `innerHTML`) for server-supplied strings (names, event text) to avoid DOM XSS. (Not confirmed vulnerable in this pass — flagged for follow-up.)
**Fix:** Audit UI components that render `event.description` / player names for `innerHTML` sinks.

---

## LOW / NOTES

- **L1 — Mass-assignment surface is limited but present:** repositories take associative arrays (`save([...])`) populated explicitly by services, not directly from request bodies, so no raw mass-assignment. Controllers cast each field individually (`(int)`, `(string)`) — good.
- **L2 — `firePlayer`/`retireTeam` swallow `NotFoundException` silently** (`TeamPageController:131,189`) — harmless but hides errors.
- **L3 — `getState` page (`MatchPageController::show`)** renders any match to any logged-in (or anonymous) viewer with no participant check; same class as H1 for the HTML page. Tighten alongside H1.
- **Positive:** SQL fully parameterized; integer money math with negative-treasury guard (`ValueObject/Treasury.php`); engine rule validation rigorously checks active-team/side ownership of acted-upon players; Twig autoescape on, no `|raw`. No secrets committed (DB password from env, default empty).

---

## Priority order to fix
1. **C1** (play others' turns) — core trust violation.
2. **C2 / C3** (create matches with / advance others' players) — IDOR.
3. **H1** (unauth match reads), **H2** (CSRF), **H3** (transactions / concurrency).
4. M1-M4, then L-items.
