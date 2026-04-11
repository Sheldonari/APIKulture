# APIkulture website

Static site for [https://www.apikulture.com](https://www.apikulture.com). Source for the app lives in the repository root; this folder is only the public web presence. Repository: [Sheldonari/APIKulture on Codeberg](https://codeberg.org/Sheldonari/APIKulture).

## Contents

| Path | Purpose |
|------|--------|
| `index.html` | Landing page |
| `css/style.css` | Styles |
| `assets/apikulture.svg` | Favicon and hero art (copy of `ui/icons/apikulture.svg` in the app tree) |
| `.domains` | Lists `www.apikulture.com` for Codeberg Pages custom domain setup |
| `robots.txt` | Crawler hints |

## Codeberg Pages

1. Enable **Codeberg Pages** for your repository and follow the current [Codeberg Pages documentation](https://docs.codeberg.org/codeberg-pages/) (branch name, Actions, or manual publish—workflows change over time).
2. Publish the **contents of this `website/` directory** as the site root (or configure your workflow to copy `website/*` into the published branch).
3. Links in `index.html` point to **https://codeberg.org/Sheldonari/APIKulture** (hero, footer, README links). Update them if the repo moves.
4. **Custom domain:** add DNS records as Codeberg documents. The `.domains` file here declares `www.apikulture.com`; you may need to copy it to the root of the published site depending on your Pages setup.

## Syncing the icon

When you change `ui/icons/apikulture.svg`, copy it to `website/assets/apikulture.svg` (or automate in CI) so the site and app stay aligned.
