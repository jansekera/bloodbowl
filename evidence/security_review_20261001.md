# P98 — security review webové aplikace (01.10.2026)

Agent `security-reviewer` (Opus), jen čtení. Riziko **HIGH při vystavení do sítě, LOW dnes** (`php -S localhost:8080`).
SQL všude parametrizované, žádné `exec/eval/unserialize`, tajemství v gitu žádná, `npm audit` 0.

| # | závažnost | nález | kde | dnes reálné? | → kniha |
|---|---|---|---|---|---|
| 1 | HIGH | **uložené XSS**: jména týmů a hráčů do `innerHTML` bez escapování; stránka zápasu bez přihlášení | `frontend/src/ui/ScoreBoard.ts:38-60`, `Tooltip.ts:119`, `ReservesPanel.ts:97`, `MatchSummary.ts:42`, `ActionPanel.ts:~47` | při vystavení | P114 |
| 2 | HIGH | **tah v libovolném zápase za libovolnou stranu** — `submitAction` nekontroluje kouče ani stranu na tahu; `away_coach_id` vždy `null` | `MatchApiController.php:157`, `MatchService.php:222,85` | ano (2+ účty) | P112 |
| 3 | HIGH | **zápas s cizími týmy** — `createMatch` nekontroluje vlastníka ani stav týmu; SPP se pak zapíší do cizích hráčů | `MatchService.php:60-70,423-475`, `MatchApiController.php:116`, `MatchPageController.php:371` | ano | P112 |
| 4 | HIGH | **level-up cizího hráče** a čtení cizích soupisek | `TeamApiController.php:673-711`, `TeamService.php:215,255` | ano | P112 |
| 5 | HIGH | **žádné CSRF**; `GET /logout`; API bere tělo bez ohledu na Content-Type | `index.php:173-228`, `getJsonBody()` | při vystavení | P113 |
| 6 | MED | session fixation (`session_regenerate_id` chybí), slabý logout, cookie bez HttpOnly/SameSite | `AuthService.php:16-48` | při vystavení | P113 |
| 7 | MED | login bez omezení pokusů; enumerace e-mailů; heslo/e-mail nevalidované na serveru | `PageController.php:456`, `AuthService.php:24` | při vystavení | P113 |
| 8 | MED | JS injekce v `confirm('Fire {{ player.name }}?')` | `templates/teams/show.html.twig:108` | self-XSS | P114 |
| 9 | MED | chyby k uživateli (`$e->getMessage()` vč. PDO), nepřihlášený → **500** místo 401, žádný globální handler | `MatchPageController.php:373-379`, `requireAuth()` | **ano** (500) | P115 (+ P99) |
| 10 | MED | **Twig 3.23.0**: 1 critical + 4 high advisories (nedosažitelné — statické šablony) | `composer audit` | ne | P116 |
| 11 | MED | žádné bezpečnostní hlavičky (CSP, frame-ancestors…), `expose_php=On` | `public/` | při vystavení | P114 |
| 12 | LOW | `Access-Control-Allow-Origin: *` na API | `index.php:30-32` | ne | P113 |
| 13 | LOW | zápasy čitelné bez přihlášení, ID po sobě | API `/matches/{id}/…` | záměr? | ❓ uživatel |
| 14 | LOW | DB `postgres` + prázdné heslo | `config.php:8-11` | — | P103 |

## Tabulka cest
Plná tabulka (43 cest, kdo smí volat) je ve výstupu kontroly; souhrn: **bez kontroly vlastníka** jsou
`POST /api/v1/matches`, `POST /api/v1/matches/{id}/actions`, `GET+POST /api/players/{id}/…`, `POST /matches/new`.
Vlastník kontrolován u všech `/teams/{id}/…` a `/api/teams/{id}/…`. Čtení zápasů a číselníků anonymní.
⇒ podklad pro **P107** (test, že každá cesta deklaruje přístup).
