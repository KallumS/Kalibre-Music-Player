// Draws Resources/Icon.svg into Resources/Icon.png at 1024 x 1024 with Chromium (Playwright):
//   node tools/render-icon.js
const { chromium } = require ('playwright');
const fs = require ('fs');
const path = require ('path');

(async () => {
    const root = path.join (__dirname, '..');
    const svg = fs.readFileSync (path.join (root, 'Resources/Icon.svg'), 'utf8');
    const browser = await chromium.launch();
    const page = await browser.newPage ({ viewport: { width: 1024, height: 1024 }, deviceScaleFactor: 1 });
    await page.setContent (`<html><body style="margin:0;background:transparent">${svg}</body></html>`);
    await page.locator ('svg').screenshot ({ path: path.join (root, 'Resources/Icon.png'), omitBackground: true });
    await browser.close();
})();
