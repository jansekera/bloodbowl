import { chromium } from '@playwright/test';

const BASE = 'http://localhost:8000';

async function currentPlayerCount(page, teamId) {
  const resp = await page.request.get(`${BASE}/teams/${teamId}`);
  const body = await resp.text();
  const m = body.match(/Players\s*<\/[^>]*>\s*<[^>]*>\s*(\d+)|Players[^0-9]{0,20}(\d+)/);
  // fallback: count hire-card forms' sibling roster rows isn't reliable; use dedicated count below
  const rows = (body.match(/class="roster-row"|class="player-row"/g) || []).length;
  return { text: body, rows };
}

async function hireEleven(page, teamId, uniq) {
  await page.goto(`${BASE}/teams/${teamId}`);
  const templateIds = await page.$$eval(
    'form.hire-card input[name="template_id"]',
    els => els.map(e => e.value)
  );
  console.log(`team ${teamId} template ids available:`, templateIds);
  let hired = 0;
  const exhausted = new Set();
  let guard = 0;
  while (hired < 11 && exhausted.size < templateIds.length && guard < 100) {
    guard++;
    const tid = templateIds.find(t => !exhausted.has(t));
    const resp = await page.request.post(`${BASE}/teams/${teamId}/hire`, {
      form: { template_id: tid, player_name: `P${hired + 1}_${uniq}` },
    });
    const body = await resp.text();
    const failed = /Validation failed|error|exceed|maximum|cannot hire/i.test(body) && resp.status() === 200;
    if (!failed && (resp.ok() || resp.status() === 302)) {
      hired++;
    } else {
      console.log(`  template ${tid} exhausted/rejected (status ${resp.status()})`);
      exhausted.add(tid);
    }
  }
  console.log(`team ${teamId}: hired ${hired} players (${guard} attempts)`);
  return hired;
}

(async () => {
  const browser = await chromium.launch();
  const page = await browser.newPage();
  const log = (...a) => console.log(...a);
  const shot = async (name) => page.screenshot({ path: `/tmp/e2e_${name}.png`, fullPage: true });

  page.on('console', msg => { if (msg.type() === 'error') log('[console.error]', msg.text()); });
  page.on('pageerror', err => log('[pageerror]', err.message));

  const uniq = Date.now();

  try {
    await page.goto(`${BASE}/register`);
    await page.fill('input[name="name"]', `coach${uniq}`);
    await page.fill('input[name="email"]', `coach${uniq}@test.local`);
    await page.fill('input[name="password"]', 'testpass123');
    await page.click('button[type="submit"]');
    await page.waitForLoadState('networkidle');
    log('logged in as coach', uniq);

    await page.goto(`${BASE}/teams/create`);
    await page.fill('input[name="name"]', `Home ${uniq}`);
    (await page.$$('input[name="race_id"]'))[0] && await (await page.$$('input[name="race_id"]'))[0].check();
    await page.click('button[type="submit"]');
    await page.waitForLoadState('networkidle');
    const team1Id = page.url().match(/teams\/(\d+)/)[1];
    log('team1 id:', team1Id);

    await page.goto(`${BASE}/teams/create`);
    await page.fill('input[name="name"]', `Away ${uniq}`);
    (await page.$$('input[name="race_id"]'))[1] && await (await page.$$('input[name="race_id"]'))[1].check();
    await page.click('button[type="submit"]');
    await page.waitForLoadState('networkidle');
    const team2Id = page.url().match(/teams\/(\d+)/)[1];
    log('team2 id:', team2Id);

    await hireEleven(page, team1Id, uniq);
    await hireEleven(page, team2Id, uniq);

    await page.goto(`${BASE}/teams/${team1Id}`);
    await shot('10_team1_roster');
    log('team1 roster snippet:', (await page.textContent('body')).replace(/\s+/g, ' ').slice(0, 200));

    // --- Create match vs AI ---
    await page.goto(`${BASE}/matches/new`);
    await page.selectOption('select[name="home_team_id"]', team1Id);
    await page.check('input[name="vs_ai"]');
    await shot('11_match_form_filled');
    await page.click('button[type="submit"]');
    await page.waitForLoadState('networkidle');
    log('after match create:', page.url());
    await shot('12_after_match_create');
    if (!/\/matches\/\d+/.test(page.url())) {
      log('MATCH CREATE FAILED, body:', (await page.textContent('body')).slice(0, 500));
    } else {
      log('match page loaded, checking for canvas/pitch...');
      const canvas = await page.$('canvas');
      log('canvas present:', !!canvas);
      await page.waitForTimeout(1000);
      await shot('13_match_page_loaded');
    }

    await browser.close();
  } catch (e) {
    console.error('SMOKE TEST FAILED:', e.message);
    await shot('99_error');
    await browser.close();
    process.exit(1);
  }
})();
